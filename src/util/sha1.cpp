/* sha1.cpp

Copyright (c) 2005 Michael D. Leonhard

http://tamale.net/

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>

#define IN_SHA1_CPP

#include "sha1.h"

namespace {

// circular left bit rotation.  MSB wraps around to LSB
inline Uint32 lrot( Uint32 x, int bits )
{
	return (x<<bits) | (x>>(32 - bits));
}

// Save a 32-bit unsigned integer to memory, in big-endian order
inline void storeBigEndianUint32( unsigned char* byte, Uint32 num )
{
	byte[0] = (unsigned char)(num>>24);
	byte[1] = (unsigned char)(num>>16);
	byte[2] = (unsigned char)(num>>8);
	byte[3] = (unsigned char)num;
}

}


// Constructor *******************************************************
SHA1::SHA1()
{
	// make sure that the data type is the right size
	static_assert( sizeof( Uint32 ) * 5 == 20 );
}

// Destructor ********************************************************
SHA1::~SHA1()
{
	// erase data
	H0 = H1 = H2 = H3 = H4 = 0;
	for( int c = 0; c < 64; c++ ) bytes[c] = 0;
	unprocessedBytes = size = 0;
}

// process ***********************************************************
void SHA1::process()
{
	assert( unprocessedBytes == 64 );
	int t;
	Uint32 a, b, c, d, e, K, f, W[80];
	// starting values
	a = H0;
	b = H1;
	c = H2;
	d = H3;
	e = H4;
	// copy and expand the message block
	for( t = 0; t < 16; t++ ) W[t] = (bytes[t*4] << 24)
									+(bytes[t*4 + 1] << 16)
									+(bytes[t*4 + 2] << 8)
									+ bytes[t*4 + 3];
	for(; t< 80; t++ ) W[t] = lrot( W[t-3]^W[t-8]^W[t-14]^W[t-16], 1 );

	/* main loop */
	Uint32 temp;
	for( t = 0; t < 80; t++ )
	{
		if( t < 20 ) {
			K = 0x5a827999;
			f = (b & c) | ((~b) & d);
		} else if( t < 40 ) {
			K = 0x6ed9eba1;
			f = b ^ c ^ d;
		} else if( t < 60 ) {
			K = 0x8f1bbcdc;
			f = (b & c) | (b & d) | (c & d);
		} else {
			K = 0xca62c1d6;
			f = b ^ c ^ d;
		}
		temp = lrot(a,5) + f + e + W[t] + K;
		e = d;
		d = c;
		c = lrot(b,30);
		b = a;
		a = temp;
		//printf( "t=%d %08x %08x %08x %08x %08x\n",t,a,b,c,d,e );
	}
	/* add variables */
	H0 += a;
	H1 += b;
	H2 += c;
	H3 += d;
	H4 += e;
	//printf( "Current: %08x %08x %08x %08x %08x\n",H0,H1,H2,H3,H4 );
	/* all bytes have been processed */
	unprocessedBytes = 0;
}

// addBytes **********************************************************
void SHA1::addBytes( const char* data, Uint32 num )
{
	assert( data );
	// add these bytes to the running total
	size += num;
	// repeat until all data is processed
	while( num > 0 )
	{
		// number of bytes required to complete block
		Uint32 needed = 64 - unprocessedBytes;
		assert( needed <= 64 );
		// number of bytes to copy (use smaller of two)
		Uint32 toCopy = (num < needed) ? num : needed;
		// Copy the bytes
		memcpy( bytes + unprocessedBytes, data, toCopy );
		// Bytes have been copied
		num -= toCopy;
		data += toCopy;
		unprocessedBytes += toCopy;

		// there is a full block
		if( unprocessedBytes == 64 ) process();
	}
}

// digest ************************************************************
void SHA1::getDigest(unsigned char *digest)
{
	// save the message size
	Uint32 totalBitsL = size << 3;
	Uint32 totalBitsH = size >> 29;
	// add 0x80 to the message
	addBytes( "\x80", 1 );

	unsigned char footer[64] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	// block has no room for 8-byte filesize, so finish it
	if( unprocessedBytes > 56 )
		addBytes( (char*)footer, 64 - unprocessedBytes);
	assert( unprocessedBytes <= 56 );
	// how many zeros do we need
	Uint32 neededZeros = 56 - unprocessedBytes;
	// store file size (in bits) in big-endian format
	storeBigEndianUint32( footer + neededZeros    , totalBitsH );
	storeBigEndianUint32( footer + neededZeros + 4, totalBitsL );
	// finish the final block
	addBytes( (char*)footer, neededZeros + 8 );
	// copy the digest bytes
	storeBigEndianUint32( digest, H0 );
	storeBigEndianUint32( digest + 4, H1 );
	storeBigEndianUint32( digest + 8, H2 );
	storeBigEndianUint32( digest + 12, H3 );
	storeBigEndianUint32( digest + 16, H4 );
}
