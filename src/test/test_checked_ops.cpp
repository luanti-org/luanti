#include <cstdint>
#include <functional>
#include <type_traits>
#include <random>
#include <limits>
#include <optional>
#include <ostream>
#include <typeinfo>

template<typename T>
struct PrintInt;
template<typename T>
std::ostream& operator <<(std::ostream& os, const PrintInt<T>& value);
template<typename T>
std::ostream& operator <<(std::ostream& os, std::optional<T> const& value);
#include "catch.h"

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

template<typename T>
struct Wrap {
	T value;
};

template<typename T, bool operation(T, T, T*)>
static constexpr const char* op_str = [](){
		if constexpr(operation == checked::add<T>) {
			return "+";
		} else if constexpr (operation == checked::sub<T>) {
			return "-";
		} else if constexpr (operation == checked::mul<T>) {
			return "*";
		} else {
			return "<?>";
		}
	}();

// force chars to stringify as integers
template<typename T>
struct PrintInt {
	T value;

	constexpr PrintInt(T t): value(t) {}
	bool operator ==(const PrintInt& other) const {
		return this->value == other.value;
	}
	bool operator !=(const PrintInt& other) const {
		return this->value != other.value;
	}
};
template<typename T>
static std::optional<PrintInt<T>> make_optional(bool did_overflow, T value) {
	if (!did_overflow) {
		return PrintInt(value);
	} else {
		return std::nullopt;
	}
}

template<typename T, bool operation(T, T, T*), typename M = char*>
static void testExpected(T a, T b, bool expected_overflow, T expected_result, const M* message = nullptr) {
	T actual_result = T(0);
	bool actual_overflow = operation(a, b, &actual_result);

	if (message != nullptr) {
		UNSCOPED_INFO( *message );
	}

	const char* const op_name = op_str<T, operation>;

	UNSCOPED_INFO( "arguments: " << PrintInt(a) << " " \
								 << op_name << " " \
								 << PrintInt(b) );

	REQUIRE( make_optional(expected_overflow, expected_result) \
			 == make_optional(actual_overflow, actual_result) );
}
// stringify an optional
template<typename T>
std::ostream& operator <<(std::ostream& os, std::optional<T> const& value) {
	if (!value) {
		os << "None";
	} else {
		os << "Some(" << *value << ")";
	}
    return os;
}

template<typename T, bool is_signed = std::is_signed_v<T>, size_t size = sizeof(T)>
extern const char* const short_name;
template<typename T>
static constexpr const char* const short_name<T, true, 1> = "s8";
template<typename T>
static constexpr const char* const short_name<T, false, 1> = "u8";
template<typename T>
static constexpr const char* const short_name<T, true, 2> = "s16";
template<typename T>
static constexpr const char* const short_name<T, false, 2> = "u16";
template<typename T>
static constexpr const char* const short_name<T, true, 4> = "s32";
template<typename T>
static constexpr const char* const short_name<T, false, 4> = "u32";
template<typename T>
static constexpr const char* const short_name<T, true, 8> = "s64";
template<typename T>
static constexpr const char* const short_name<T, false, 8> = "u64";
template<typename T>
static constexpr const char* const short_name<T, true, 16> = "s128";
template<typename T>
static constexpr const char* const short_name<T, false, 16> = "u128";

