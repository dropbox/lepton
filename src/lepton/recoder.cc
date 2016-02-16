/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include <time.h>
#include <stdio.h>
#include <iostream>

#include "uncompressed_components.hh"
#include "recoder.hh"
#include "bitops.hh"
#include "lepton_codec.hh"
#include "../io/BoundedMemWriter.hh"
int encode_block_seq( abitwriter* huffw, huffCodes* dctbl, huffCodes* actbl, short* block);
int next_mcupos( int* mcu, int* cmp, int* csc, int* sub, int* dpos, int* rstw );
extern BaseDecoder *g_decoder;
extern UncompressedComponents colldata; // baseline sorted DCT coefficients
extern componentInfo cmpnfo[ 4 ];
extern char padbit;
extern int cmpc; // component count
extern int grbs;   // size of garbage
extern int            hdrs;   // size of header
extern unsigned short qtables[4][64];                // quantization tables
extern huffCodes      hcodes[2][4];                // huffman codes
extern huffTree       htrees[2][4];                // huffman decoding trees
extern unsigned char  htset[2][4];                    // 1 if huffman table is set
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

void check_decompression_memory_bound_ok();

bool parse_jfif_jpg( unsigned char type, unsigned int len, unsigned char* segment );
#define B_SHORT(v1,v2)    ( ( ((int) v1) << 8 ) + ((int) v2) )

static bool aligned_memchr16ff(const unsigned char *local_huff_data) {
#if 1
    __m128i buf = _mm_load_si128((__m128i const*)local_huff_data);
    __m128i ff = _mm_set1_epi8(-1);
    __m128i res = _mm_cmpeq_epi8(buf, ff);
    uint32_t movmask = _mm_movemask_epi8(res);
    bool retval = movmask != 0x0;
    assert (retval == (memchr(local_huff_data, 0xff, 16) != NULL));
    return retval;
#endif
    return memchr(local_huff_data, 0xff, 16) != NULL;
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
        for ( ; progress_ipos & 0xf; progress_ipos++ ) {
            if (__builtin_expect(!(progress_ipos < max_byte_coded), 0)) {
                break;
            }
            uint8_t byte_to_write = local_huff_data[progress_ipos];
            str_out->write_byte(byte_to_write);
            // check current byte, stuff if needed
            if (__builtin_expect(byte_to_write == 0xFF, 0))
                str_out->write_byte(stv);
        }

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
            if (__builtin_expect(byte_to_write == 0xFF, 0))
                str_out->write_byte(stv);
        }
    }
}

extern int cs_cmp[ 4 ];
template <class OutputWriter>
bool recode_one_mcu_row(abitwriter *huffw, int mcu,
                        OutputWriter*str_out,
                        Sirikata::Array1d<int16_t, (size_t)ColorChannel::NumBlockTypes> &lastdc,
                        BlockBasedImagePerChannel<false> &framebuffer) {
    int cmp = cs_cmp[ 0 ];
    int csc = 0, sub = 0;
    int dpos = mcu * cmpnfo[ cmp ].sfv * cmpnfo[ cmp ].sfh;
    int rstw = rsti ? rsti - mcu % rsti : 0;
    int cumulative_reset_markers = rstw ? mcu / rsti : 0;

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
            else {
                int test_cmp = cmp;
                int test_dpos = dpos;
                int test_rstw = rstw;
                sta = next_mcupos( &mcu, &cmp, &csc, &sub, &dpos, &rstw );
            }
            if (sta == 0 && huffw->no_remainder()) {
                escape_0xff_huffman_and_write(str_out, huffw->peekptr(), huffw->getpos());
                huffw->reset();
            }
            if (str_out->has_reached_bound()) {
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
                }
                // (re)set rst wait counter
                rstw = rsti;
                // (re)set last DCs for diff coding
                lastdc.memset(0);
            }
        }
        assert(huffw->no_remainder() && "this should have been padded");
    }
    return true;
}

