#include "catch.h"

#include <cstdint>
#include <functional>
#include <type_traits>
#include <random>
#include <limits>

#include "util/checked_ops.h"

template<typename T>
static constexpr const T high_bit = T(1) << (sizeof(T) * 8 - 1);

static auto seed_rng() {
	return std::random_device {};
}
static auto get_rng() {
	static const std::default_random_engine engine(seed_rng()());
	// Copy the seeded engine
	return engine;
}

template<typename T, bool operation(T, T, T*)>
static void testExpected(T a, T b, bool expected_overflow, T expected_result) {
	T actual_result = T(0);
	bool actual_overflow = operation(a, b, &actual_result);

	REQUIRE( expected_overflow == actual_overflow );
	REQUIRE( expected_result == actual_result );
}

TEST_CASE("fixed checked (+)/(-) tests") {
	// These feel kind of weak?
	SECTION( "~0 + ~0 overflows for unsigned" ) {
		testExpected<int, checked::add>(-1, -1, false, -2);
		testExpected<long long, checked::add>(-1ll, -1ll, false, -2ll);
		testExpected<unsigned, checked::add>(~0u, ~0u,
		                                     true, 0u /* stays the same */);
		testExpected<unsigned long long, checked::add>(
				~0ull, ~0ull,
				true, 0ull /* stays the same */
			);
	}

	SECTION( "high_bit + high_bit always overflows" ) {
		testExpected<int, checked::add>(high_bit<int>, high_bit<int>,
		                                true, 0 /* stays the same */);
		testExpected<long long, checked::add>(
				high_bit<long long>, high_bit<long long>,
				true, 0 /* stays the same */
			);
		testExpected<unsigned, checked::add>(
				high_bit<unsigned>, high_bit<unsigned>,
				true, 0 /* stays the same */
			);
		testExpected<unsigned long long, checked::add>(
				high_bit<unsigned long long>, high_bit<unsigned long long>,
				true, 0ull /* stays the same */
			);
	}

	SECTION( "1 + 2 never overflows" ) {
		testExpected<int, checked::add>(1, 2, false, 3);
		testExpected<long long, checked::add>(1ll, 2ll, false, 3ll);
		testExpected<unsigned int, checked::add>(1u, 2u, false, 3u);
		testExpected<unsigned long long, checked::add>(1ull, 2ull, false, 3ull);
	}
}
TEST_CASE("fixed checked (*) tests") {
	SECTION( "2 * 3 never overflows" ) {
		testExpected<int, checked::mul>(2, 3, false, 6);
		testExpected<long long int, checked::mul>(2ll, 3ll, false, 6ll);
		testExpected<unsigned, checked::mul>(2u, 3u, false, 6u);
		testExpected<unsigned long long, checked::mul>(2ull, 3ull, false, 6ull);
	}

	SECTION( "~0 * 2 overflows for unsigned" ) {
		testExpected<int, checked::mul>(~0, 2, false, -2);
		testExpected<long long int, checked::mul>(~0ll, 2, false, -2);
		testExpected<unsigned int, checked::mul>(
				~0, 2, true, 0 /* stays the same */
			);
		testExpected<unsigned long long int, checked::mul>(
				~0ull, 2, true, 0 /* stays the same */
			);
		testExpected<int, checked::mul>(2, ~0, false, -2);
		testExpected<long long int, checked::mul>(2, ~0ll, false, -2);
		testExpected<unsigned int, checked::mul>(
				2, ~0u, true, 0 /* stays the same */
			);
		testExpected<unsigned long long int, checked::mul>(
				2, ~0ull, true, 0 /* stays the same */
			);
	}

	SECTION( "~0 * ~0 overflows for unsigned" ) {
		testExpected<int, checked::mul>(~0, ~0, false, 1);
		testExpected<long long int, checked::mul>(~0ll, ~0ll, false, 1ll);
		testExpected<unsigned int, checked::mul>(
				~0u, ~0u, true, 0 /* stays the same */
			);
		testExpected<unsigned long long int, checked::mul>(
				~0ull, ~0ull, true, 0 /* stays the same */
			);
	}

	SECTION( "high_bit * high_bit always overflows" ) {
		testExpected<int, checked::mul>(
				high_bit<int>, high_bit<int>, true, 0 /* stays the same */
			);
		testExpected<long long, checked::mul>(
				high_bit<long long>, high_bit<long long>,
				true, 0ll /* stays the same */
			);
		testExpected<unsigned, checked::mul>(
				high_bit<unsigned>, high_bit<unsigned>, true, 0u /* stays the same */
			);
		testExpected<unsigned long long, checked::mul>(
				high_bit<unsigned long long>, high_bit<unsigned long long>,
				true, 0ull /* stays the same */
			);
	}
}

#ifdef __SIZEOF_INT128__tomakethesymbolfail
using s128 = __int128;
using u128 = unsigned __int128;
#else

