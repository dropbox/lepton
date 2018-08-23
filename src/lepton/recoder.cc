/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include <time.h>
#include <stdio.h>
#include <iostream>
#ifdef _WIN32
#include <stdint.h>
#endif
#include "uncompressed_components.hh"
#include "recoder.hh"
#include "bitops.hh"
#include "lepton_codec.hh"
#include "vp8_decoder.hh"
#include "../io/BoundedMemWriter.hh"
#include "../vp8/util/memory.hh"
#define ENVLI(s,v)        ( ( v > 0 ) ? v : ( v - 1 ) + ( 1 << s ) )

int next_mcuposn(int* cmp, int* dpos, int* rstw );
extern unsigned char ujgversion;
extern BaseDecoder *g_decoder;
extern UncompressedComponents colldata; // baseline sorted DCT coefficients

extern int8_t padbit; // signed


extern Sirikata::Array1d<int, 4> cs_cmp; // component numbers  in current scan
extern Sirikata::Array1d<componentInfo, 4> cmpnfo;

extern bool embedded_jpeg;
extern int grbs;   // size of garbage
extern uint32_t            hdrs;   // size of header
extern Sirikata::Array1d<Sirikata::Array1d<unsigned short, 64>, 4> qtables; // quantization tables
extern Sirikata::Array1d<Sirikata::Array1d<huffCodes, 4>, 2> hcodes; // huffman codes
extern Sirikata::Array1d<Sirikata::Array1d<huffTree, 4>, 2> htrees; // huffman decoding trees
extern Sirikata::Array1d<Sirikata::Array1d<unsigned char, 4>, 2> htset;// 1 if huffman table is set

extern unsigned char* grbgdata;    // garbage data
extern unsigned char* hdrdata;   // header data
extern int            rsti;
extern int mcuv; // mcus per line
extern unsigned int mcuh; // mcus per column
extern int mcuc; // count of mcus
extern std::vector<unsigned char> rst_err;   // number of wrong-set RST markers per scan
extern bool rst_cnt_set;
extern std::vector<unsigned int> rst_cnt;
extern int prefix_grbs;   // size of prefix garbage
extern unsigned char *prefix_grbgdata; // the actual prefix garbage: if present, hdrdata not serialized

static void nop(){}

void check_decompression_memory_bound_ok();

bool parse_jfif_jpg( unsigned char type, unsigned int len, uint32_t alloc_len, unsigned char* segment );
#define B_SHORT(v1,v2)    ( ( ((int) v1) << 8 ) + ((int) v2) )

int find_aligned_end_64_scalar(const int16_t *block) {
    int end = 63;
    while (end && !block[end]) {
        --end;
    }
    return end;
}

#if defined(__AVX2__) && !defined(USE_SCALAR)
int find_aligned_end_64_avx2(const int16_t *block) {
    uint32_t mask = 0;
    int iter;
    for (iter = 48; iter >= 0; iter -=16) {
        __m256i row = _mm256_load_si256((const __m256i*)(const char*)(block + iter));
        __m256i row_cmp = _mm256_cmpeq_epi16(row, _mm256_setzero_si256());
        mask = _mm256_movemask_epi8(row_cmp);
        if (mask != 0xffffffffU) {
            break;
        }
    }
    if (mask == 0xffffffffU) {
        dev_assert(find_aligned_end_64_scalar(block) == 0);
        return 0;
    }
    unsigned int bitpos = 32 - __builtin_clz((~mask) & 0xffffffffU);
    int retval = iter + ((bitpos >> 1) - 1) ;

    dev_assert(retval == find_aligned_end_64_scalar(block));
    return retval;
}
#elif !defined(USE_SCALAR)
/**
 * SSE4.2 Based implementation for machines that don't support AVX2
 */
int find_aligned_end_64_sse42(const int16_t *block) {
    unsigned int mask = 0;
    int iter;
    for (iter = 56; iter >= 0; iter -=8) {
        __m128i row = _mm_load_si128((__m128i*)(block + iter));
        __m128i row_cmp = _mm_cmpeq_epi16(row, _mm_setzero_si128());
        mask = _mm_movemask_epi8(row_cmp);
        if (mask != 0xffff) {
            break;
        }
    }
    if (mask == 0xffff) {
        dev_assert(find_aligned_end_64_scalar(block) == 0);
        return 0;
    }
    unsigned int bitpos = 32 - __builtin_clz((~mask) & 0xffff);
    int retval = iter + ((bitpos >> 1) - 1) ;

    dev_assert(retval == find_aligned_end_64_scalar(block));
    return retval;
}
#endif

int find_aligned_end_64(const int16_t *block) {
#if defined(USE_SCALAR)
    return find_aligned_end_64_scalar(block);
#elif defined(__AVX2__)
    return find_aligned_end_64_avx2(block);
#elif defined(__SSE_4_2)
    return find_aligned_end_64_sse42(block);
#else
    return find_aligned_end_64_scalar(block);
#endif
}

