// encipher() and decipher() copied from https://en.wikipedia.org/wiki/XTEA#Implementations
//	("public domain by David Wheeler and Roger Needham")

#include <cstdint>
namespace xtea {

/* take 64 bits of data in v[0] and v[1] and 128 bits of key[0] - key[3] */
void encipher(unsigned int num_rounds, uint32_t v[2], uint32_t const key[4]) {
	unsigned int i;
	uint32_t v0=v[0], v1=v[1], sum=0, delta=0x9E3779B9;
	for (i=0; i < num_rounds; i++) {
		v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
		sum += delta;
		v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
	}
	v[0]=v0; v[1]=v1;
}

void decipher(unsigned int num_rounds, uint32_t v[2], uint32_t const key[4]) {
	unsigned int i;
	uint32_t v0=v[0], v1=v[1], delta=0x9E3779B9, sum=delta*num_rounds;
	for (i=0; i < num_rounds; i++) {
		v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
		sum -= delta;
		v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
	}
	v[0]=v0; v[1]=v1;
}

static std::array<uint32_t, 4> getKey() {
	static std::array<uint32_t, 4> key = [](){
#ifdef __linux__
		std::random_device dev {"/dev/urandom"};
#else
		std::random_device dev {};
#endif

		std::uniform_int_distribution<uint32_t> dist (0u, 0xFFFFFFFFu);
		auto key = std::array<uint32_t, 4> {};
		for (uint32_t& keypart : key) {
			keypart = dist(dev);
		}

		return key;
	}();

	return key;
}

static constexpr const unsigned NUM_ROUNDS = 32;
uint64_t cipher_pointer(void* ptr) {
	// "The recommended value for the "num_rounds" parameter is 32, not 64, as each iteration of the loop does two Feistel-cipher rounds"
	auto key = getKey();

	// 0-extend the pointer to 64-bit on 32-bit platforms
	uint64_t extended = uint64_t(ptr);
	// split the 64-bit value into 2 32-bit values
	uint32_t v[2] = {uint32_t(extended >> 32), uint32_t(extended & 0xFFFFFFFFu)};

	encipher(NUM_ROUNDS, v, key.data());
	// reassemble "extended", and return
	extended = (uint64_t(v[0]) << 32) | uint64_t(v[1]);
	return extended;
}

} // namespace xtea

