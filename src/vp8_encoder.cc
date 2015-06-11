/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <string>
#include <cassert>

#include "bitops.hh"
#include "component_info.hh"
#include "uncompressed_components.hh"
#include "jpgcoder.hh"
#include "vp8_encoder.hh"

using namespace std;

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

        str_out->write( colldata->full_component_write( cmp ),
                        sizeof( short ),
                        colldata->component_size_in_shorts( cmp ) );
	}
}