static bool aligned_memchr16ff(const unsigned char *local_huff_data) {
#ifdef USE_SCALAR
    return memchr(local_huff_data, 0xff, 16) != NULL;
#else
    __m128i buf = _mm_load_si128((__m128i const*)local_huff_data);
    __m128i ff = _mm_set1_epi8(-1);
    __m128i res = _mm_cmpeq_epi8(buf, ff);
    uint32_t movmask = _mm_movemask_epi8(res);
    bool retval = movmask != 0x0;
    dev_assert (retval == (memchr(local_huff_data, 0xff, 16) != NULL));
    return retval;
#endif
}


/**
 * This function takes local byte-aligned huffman data and writes it to the file
 * This function escapes any 0xff bytes found in the huffman data
 */
template<class OutputWriter>
void escape_0xff_huffman_and_write(OutputWriter* str_out,
                                   const unsigned char * local_huff_data,
                                   unsigned int max_byte_coded) {
    unsigned int progress_ipos = 0;
    //write a single scan
    {
        // write & expand huffman coded image data
        const unsigned char stv = 0x00; // 0xFF stuff value
        while(true) {
            if (__builtin_expect(!(progress_ipos + 15 < max_byte_coded), 0)) {
                break;
            }
            if ( __builtin_expect(aligned_memchr16ff(local_huff_data + progress_ipos), 0)){
                // insert restart markers if needed
                for (int veci = 0 ; veci < 16; ++veci, ++progress_ipos ) {
                    uint8_t byte_to_write = local_huff_data[progress_ipos];
                    str_out->write_byte(byte_to_write);
                    // check current byte, stuff if needed
                    if (__builtin_expect(byte_to_write == 0xFF, 0)) {
                        write_byte_bill(Billing::DELIMITERS, false, 1);
                        str_out->write_byte(stv);
                    }
                }
            } else {
                str_out->write(local_huff_data + progress_ipos, 16);
                progress_ipos+=16;
            }
        }
        for ( ; ; progress_ipos++ ) {
            if (__builtin_expect(!(progress_ipos < max_byte_coded), 0)) {
                break;
            }
            uint8_t byte_to_write = local_huff_data[progress_ipos];
            str_out->write_byte(byte_to_write);
            // check current byte, stuff if needed
            if (__builtin_expect(byte_to_write == 0xFF, 0)) {
                write_byte_bill(Billing::DELIMITERS, false, 1);
                str_out->write_byte(stv);
            }
        }
    }
}

/* -----------------------------------------------
    calculates next position for MCU
    ----------------------------------------------- */
int next_mcupos( int* mcu, int* cmp, int* csc, int* sub, int* dpos, int* rstw, int cs_cmpc)
{
    int sta = 0; // status
    unsigned int local_mcuh = mcuh;
    unsigned int local_mcu = *mcu;
    unsigned int local_cmp = *cmp;
    unsigned int local_sub;
    // increment all counts where needed
    if ( (local_sub = ++(*sub) ) >= (unsigned int)cmpnfo[local_cmp].mbs) {
        local_sub = (*sub) = 0;

        if ( ( ++(*csc) ) >= cs_cmpc ) {
            (*csc) = 0;
            local_cmp = (*cmp) = cs_cmp[ 0 ];
            local_mcu = ++(*mcu);
            if ( local_mcu >= (unsigned int)mcuc ) {
                sta = 2;
            } else if ( rsti > 0 ){
                if ( --(*rstw) == 0 ) {
                    sta = 1;
                }
            }
        }
        else {
            local_cmp = (*cmp) = cs_cmp[(*csc)];
        }
    }
    unsigned int sfh = cmpnfo[local_cmp].sfh;
    unsigned int sfv = cmpnfo[local_cmp].sfv;
    // get correct position in image ( x & y )
    if ( sfh > 1 ) { // to fix mcu order
        unsigned int mcu_o_mcuh = local_mcu / local_mcuh;
        unsigned int sub_o_sfv = local_sub / sfv;
        unsigned int mcu_mod_mcuh = local_mcu - mcu_o_mcuh * local_mcuh;
        unsigned int sub_mod_sfv = local_sub - sub_o_sfv * sfv;
        unsigned int local_dpos = mcu_o_mcuh * sfh + sub_o_sfv;
        local_dpos *= cmpnfo[local_cmp].bch;
        local_dpos += mcu_mod_mcuh * sfv + sub_mod_sfv;
        *dpos = local_dpos;
    }
    else if ( sfv > 1 ) {
        // simple calculation to speed up things if simple fixing is enough
        (*dpos) = local_mcu * cmpnfo[local_cmp].mbs + local_sub;
    }
    else {
        // no calculations needed without subsampling
        (*dpos) = (*mcu);
    }

    return sta;
}

