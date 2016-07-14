/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/**
Copyright (c) 2006...2016, Matthias Stirner and HTW Aalen University
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are 
met: 

1. Redistributions of source code must retain the above copyright 
notice, this list of conditions and the following disclaimer. 

2. Redistributions in binary form must reproduce the above copyright 
notice, this list of conditions and the following disclaimer in the 
documentation and/or other materials provided with the distribution. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS 
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A 
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED 
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 **/

/* -----------------------------------------------
	compression helper tables
	----------------------------------------------- */

// maxima for each frequency in zigzag order
const unsigned short int freqmax[] =
{
	1024,  931,  932,  985,  858,  985,  968,  884, 
	 884,  967, 1020,  841,  871,  840, 1020,  968, 
	 932,  875,  876,  932,  969, 1020,  838,  985, 
	 844,  985,  838, 1020, 1020,  854,  878,  967, 
	 967,  878,  854, 1020,  854,  871,  886, 1020, 
	 886,  871,  854,  854,  870,  969,  969,  870, 
	 854,  838, 1010,  838, 1020,  837, 1020,  969, 
	 969, 1020,  838, 1020,  838, 1020, 1020,  838
};
/*
const unsigned short int freqmax[] =
{
	1024,  924,  924,  942,  838,  942,  924,  854,
	 854,  924, 1020,  837,  871,  838, 1020,  924,
	 924,  854,  854,  924,  924,  942,  838,  942,
	 837,  942,  838,  942,  924,  854,  854,  924,
	 924,  854,  854,  924,  838,  871,  838, 1020,
	 837,  871,  838,  854,  854,  924,  924,  854,
	 854,  838,  942,  838,  942,  837,  924,  854,
	 854,  924,  838,  871,  838,  854,  854,  838
};
*/

// maxima for each frequency - IJG DCT float
const unsigned short int freqmax_float[] =
{
	1024,  924,  942,  924, 1020,  924,  942,  924,
	 924,  837,  854,  837,  924,  837,  854,  837,
	 942,  854,  871,  854,  942,  854,  871,  854,
	 924,  837,  854,  837,  924,  837,  854,  837,
	1020,  924,  942,  924, 1020,  924,  942,  924,
	 924,  837,  854,  837,  924,  837,  854,  837,
	 942,  854,  871,  854,  942,  854,  871,  854,
	 924,  837,  854,  837,  924,  837,  854,  837
};

// maxima for each frequency - IJG DCT int
const unsigned short int freqmax_int[] =
{
	1024,  924,  942,  924, 1020,  924,  942,  924,
	 924,  838,  854,  838,  924,  838,  854,  838,
	 942,  854,  871,  854,  942,  854,  871,  854,
	 924,  837,  854,  837,  924,  837,  854,  837,
	1020,  924,  942,  924, 1020,  924,  942,  924,
	 924,  838,  854,  838,  924,  838,  854,  838,
	 942,  854,  871,  854,  942,  854,  871,  854,
	 924,  838,  854,  838,  924,  838,  854,  838
};

// maxima for each frequency - IJG DCT fast
const unsigned short int freqmax_fast[] =
{
	1024,  931,  985,  968, 1020,  968, 1020, 1020,
	 932,  858,  884,  840,  932,  812,  854,  854,
	 985,  884,  849,  875,  985,  878,  821,  821,
	 967,  841,  876,  844,  967,  886,  870,  726,
	1020,  932,  985,  967, 1020,  969, 1020, 1020,
	 969,  812,  878,  886,  969,  829,  969,  727,
	1020,  854,  821,  870, 1010,  969, 1020, 1020,
	1020,  854,  821,  725, 1020,  727, 1020,  510
};

// maxima for each frequency - IJG DCT max
const unsigned short int freqmax_ijg[] =
{
	1024,  931,  985,  968, 1020,  968, 1020, 1020,
	 932,  858,  884,  840,  932,  838,  854,  854,
	 985,  884,  871,  875,  985,  878,  871,  854,
	 967,  841,  876,  844,  967,  886,  870,  837,
	1020,  932,  985,  967, 1020,  969, 1020, 1020,
	 969,  838,  878,  886,  969,  838,  969,  838,
	1020,  854,  871,  870, 1010,  969, 1020, 1020,
	1020,  854,  854,  838, 1020,  838, 1020,  838
};

