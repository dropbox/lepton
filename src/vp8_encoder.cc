/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <string>
#include <cassert>

#include "bitops.hh"
#include "component_info.hh"
#include "uncompressed_components.hh"
#include "jpgcoder.hh"
#include "vp8_encoder.hh"

using namespace std;

CodingReturnValue VP8ComponentEncoder::encode_chunk(const UncompressedComponents *input, ::iostream *output) {
    vp8_full_encoder(input, output);
    return CODING_DONE;
}
void VP8ComponentEncoder::vp8_full_encoder( const UncompressedComponents * const colldata,
                                            ::iostream * const str_out)
{
    /* cmpc is a global variable with the component count */
    /* verify JFIF is Y' or Y'CbCr only */
    assert( (cmpc == 1) or (cmpc == 3) );

    for ( int cmp = 0; cmp < cmpc; cmp++ ) {
        const string ujpg_mark { "CMP" + to_string( cmp ) };

        assert( ujpg_mark.size() == 4 );

		str_out->write( ujpg_mark.data(), 1, 4 );
		// data: coefficient data in zigzag collection order

        for ( unsigned int block = 0;
              block < colldata->component_size_in_blocks( cmp );
              block++ ) {
            for ( unsigned int component = 0; component < 64; component++ ) {
                const short value = colldata->at_nosync( cmp, component, block );
                str_out->write( &value, sizeof( short ), 1 );
            }
        }
	}
}
