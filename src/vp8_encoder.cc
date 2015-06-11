/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "bitops.hh"
#include "component_info.hh"
#include "uncompressed_components.hh"
#include "jpgcoder.hh"
#include "vp8_encoder.hh"
void VP8ComponentEncoder::vp8_full_encoder(UncompressedComponents* colldata, iostream *str_out) {
	char ujpg_mrk[ 64 ];
	// write actual decompressed coefficient data to file
	for ( int cmp = 0; cmp < cmpc; cmp++ ) {
		sprintf( ujpg_mrk, "CMP%i", cmp );
		str_out->write( (void*) ujpg_mrk, 1, 4 );
		// data: coefficient data in zigzag collection order
        str_out->write( (void*) colldata->full_component_write( cmp ), sizeof( short ), colldata->component_size_in_shorts(cmp));
	}
    
}