// -----------------------------------------------
//    sequential block encoding routine
// ----------------------------------------------- 
int encode_block_seq( abitwriter* huffw, huffCodes* dctbl, huffCodes* actbl, short* block )
{
    unsigned short n;
    unsigned char  s;
    int bpos;
    int hc;
    short tmp;


    // encode DC
    tmp = block[ 0 ];
    s = uint16bit_length(tmp > 0 ? tmp : -tmp);
    n = ENVLI( s, tmp );
    huffw->write( dctbl->cval[ s ], dctbl->clen[ s ] );
    write_multi_bit_bill(dctbl->clen[s], false, Billing::EXP0_DC, Billing::EXPN_DC);
    huffw->write( n, s );
    if (s) {
        write_bit_bill(Billing::RES_DC, false, s - 1);
        write_bit_bill(Billing::SIGN_DC, false, 1);
    }

    signed z = -1;
    // encode AC
    z = 0;
    int end = find_aligned_end_64(block);
    for ( bpos = 1; bpos <= end; bpos++ )
    {
        // if nonzero is encountered
        tmp = block[bpos];
        if (tmp == 0) {
            ++z;
            continue;
        }
        // vli encode
        s = nonzero_bit_length(tmp > 0 ? tmp : -tmp);
        n = ENVLI(s, tmp);
        hc = ( ( (z & 0xf) << 4 ) + s );
        if (__builtin_expect(z & 0xf0, 0)) {
            // write remaining zeroes
            do {
                huffw->write( actbl->cval[ 0xF0 ], actbl->clen[ 0xF0 ] );
                write_multi_bit_bill(actbl->clen[0xF0], false,
                                     is_edge(bpos) ? Billing::BITMAP_EDGE : Billing::BITMAP_7x7, // this is pure bitmap
                                     is_edge(bpos) ? Billing::BITMAP_EDGE : Billing::BITMAP_7x7);
                z -= 16;
            } while ( z & 0xf0 );
        }
        // write to huffman writer
        huffw->write( actbl->cval[ hc ], actbl->clen[ hc ] );
        write_multi_bit_bill(actbl->clen[hc], false,
                             is_edge(bpos) ? Billing::BITMAP_EDGE : Billing::BITMAP_7x7, // this is pure bitmap
                             is_edge(bpos) ? Billing::EXPN_EDGE : Billing::EXPN_7x7);
        huffw->write( n, s );
        if (s) {
            write_bit_bill(is_edge(bpos) ? Billing::RES_EDGE : Billing::RES_7x7, false, s - 1);
            write_bit_bill(is_edge(bpos) ? Billing::SIGN_EDGE : Billing::SIGN_7x7, false, 1);
        }

        // reset zeroes
        z = 0;
    }
    // write eob if needed
    if ( end != 63 ) {
        huffw->write( actbl->cval[ 0x00 ], actbl->clen[ 0x00 ] );
        write_eob_bill(end, false, actbl->clen[ 0x00 ]);
    }

    return end + 1;
}

template <class OutputWriter>
bool recode_one_mcu_row(abitwriter *huffw, int mcu,
                        OutputWriter*str_out,
                        Sirikata::Array1d<int16_t, (size_t)ColorChannel::NumBlockTypes> &lastdc,
                        const BlockBasedImagePerChannel<true> framebuffer) {
    int cmp = cs_cmp[ 0 ];
    int csc = 0, sub = 0;
    int mcumul = cmpnfo[ cmp ].sfv * cmpnfo[ cmp ].sfh;
    int dpos = mcu * mcumul;
    int rstw = rsti ? rsti - mcu % rsti : 0;
    unsigned int cumulative_reset_markers = rstw ? mcu / rsti : 0;
    unsigned char cmpc = 0;
    for (;cmpc < framebuffer.size() && framebuffer[cmpc] != NULL; ++cmpc) {
    }
    Sirikata::Aligned256Array1d<int16_t, 64> block; // store block for coeffs
    bool end_of_row = false;
    // JPEG imagedata encoding routines
    while (!end_of_row) {

        // (re)set status
        int sta = 0;

        // ---> sequential interleaved encoding <---
        while ( sta == 0 ) {
            // copy from colldata
            const AlignedBlock &aligned_block = framebuffer[cmp]->raster(dpos);
            //fprintf(stderr, "Reading from cmp(%d) dpos %d\n", cmp, dpos);
            for ( int bpos = 0; bpos < 64; bpos++ ) {
                block[bpos] = aligned_block.coefficients_zigzag(bpos);
            }

            int16_t dc = block[0];
            // diff coding for dc
            block[ 0 ] -= lastdc[ cmp ];
            lastdc[ cmp ] = dc;

            // encode block
            int eob = encode_block_seq(huffw,
                                       &(hcodes[ 0 ][ cmpnfo[cmp].huffdc ]),
                                       &(hcodes[ 1 ][ cmpnfo[cmp].huffac ]),
                                       block.begin() );
            int old_mcu = mcu;
            // check for errors, proceed if no error encountered
            if ( eob < 0 ) sta = -1;
            else if (__builtin_expect(framebuffer.size() == 1 || framebuffer[1] == NULL, 0)) {
                sta = next_mcuposn(&cmp, &dpos, &rstw );
                mcu = dpos / mcumul;
            } else {
                sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw, cmpc); // we can pass in cmpc instead of CMPC
            }
            if (sta == 0 && huffw->no_remainder()) {
                escape_0xff_huffman_and_write(str_out, huffw->peekptr(), huffw->getpos());
                huffw->reset();
            }
            if (str_out->has_exceeded_bound()) {
                sta = 2;
            }
            if (old_mcu != mcu && mcu % mcuh == 0) {
                end_of_row = true;
                if (sta == 0) {
                    return true;
                }
            }
        }

        // pad huffman writer
        huffw->pad( padbit );
        if (huffw->no_remainder()) {
            escape_0xff_huffman_and_write(str_out, huffw->peekptr(), huffw->getpos());
            huffw->reset();
        }
        // evaluate status
        if ( sta == -1 ) { // status -1 means error
            delete huffw;
            return false;
        }
        else if ( sta == 2 ) { // status 2 means done
            break; // leave decoding loop, everything is done here
        }
        else if ( sta == 1 ) { // status 1 means restart
            if ( rsti > 0 ) {
                if (rst_cnt.empty() || (!rst_cnt_set) || cumulative_reset_markers < rst_cnt[0]) {
                    const unsigned char rst = 0xD0 + ( cumulative_reset_markers & 7);
                    str_out->write_byte(0xFF);
                    str_out->write_byte(rst);
                    cumulative_reset_markers++;
                    write_byte_bill(Billing::DELIMITERS, false, 2);
                }
                // (re)set rst wait counter
                rstw = rsti;
                // (re)set last DCs for diff coding
                lastdc.memset(0);
            }
        }
        always_assert(huffw->no_remainder() && "this should have been padded");
    }
    return true;
}

