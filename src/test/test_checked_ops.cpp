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
std::ostream &operator <<(std::ostream &os, const PrintInt<T> &value);
template<typename T>
std::ostream &operator <<(std::ostream &os, const std::optional<T> &value);
#include "catch.h"

#include "util/checked_ops.h"

template<typename T>
static constexpr const T high_bit = T(1) << (sizeof(T) * 8 - 1);

static auto get_rng() {
	static const std::default_random_engine engine {std::random_device()()};
	// Copy the seeded engine
	return engine;
}

// not constexpr: the comparison involves casting to a function pointer
template<typename T, bool operation(T, T, T*)>
static const char *op_str = [](){
		// Use explicit casts to function pointers to satisfy GCC 9
		using OpType = bool (*)(T, T, T*);
		if (OpType(operation) == OpType(checked::add<T>)) {
			return "+";
		} else if (OpType(operation) == OpType(checked::sub<T>)) {
			return "-";
		} else if (OpType(operation) == OpType(checked::mul<T>)) {
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

template<typename T, bool operation(T, T, T*), bool overflows(T, T), typename M = char*>
static void testExpected(T a, T b, bool expected_overflow, T expected_result, const M *message = nullptr) {
	if (message != nullptr) {
		UNSCOPED_INFO( *message );
	}

	const char *const op_name = op_str<T, operation>;

	UNSCOPED_INFO( "arguments: " << PrintInt(a) << " " \
							 << op_name << " " \
							 << PrintInt(b) );


	// hope that using the comma operator lets Catch2 print the values on failure
	SECTION("bool overflows(T, T)") {
		REQUIRE( overflows(a, b) == expected_overflow );
	}
	SECTION("bool operation(T, T, T*)") {
		T actual_result {0};
		bool actual_overflow = operation(a, b, &actual_result);

		REQUIRE( \
			 make_optional(expected_overflow, expected_result) \
			 == make_optional(actual_overflow, actual_result) );
	}
}
// stringify an optional
template<typename T>
std::ostream &operator <<(std::ostream &os, const std::optional<T> &value) {
	if (!value) {
		os << "None";
	} else {
		os << "Some(" << *value << ")";
	}
	return os;
}

template<typename T, bool is_signed = std::is_signed_v<T>, size_t size = sizeof(T)>
extern const char *const short_name;
#define MakeShortName(is_signed, bytes, namestr) \
	template<typename T> \
	static constexpr const char *const short_name<T, is_signed, bytes> = namestr;

MakeShortName(false, 1, "u8")
MakeShortName(false, 2, "u16")
MakeShortName(false, 4, "u32")
MakeShortName(false, 8, "u64")
MakeShortName(false, 16, "u128")
MakeShortName(true, 1, "s8")
MakeShortName(true, 2, "s16")
MakeShortName(true, 4, "s32")
MakeShortName(true, 8, "s64")
MakeShortName(true, 16, "s128")

#undef MakeShortName

// stringify a PrintInt
template<typename T>
std::ostream &operator <<(std::ostream &os, const PrintInt<T> &value) {
	if constexpr (std::is_same_v<unsigned char, std::make_unsigned_t<T>>) {
		os << (int) value.value << short_name<T>;
	} else {
		os << value.value << short_name<T>;
	}
	return os;
}


template<typename T>
static constexpr const T MIN = std::numeric_limits<T>::min();

TEST_CASE("fixed checked (+)/(-) tests") {
	SECTION( "std::optional interface" ) {
		using OI = std::optional<int>;
		// check for support of optional return values
		REQUIRE( checked::add(0, 0) == OI(0) );
		REQUIRE( checked::add(1, 0) == OI(1) );
		REQUIRE( checked::add(1, 1) == OI(2) );
		REQUIRE( checked::add(0, 1) == OI(1) );
		REQUIRE( checked::add(MIN<int>, MIN<int>) == OI() );
	}

	SECTION( "~0 + ~0 overflows for unsigned" ) {
		testExpected<int, checked::add, checked::add_overflows>(-1, -1, false, -2);
		testExpected<long long, checked::add, checked::add_overflows>(-1ll, -1ll, false, -2ll);
		testExpected<unsigned, checked::add, checked::add_overflows>(~0u, ~0u,
		                                     true, 0u /* stays the same */);
		testExpected<unsigned long long, checked::add, checked::add_overflows>(
				~0ull, ~0ull,
				true, 0ull /* stays the same */
			);
	}

	SECTION( "high_bit + high_bit always overflows" ) {
		testExpected<int, checked::add, checked::add_overflows>(high_bit<int>, high_bit<int>,
		                                true, 0 /* stays the same */);
		testExpected<long long, checked::add, checked::add_overflows>(
				high_bit<long long>, high_bit<long long>,
				true, 0 /* stays the same */
			);
		testExpected<unsigned, checked::add, checked::add_overflows>(
				high_bit<unsigned>, high_bit<unsigned>,
				true, 0 /* stays the same */
			);
		testExpected<unsigned long long, checked::add, checked::add_overflows>(
				high_bit<unsigned long long>, high_bit<unsigned long long>,
				true, 0ull /* stays the same */
			);
	}

	SECTION( "1 + 2 never overflows" ) {
		testExpected<int, checked::add, checked::add_overflows>(1, 2, false, 3);
		testExpected<long long, checked::add, checked::add_overflows>(1ll, 2ll, false, 3ll);
		testExpected<unsigned int, checked::add, checked::add_overflows>(1u, 2u, false, 3u);
		testExpected<unsigned long long, checked::add, checked::add_overflows>(1ull, 2ull, false, 3ull);
	}
}

TEST_CASE("fixed checked (*) tests") {
	SECTION( "2 * 3 never overflows" ) {
		testExpected<int, checked::mul, checked::mul_overflows>(2, 3, false, 6);
		testExpected<long long int, checked::mul, checked::mul_overflows>(2ll, 3ll, false, 6ll);
		testExpected<unsigned, checked::mul, checked::mul_overflows>(2u, 3u, false, 6u);
		testExpected<unsigned long long, checked::mul, checked::mul_overflows>(2ull, 3ull, false, 6ull);
	}

	SECTION( "~0 * 2 overflows for unsigned" ) {
		testExpected<int, checked::mul, checked::mul_overflows>(~0, 2, false, -2);
		testExpected<long long int, checked::mul, checked::mul_overflows>(~0ll, 2, false, -2);
		testExpected<unsigned int, checked::mul, checked::mul_overflows>(
				~0, 2, true, 0 /* stays the same */
			);
		testExpected<unsigned long long int, checked::mul, checked::mul_overflows>(
				~0ull, 2, true, 0 /* stays the same */
			);
		testExpected<int, checked::mul, checked::mul_overflows>(2, ~0, false, -2);
		testExpected<long long int, checked::mul, checked::mul_overflows>(2, ~0ll, false, -2);
		testExpected<unsigned int, checked::mul, checked::mul_overflows>(
				2, ~0u, true, 0 /* stays the same */
			);
		testExpected<unsigned long long int, checked::mul, checked::mul_overflows>(
				2, ~0ull, true, 0 /* stays the same */
			);
	}

	SECTION( "~0 * ~0 overflows for unsigned" ) {
		testExpected<int, checked::mul, checked::mul_overflows>(~0, ~0, false, 1);
		testExpected<long long int, checked::mul, checked::mul_overflows>(~0ll, ~0ll, false, 1ll);
		testExpected<unsigned int, checked::mul, checked::mul_overflows>(
				~0u, ~0u, true, 0 /* stays the same */
			);
		testExpected<unsigned long long int, checked::mul, checked::mul_overflows>(
				~0ull, ~0ull, true, 0 /* stays the same */
			);
	}

	SECTION( "min<T> * ~0 overflows for signed" ) {
		testExpected<int, checked::mul, checked::mul_overflows>(
				MIN<int>, ~0, true, 0 /* stays the same */
			);
		testExpected<long long, checked::mul, checked::mul_overflows>(
				MIN<long long>, ~0ll, true, 0ll /* stays the same */
			);
		testExpected<unsigned, checked::mul, checked::mul_overflows>(
				MIN<unsigned>, ~0u, false, 0u
			);
		testExpected<unsigned long long, checked::mul, checked::mul_overflows>(
				MIN<unsigned long long>, ~0ull, false, 0ull
			);
	}
	SECTION( "min<T> * 1 never overflows" ) {
		testExpected<int, checked::mul, checked::mul_overflows>(
				MIN<int>, ~0, false, MIN<int>
			);
		testExpected<long long, checked::mul, checked::mul_overflows>(
				MIN<long long>, ~0ll, false, MIN<long long>
			);
		testExpected<unsigned, checked::mul, checked::mul_overflows>(
				MIN<unsigned>, ~0u, false, 0u
			);
		testExpected<unsigned long long, checked::mul, checked::mul_overflows>(
				MIN<unsigned long long>, ~0ull, false, 0ull
			);
	}

	SECTION( "high_bit * high_bit always overflows" ) {
		testExpected<int, checked::mul, checked::mul_overflows>(
				high_bit<int>, high_bit<int>, true, 0 /* stays the same */
			);
		testExpected<long long, checked::mul, checked::mul_overflows>(
				high_bit<long long>, high_bit<long long>,
				true, 0ll /* stays the same */
			);
		testExpected<unsigned, checked::mul, checked::mul_overflows>(
				high_bit<unsigned>, high_bit<unsigned>, true, 0u /* stays the same */
			);
		testExpected<unsigned long long, checked::mul, checked::mul_overflows>(
				high_bit<unsigned long long>, high_bit<unsigned long long>,
				true, 0ull /* stays the same */
			);
	}
}

#ifdef __SIZEOF_INT128__
using s128 = __int128;
using u128 = unsigned __int128;
#endif

template<typename T>
static constexpr auto higher_val() {
#ifdef __SIZEOF_INT128__
	if constexpr (sizeof(T) == 8) {
		if constexpr (std::is_signed_v<T>) {
			return s128 ();
		} else {
			return u128 ();
		}
	} else {
		// can't lift this branch into the scope above because
		//		then the compiler will always compile it
#endif
		return checked::NextSize<T> ();
#ifdef __SIZEOF_INT128__
	}
#endif
}

template<typename T>
static T max(T a, T b) {
	return a >= b? a: b;
}

// generate int for char types, and T otherwise
template<typename T>
static constexpr auto gentype_fn() {
	if constexpr (std::is_same_v<char, T>
	           || std::is_same_v<unsigned char, T>
	           || std::is_same_v<  signed char, T>) {
		return int();
	} else {
		return T();
	}
}

TEMPLATE_TEST_CASE("random checked operation tests", "", \
					char, int8_t, uint8_t, int16_t, uint16_t, \
					int32_t, uint32_t
#ifdef __SIZE_INT128__
					// test it on 64-bit integers too
					, int64_t, uint64_t
#endif
					) {
	using T = TestType;
	using Higher = decltype(higher_val<T>());

	using GenType = decltype(gentype_fn<T>());

	auto rng = get_rng();

	// full range
	std::uniform_int_distribution<GenType> full(std::numeric_limits<T>::min(),
	                                            std::numeric_limits<T>::max());

	SECTION( "random, in half range (can't overflow)" ) {
		for (unsigned i = 0; i < 1000; ++i) {
			T add_a = T(full(rng)) >> 1;
			T add_b = T(full(rng)) >> 1;

			T expected_result = add_a + add_b;
			bool expected_overflow = false;
			testExpected<T, checked::add, checked::add_overflows>(add_a, add_b, expected_overflow, expected_result);
		}
	}

	SECTION( "random, in full range" ) {
		for (unsigned i = 0; i < 16384; ++i) {
			T a = T(full(rng));
			T b = T(full(rng));

			T expected_result = T(0);

			bool expected_overflow = checked::check_widening<T, std::plus, Higher>(
					a, b, &expected_result);
			testExpected<T, checked::add, checked::add_overflows>(a, b, expected_overflow, expected_result);

			expected_result = T(0);
			expected_overflow = checked::check_widening<T, std::minus, Higher>(
					a, b, &expected_result);
			testExpected<T, checked::sub, checked::sub_overflows>(a, b, expected_overflow, expected_result);

			expected_result = T(0);
			expected_overflow = checked::check_widening<T, std::multiplies, Higher>(
					a, b, &expected_result);
			testExpected<T, checked::mul, checked::mul_overflows>(a, b, expected_overflow, expected_result);
		}
	}
}
