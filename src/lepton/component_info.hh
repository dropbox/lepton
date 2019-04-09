/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#ifndef COMPONENT_INFO_HH_
#define COMPONENT_INFO_HH_
struct componentInfo {
	unsigned short* qtable; // quantization table
	int huffdc; // no of huffman table (DC)
	int huffac; // no of huffman table (AC)
	int sfv; // sample factor vertical
	int sfh; // sample factor horizontal	
	int mbs; // blocks in mcu		
	int bcv; // block count vertical (interleaved)
	int bch; // block count horizontal (interleaved)
	int bc;  // block count (all) (interleaved)
	int ncv; // block count vertical (non interleaved)
	int nch; // block count horizontal (non interleaved)
	int nc;  // block count (all) (non interleaved)
	int sid; // statistical identity
	int jid; // jpeg internal id
    componentInfo() {
        memset(this, 0, sizeof(*this));
    }
    void check_valid_value_range() const {
        always_assert(bch && "zero height");
        always_assert(bcv && "zero height");
        always_assert(sfv && "zero sfv");
        always_assert(sfh && "zero sfh");
    }

};

#endif
