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
	constexpr bool add(T a, T b, T* result);

	template<typename T>
	constexpr bool sub(T a, T b, T* result);

	template<typename T>
	constexpr bool mul(T a, T b, T* result);

	// Potentially clearer, std::optional interface
	template<typename T, bool op(T, T, T* result)>
	static std::optional<T> wrap_in_optional(T a, T b) {
		T out {};
		if (op(a, b, &out)) {
			return std::nullopt;
		} else {
			return out;
		}
	}

	template<typename T>
	struct RemoveOneOptional_ {
		using Type = T;
	};
	template<typename T>
	struct RemoveOneOptional_<std::optional<T>> {
		using Type = T;
	};
	template<typename T>
	using RemoveOneOptional = typename RemoveOneOptional_<T>::Type;

	template<typename T>
	struct RemoveAllOptionals_ {
		using Type = T;
	};
	template<typename T>
	struct RemoveAllOptionals_<std::optional<T>> {
		using Type = typename RemoveAllOptionals_<T>::Type;
	};
	template<typename T>
	using RemoveAllOptionals = typename RemoveAllOptionals_<T>::Type;

	template<typename T>
	static constexpr auto flatten_optionals(T t) {
		using ROT = RemoveOneOptional<T>;
		if constexpr (std::is_same_v<T, ROT>) {
			return std::optional<T>(t);
		} else if (!t.has_value()) {
			return std::optional<RemoveAllOptionals<T>>();
		} else {
			return flatten_optionals<ROT>(t.value());
		}
	}

	template<typename A, typename B, typename T, bool op(T, T, T* result)>
	static constexpr std::optional<T> do_optless_op(A a, B b) {
		using ROA = RemoveAllOptionals<A>;
		using ROB = RemoveAllOptionals<B>;

		if constexpr (std::is_same_v<ROA, A>) {
			if constexpr (std::is_same_v<ROB, B>) {
				static_assert(std::is_same_v<A, B>,
							"both arguments need to have the same underlying integer type");
				return wrap_in_optional<T, op>(a, b);
			} else {
				// B needs unwrapping
				auto b_flat = flatten_optionals(b);
				if (!b_flat.has_value()) {
					return std::nullopt;
				} else {
					return wrap_in_optional<T, op>(a, b_flat.value());
				}
			}
		} else {
			if constexpr (std::is_same_v<ROB, B>) {
				// A needs unwrapping
				auto a_flat = flatten_optionals(a);
				if (!a_flat.has_value()) {
					return std::nullopt;
				} else {
					return wrap_in_optional<T, op>(a_flat.value(), b);
				}
			} else {
				// both need unwrapping
				auto a_flat = flatten_optionals(a);
				auto b_flat = flatten_optionals(b);
				if (!a_flat.has_value() || !b_flat.has_value()) {
					return std::nullopt;
				} else {
					return wrap_in_optional<T, op>(a_flat.value(), b_flat.value());
				}
			}
		}
	}

	// optional interface (accepts std::optional<T> arguments too, for easy chaining)
	// Returns a std::optional() with a value when the operation does not overflow
	template<typename A, typename B>
	auto add(A a, B b) {
		using TA = RemoveAllOptionals<A>;
		std::optional<TA> result = do_optless_op<A, B, TA, add<TA>>(a, b);
		return result;
	}
	template<typename A, typename B>
	auto sub(A a, B b) {
		using TA = RemoveAllOptionals<A>;
		std::optional<TA> result = do_optless_op<A, B, TA, sub<TA>>(a, b);
		return result;
	}
	template<typename A, typename B>
	auto mul(A a, B b) {
		using TA = RemoveAllOptionals<A>;
		std::optional<TA> result = do_optless_op<A, B, TA, mul<TA>>(a, b);
		return result;
	}

	template<typename T>
	struct NextSize_;
	// C++ doesn't specify whether char is signed or unsigned
	//		This specialization might be unnecessary
	template<>
	struct NextSize_<char> {
		using Type = std::conditional_t<std::is_signed_v<char>, int_fast16_t, uint_fast16_t>;
	};
	template<>
	struct NextSize_<int8_t> {
		using Type = int_fast16_t;
	};
	template<>
	struct NextSize_<uint8_t> {
		using Type = uint_fast16_t;
	};
	template<>
	struct NextSize_<int16_t> {
		using Type = int_fast32_t;
	};
	template<>
	struct NextSize_<uint16_t> {
		using Type = uint_fast32_t;
	};
	template<>
	struct NextSize_<int32_t> {
		using Type = int_fast64_t;
	};
	template<>
	struct NextSize_<uint32_t> {
		using Type = uint_fast64_t;
	};
	template<typename T>
	using NextSize = typename NextSize_<T>::Type;

	// Operator is a templated callable struct
	// This function implements checked arithmetic using a wider type
	template<typename T, template<typename> typename Operator, typename Wider = NextSize<T>>
	constexpr bool check_widening(T a, T b, T* result) {
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
	static constexpr bool checked_op(T a, T b, T* result) {
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
	constexpr bool add(T a, T b, T* result) {
		// these two branches can work at compile time
		if constexpr (sizeof(T) < 4) {
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
	constexpr bool sub(T a, T b, T* result) {
		if constexpr (sizeof(T) < 4) {
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
	constexpr bool mul(T a, T b, T* result) {
		if constexpr (sizeof(T) < 4) {
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
	constexpr bool add(T a, T b, T* result) {
		if constexpr (sizeof(T) <= 4) {
			return check_widening<T, std::plus>(a, b, result);
		} else {
			// T is 64-bit, so use the complicated version because we don't have a 128-bit type
			return checked_op<T, add_overflows<T>,
			                         std::plus<T>>(a, b, result);
		}
	}

	template<typename T>
	constexpr bool sub(T a, T b, T* result) {
		if constexpr (sizeof(T) <= 4) {
			return check_widening<T, std::minus>(a, b, result);
		} else {
			// T is 64-bit, so use the complicated version because we don't have a 128-bit type
			return checked_op<T, sub_overflows<T>,
			                         std::minus<T>>(a, b, result);
		}
	}

	template<typename T>
	constexpr bool mul(T a, T b, T* result) {
		if constexpr (sizeof(T) <= 4) {
			return check_widening<T, std::multiplies>(a, b, result);
		} else {
			return checked_op<T, mul_overflows<T>, std::multiplies<T>>(
					a, b, result
				);
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

	template<typename T, bool do_add>
	static constexpr bool addsub_overflows(T x, T y) {
		// Formulas from Hacker's Delight Second Edition
		if constexpr (std::is_signed_v<T>) {
			// Page 30,
			// "as the formulas that do not even use the carry bit"
			using U = std::make_unsigned_t<T>;
			if constexpr (do_add) {
				return get_sign_bit(~(x ^ y) & (T(U(x) + U(y)) ^ x)) != 0;
			} else {
				return get_sign_bit((x ^ y) & (T(U(x) - U(y)) ^ x)) != 0;
			}
		} else {
			// page 31
			// "For unsigned addition, overflow (carry) occurs if
			//		the sum is less (by unsigned comparison)
			//		than either of the operands."
			if constexpr (do_add) {
				return T(~x) < y;
			} else {
				return x < y;
			}
		}
	}

	template<typename T>
	constexpr bool add_overflows(T a, T b) {
		return addsub_overflows<T, true>(a, b);
	}
	template<typename T>
	constexpr bool sub_overflows(T a, T b) {
		return addsub_overflows<T, false>(a, b);
	}

	// because std::abs is not constexpr until C++23
	template<typename T>
	static constexpr T abs(T t) {
		if constexpr (std::is_unsigned_v<T>) {
			return t;
		} else {
			return t >= 0? t: -t;
		}
	}
	template<typename T>
	constexpr bool mul_overflows(T x, T y) {
		// Formulas from Hacker's Delight Second Edition
		if constexpr (!std::is_signed_v<T>) {
			// unsigned
			// Page 32, center
			return y != 0 && (x > full_mask<T>() / y);
		} else {
			// signed
			// page 33, top
			// unsigned type version of T,
			//		to ensure some unsigned operations
			using U = std::make_unsigned_t<T>;

			U c = U(~(x ^ y) >> (sizeof(T) * 8 - 1)) + sign_bit<U>();
			x = abs(x);
			y = abs(y);
			return y != 0 && (U(x) > (c / U(y)));
		}
	}
} // namespace checked
