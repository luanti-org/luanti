// encipher() and decipher() copied from https://en.wikipedia.org/wiki/XTEA#Implementations
//	("public domain by David Wheeler and Roger Needham")

// for thread_local before C23
#include <threads.h>
// for bool type
#include <stdbool.h>
// for uint64_t, uint32_t, and uint8_t
#include <stdint.h>
// for exit()
#include <stdlib.h>

#ifdef _WIN32
// https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom
#include <bcrypt.h>

static void fill_with_random(uint8_t buf[], size_t size) {
	NTSTATUS result = BCryptGenRandom(NULL, buf,
			(unsigned long) size, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	if (result != STATUS_SUCCESS) {
		exit(-1);
	}
}
#else
// POSIX platforms
#include <stdio.h>

#include <stdassert.h>
static void fill_with_random(uint8_t buf[], size_t size) {
	FILE* random = fopen("/dev/urandom", "rb");
	if (random == NULL) {
		fputs("Failed to open /dev/urandom for reading\n", stderr);
		exit(-1);
	}

	size_t count_read = fread(buf, size, 1, random);
	fclose(random);
	if (count_read < 1) {
		fprintf(stderr, "Failed to read %zu bytes from /dev/urandom",
				size);
		exit(-1);
	}
}
#endif

/* take 64 bits of data in v[0] and v[1] and 128 bits of key[0] - key[3] */
void luaxtea_encipher(unsigned int num_rounds, uint32_t v[2], uint32_t const key[4]) {
	unsigned int i;
	uint32_t v0=v[0], v1=v[1], sum=0, delta=0x9E3779B9;
	for (i=0; i < num_rounds; i++) {
		v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
		sum += delta;
		v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
	}
	v[0]=v0; v[1]=v1;
}

void luaxtea_decipher(unsigned int num_rounds, uint32_t v[2], uint32_t const key[4]) {
	unsigned int i;
	uint32_t v0=v[0], v1=v[1], delta=0x9E3779B9, sum=delta*num_rounds;
	for (i=0; i < num_rounds; i++) {
		v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
		sum -= delta;
		v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
	}
	v[0]=v0; v[1]=v1;
}

static thread_local uint32_t xtea_key[4] = 0;
static thread_local bool xtea_key_initialized = false;
static uint32_t* getKey() {
	if (!xtea_key_initialized) {
		fill_with_random((uint8_t*) xtea_key, sizeof(uint32_t) * 4);
		xtea_key_initialized = true;
	}
	return luaxtea_key;
}

static constexpr const unsigned NUM_ROUNDS = 32;
uint64_t luaxtea_cipher_pointer(void* ptr) {
	// "The recommended value for the "num_rounds" parameter is 32, not 64, as each iteration of the loop does two Feistel-cipher rounds"
	uint32_t* key = getKey();

	// 0-extend the pointer to 64-bit on 32-bit platforms
	uint64_t extended = uint64_t(ptr);
	// split the 64-bit value into 2 32-bit values
	uint32_t v[2] = {uint32_t(extended >> 32), uint32_t(extended & 0xFFFFFFFFu)};

	luaxtea_encipher(NUM_ROUNDS, v, key.data());
	// reassemble "extended", and return
	extended = (uint64_t(v[0]) << 32) | uint64_t(v[1]);
	return extended;
}

