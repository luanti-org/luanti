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

// not constexpr: the comparison involves casting to a function pointer
template<typename T, bool operation(T, T, T*)>
static const char* op_str = [](){
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
template<typename T>
static constexpr auto higher_val() {
	if constexpr (sizeof(T) == 8) {
		if constexpr (std::is_signed_v<T>) {
			return __int128 ();
		} else {
			return (unsigned __int128) 0;
		}
	} else {
		return checked::NextSize<T> ();
	}
}
#else
template<typename T>
static constexpr auto higher_val() {
	return checked::NextSize<T> ();
}
#endif

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
#ifdef __SIZEOF_INT128__
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
			testExpected<T, checked::add>(add_a, add_b, expected_overflow, expected_result);
		}
	}

	SECTION( "random, in full range" ) {
		for (unsigned i = 0; i < 16384; ++i) {
			T a = T(full(rng));
			T b = T(full(rng));

			T expected_result = T(0);

			bool expected_overflow =
				checked::check_widening<T, std::plus, Higher>(
						a, b, &expected_result
					);
			testExpected<T, checked::add>(a, b, expected_overflow, expected_result);

			expected_result = T(0);
			expected_overflow =
				checked::check_widening<T, std::minus, Higher>(
						a, b, &expected_result
					);
			testExpected<T, checked::sub>(a, b, expected_overflow, expected_result);

			expected_result = T(0);
			expected_overflow =
				checked::check_widening<T, std::multiplies, Higher>(
						a, b, &expected_result
					);
			testExpected<T, checked::mul>(a, b, expected_overflow, expected_result);
		}
	}
}

#ifdef __SIZEOF_INT128__

template<typename T, template<typename> typename Operation, typename Wider = decltype(higher_val<T>())>
bool has_overflow_wider(T a, T b) {
	Wider result = Operation<Wider>()(Wider(a), Wider(b));
	// It overflowed if the Wider result isn't round-trip'able to T and back
	return result != Wider(T(result));
}

// Test bit-operation overflow-test methods against __int128
TEMPLATE_TEST_CASE("random __int128 vs bit-op overflow tests", "", \
					char, int8_t, uint8_t, int16_t, uint16_t, \
					int32_t, uint32_t, int64_t, uint64_t) {
	using T = TestType;
	using Higher = decltype(higher_val<T>());

	using GenType = decltype(gentype_fn<T>());

	auto rng = get_rng();

	std::uniform_int_distribution<GenType> full(std::numeric_limits<T>::min(),
	                                            std::numeric_limits<T>::max());

	for (unsigned i = 0; i < 16384; ++i) {
		T a = T(full(rng));
		T b = T(full(rng));

		UNSCOPED_INFO("testing overflow on " << GenType(a) << short_name<T> << " + " << GenType(b) << short_name<T>);
		bool expected_overflowed = has_overflow_wider<T, std::plus, Higher>(a, b);
		bool bitop_overflowed = checked::add_overflows(a, b);
		REQUIRE( expected_overflowed == bitop_overflowed );

		UNSCOPED_INFO("testing overflow on " << GenType(a) << short_name<T> << " - " << GenType(b) << short_name<T>);
		expected_overflowed = has_overflow_wider<T, std::minus, Higher>(a, b);
		bitop_overflowed = checked::sub_overflows(a, b);
		REQUIRE( expected_overflowed == bitop_overflowed );

		UNSCOPED_INFO("testing overflow on " << GenType(a) << short_name<T> << " * " << GenType(b) << short_name<T>);
		expected_overflowed = has_overflow_wider<T, std::multiplies, Higher>(a, b);
		bitop_overflowed = checked::mul_overflows(a, b);
		REQUIRE( expected_overflowed == bitop_overflowed );
	}
}

#endif
