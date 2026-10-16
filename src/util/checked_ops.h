#include <type_traits>
#include <cstdint>
#include <limits>
#include <functional>

// declarations
namespace checked {
	// These functions return true on overflow, and false otherwise
	// It is unspecifified whether *result is modified on overflow

	template<typename T>
	bool add(T a, T b, T* result);

	template<typename T>
	bool sub(T a, T b, T* result);

	template<typename T>
	bool mul(T a, T b, T* result);


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
	template<typename T, template<typename> typename Operator, typename Wider = NextSize<T>>
	bool check_widening(T a, T b, T* result) {
		Wider w_result = Operator<Wider>()(Wider(a), Wider(b));
		if (w_result == Wider(T(w_result))) {
			*result = T(w_result);
			return false; // didn't overflow
		}
		return true; // did overflow
	}

#ifdef __GNUC__

// We have some builtins!
	template<typename T>
	bool add(T a, T b, T* result) {
		if constexpr (std::is_signed_v<T>) {
			if constexpr (std::is_same_v<T, int>) {
				return __builtin_sadd_overflow(a, b, result);
			} else if constexpr (std::is_same_v<T, long>) {
				return __builtin_saddl_overflow(a, b, result);
			} else if constexpr (std::is_same_v<T, long long>) {
				return __builtin_saddll_overflow(a, b, result);
			}
			// fallthrough
		} else {
			if constexpr (std::is_same_v<T, unsigned>) {
				return __builtin_uadd_overflow(a, b, result);
			} else if constexpr (std::is_same_v<T, unsigned long>) {
				return __builtin_uaddl_overflow(a, b, result);
			} else if constexpr (std::is_same_v<T, unsigned long long>) {
				return __builtin_uaddll_overflow(a, b, result);
			}
			// fallthrough
		}
		// otherwise
		if constexpr (sizeof(T) < 4) {
			return check_widening<T, std::plus>(a, b, result);
		}
	}
	template<typename T>
	bool sub(T a, T b, T* result) {
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
		if constexpr (sizeof(T) < 4) {
			return check_widening<T, std::minus>(a, b, result);
		}
	}

	template<typename T>
	bool mul(T a, T b, T* result) {
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
		if constexpr (sizeof(T) < 4) {
			return check_widening<T, std::multiplies>(a, b, result);
		}
	}

#else
// MSVC, or something else, so we don't have builtins (it seems?)
// We could theoretically use arch-specific intrinsics,
//		but that sounds more annoying

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
			if constexpr (do_add) {
				return get_sign_bit(~(x ^ y) & ((x + y) ^ x)) != 0;
			} else {
				return get_sign_bit((x ^ y) & ((x - y) ^ x)) != 0;
			}
		} else {
			// page 31
			// "For unsigned addition, overflow (carry) occurs if
			//		the sum is less (by unsigned comparison)
			//		than either of the operands."
			if constexpr (do_add) {
				return (~x) < y;
			} else {
				return x < y;
			}
		}
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
	static constexpr bool mul_overflows(T x, T y) {
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
	template<typename T, bool op_overflows(T, T), typename Operator>
	static constexpr bool checked_op(T a, T b, T* result) {
		if (op_overflows(a, b)) {
			return true; // overflowed
		}
		// didn't overflow!
		*result = Operator()(a, b);
		return false;
	}

	template<typename T>
	bool add(T a, T b, T* result) {
		if constexpr (sizeof(T) <= 4) {
			return check_widening<T, std::plus>(a, b, result);
		} else {
			// T is 64-bit, so use the complicated version because we don't have a 128-bit type (afaik)
			return checked_op<T, addsub_overflows<T, true>,
			                         std::plus<T>>(a, b, result);
		}
	}

	template<typename T>
	bool sub(T a, T b, T* result) {
		if constexpr (sizeof(T) <= 4) {
			return check_widening<T, std::minus>(a, b, result);
		} else {
			// T is 64-bit, so use the complicated version because we don't have a 128-bit type (afaik)
			return checked_op<T, addsub_overflows<T, false>,
			                         std::minus<T>>(a, b, result);
		}
	}

	template<typename T>
	bool mul(T a, T b, T* result) {
		if constexpr (sizeof(T) <= 4) {
			return check_widening<T, std::multiplies>(a, b, result);
		} else {
			return checked_op<T, mul_overflows<T>, std::multiplies<T>>(
					a, b, result
				);
		}
	}
#endif
} // namespace checked
