/* sha1.h

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

#pragma once

#if !defined(IN_HASHING_CPP) && !defined(IN_SHA1_CPP)
#error do not include directly
#endif

#include <cstdint>
#include <string>
#include <string_view>

typedef uint32_t Uint32;

class SHA1
{
private:
	// fields
	Uint32 H0 = 0x67452301;
	Uint32 H1 = 0xefcdab89;
	Uint32 H2 = 0x98badcfe;
	Uint32 H3 = 0x10325476;
	Uint32 H4 = 0xc3d2e1f0;
	unsigned char bytes[64];
	Uint32 unprocessedBytes = 0;
	Uint32 size = 0;
	void process();

public:
	SHA1();
	~SHA1();
	void addBytes(const char *data, Uint32 num);
	inline void addBytes(std::string_view data) {
		addBytes(data.data(), data.size());
	}
	void getDigest(unsigned char *to);
	inline std::string getDigest() {
		std::string ret(20, '\000');
		getDigest(reinterpret_cast<unsigned char*>(ret.data()));
		return ret;
	}
};