unsigned int handle_initial_segments( bounded_iostream * const str_out )
{
    unsigned int byte_position = 0;

    while ( true ) {
        /* step 1: have we exhausted the headers without reaching the scan? */
        if ( static_cast<uint32_t>( byte_position + 3 ) >= hdrs ) {
            std::cerr << "overran headers\n";
            return -1;
        }

        /* step 2: verify we are at the start of a segment header */
        if ( hdrdata[ byte_position ] != 0xff ) {
            std::cerr << "not start of segment\n";
            return -1;
        }

        /* step 3: get info about the segment */
        const unsigned char type = hdrdata[ byte_position + 1 ];
        const unsigned int len = 2 + B_SHORT( hdrdata[ byte_position + 2 ],
                                              hdrdata[ byte_position + 3 ] );

        /* step 4: if it's a DHT (0xC4), DRI (0xDD), or SOS (0xDA), parse to mutable globals */
        if ( type == 0xC4 || type == 0xDD || type == 0xDA ) {
            /* XXX make sure parse_jfif_jpg can't overrun hdrdata */
            if ( !parse_jfif_jpg( type, len, hdrs - byte_position > len ? len : hdrs - byte_position, hdrdata + byte_position ) ) { return -1; }
        }

        /* step 5: we parsed the header -- accumulate byte position */
        byte_position += len;

        /* step 6: if it's an SOS (start of scan),
           then return the byte position -- done with initial headers */

        if ( type == 0xDA ) {
            if (prefix_grbgdata) {
                str_out->write(prefix_grbgdata, prefix_grbs);
            }
            if (embedded_jpeg || !prefix_grbgdata) {
                unsigned char SOI[ 2 ] = { 0xFF, 0xD8 }; // SOI segment
                // write SOI
                str_out->write( SOI, 2 );
                str_out->write( hdrdata, byte_position );
            }
            return byte_position; /* ready for the scan */
        }
    }
}

void abitwriter::debug() const
{
    using std::cerr;

    cerr << "abitwriter: no_remainder=" << no_remainder() << ", getpos=" << getpos() << ", bits="<<cbit2<<", buf="<<std::hex<<buf<<std::dec<<"\n";
}

