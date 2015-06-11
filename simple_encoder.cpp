#include "bitops.h"
#include "component_info.h"
#include "uncompressed_components.h"
#include "jpgcoder.h"
#include "simple_encoder.h"
void SimpleComponentEncoder::simple_full_encoder(UncompressedComponents* colldata, iostream *str_out) {
	char ujpg_mrk[ 64 ];
	// write actual decompressed coefficient data to file
	for ( int cmp = 0; cmp < cmpc; cmp++ ) {
		sprintf( ujpg_mrk, "CMP%i", cmp );
		str_out->write( (void*) ujpg_mrk, 1, 4 );
		// data: coefficient data in zigzag collection order
        str_out->write( (void*) colldata->full_component_write( cmp ), sizeof( short ), colldata->component_size_in_shorts(cmp));
	}
    
}