// standard scan = zigzag scan
unsigned char stdscan[] =
{
	 0,  1,  2,  3,  4,  5,  6,  7,
	 8,  9, 10, 11, 12, 13, 14, 15,
	16,	17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55,
	56, 57, 58, 59, 60, 61, 62, 63
};

// zagzig scan, can be used instead of zigzag scan
unsigned char zagscan[] =
{
	 0,  2,  1,  5,  4,  3,  9,  8,
	 7,  6, 14, 13, 12, 11, 10, 20,
	19,	18, 17, 16, 15, 27, 26, 25,
	24, 23, 22, 21, 35, 34, 33, 32,
	31, 30, 29, 28, 42, 41, 40, 39,
	38, 37, 36, 48, 47, 46, 45, 44,
	43, 53, 52, 51, 50, 49, 57, 56,
	55, 54, 60, 59, 58, 62, 61, 63
};


// zigzag scan reverse conversion table
const int jpeg_natural_order[] =
{
	 0,  1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

// scans for each frequency 
const char freqalign[] =
{
	'm', 'v', 'v', 'v', 'v', 'v', 'v', 'v',
	'h', 'm', 'v', 'v', 'v', 'v', 'v', 'v',
	'h', 'h', 'm', 'v', 'v', 'v', 'v', 'v',
	'h', 'h', 'h', 'm', 'v', 'v', 'v', 'v',
	'h', 'h', 'h', 'h', 'm', 'v', 'v', 'v',
	'h', 'h', 'h', 'h', 'h', 'm', 'v', 'v',
	'h', 'h', 'h', 'h', 'h', 'h', 'm', 'v',
	'h', 'h', 'h', 'h', 'h', 'h', 'h', 'm'
};

// chengjie tu subband classification
const int ctxclass[] =
{
	0, 1, 3, 3, 3, 6, 6, 6, // 0 -> DC (DC subband)
	2, 5, 5, 5, 6, 6, 6, 6, // 1 -> PV (principal vertical)
	4, 5, 5, 5, 6, 6, 6, 6, // 2 -> PH (principal horizontal)
	4, 5, 5, 6, 6, 6, 6, 6, // 3 -> LV (low-frequency vertical)
	4, 6, 6, 6, 6, 6, 6, 6, // 4 -> LH (low-frequency horizontal)
	6, 6, 6, 6, 6, 6, 6, 6, // 5 -> LD (low-frequency diagonal)
	6, 6, 6, 6, 6, 6, 6, 6, // 6 -> HP (high-pass)
	6, 6, 6, 6, 6, 6, 6, 6
};

// standard huffman tables, found in JPEG specification, Chapter K.3
const unsigned char std_huff_tables[4][272] =
{
	{	// standard luma dc table (0/0)
		0x00,0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B
	},
	{	// standard chroma dc table (0/1)
		0x00,0x03,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,
		0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B
	},
	{	// standard luma ac table (1/0)
		0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,0x00,0x01,0x7D,
		0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
		0x22,0x71,0x14,0x32,0x81,0x91,0xA1,0x08,0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,
		0x24,0x33,0x62,0x72,0x82,0x09,0x0A,0x16,0x17,0x18,0x19,0x1A,0x25,0x26,0x27,0x28,
		0x29,0x2A,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
		0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
		0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
		0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,
		0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,
		0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xE1,0xE2,
		0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,
		0xF9,0xFA
	},
	{	// standard chroma ac table (1/1)
		0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,0x04,0x04,0x00,0x01,0x02,0x77,
		0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
		0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xA1,0xB1,0xC1,0x09,0x23,0x33,0x52,0xF0,
		0x15,0x62,0x72,0xD1,0x0A,0x16,0x24,0x34,0xE1,0x25,0xF1,0x17,0x18,0x19,0x1A,0x26,
		0x27,0x28,0x29,0x2A,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,
		0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,
		0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x82,0x83,0x84,0x85,0x86,0x87,
		0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,
		0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,
		0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,
		0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,
		0xF9,0xFA
	}
};

// lengths of standard huffmann tables
const unsigned char std_huff_lengths[ 4 ] =	{ 28, 28, 178, 178 };