//currently returns the overhang byte and num_overhang_bits -- these will be factored out when the encoder serializes them
template<class BoundedWriter>
ThreadHandoff recode_row_range(BoundedWriter *stream_out,
                               BlockBasedImagePerChannel<true> &framebuffer,
                               int mcuv,
                               const ThreadHandoff &thread_handoff,
                               Sirikata::Array1d<uint32_t, (uint32_t)ColorChannel::NumBlockTypes> max_coded_heights,
                               Sirikata::Array1d<uint32_t, (uint32_t)ColorChannel::NumBlockTypes> component_size_in_blocks,
                               int physical_thread_id,
                               int logical_thread_id,
                               abitwriter *huffw) {
    ThreadHandoff retval = thread_handoff;

    huffw->fillbit = padbit;
    huffw->reset_from_overhang_byte_and_num_bits(retval.overhang_byte,
                                                retval.num_overhang_bits);
    int decode_index = 0;
    while (true) {
        LeptonCodec_RowSpec cur_row = LeptonCodec_row_spec_from_index(decode_index++,
                                                                        framebuffer,
                                                                        mcuv,
                                                                        max_coded_heights);
        /*
        fprintf(stderr, "%d] (%d) %d - %d  %d[%d]  [%d %d %d]\n",
                decode_index,
                logical_thread_id,
                thread_handoff.luma_y_start,
                thread_handoff.luma_y_end,
                retval.overhang_byte,
                retval.num_overhang_bits,
                retval.last_dc[0],
                retval.last_dc[1],
                retval.last_dc[2]); 
        */
        if (cur_row.done) {
            break;
        }
        if (cur_row.skip) {
            continue;
        }
        if (cur_row.min_row_luma_y < thread_handoff.luma_y_start) {
            continue;
        }
        if (cur_row.next_row_luma_y > thread_handoff.luma_y_end) {
            break; // we're done here
        }
        g_decoder->decode_row(physical_thread_id,
                              framebuffer,
                              component_size_in_blocks,
                              cur_row.component,
                              cur_row.curr_y);
        if (cur_row.last_row_to_complete_mcu) {
            if ( !recode_one_mcu_row(huffw,
                                     cur_row.mcu_row_index * mcuh,
                                     stream_out,
                                     retval.last_dc,
                                     framebuffer) ) {
                custom_exit(ExitCode::CODING_ERROR);
            }
            const unsigned char * flushed_data = huffw->partial_bytewise_flush();
            escape_0xff_huffman_and_write(stream_out, flushed_data, huffw->getpos() );
            huffw->reset_crystallized_bytes();
            if (!huffw->bound_reached()) {
                retval.num_overhang_bits = huffw->get_num_overhang_bits();
                retval.overhang_byte = huffw->get_overhang_byte();
            } else {
                retval.num_overhang_bits = 0;
                retval.overhang_byte = 0;
            }
            if ( huffw->error ) {
                custom_exit(ExitCode::CODING_ERROR);
            }
        }
    }
    return retval;
}