template<typename T64>
struct Int128 {
	T64 high;
	T64 low;

	Int128(): high(), low() {}
	// high is sign-extended from t if signed
	Int128(T64 t): high((t >> 63) >> 1), low(t) {}
	Int128(T64 h, T64 l): high(h), low(l) {}

	static constexpr T64 safe_mul(T64 a, T64 b) {
		return T64(uint64_t(a) * uint64_t(b));
	}
	static constexpr T64 safe_add(T64 a, T64 b) {
		return T64(uint64_t(a) + uint64_t(b));
	}

	static constexpr T64 carry_bit = 0x8000000000000000;
	Int128 operator +(const Int128& b) const {
		// there was a carry if the two arguments have the same sign and the sign of the result is different
		bool carry = (b.low & carry_bit) == (this->low & carry_bit)
			&& ((safe_add(this->low, b.low) & carry_bit) != (b.low & carry_bit));

		return { safe_add(safe_add(this->high, b.high), carry), safe_add(b.low, this->high) };
	}
	Int128 operator -() const {
		// -x is equivalent to (~x) + 1, in t's complement
		return Int128{~this->high, ~this->low} + Int128(1);
	}
	Int128 operator -(const Int128& b) const {
		return *this + -b;
	}

	static T64 mult_high(T64 a, T64 b) {
		// from https://stackoverflow.com/questions/28868367/getting-the-high-part-of-64-bit-integer-multiplication/28904636#28904636
		uint64_t a_lo = (uint32_t) a;
		uint64_t a_hi = a >> 32;
		uint64_t b_lo = (uint32_t) b;
		uint64_t b_hi = b >> 32;

		uint64_t a_x_b_hi =  a_hi * b_hi;
		uint64_t a_x_b_mid = a_hi * b_lo;
		uint64_t b_x_a_mid = b_hi * a_lo;
		uint64_t a_x_b_lo =  a_lo * b_lo;

		uint64_t carry_bit = ((uint64_t)(uint32_t)a_x_b_mid +
		                      (uint64_t)(uint32_t)b_x_a_mid +
		                      (a_x_b_lo >> 32) ) >> 32;

		uint64_t multhi = a_x_b_hi +
		                  (a_x_b_mid >> 32) + (b_x_a_mid >> 32) +
		                  carry_bit;

		return T64(multhi);
	}

	Int128 operator *(const Int128& b) const {
		return {
			mult_high(b.low, this->low),
			safe_mul(b.low, this->low)
		};
	}
	operator T64() const {
		return this->low;
	}

	bool operator ==(const Int128& b) const {
		return this->high == b.high && this->low == b.low;
	}
	bool operator !=(const Int128& b) const {
		return !(*this == b);
	}
};

using s128 = Int128<int64_t>;
using u128 = Int128<uint64_t>;

#endif

template<typename T>
static constexpr auto higher_val() {
	if constexpr (sizeof(T) == 8) {
		if constexpr (std::is_signed_v<T>) {
			return s128 ();
		} else {
			return u128 ();
		}
	} else {
		return checked::NextSize<T> ();
	}
}

TEMPLATE_TEST_CASE("random checked operation tests", "", \
					char, int8_t, uint8_t, int16_t, uint16_t, \
					int32_t, uint32_t, int64_t, uint64_t) {
	using T = TestType;
	using Higher = decltype(higher_val<T>());

	auto rng = get_rng();

	// full range
	std::uniform_int_distribution<T> full(std::numeric_limits<T>::min(),
	                                      std::numeric_limits<T>::max());

	SECTION( "random, in half range (can't overflow)" ) {
		for (unsigned i = 0; i < 1000; ++i) {
			T a = full(rng) >> 1;
			T b = full(rng) >> 1;

			T expected_result = a + b;
			bool expected_overflow = false;
			testExpected<T, checked::add>(a, b, expected_overflow, expected_result);

			expected_result = a - b;
			testExpected<T, checked::sub>(a, b, expected_overflow, expected_result);

			expected_result = a * b;
			testExpected<T, checked::mul>(a, b, expected_overflow, expected_result);
		}
	}

	SECTION( "random, in full range" ) {
		for (unsigned i = 0; i < 1000; ++i) {
			T a = full(rng);
			T b = full(rng);

			T expected_result = T(0);

			bool expected_overflow = checked::check_widening<T, std::plus, Higher>(
					a, b, &expected_result);
			testExpected<T, checked::add>(a, b, expected_overflow, expected_result);

			expected_result = T(0);
			expected_overflow = checked::check_widening<T, std::minus, Higher>(
					a, b, &expected_result);
			testExpected<T, checked::sub>(a, b, expected_overflow, expected_result);

			expected_result = T(0);
			expected_overflow = checked::check_widening<T, std::multiplies, Higher>(
					a, b, &expected_result);
			testExpected<T, checked::mul>(a, b, expected_overflow, expected_result);
		}
	}
}
