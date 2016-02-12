/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include <time.h>
#include <stdio.h>
#include <iostream>

#include "uncompressed_components.hh"
#include "recoder.hh"
#include "bitops.hh"
int encode_block_seq( abitwriter* huffw, huffCodes* dctbl, huffCodes* actbl, short* block);
int next_mcupos( int* mcu, int* cmp, int* csc, int* sub, int* dpos, int* rstw );
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
void escape_0xff_huffman_and_write(bounded_iostream* str_out,
                                   const unsigned char * local_huff_data,
                                   unsigned int max_byte_coded,
                                   bool flush) {
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

bool recode_one_mcu_row(abitwriter *huffw, int mcu, int &cumulative_reset_markers,
                        bounded_iostream*str_out, int lastdc[4] ) {
  int cmp = cs_cmp[ 0 ];
  int csc = 0, sub = 0;
  int dpos = mcu * cmpnfo[ cmp ].sfv * cmpnfo[ cmp ].sfh;
  int rstw = rsti ? rsti - mcu % rsti : 0;

  Sirikata::Aligned256Array1d<int16_t, 64> block; // store block for coeffs
    bool end_of_row = false;
    // JPEG imagedata encoding routines
    while (!end_of_row) {

        // (re)set status
        int sta = 0;

        // ---> sequential interleaved encoding <---
        while ( sta == 0 ) {
            // copy from colldata
            const AlignedBlock &aligned_block = colldata.block((BlockType)cmp, dpos);
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
                escape_0xff_huffman_and_write(str_out, huffw->peekptr(), huffw->getpos(), false);
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
            escape_0xff_huffman_and_write(str_out, huffw->peekptr(), huffw->getpos(), false);
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
                lastdc[ 0 ] = 0;
                lastdc[ 1 ] = 0;
                lastdc[ 2 ] = 0;
                lastdc[ 3 ] = 0;
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
            str_out->write( hdrdata, byte_position );
            return byte_position; /* ready for the scan */
        }
    }
}

/* -----------------------------------------------
    JPEG encoding routine
    ----------------------------------------------- */
bool recode_baseline_jpeg(bounded_iostream*str_out,
                          int max_file_size)
{
    abitwriter*  huffw; // bitwise writer for image data


    int lastdc[ 4 ] = {0, 0, 0, 0}; // last dc for each component
    int rstw = 0; // restart wait counter

    // open huffman coded image data in abitwriter
    huffw = new abitwriter( 16384, max_file_size);
    huffw->fillbit = padbit;

    str_out->set_bound(max_file_size - grbs);
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

    /* step 2: decode the scan, row by row */
    int cumulative_reset_markers = 0;
    for ( unsigned int row = 0; row < mcuv and !str_out->has_reached_bound(); row++ ) {
        if ( !recode_one_mcu_row(huffw, row * mcuh, cumulative_reset_markers, str_out, lastdc) ) {
            return false;
        }
    }

    /* step 3: blit any trailing data */
    if ( not str_out->has_reached_bound() ) {
        str_out->write( hdrdata + byte_position, hdrs - byte_position );
    }

    if(str_out->has_reached_bound()) {
        check_decompression_memory_bound_ok();
    }

    // safety check for error in huffwriter
    if ( huffw->error ) {
        custom_exit(ExitCode::OOM);
    }

    assert(huffw->no_remainder() && "this should have been padded");
    escape_0xff_huffman_and_write(str_out, huffw->peekptr(), huffw->getpos(), true);
    huffw->reset();

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