std::pair<int, int> logical_thread_range_from_physical_thread_id(int physical_thread_id, int num_logical_threads) {
    int num_physical_threads = g_threaded ? NUM_THREADS : 1;

    int logical_thread_start = (physical_thread_id * num_logical_threads) / num_physical_threads;
    int logical_thread_end = std::min(((physical_thread_id  + 1) * num_logical_threads) / num_physical_threads,
                                      num_logical_threads);
    if (num_logical_threads < num_physical_threads) {
        // this is an optimization so we don't have to call the reset logic as often
        logical_thread_start = std::min(physical_thread_id, num_logical_threads);
        logical_thread_end = std::min(physical_thread_id + 1, num_logical_threads);
    }
    return std::pair<int, int>(logical_thread_start, logical_thread_end);
}
template<class BoundedWriter>
void recode_physical_thread(BoundedWriter *stream_out,
                            BlockBasedImagePerChannel<true> &framebuffer,
                            int mcuv,
                            const std::vector<ThreadHandoff> &thread_handoffs,
                            Sirikata::Array1d<uint32_t,
                                              (uint32_t)ColorChannel::NumBlockTypes> max_coded_heights,
                            Sirikata::Array1d<uint32_t,
                                              (uint32_t)ColorChannel::NumBlockTypes> component_size_in_blocks,
                            int physical_thread_id,
                            abitwriter *huffw) {
    int num_logical_threads = thread_handoffs.size();

    int logical_thread_start, logical_thread_end;
    std::tie(logical_thread_start, logical_thread_end)
        = logical_thread_range_from_physical_thread_id(physical_thread_id, num_logical_threads);
    //fprintf(stderr, "Worker %d running %d - %d - %d\n", physical_thread_id, logical_thread_start, (int)thread_handoffs.size(), logical_thread_end);
    always_assert((size_t)logical_thread_start < thread_handoffs.size()
                  && (size_t)logical_thread_end <= thread_handoffs.size());
    ThreadHandoff th = thread_handoffs[logical_thread_start];
    size_t original_bound = stream_out->get_bound();
    bool changed_bounds = false;
    for (int logical_thread_id = logical_thread_start; logical_thread_id < logical_thread_end; ++logical_thread_id) {
        TimingHarness::timing[logical_thread_id % MAX_NUM_THREADS][TimingHarness::TS_ARITH_STARTED] = TimingHarness::get_time_us();
        if (thread_handoffs[logical_thread_id].is_legacy_mode()) {
            if (logical_thread_id == logical_thread_start) {
                th.num_overhang_bits = 0; // clean start
            }
            ThreadHandoff tmp = thread_handoffs[logical_thread_id];
            tmp.overhang_byte = th.overhang_byte;
            tmp.num_overhang_bits = th.num_overhang_bits;
            memcpy(tmp.last_dc.begin(), th.last_dc.begin(), sizeof(th.last_dc));
            th = tmp; // copy the dynamic data in
        } else {
            dev_assert(memcmp(thread_handoffs[logical_thread_id].last_dc.begin(), th.last_dc.begin(), sizeof(th.last_dc)) == 0);
            dev_assert(th.overhang_byte == thread_handoffs[logical_thread_id].overhang_byte);
            dev_assert(th.num_overhang_bits == thread_handoffs[logical_thread_id].num_overhang_bits);
            th = thread_handoffs[logical_thread_id];
            // in the v1 encoding, the first thread's output is unbounded in size but
            // following threads are bound to their segment_size.
            // In the future (v2 and beyond) all threads are bound by their segment size
            bool legacy_truncation_mode = get_current_file_lepton_version() == 1;
            bool worker_thread_and_many_to_one_mapping =
                logical_thread_end - logical_thread_start != 1
                && logical_thread_id != 0;

            if (worker_thread_and_many_to_one_mapping || !legacy_truncation_mode) {
                size_t new_bound = stream_out->bytes_written() + thread_handoffs[logical_thread_id].segment_size;
                if (new_bound < original_bound) {
                    stream_out->set_bound(new_bound);
                    changed_bounds = true;
                } else if (stream_out->get_bound() != original_bound) {
                    stream_out->set_bound(original_bound);
                }
            }
        }
        //if (logical_thread_id != physical_thread_id) {
        g_decoder->clear_thread_state(logical_thread_id, physical_thread_id, framebuffer);

        //}
        ThreadHandoff outth = recode_row_range(stream_out,
                                               framebuffer,
                                               mcuv,
                                               th,
                                               max_coded_heights,
                                               component_size_in_blocks,
                                               physical_thread_id,
                                               logical_thread_id,
                                               huffw);        
        if (logical_thread_id + 1 < num_logical_threads
            && !thread_handoffs[logical_thread_id + 1].is_legacy_mode()) {
            if (thread_handoffs[logical_thread_id+1].luma_y_start !=
                thread_handoffs[logical_thread_id+1].luma_y_end || ujgversion ==1 ) {
            // make sure we computed the same item that was stored
                if (g_threaded) {
                    always_assert(outth.num_overhang_bits ==  thread_handoffs[logical_thread_id + 1].num_overhang_bits);
                    always_assert(outth.overhang_byte ==  thread_handoffs[logical_thread_id + 1].overhang_byte);
                }
                if (g_threaded || thread_handoffs[logical_thread_id + 1].segment_size > 1) {
                    always_assert(memcmp(outth.last_dc.begin(), thread_handoffs[logical_thread_id + 1].last_dc.begin(), sizeof(outth.last_dc)) == 0);
                }
            }
            if (physical_thread_id > 0 && stream_out->bytes_written()) { // if 0 are written the bound is not tight
                always_assert(stream_out->get_bound() == stream_out->bytes_written());
            }
        }
        th = outth;
        TimingHarness::timing[logical_thread_id % MAX_NUM_THREADS][TimingHarness::TS_ARITH_FINISHED] = TimingHarness::get_time_us();
    }
    if (changed_bounds) {
        stream_out->set_bound(original_bound);
    }
}
void recode_physical_thread_wrapper(Sirikata::BoundedMemWriter *stream_out,
                            BlockBasedImagePerChannel<true> &framebuffer,
                            int mcuv,
                            const std::vector<ThreadHandoff> &thread_handoffs,
                            Sirikata::Array1d<uint32_t,
                                              (uint32_t)ColorChannel::NumBlockTypes> max_coded_heights,
                            Sirikata::Array1d<uint32_t,
                                              (uint32_t)ColorChannel::NumBlockTypes> component_size_in_blocks,
                            int physical_thread_id,
                            abitwriter * huffw) {
    recode_physical_thread(stream_out,
                           framebuffer,
                           mcuv,
                           thread_handoffs,
                           max_coded_heights,
                           component_size_in_blocks,
                           physical_thread_id,
                           huffw);
}
void recode_physical_first_thread_wrapper(bounded_iostream*stream_out,
                            BlockBasedImagePerChannel<true> &framebuffer,
                            int mcuv,
                            const std::vector<ThreadHandoff> &thread_handoffs,
                            Sirikata::Array1d<uint32_t,
                                              (uint32_t)ColorChannel::NumBlockTypes> max_coded_heights,
                            Sirikata::Array1d<uint32_t,
                                              (uint32_t)ColorChannel::NumBlockTypes> component_size_in_blocks,
                            int physical_thread_id,
                            abitwriter * huffw) {
    recode_physical_thread(stream_out,
                           framebuffer,
                           mcuv,
                           thread_handoffs,
                           max_coded_heights,
                           component_size_in_blocks,
                           physical_thread_id,
                           huffw);
}
/* -----------------------------------------------
    JPEG encoding routine
    ----------------------------------------------- */