unsigned int handle_initial_segments( bounded_iostream * const str_out )
{
    unsigned int byte_position = 0;

    while ( true ) {
        /* step 1: have we exhausted the headers without reaching the scan? */
        if ( static_cast<int>( byte_position + 3 ) >= hdrs ) {
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
        if ( type == 0xC4 or type == 0xDD or type == 0xDA ) {
            /* XXX make sure parse_jfif_jpg can't overrun hdrdata */
            if ( !parse_jfif_jpg( type, len, hdrdata + byte_position ) ) { return -1; }
        }

        /* step 5: we parsed the header -- accumulate byte position */
        byte_position += len;

        /* step 6: if it's an SOS (start of scan),
           then return the byte position -- done with initial headers */

        if ( type == 0xDA ) {
            if (prefix_grbgdata) {
                str_out->write(prefix_grbgdata, prefix_grbs);
            } else {
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
                                             BlockBasedImagePerChannel<false> &framebuffer,
                                             const ThreadHandoff &thread_handoff,
                                             Sirikata::Array1d<uint32_t, (uint32_t)ColorChannel::NumBlockTypes> max_coded_heights,
                                             Sirikata::Array1d<uint32_t, (uint32_t)ColorChannel::NumBlockTypes> component_size_in_blocks,
                                             int physical_thread_id,
                                             int logical_thread_id,
                                             int max_file_size) {
    ThreadHandoff retval = thread_handoff;

    // open huffman coded image data in abitwriter
    abitwriter huffw(16384, max_file_size);
    huffw.fillbit = padbit;
    huffw.reset_from_overhang_byte_and_num_bits(retval.overhang_byte,
                                                retval.num_overhang_bits);
    int decode_index = 0;
    while (true) {
        LeptonCodec::RowSpec cur_row = LeptonCodec::row_spec_from_index(decode_index++,
                                                                        framebuffer,
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
            if ( !recode_one_mcu_row(&huffw,
                                     cur_row.mcu_row_index * mcuh,
                                     stream_out,
                                     retval.last_dc,
                                     framebuffer) ) {
                custom_exit(ExitCode::CODING_ERROR);
            }
            const unsigned char * flushed_data = huffw.partial_bytewise_flush();
            escape_0xff_huffman_and_write(stream_out, flushed_data, huffw.getpos() );
            huffw.reset_crystalized_bytes();
            retval.num_overhang_bits = huffw.get_num_overhang_bits();
            retval.overhang_byte = huffw.get_overhang_byte();
            if ( huffw.error ) {
                custom_exit(ExitCode::CODING_ERROR);
            }
        }
    }
    return retval;
}


template<class BoundedWriter>
void recode_physical_thread(BoundedWriter *stream_out,
                            BlockBasedImagePerChannel<false> &framebuffer,
                            const std::vector<ThreadHandoff> &thread_handoffs,
                            Sirikata::Array1d<uint32_t,
                                              (uint32_t)ColorChannel::NumBlockTypes> max_coded_heights,
                            Sirikata::Array1d<uint32_t,
                                              (uint32_t)ColorChannel::NumBlockTypes> component_size_in_blocks,
                            int physical_thread_id,
                            int max_file_size,
                            bool reset_bound) {
    int num_physical_threads = g_threaded ? NUM_THREADS : 1;
    int num_logical_threads = thread_handoffs.size();
    int logical_thread_start = (physical_thread_id * num_logical_threads) / num_physical_threads;
    int logical_thread_end = std::min(((physical_thread_id  + 1) * num_logical_threads) / num_physical_threads,
                                      num_logical_threads);
    if (num_logical_threads < num_physical_threads) {
        // this is an optimization so we don't have to call the reset logic as often
        logical_thread_start = std::min(physical_thread_id, num_logical_threads);
        logical_thread_end = std::min(physical_thread_id + 1, num_logical_threads);
    }
    if (reset_bound) {
        int work_size = 0;
        for (int logical_thread_id = logical_thread_start; logical_thread_id < logical_thread_end; ++logical_thread_id) {
            work_size += thread_handoffs[logical_thread_id].segment_size;
        }
        if (!work_size) {
            work_size = max_file_size;
        }
        stream_out->set_bound(work_size);
    }
    ThreadHandoff th = thread_handoffs[logical_thread_start];
    for (int logical_thread_id = logical_thread_start; logical_thread_id < logical_thread_end; ++logical_thread_id) {

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
            assert(memcmp(thread_handoffs[logical_thread_id].last_dc.begin(), th.last_dc.begin(), sizeof(th.last_dc)) == 0);
            assert(th.overhang_byte == thread_handoffs[logical_thread_id].overhang_byte);
            assert(th.num_overhang_bits == thread_handoffs[logical_thread_id].num_overhang_bits);
            th = thread_handoffs[logical_thread_id];
        }
        if (logical_thread_id != physical_thread_id) {
            g_decoder->clear_thread_state(logical_thread_id, physical_thread_id, framebuffer);
        }
        th = recode_row_range(stream_out,
                              framebuffer,
                              th,
                              max_coded_heights,
                              component_size_in_blocks,
                              physical_thread_id,
                              logical_thread_id,
                              max_file_size);        
        if (logical_thread_id + 1 < num_logical_threads
            && !thread_handoffs[logical_thread_id + 1].is_legacy_mode()) {
            // make sure we computed the same item that was stored
            assert(memcmp(&th, &thread_handoffs[logical_thread_id + 1], sizeof(th)) == 0);
        }
    }
}
/* -----------------------------------------------
    JPEG encoding routine
    ----------------------------------------------- */
bool recode_baseline_jpeg(bounded_iostream*str_out,
                          int max_file_size)
{    

    unsigned int local_bound = max_file_size - grbs;
    str_out->set_bound(local_bound);
    {
        unsigned char SOI[ 2 ] = { 0xFF, 0xD8 }; // SOI segment
        // write SOI
        str_out->write( SOI, 2 );
    }

    /* step 1: handle the initial segments */
    unsigned int byte_position = handle_initial_segments( str_out );
    if ( byte_position == -1 ) {
        return false;
    }
    /* step 2: setup multithreaded decoder with framebuffer for each */
    Sirikata::Array1d<uint32_t,
                      (size_t)ColorChannel::NumBlockTypes> max_coded_heights
        = colldata.get_max_coded_heights();
    Sirikata::Array1d<uint32_t, (uint32_t)ColorChannel::NumBlockTypes> component_size_in_blocks
        = colldata.get_component_size_in_blocks();
    Sirikata::Array1d<BlockBasedImagePerChannel<false>,
                      NUM_THREADS> framebuffer;

    for (size_t thread_id = 0; thread_id < NUM_THREADS; ++thread_id) {
        for(int cmp = 0; cmp < colldata.get_num_components(); ++cmp) {
            framebuffer[thread_id][cmp] = new BlockBasedImageBase<false>;
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
    /* step 3: decode the scan, row by row */
    std::tuple<uint8_t, uint8_t, Sirikata::Array1d<int16_t, (size_t)ColorChannel::NumBlockTypes> > overhang_byte_and_bit_count;
    std::get<0>(overhang_byte_and_bit_count) = 0;
    std::get<1>(overhang_byte_and_bit_count) = 0;
    std::get<2>(overhang_byte_and_bit_count).memset(0);
    
    for (int physical_thread_id = 0;physical_thread_id < (g_threaded ? NUM_THREADS : 1); ++physical_thread_id) {
        if (physical_thread_id == 0) {
            recode_physical_thread(str_out,
                                   framebuffer[physical_thread_id],
                                   luma_bounds,
                                   max_coded_heights,
                                   component_size_in_blocks,
                                   physical_thread_id,
                                   max_file_size,
                                   false);
        } else {//FIXME: spawn a thread for each, once we have the overhang_byte_and_bit_count deserialized
            Sirikata::JpegAllocator<uint8_t> alloc;
            // the reason local_buffer isn't contained entirely in the loop is one purely of performance
            // The allocation/deallocation of the vector just takes ages with test_hq
            // However, this doesn't mean the contents are shared: it gets treated as cleared each time
            Sirikata::BoundedMemWriter local_buffer(alloc);
            recode_physical_thread(&local_buffer,
                                   framebuffer[physical_thread_id],
                                   luma_bounds,
                                   max_coded_heights,
                                   component_size_in_blocks,
                                   physical_thread_id,
                                   max_file_size,
                                   true);
            size_t bytes_to_copy = local_buffer.bytes_written();
            if (bytes_to_copy) {
                local_bound -= bytes_to_copy;
                str_out->write(&local_buffer.buffer()[0],
                               bytes_to_copy);
            }
        }
    }



    /* step 3: blit any trailing data */
    if ( not str_out->has_reached_bound() ) {
        str_out->write( hdrdata + byte_position, hdrs - byte_position );
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

    // errormessage if write error
    if ( str_out->chkerr() ) {
        fprintf( stderr, "write error, possibly drive is full" );
        return false;
    }
    return true;
}