// stringify a PrintInt
template<typename T>
std::ostream& operator <<(std::ostream& os, const PrintInt<T>& value) {
	if constexpr (std::is_same_v<unsigned char, std::make_unsigned_t<T>>) {
		os << (int) value.value << short_name<T>;
	} else {
		os << value.value << short_name<T>;
	}
	return os;
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

#ifdef __SIZEOF_INT128__
using s128 = __int128;
using u128 = unsigned __int128;
#endif

template<typename T64>
struct Int128 {
	T64 high;
	T64 low;

	constexpr Int128(): high(), low() {}
	// high is sign-extended from t if signed
	constexpr Int128(T64 t): high((t >> 63) >> 1), low(t) {}
	constexpr Int128(T64 h, T64 l): high(h), low(l) {}

	static Int128 fromHighLow(T64 high, T64 low) {
		return { high, low };
	}
	static Int128 fromLowHigh(T64 low, T64 high) {
		return { high, low };
	}

	static constexpr T64 safe_mul(T64 a, T64 b) {
		return T64(uint64_t(a) * uint64_t(b));
	}
	static constexpr T64 safe_add(T64 a, T64 b) {
		return T64(uint64_t(a) + uint64_t(b));
	}

	static constexpr T64 carry_bit = 0x8000000000000000;
	Int128 operator +(const Int128& b) const {
		// break it into 32-bit chunks for simplicity
		using U64 = uint64_t;

		U64 a_lowlow   = U64(low)  & 0xffffffffu;
		U64 a_lowhigh  = U64(low)  >> 32;
		U64 a_highlow  = U64(high) & 0xffffffffu;
		U64 a_highhigh = U64(high) >> 32;

		U64 b_lowlow   = U64(b.low)  & 0xffffffffu;
		U64 b_lowhigh  = U64(b.low)  >> 32;
		U64 b_highlow  = U64(b.high) & 0xffffffffu;
		U64 b_highhigh = U64(b.high) >> 32;

		bool carry = false;

		U64 new_lowlow = a_lowlow + b_lowlow;
		carry = U64(uint32_t(new_lowlow)) != new_lowlow;
		new_lowlow &= 0xffffffffu;

		U64 new_lowhigh = carry + a_lowhigh + b_lowhigh;
		carry = U64(uint32_t(new_lowhigh)) != new_lowhigh;
		new_lowhigh &= 0xffffffffu;

		U64 new_highlow = carry + a_highlow + b_highlow;
		carry = U64(uint32_t(new_highlow)) != new_highlow;
		new_highlow &= 0xffffffffu;

		U64 new_highhigh = carry + a_highhigh + b_highhigh;
		//carry = U64(uint32_t(new_highhigh)) != new_highhigh;
		new_highhigh &= 0xffffffffu;

		return Int128( T64((new_highhigh << 32) | new_highlow),
					   T64((new_lowhigh << 32 ) | new_lowlow ) ) ;
	}
	Int128 operator -() const {
		// -x is equivalent to (~x) + 1, in 2's complement
		return Int128{~this->high, ~this->low} + Int128(1);
	}
	Int128 operator -(const Int128& b) const {
		return *this + -b;
	}

	static Int128 mult(const Int128& a, const Int128& b) {
		using U64 = uint64_t;

		U64 a_lowlow   = U64(a.low)  & 0xffffffffu;
		U64 a_lowhigh  = U64(a.low)  >> 32;
		U64 a_highlow  = U64(a.high) & 0xffffffffu;
		U64 a_highhigh = U64(a.high) >> 32;

		U64 b_lowlow   = U64(b.low)  & 0xffffffffu;
		U64 b_lowhigh  = U64(b.low)  >> 32;
		U64 b_highlow  = U64(b.high) & 0xffffffffu;
		U64 b_highhigh = U64(b.high) >> 32;

		// (a_lowlow * 2^0 + a_lowhigh * 2^32 + a_highlow * 2^64 + a_highhigh * 2^96) *
		// (b_lowlow * 2^0 + b_lowhigh * 2^32 + b_highlow * 2^64 + b_highhigh * 2^96) =>
		// (a_lowlow * b_lowlow + a_lowlow * b_lowhigh * 2^32 + a_lowlow * b_highlow * 2^64 + a_lowlow * b_highhigh * 2^96
		//	+ a_lowhigh * b_lowlow * 2^32 + a_lowhigh * b_lowhigh * 2^64 + a_lowhigh * b_highlow * 2^96 + a_lowhigh * b_highhigh * 2^128
		//	+ a_highlow * b_lowlow * 2^64 + a_highlow * b_lowhigh * 2^96 + a_highlow * b_highlow * 2^128 + a_highlow * b_highhigh * 2^160
		//	+ a_highhigh * b_lowlow * 2^96 + a_highhigh + b_lowhigh * 2^128 + a_highhigh * b_highlow * 2^160 + a_highhigh * b_highhigh * 2^192)
		// =>
		// (a_lowlow * b_lowlow + a_lowlow * b_lowhigh * 2^32 + a_lowlow * b_highlow * 2^64 + a_lowlow * b_highhigh * 2^96
		//	+ a_lowhigh * b_lowlow * 2^32 + a_lowhigh * b_lowhigh * 2^64 + a_lowhigh * b_highlow * 2^96
		//	+ a_highlow * b_lowlow * 2^64 + a_highlow * b_lowhigh * 2^96
		//	+ a_highhigh * b_lowlow * 2^96)

		// 2^0
		U64 all_bll = a_lowlow  * b_lowlow ;
		// 2^32
		U64 all_blh = a_lowlow  * b_lowhigh;
		U64 alh_bll = a_lowhigh * b_lowlow ;
		// 2^64
		U64 all_bhl = a_lowlow  * b_highlow;
		U64 alh_blh = a_lowhigh * b_lowhigh;
		U64 ahl_bll = a_highlow * b_lowlow;
		// 2^96
		U64 all_bhh = a_lowlow * b_highhigh;
		U64 alh_bhl = a_lowhigh * b_highlow;
		U64 ahl_blh = a_highlow * b_lowhigh;
		U64 ahh_bll = a_highhigh * b_lowlow;

		// 2^0 and 2^64: all_bll, all_bhl
		Int128 a_ll1 = { T64(all_bhl), T64(all_bll) };
		// 2^32 and 2^96: all_blh, all_bhh
		Int128 a_ll2 = Int128::fromLowHigh(T64(all_blh << 32),
		                                   T64((all_bhh << 32) | (all_blh >> 32)) );
		Int128 a_ll = a_ll1 + a_ll2;

		// 2^0 and 2^64: 0, alh_blh
		Int128 a_lh1 = { T64(alh_blh), T64(0) };
		// 2^32 and 2^96: alh_bll, alh_bhl
		Int128 a_lh2 = Int128::fromLowHigh(T64(alh_bll << 32),
		                                   T64((alh_bhl << 32) | (alh_bll >> 32)) );
		Int128 a_lh = a_lh1 + a_lh2;

		// 2^0 and 2^64: 0, ahl_bll
		//Int128 a_hl1 = { T64(ahl_bll), T64(0) };
		// 2^32 and 2^96: 0, ahl_blh
		//Int128 a_hl2 = { T64(ahl_blh << 32), T64(0) };
		// safe to merge because adding the above two
		//			can't involve any carries between 32-bit chunks
		Int128 a_hl = { T64(ahl_bll + (ahl_blh << 32)), T64(0) };

		Int128 a_hh = { T64(ahh_bll << 32), T64(0) };

		return a_ll + a_lh + a_hl + a_hh;
	}

	Int128 operator *(const Int128& b) const {
		return Int128::mult(*this, b);
	}

	bool operator ==(const Int128& b) const {
		return this->high == b.high && this->low == b.low;
	}
	bool operator !=(const Int128& b) const {
		return !(*this == b);
	}

	friend std::ostream& operator <<(std::ostream& os, const Int128& self) {
		os << std::hex << uint64_t(self.high)
		   << std::hex << uint64_t(self.low);
		os << '_' << (std::is_signed_v<T64>? 's': 'u') << "128";
		return os;
	}
};

namespace std {
	// specialize std::numeric_limits for Int128
	template<>
	class numeric_limits<Int128<int64_t>> {
		static constexpr bool is_specialized = true;
		static constexpr bool is_signed = true;
		static constexpr bool is_integer = true;
		static constexpr bool is_exact = true;
		static constexpr bool has_infinity = false;
		static constexpr bool has_quiet_NaN = false;
		static constexpr bool has_signaling_NaN = false;
		static constexpr std::float_denorm_style denorm_style =
										std::denorm_absent;
		static constexpr bool has_denorm_loss = false;
		static constexpr bool round_style = std::round_toward_zero;
		static constexpr bool is_iec559 = false;
		static constexpr bool is_bounded = true;
		static constexpr bool is_modulo = true;
		static constexpr int digits = 128 - 1; // 1 effective sign bit
		static constexpr int digits10 = 38;
		static constexpr int max_digits10 = 0;
		static constexpr int radix = 2;
		static constexpr int min_exponent = 0;
		static constexpr int min_exponent10 = 0;
		static constexpr int max_exponent = 0;
		static constexpr int max_exponent10 = 0;
		static constexpr int traps =
			std::numeric_limits<int64_t>::traps;
		static constexpr bool tinyness_before = false;

		static constexpr auto min() noexcept {
			// assumes int64_t is two's complement
			return Int128<int64_t>{
				std::numeric_limits<int64_t>::min(), 0
			};
		}
		static constexpr auto lowest() noexcept {
			return min();
		}
		static constexpr auto max() noexcept {
			// assumes int64_t is two's complement
			return Int128<int64_t>{
				// 0x8ffffffffff... , 0xfffffffff...
				std::numeric_limits<int64_t>::max(), -1ll
			};
		}
		static constexpr Int128<int64_t> epsilon() noexcept {
			return 0;
		}
		static constexpr Int128<int64_t> round_error() noexcept {
			return 0;
		}
		static constexpr Int128<int64_t> infinity() noexcept {
			return 0;
		}
		static constexpr Int128<int64_t> quiet_NaN() noexcept {
			return 0;
		}
		static constexpr Int128<int64_t> signaling_NaN() noexcept {
			return 0;
		}
		static constexpr Int128<int64_t> denorm_min() noexcept {
			return 0;
		}
	};
	template<>
	class numeric_limits<Int128<uint64_t>> {
		static constexpr bool is_specialized = true;
		static constexpr bool is_signed = false;
		static constexpr bool is_integer = true;
		static constexpr bool is_exact = true;
		static constexpr bool has_infinity = false;
		static constexpr bool has_quiet_NaN = false;
		static constexpr bool has_signaling_NaN = false;
		static constexpr std::float_denorm_style denorm_style =
										std::denorm_absent;
		static constexpr bool has_denorm_loss = false;
		static constexpr bool round_style = std::round_toward_zero;
		static constexpr bool is_iec559 = false;
		static constexpr bool is_bounded = true;
		static constexpr bool is_modulo = true;
		static constexpr int digits = 128;
		static constexpr int digits10 = 38;
		static constexpr int max_digits10 = 0;
		static constexpr int radix = 2;
		static constexpr int min_exponent = 0;
		static constexpr int min_exponent10 = 0;
		static constexpr int max_exponent = 0;
		static constexpr int max_exponent10 = 0;
		static constexpr int traps =
			std::numeric_limits<int64_t>::traps;
		static constexpr bool tinyness_before = false;

		static constexpr auto min() noexcept {
			return Int128<uint64_t>{0ull, 0ull};
		}
		static constexpr auto lowest() noexcept {
			return min();
		}
		static constexpr auto max() noexcept {
			uint64_t u64max = std::numeric_limits<uint64_t>::max();
			return Int128<uint64_t>{ u64max, u64max };
		}
		static constexpr Int128<uint64_t> epsilon() noexcept {
			return 0;
		}
		static constexpr Int128<uint64_t> round_error() noexcept {
			return 0;
		}
		static constexpr Int128<uint64_t> infinity() noexcept {
			return 0;
		}
		static constexpr Int128<uint64_t> quiet_NaN() noexcept {
			return 0;
		}
		static constexpr Int128<uint64_t> signaling_NaN() noexcept {
			return 0;
		}
		static constexpr Int128<uint64_t> denorm_min() noexcept {
			return 0;
		}
	};
}

#ifndef __SIZEOF_INT128__
// use our polyfill as the int128 type, for testing
using s128 = Int128<int64_t>;
using u128 = Int128<uint64_t>;
#else
// test our polyfill against __int128

using u_int128 = unsigned __int128;

// __int128 vs Int128 tests
// TEMPLATE_TEST_CASE_SIG didn't work, so...
template<typename N128_, typename My128_, typename N64_>
struct Int128Triple {
	using My128 = My128_;
	using N128 = N128_;
	using N64 = N64_;
};
TEMPLATE_TEST_CASE("__int128 vs Int128 tests", "", \
			(Int128Triple<__int128, Int128<int64_t>, int64_t>), \
			(Int128Triple<u_int128, Int128<uint64_t>, uint64_t>) \
		) {
	using u64 = uint64_t;
	using My128 = typename TestType::My128;
	using N128 = typename TestType::N128;
	using N64 = typename TestType::N64;

	SECTION("fixed tests") {
		CAPTURE( My128(N128(~0ull)), My128(~0ull) );
	}

	SECTION("random operations") {
		auto rng = get_rng();

		std::uniform_int_distribution<u64> full_u64(
				std::numeric_limits<u64>::min(),
		        std::numeric_limits<u64>::max()
			);
		// in Rust this'd need rng as a parameter,
		//		because of the mutable borrow
		auto gen_n128 = [&](){
			static_assert(sizeof(N128) >= 16, "N128 must be 128-bit");
			N128 high = full_u64(rng);
			high = high << 64;
			// cast to unsigned first to be sure not to sign-extend
		    return high | N128((unsigned __int128) full_u64(rng));
		};
		auto my128_from_n128 = [](N128 n128) {
			return My128( (N64) uint64_t(n128 >> 64), (N64) uint64_t(n128) );
		};

		for (unsigned i = 0; i < 16384; ++i) {
			auto a_n = gen_n128();
			auto a_m = my128_from_n128(a_n);
			auto b_n = gen_n128();
			auto b_m = my128_from_n128(b_n);

			INFO( "arguments: " << a_m << " and " << b_m );
			REQUIRE( my128_from_n128(a_n + b_n) == a_m + b_m );
			REQUIRE( my128_from_n128(a_n - b_n) == a_m - b_m );
			REQUIRE( my128_from_n128(a_n * b_n) == a_m * b_m );
		}
	}
}

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

template<typename T>
static T max(T a, T b) {
	return a >= b? a: b;
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
	/*
	// safe sub
	std::uniform_int_distribution<T> safe_sub(std::numeric_limits<T>::min() >> 1,
	                                          std::numeric_limits<T>::max() >> 1);

	// safe mul
	std::uniform_int_distribution<T> safe_mul(std::numeric_limits<T>::min() >> (sizeof(T) * 4),
	                                          std::numeric_limits<T>::max() >> (sizeof(T) * 4));*/

	SECTION( "random, in half range (can't overflow)" ) {
		for (unsigned i = 0; i < 1000; ++i) {
			T add_a = full(rng) >> 1;
			T add_b = full(rng) >> 1;

			T expected_result = add_a + add_b;
			bool expected_overflow = false;
			testExpected<T, checked::add>(add_a, add_b, expected_overflow, expected_result);

			// TODO: ranges are wrong for this, so don't use these tests for now
			/*T sub_a = safe_sub(rng);
			T sub_b = safe_sub(rng);
			expected_result = sub_a - sub_b;
			testExpected<T, checked::sub>(sub_a, sub_b, expected_overflow, expected_result);

			T mul_a = safe_mul(rng);
			T mul_b = safe_mul(rng);
			expected_result = mul_a * mul_b;
			testExpected<T, checked::mul>(mul_a, mul_b, expected_overflow, expected_result);*/
		}
	}

	SECTION( "random, in full range" ) {
		for (unsigned i = 0; i < 16384; ++i) {
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