bool recode_baseline_jpeg(bounded_iostream*str_out,
                          int max_file_size)
{
    always_assert(max_file_size > grbs && "Lepton only supports files that have some scan data");
    // if the entire file is garbage, lepton will not support the file
    unsigned int local_bound = max_file_size - grbs;
    str_out->set_bound(local_bound);

    /* step 1: handle the initial segments */
    unsigned int byte_position = handle_initial_segments( str_out );
    if ( byte_position == static_cast<unsigned int>( -1 ) ) {
        return false;
    }
    /* step 2: setup multithreaded decoder with framebuffer for each */
    Sirikata::Array1d<uint32_t,
                      (size_t)ColorChannel::NumBlockTypes> max_coded_heights
        = colldata.get_max_coded_heights();
    Sirikata::Array1d<uint32_t, (uint32_t)ColorChannel::NumBlockTypes> component_size_in_blocks
        = colldata.get_component_size_in_blocks();
    int mcu_count_vertical = colldata.get_mcu_count_vertical();
    Sirikata::Array1d<BlockBasedImagePerChannel<true>,
                      MAX_NUM_THREADS> framebuffer;

    for (size_t thread_id = 0; thread_id < NUM_THREADS; ++thread_id) {
        for(int cmp = 0; cmp < colldata.get_num_components(); ++cmp) {
            framebuffer[thread_id][cmp] = new BlockBasedImageBase<true>;
            colldata.allocate_channel_framebuffer(cmp,
                                                  framebuffer[thread_id][cmp],
                                                  true);
        }
        if (!g_threaded) {
            break;
        }
    }
    std::vector<ThreadHandoff> luma_bounds = g_decoder->initialize_baseline_decoder(&colldata,
                                                                                    framebuffer);

    if (luma_bounds.size() && luma_bounds[0].is_legacy_mode()) {
        g_threaded = false;
    }
    g_decoder->reset_all_comm_buffers();
    for (unsigned int physical_thread_id = 1; physical_thread_id < (g_threaded ? NUM_THREADS : 1); ++physical_thread_id) {
        int logical_thread_start, logical_thread_end;
        std::tie(logical_thread_start, logical_thread_end)
            = logical_thread_range_from_physical_thread_id(physical_thread_id, luma_bounds.size());
        for (int log_thread = logical_thread_start; log_thread < logical_thread_end; ++log_thread) {
            g_decoder->map_logical_thread_to_physical_thread(log_thread, physical_thread_id);
        }
    }
    /* step 3: decode the scan, row by row */
    std::tuple<uint8_t, uint8_t, Sirikata::Array1d<int16_t, (size_t)ColorChannel::NumBlockTypes> > overhang_byte_and_bit_count;
    std::get<0>(overhang_byte_and_bit_count) = 0;
    std::get<1>(overhang_byte_and_bit_count) = 0;
    std::get<2>(overhang_byte_and_bit_count).memset(0);
    Sirikata::JpegAllocator<uint8_t> alloc;
    Sirikata::Array1d<Sirikata::BoundedMemWriter, MAX_NUM_THREADS - 1> local_buffers;
    Sirikata::Array1d<abitwriter *, MAX_NUM_THREADS> huffws;
    huffws.memset(0);
    for (size_t i = 0; i < NUM_THREADS; ++i) {
        huffws[i] = new abitwriter(65536, max_file_size);
    }

    for (unsigned int physical_thread_id = 0; physical_thread_id < (g_threaded ? NUM_THREADS : 1); ++physical_thread_id) {
        int logical_thread_start, logical_thread_end;
        std::tie(logical_thread_start, logical_thread_end)
            = logical_thread_range_from_physical_thread_id(physical_thread_id, luma_bounds.size());
        for (int logical_thread_id = logical_thread_start; logical_thread_id < logical_thread_end; ++logical_thread_id) {
            g_decoder->map_logical_thread_to_physical_thread(logical_thread_id, physical_thread_id);
        }

    }
    if (NUM_THREADS != 1 && g_threaded) {
        for (unsigned int physical_thread_id = 0; physical_thread_id < (g_threaded ? g_decoder->getNumWorkers() : 1); ++physical_thread_id) {
            g_decoder->getWorker(physical_thread_id)->work = nop;
        }
        for (unsigned int physical_thread_id = 0; physical_thread_id < (g_threaded ? NUM_THREADS : 1); ++physical_thread_id) {
            int work_size = 0;
            unsigned int physical_thread_offset = physical_thread_id;
            int logical_thread_start, logical_thread_end;
            std::tie(logical_thread_start, logical_thread_end)
                = logical_thread_range_from_physical_thread_id(physical_thread_id, luma_bounds.size());

            for (int logical_thread_id = logical_thread_start; logical_thread_id < logical_thread_end; ++logical_thread_id) {
                work_size += luma_bounds[logical_thread_id].segment_size;
            }
            if (!work_size) {
                work_size = max_file_size;
            }
            if (physical_thread_id != 0) {
                local_buffers[physical_thread_offset - 1].set_bound(work_size);
                auto work_fn = std::bind(&recode_physical_thread_wrapper,
                                         &local_buffers[physical_thread_id - 1],
                                         framebuffer[physical_thread_id],
                                         mcu_count_vertical,
                                         luma_bounds,
                                         max_coded_heights,
                                         component_size_in_blocks,
                                         physical_thread_id,
                                         huffws[physical_thread_id]);
                g_decoder->getWorker(physical_thread_offset)->work = work_fn;
            } else {
                auto work_fn = std::bind(&recode_physical_first_thread_wrapper,
                                         str_out,
                                         framebuffer[physical_thread_id],
                                         mcu_count_vertical,
                                         luma_bounds,
                                         max_coded_heights,
                                         component_size_in_blocks,
                                         physical_thread_id,
                                         huffws[physical_thread_id]);
                g_decoder->getWorker(physical_thread_offset)->work = work_fn;
            }
            g_decoder->getWorker(physical_thread_offset)->activate_work();
        }
        g_decoder->flush();
        for (unsigned int physical_thread_id = 0; physical_thread_id < (g_threaded ? NUM_THREADS : 1); ++physical_thread_id) {
            unsigned int physical_thread_offset = physical_thread_id;
            TimingHarness::timing[physical_thread_id][TimingHarness::TS_THREAD_WAIT_STARTED] = TimingHarness::get_time_us();
            
            g_decoder->getWorker(physical_thread_offset)->main_wait_for_done();
            TimingHarness::timing[physical_thread_id][TimingHarness::TS_THREAD_WAIT_FINISHED] =
                TimingHarness::timing[physical_thread_id][TimingHarness::TS_JPEG_RECODE_STARTED] = TimingHarness::get_time_us();
            if (physical_thread_id > 0) { // the first guy goes right to stdout
                size_t bytes_to_copy = local_buffers[physical_thread_id - 1].bytes_written();
                if (bytes_to_copy) {
                    local_bound -= bytes_to_copy;
                    str_out->write(&local_buffers[physical_thread_id - 1].buffer()[0],
                                   bytes_to_copy);
                }
            }
            TimingHarness::timing[physical_thread_id][TimingHarness::TS_JPEG_RECODE_FINISHED] = TimingHarness::get_time_us();
        }
    } else {
        TimingHarness::timing[0][TimingHarness::TS_JPEG_RECODE_STARTED] = TimingHarness::get_time_us();
        recode_physical_thread(str_out,
                               framebuffer[0],
                               mcu_count_vertical,
                               luma_bounds,
                               max_coded_heights,
                               component_size_in_blocks,
                               0,
                               huffws[0]);
    }

    if (!rst_err.empty()) {
        unsigned int cumulative_reset_markers = rsti ? (mcuh * mcuv - 1)/ rsti : 0;
        for (unsigned char i = 0; i < rst_err[0]; ++i) {
            const unsigned char mrk = 0xFF;
            const unsigned char rst = 0xD0 + ( (cumulative_reset_markers + i) & 7 );
            str_out->write_byte(mrk);
            str_out->write_byte(rst);

        }
    }

    /* step 3: blit any trailing data */
    if (!str_out->has_reached_bound() ) {
        str_out->write( hdrdata + byte_position, hdrs - byte_position );
    }
    if (ujgversion != 1) {
        for (size_t i = 0; i < NUM_THREADS; ++i) {
            delete huffws[i];
        }
        huffws.memset(0);
        for (size_t thread_id = 0; thread_id < NUM_THREADS; ++thread_id) {
            for(int cmp = 0; cmp < colldata.get_num_components(); ++cmp) {
                framebuffer[thread_id][cmp]->reset();
                delete framebuffer[thread_id][cmp];
                framebuffer[thread_id][cmp] = NULL;
            }
            if (!g_threaded) {
                break;
            }
        }
    }
    check_decompression_memory_bound_ok();

    // write EOI (now EOI is stored in garbage of at least 2 bytes)
    // this guarantees that we can stop the write in time.
    // if it used too much memory
    // str_out->write( EOI, 1, 2 );
    str_out->set_bound(max_file_size);
    check_decompression_memory_bound_ok();
    // write garbage if needed
    if ( grbs > 0 )
        str_out->write( grbgdata, grbs );
    check_decompression_memory_bound_ok();
    str_out->flush();
    TimingHarness::timing[0][TimingHarness::TS_JPEG_RECODE_FINISHED] = TimingHarness::get_time_us();

    // errormessage if write error
    if ( str_out->chkerr() ) {
        fprintf( stderr, "write error, possibly drive is full" );
        return false;
    }
    return true;
}
