#include <type_traits>
#include <cstdint>
#include <limits>
#include <functional>
#include <optional>

// declarations
namespace checked {

// These functions return true on overflow, and false otherwise
// It is unspecifified whether *result is modified on overflow

template<typename T>
constexpr bool add(T a, T b, T *result);

template<typename T>
constexpr bool sub(T a, T b, T *result);

template<typename T>
constexpr bool mul(T a, T b, T *result);

// Potentially clearer, std::optional interface
template<typename T, bool op(T, T, T *result)>
static std::optional<T> wrap_in_optional(T a, T b) {
	T out {};
	if (op(a, b, &out)) {
		return std::nullopt;
	} else {
		return out;
	}
}

// optional interface (accepts std::optional<T> arguments too, for easy chaining)
// Returns a std::optional() with a value when the operation does not overflow
template<typename T>
std::optional<T> add(T a, T b) {
	return wrap_in_optional<T, add<T>>(a, b);
}
template<typename T>
std::optional<T> sub(T a, T b) {
	return wrap_in_optional<T, sub<T>>(a, b);
}
template<typename T>
std::optional<T> mul(T a, T b) {
	return wrap_in_optional<T, mul<T>>(a, b);
}


template<typename T>
struct NextSize_;
// C++ doesn't specify whether char is signed or unsigned
//		This specialization might be unnecessary
template<>
struct NextSize_<char> {
	using Type = std::conditional_t<std::is_signed_v<char>, int_fast16_t, uint_fast16_t>;
};
// Make the macro normal instead of having `Larger` be __VA_ARGS__ to incorporate the above,
//		so it's more understandable
#define MakeNextSize(TypeName, Larger) \
	template<> \
	struct NextSize_<TypeName> { \
		using Type = Larger; \
	};
MakeNextSize(int8_t, int_fast16_t);
MakeNextSize(uint8_t, uint_fast16_t);
MakeNextSize(int16_t, int_fast32_t);
MakeNextSize(uint16_t, uint_fast32_t);
MakeNextSize(int32_t, int_fast64_t);
MakeNextSize(uint32_t, uint_fast64_t);
template<typename T>
using NextSize = typename NextSize_<T>::Type;
#undef MakeNextSize

// Operator is a templated callable struct
// This function implements checked arithmetic using a wider type
template<typename T, template<typename> typename Operator, typename Wider = NextSize<T>>
constexpr bool check_widening(T a, T b, T *result) {
	Wider w_result = Operator<Wider>()(Wider(a), Wider(b));
	if (w_result == Wider(T(w_result))) {
		*result = T(w_result);
		return false; // didn't overflow
	}
	return true; // did overflow
}

// Check whether the operation would overflow,
//		using bit-operations
template<typename T>
constexpr bool add_overflows(T a, T b);
template<typename T>
constexpr bool sub_overflows(T a, T b);
template<typename T>
constexpr bool mul_overflows(T a, T b);

template<typename T, bool op_overflows(T, T), typename Operator>
static constexpr bool checked_op(T a, T b, T *result) {
	if (op_overflows(a, b)) {
		return true; // overflowed
	}
	// didn't overflow!
	*result = Operator()(a, b);
	return false;
}
template<typename T, bool do_add>
static constexpr bool addsub_overflows(T x, T y);

#if defined(__GNUC__) || defined(__clang__)

// We have some builtins!
template<typename T>
constexpr bool add(T a, T b, T *result) {
	// These two branches allow checked add to be constant-evaluated
	//		(run at compile time)
	// The `__builtin_is_constant_evaluated` branch
	//		can't be `if constexpr` because then the condition would
	//		always be evaluated at compile time (returning true),
	//		instead of detecting whether it's this call is at runtime
	//		and using builtins if so.
	if constexpr (sizeof(T) < 4) {
		// Builtins only go down to int-size,
		//	so for anything smaller we need to either
		//	do a widened operation and check if the result fits,
		//	or shift both operands to fit in the high end of an int/uint
		//
		//	I picked the former because it works at compile time
		return check_widening<T, std::plus>(a, b, result);
	} else if (__builtin_is_constant_evaluated()) {
		return checked_op<T, add_overflows<T>, std::plus<T>>(a, b, result);
	}
	if constexpr (std::is_signed_v<T>) {
		if constexpr (std::is_same_v<T, int>) {
			return __builtin_sadd_overflow(a, b, result);
		} else if constexpr (std::is_same_v<T, long>) {
			return __builtin_saddl_overflow(a, b, result);
		} else if constexpr (std::is_same_v<T, long long>) {
			return __builtin_saddll_overflow(a, b, result);
		}
	} else {
		if constexpr (std::is_same_v<T, unsigned>) {
			return __builtin_uadd_overflow(a, b, result);
		} else if constexpr (std::is_same_v<T, unsigned long>) {
			return __builtin_uaddl_overflow(a, b, result);
		} else if constexpr (std::is_same_v<T, unsigned long long>) {
			return __builtin_uaddll_overflow(a, b, result);
		}
	}
}
template<typename T>
constexpr bool sub(T a, T b, T *result) {
	// These two branches allow constant-evaluation
	//		see add(T, T, T *) for details
	if constexpr (sizeof(T) < 4) {
		// This takes care of types smaller than an int,
		//		because that's the size of the smallest builtin
		// See add(T, T, T *) for rationale
		return check_widening<T, std::minus>(a, b, result);
	} else if (__builtin_is_constant_evaluated()) {
		return checked_op<T, sub_overflows<T>, std::minus<T>>(a, b, result);
	}
	if constexpr (std::is_signed_v<T>) {
		if constexpr (std::is_same_v<T, int>) {
			return __builtin_ssub_overflow(a, b, result);
		} else if constexpr (std::is_same_v<T, long>) {
			return __builtin_ssubl_overflow(a, b, result);
		} else if constexpr (std::is_same_v<T, long long>) {
			return __builtin_ssubll_overflow(a, b, result);
		}
	} else {
		if constexpr (std::is_same_v<T, unsigned>) {
			return __builtin_usub_overflow(a, b, result);
		} else if constexpr (std::is_same_v<T, unsigned long>) {
			return __builtin_usubl_overflow(a, b, result);
		} else if constexpr (std::is_same_v<T, unsigned long long>) {
			return __builtin_usubll_overflow(a, b, result);
		}
	}
}

template<typename T>
constexpr bool mul(T a, T b, T *result) {
	// These two branches allow constant-evaluation
	//		see add(T, T, T *) for details
	if constexpr (sizeof(T) < 4) {
		// This takes care of types smaller than an int,
		//		because that's the size of the smallest builtin
		// See add(T, T, T *) for rationale
		return check_widening<T, std::multiplies>(a, b, result);
	} else if (__builtin_is_constant_evaluated()) {
		return checked_op<T, mul_overflows<T>, std::multiplies<T>>(a, b, result);
	}
	if constexpr (std::is_signed_v<T>) {
		if constexpr (std::is_same_v<T, int>) {
			return __builtin_smul_overflow(a, b, result);
		} else if constexpr (std::is_same_v<T, long>) {
			return __builtin_smull_overflow(a, b, result);
		} else if constexpr (std::is_same_v<T, long long>) {
			return __builtin_smulll_overflow(a, b, result);
		}
	} else {
		if constexpr (std::is_same_v<T, unsigned>) {
			return __builtin_umul_overflow(a, b, result);
		} else if constexpr (std::is_same_v<T, unsigned long>) {
			return __builtin_umull_overflow(a, b, result);
		} else if constexpr (std::is_same_v<T, unsigned long long>) {
			return __builtin_umulll_overflow(a, b, result);
		}
	}
}

#else
// MSVC, or something else, so we don't have nice builtins
// We could theoretically use arch-specific intrinsics,
//		but that sounds more annoying

template<typename T>
constexpr bool add(T a, T b, T *result) {
	if constexpr (sizeof(T) <= 4) {
		return check_widening<T, std::plus>(a, b, result);
	} else {
		// T is 64-bit, so use the complicated version
		//		because we don't have a 128-bit type
		return checked_op<T, add_overflows<T>, std::plus<T>>(a, b, result);
	}
}

template<typename T>
constexpr bool sub(T a, T b, T *result) {
	if constexpr (sizeof(T) <= 4) {
		return check_widening<T, std::minus>(a, b, result);
	} else {
		// T is 64-bit, so use the complicated version
		//		because we don't have a 128-bit type
		return checked_op<T, sub_overflows<T>, std::minus<T>>(a, b, result);
	}
}

template<typename T>
constexpr bool mul(T a, T b, T *result) {
	if constexpr (sizeof(T) <= 4) {
		return check_widening<T, std::multiplies>(a, b, result);
	} else {
		// T is 64-bit, so use the complicated version
		//		because we don't have a 128-bit type
		return checked_op<T, mul_overflows<T>, std::multiplies<T>>(a, b, result);
	}
}
#endif

template<typename T>
static constexpr const T full_mask() {
	return ~T(0);
}

template<typename T>
static constexpr T sign_bit() {
	return T(1) << (sizeof(T) * 8 - 1);
}
template<typename T>
static constexpr T get_sign_bit(T t) {
	return t & sign_bit<T>();
}

template<typename T>
constexpr bool add_overflows(T x, T y) {
	// Formulas from Hacker's Delight Second Edition
	if constexpr (std::is_signed_v<T>) {
		// Page 30,
		// "as the formulas that do not even use the carry bit"
		using U = std::make_unsigned_t<T>;
		return get_sign_bit(~(x ^ y) & (T(U(x) + U(y)) ^ x)) != 0;
	} else {
		// page 31
		// ~x + x == max<T>, thus x + y <= max<T> iff y <= ~x
		// Therefor there's an overflow if y > ~x
		return y > T(~x);
	}
}

template<typename T>
constexpr bool sub_overflows(T x, T y) {
	// Formulas from Hacker's Delight Second Edition
	if constexpr (std::is_signed_v<T>) {
		// Page 30,
		// "as the formulas that do not even use the carry bit"
		using U = std::make_unsigned_t<T>;
		return get_sign_bit((x ^ y) & (T(U(x) - U(y)) ^ x)) != 0;
	} else {
		// page 31
		// very straightforward: x - y >= 0 iff x >= y,
		//		so it overflows if x < y
		return x < y;
	}
}

// because std::abs is not constexpr until C++23
template<typename T>
static constexpr std::make_unsigned_t<T> abs(T t) {
	using U = std::make_unsigned_t<T>;
	return t >= 0? U(t): U(-t);
}
template<typename T>
constexpr bool mul_overflows(T x, T y) {
	// Formulas from Hacker's Delight Second Edition
	if constexpr (!std::is_signed_v<T>) {
		// unsigned
		// Page 32, center
		// x * y <= max<T> => x <= max<T> / y && y != 0
		return y != 0 && (x > (full_mask<T>() / y));
	} else {
		// signed
		// page 33, top
		using U = std::make_unsigned_t<T>;

		// subtract 1 from the sign bit if the result would be positive
		U c = sign_bit<U>() - ((x > 0) == (y > 0));
		U ux = abs(x);
		U uy = abs(y);
		return uy != 0 && (ux > (c / uy));
	}
}

} // namespace checked
