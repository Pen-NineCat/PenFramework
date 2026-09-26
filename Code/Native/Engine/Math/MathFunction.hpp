// File /Native/Engine/Math/MathFunction.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// ============================================================================
// NOTICE: ScalarSinCos 的参数归约流程与多项式系数改编自 Microsoft DirectXMath。
// 原始版权：Copyright (c) Microsoft Corporation，MIT License。
// http://go.microsoft.com/fwlink/?LinkID=615560
// ============================================================================

#pragma once

#include "MathConstant.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <limits>
#include <type_traits>

namespace PenEngine
{
	namespace Internal
	{
		/// @brief 浮点标量的 IEEE-754 位级描述
		/// @tparam T 仅 float/double（MathFloatingPoint 已保证 4/8 字节布局）
		///
		/// I1. 判定/取整/符号全部走位运算，不调用 <cmath>：这样它们在 consteval 下
		///     一定可用，不依赖标准库是否把 isnan/floor/signbit 标成 constexpr
		/// I2. 掩码全部按 sizeof(T) 分派，不再对 `long double` 做特例（旧实现用
		///     `sizeof(long double) == sizeof(long)` 判定，在 MSVC 上恒为假）
		template <MathFloatingPoint T>
		struct FloatTraits
		{
			static constexpr int TotalBits = static_cast<int>(sizeof(T) * BitsPerBytes);
			static constexpr int ExponentBits = sizeof(T) == 4 ? 8 : 11;
			static constexpr int MantissaBits = sizeof(T) == 4 ? 23 : 52;
			static constexpr int ExponentBias = sizeof(T) == 4 ? 127 : 1023;
			static constexpr int SignShift = TotalBits - 1;

			using BitsType = std::conditional_t<sizeof(T) == 4, U32, U64>;

			static constexpr BitsType SignMask = static_cast<BitsType>(BitsType(1) << SignShift);
			static constexpr BitsType MantissaMask = static_cast<BitsType>((BitsType(1) << MantissaBits) - 1);
			static constexpr BitsType ExponentMask =
				static_cast<BitsType>(((BitsType(1) << ExponentBits) - 1) << MantissaBits);
		};

		/// @brief 取浮点值的位表示
		template <MathFloatingPoint T>
		[[nodiscard]] constexpr typename FloatTraits<T>::BitsType ToBits(T value) noexcept
		{
			return std::bit_cast<typename FloatTraits<T>::BitsType>(value);
		}
	}

	// ------------------------------------------------------------------------
	// 特殊值判定
	// ------------------------------------------------------------------------

	/// @brief 是否为无穷（+inf / −inf）
	template <MathFloatingPoint T>
	[[nodiscard]] constexpr bool IsInfinity(T value) noexcept
	{
		using Traits = Internal::FloatTraits<T>;
		const auto bits = Internal::ToBits(value);
		return (bits & Traits::ExponentMask) == Traits::ExponentMask && (bits & Traits::MantissaMask) == 0;
	}

	/// @brief 是否为 NaN（含 quiet/signaling，不区分符号）
	template <MathFloatingPoint T>
	[[nodiscard]] constexpr bool IsNaN(T value) noexcept
	{
		using Traits = Internal::FloatTraits<T>;
		const auto bits = Internal::ToBits(value);
		return (bits & Traits::ExponentMask) == Traits::ExponentMask && (bits & Traits::MantissaMask) != 0;
	}

	/// @brief 取符号位
	/// @retval true 符号位为 1（含 −0.0 与负数 NaN）
	/// @retval false 符号位为 0
	template <MathFloatingPoint T>
	[[nodiscard]] constexpr bool SignBit(T value) noexcept
	{
		return (Internal::ToBits(value) & Internal::FloatTraits<T>::SignMask) != 0;
	}

	/// @brief 取值的正负号
	/// @retval -1 负值
	/// @retval 0 零（含 ±0.0）
	/// @retval 1 正值（NaN 亦视为正）
	template <MathArithmetic T>
	[[nodiscard]] constexpr int Sign(T value) noexcept
	{
		if (value == T(0))
			return 0;

		return value < T(0) ? -1 : 1;
	}

	/// @brief 把 magnitude 的大小与 sign 的符号组合起来（constexpr copysign）
	/// @param magnitude 提供绝对值大小的值
	/// @param sign 提供符号的值
	/// @return 具有 sign 的符号、magnitude 的绝对值的值
	template <MathFloatingPoint T>
	[[nodiscard]] constexpr T CopySign(T magnitude, T sign) noexcept
	{
		using Traits = Internal::FloatTraits<T>;
		using BitsType = typename Traits::BitsType;

		const BitsType magnitudeBits = Internal::ToBits(magnitude);
		const BitsType signBits = Internal::ToBits(sign);

		return std::bit_cast<T>(static_cast<BitsType>((magnitudeBits & ~Traits::SignMask) | (signBits & Traits::SignMask)));
	}

	// ------------------------------------------------------------------------
	// 绝对值
	// ------------------------------------------------------------------------

	/// @brief 绝对值（有符号整数）
	/// @warning T 为最小值（如 INT32_MIN）时其绝对值不可表示，取负是 UB（与 std::abs 一致）
	template <MathIntegral T> requires std::signed_integral<T>
	[[nodiscard]] constexpr T Abs(T value) noexcept
	{
		return value < T(0) ? static_cast<T>(-value) : value;
	}

	/// @brief 绝对值（无符号整数，恒等）
	template <MathUnsignedIntegral T>
	[[nodiscard]] constexpr T Abs(T value) noexcept
	{
		return value;
	}

	/// @brief 绝对值（浮点，位掩码实现，−0.0 → +0.0，NaN 只清符号位）
	template <MathFloatingPoint T>
	[[nodiscard]] constexpr T Abs(T value) noexcept
	{
		using Traits = Internal::FloatTraits<T>;
		return std::bit_cast<T>(static_cast<typename Traits::BitsType>(Internal::ToBits(value) & ~Traits::SignMask));
	}

	// ------------------------------------------------------------------------
	// 取整（consteval 路径为 O(1) 位运算截断，不再有循环）
	// ------------------------------------------------------------------------

	/// @brief 向零取整
	/// @note NaN/±inf/±0.0 原样返回（保留符号与原位模式）
	template <MathFloatingPoint T>
	[[nodiscard]] constexpr T Trunc(T value) noexcept
	{
		if consteval
		{
			using Traits = Internal::FloatTraits<T>;
			using BitsType = typename Traits::BitsType;

			if (IsNaN(value) || IsInfinity(value) || Abs(value) == T(0))
				return value;

			const BitsType bits = Internal::ToBits(value);
			const int exponent =
				static_cast<int>((bits & Traits::ExponentMask) >> Traits::MantissaBits) - Traits::ExponentBias;

			if (exponent < 0)
				return CopySign(T(0), value); // |value| < 1：向零取整为带符号的 0

			if (exponent >= Traits::MantissaBits)
				return value; // 尾数装不下小数位：本身就是整数

			const BitsType fractionMask =
				static_cast<BitsType>((BitsType(1) << (Traits::MantissaBits - exponent)) - 1);

			return std::bit_cast<T>(static_cast<BitsType>(bits & ~fractionMask));
		}

		return std::trunc(value);
	}

	/// @brief 向下取整（floor）
	/// @note NaN/±inf/±0.0 原样返回
	template <MathFloatingPoint T>
	[[nodiscard]] constexpr T Floor(T value) noexcept
	{
		if consteval
		{
			if (IsNaN(value) || IsInfinity(value))
				return value;

			const T truncated = Trunc(value);

			if (truncated == value)
				return truncated;

			return value < T(0) ? static_cast<T>(truncated - T(1)) : truncated;
		}

		return std::floor(value);
	}

	/// @brief 向上取整（ceil）
	/// @note NaN/±inf/±0.0 原样返回
	template <MathFloatingPoint T>
	[[nodiscard]] constexpr T Ceil(T value) noexcept
	{
		if consteval
		{
			if (IsNaN(value) || IsInfinity(value))
				return value;

			const T truncated = Trunc(value);

			if (truncated == value)
				return truncated;

			return value > T(0) ? static_cast<T>(truncated + T(1)) : truncated;
		}

		return std::ceil(value);
	}

	// ------------------------------------------------------------------------
	// 幂与根
	// ------------------------------------------------------------------------

	/// @brief 平方根
	/// @param value 被开方数
	/// @return 平方根；负输入（含 −inf）返回 NaN，−0.0 → −0.0，+inf → +inf（与 IEEE 一致）
	///
	/// @note consteval 路径为「指数折半作初值 + 固定次数 Newton 迭代」，结果精度约 1 ULP，
	///       不做最后一位正确舍入；运行期直接用 std::sqrt（正确舍入）
	template <MathFloatingPoint T>
	[[nodiscard]] constexpr T Sqrt(T value) noexcept
	{
		if consteval
		{
			using Traits = Internal::FloatTraits<T>;

			if (IsNaN(value))
				return value;

			if (Abs(value) == T(0))
				return value; // ±0.0：保持符号

			if (SignBit(value))
				return std::numeric_limits<T>::quiet_NaN(); // 负值（含 −inf，与 std::sqrt 一致）

			if (IsInfinity(value))
				return value; // +inf

			const int exponent =
				static_cast<int>((Internal::ToBits(value) & Traits::ExponentMask) >> Traits::MantissaBits) - Traits::ExponentBias;
			const int halfExponent = exponent / 2; // 负指数向零取整，此处只作初值，不影响收敛

			T guess = std::bit_cast<T>(static_cast<typename Traits::BitsType>(
				static_cast<typename Traits::BitsType>(halfExponent + Traits::ExponentBias) << Traits::MantissaBits));

			constexpr int iterationCount = sizeof(T) == 4 ? 5 : 8;
			for (int iteration = 0; iteration < iterationCount; ++iteration)
				guess = static_cast<T>((guess + value / guess) * T(0.5));

			return guess;
		}

		return std::sqrt(value);
	}

	/// @brief 幂（平方-乘，指数为非负整数）
	/// @param base 底数
	/// @param exponent 指数
	/// @return base^exponent；0^0 == 1（与 std::pow 一致）
	///
	/// @note 指数限定为非负整数才可全程 constexpr：旧实现允许浮点指数，
	///       但 consteval 分支对 0.5 这类指数会直接算错（返回 1），故此处收窄签名
	/// @note 需要负指数请写 `T(1) / Pow(base, n)`
	template <MathFloatingPoint T, MathUnsignedIntegral E>
	[[nodiscard]] constexpr T Pow(T base, E exponent) noexcept
	{
		T result = T(1);
		T factor = base;
		E remaining = exponent;

		while (remaining != E(0))
		{
			if ((remaining & E(1)) != E(0))
				result = static_cast<T>(result * factor);

			factor = static_cast<T>(factor * factor);
			remaining = static_cast<E>(remaining >> 1);
		}

		return result;
	}

	// ------------------------------------------------------------------------
	// 区间、插值、近似比较
	// ------------------------------------------------------------------------

	/// @brief 把 value 夹到 [low, high]
	/// @pre low <= high
	/// @note NaN 输入返回 NaN（用比较而非 std::clamp，避免违反前置条件时的 UB）
	template <MathArithmetic T>
	[[nodiscard]] constexpr T Clamp(T value, T low, T high) noexcept
	{
		if (value < low)
			return low;

		return high < value ? high : value;
	}

	/// @brief 把 value 夹到 [0, 1]
	template <MathArithmetic T>
	[[nodiscard]] constexpr T Clamp01(T value) noexcept
	{
		return Clamp(value, T(0), T(1));
	}

	/// @brief 线性插值（t 被夹到 [0, 1]，不外插）
	/// @param from 起点
	/// @param to 终点
	/// @param t 插值参数
	/// @return from + (to − from) * clamp01(t)，结果按 T 返回（整数 T 会截断）
	template <MathArithmetic T, MathFloatingPoint F>
	[[nodiscard]] constexpr T Lerp(T from, T to, F t) noexcept
	{
		const F clamped = Clamp01(t);
		return static_cast<T>(from + (to - from) * clamped);
	}

	/// @brief 绝对误差近似相等
	/// @param a 待比较值
	/// @param b 待比较值
	/// @param epsilon 容差
	/// @retval true |a − b| <= epsilon，或 a == b（含同号无穷、±0.0）
	/// @retval false 任一侧为 NaN，或异号无穷，或超出容差
	template <MathFloatingPoint T>
	[[nodiscard]] constexpr bool NearAbs(T a, T b, T epsilon = static_cast<T>(MathEpsilon)) noexcept
	{
		if (a == b)
			return true;

		if (IsNaN(a) || IsNaN(b) || IsInfinity(a) || IsInfinity(b))
			return false;

		return Abs(a - b) <= epsilon;
	}

	/// @brief 相对误差近似相等
	/// @param a 待比较值
	/// @param b 待比较值
	/// @param epsilon 相对容差
	/// @retval true |a − b| <= epsilon * max(|a|, |b|)，或 a == b
	/// @retval false 任一侧为 NaN，或异号无穷，或超出容差
	/// @note 两侧都为 0 时由 a == b 命中；一侧为 0 时相对误差无意义，按不相等处理
	template <MathFloatingPoint T>
	[[nodiscard]] constexpr bool NearRel(T a, T b, T epsilon = static_cast<T>(MathEpsilon)) noexcept
	{
		if (a == b)
			return true;

		if (IsNaN(a) || IsNaN(b) || IsInfinity(a) || IsInfinity(b))
			return false;

		const T absoluteA = Abs(a);
		const T absoluteB = Abs(b);

		return Abs(a - b) <= epsilon * (absoluteA > absoluteB ? absoluteA : absoluteB);
	}

	// ------------------------------------------------------------------------
	// 整数对齐
	// ------------------------------------------------------------------------

	/// @brief 是否为 2 的幂（0 不是）
	template <MathUnsignedIntegral T>
	[[nodiscard]] constexpr bool IsPow2(T value) noexcept
	{
		return value != T(0) && (value & static_cast<T>(value - T(1))) == T(0);
	}

	/// @brief 向上对齐到 alignment 的倍数
	/// @param value 待对齐值
	/// @param alignment 对齐粒度，必须是 2 的幂（见 IsPow2）
	/// @return 最小的、>= value 的 alignment 倍数
	/// @warning value 接近 T 的最大值时 `value + alignment − 1` 会回绕
	template <MathUnsignedIntegral T, MathUnsignedIntegral A>
	[[nodiscard]] constexpr T AlignUp(T value, A alignment) noexcept
	{
		const T mask = static_cast<T>(alignment) - T(1);
		return static_cast<T>((value + mask) & static_cast<T>(~mask));
	}

	/// @brief 向下对齐到 alignment 的倍数
	/// @param value 待对齐值
	/// @param alignment 对齐粒度，必须是 2 的幂（见 IsPow2）
	/// @return 最大的、<= value 的 alignment 倍数
	template <MathUnsignedIntegral T, MathUnsignedIntegral A>
	[[nodiscard]] constexpr T AlignDown(T value, A alignment) noexcept
	{
		const T mask = static_cast<T>(alignment) - T(1);
		return static_cast<T>(value & static_cast<T>(~mask));
	}

	/// @brief 向上取整的整数除法：value 需要多少个 divisor 才能装下
	/// @param value 总量
	/// @param divisor 单块容量
	/// @return ceil(value / divisor)；value == 0 时为 0
	/// @pre divisor != 0
	/// @note 旧框架的 CeilPow2 / Ceil4..Ceil64 就是本语义（返回的是块数，不是对齐后的值），
	///       旧名字与注释（"向上取整到 N 的倍数"）不符，故在此正名
	/// @note 实现不做 `value + divisor − 1`，避免接近最大值时回绕
	template <MathUnsignedIntegral T, MathUnsignedIntegral D>
	[[nodiscard]] constexpr T DivRoundUp(T value, D divisor) noexcept
	{
		const T quotient = static_cast<T>(value / divisor);
		const T remainder = static_cast<T>(value % divisor);
		return remainder != T(0) ? static_cast<T>(quotient + T(1)) : quotient;
	}

	// ------------------------------------------------------------------------
	// 三角函数
	// ------------------------------------------------------------------------

	/// @brief 同时计算正弦与余弦（float 专用多项式逼近）
	/// @param outSin 输出 sin(value)
	/// @param outCos 输出 cos(value)
	/// @param value 弧度
	///
	/// @note 参数归约流程与多项式系数改编自 Microsoft DirectXMath（MIT License）
	/// @pre |value| 不宜超过约 1e6 弧度：内部用 int 归约商，更大时会整型溢出，
	///      且 float 在该量级已无有效位，调用前请自行归约到 [−π, π] 附近
	constexpr void ScalarSinCos(float& outSin, float& outCos, float value) noexcept
	{
		constexpr float TwoPi = static_cast<float>(2) * Pi<float>;

		// q = round(value / 2π)
		float quotient = value / TwoPi;

		if (value >= 0.0f)
			quotient = static_cast<float>(static_cast<int>(quotient + 0.5f));
		else
			quotient = static_cast<float>(static_cast<int>(quotient - 0.5f));

		// 归约到 [−π, π]
		float y = value - TwoPi * quotient;

		// 再用 sin(π − y) = sin(y)、cos(π − y) = −cos(y) 归约到 [−π/2, π/2]
		float sign;
		if (y > HalfPi<float>)
		{
			y = Pi<float> - y;
			sign = -1.0f;
		}
		else if (y < -HalfPi<float>)
		{
			y = -Pi<float> - y;
			sign = -1.0f;
		}
		else
		{
			sign = 1.0f;
		}

		const float y2 = y * y;

		// sin(y) ≈ y * P(y²)，11 次泰勒多项式
		outSin = (((((-2.3889859e-08f * y2 + 2.7525562e-06f) * y2 - 0.00019840874f) * y2 + 0.0083333310f) * y2
			- 0.16666667f) * y2 + 1.0f) * y;

		// cos(y) ≈ P(y²) * sign
		const float cosPolynomial =
			((((-2.6051615e-07f * y2 + 2.4760495e-05f) * y2 - 0.0013888378f) * y2 + 0.041666638f) * y2 - 0.5f) * y2
			+ 1.0f;

		outCos = sign * cosPolynomial;
	}

	/// @brief 正弦（运行期薄封装，统一命名与调用点；constexpr 场景请用 ScalarSinCos）
	template <MathFloatingPoint T>
	[[nodiscard]] T Sin(T radians) noexcept
	{
		return std::sin(radians);
	}

	/// @brief 余弦（运行期薄封装）
	template <MathFloatingPoint T>
	[[nodiscard]] T Cos(T radians) noexcept
	{
		return std::cos(radians);
	}

	/// @brief 正切（运行期薄封装）
	template <MathFloatingPoint T>
	[[nodiscard]] T Tan(T radians) noexcept
	{
		return std::tan(radians);
	}

	/// @brief 反正弦（运行期薄封装），返回弧度
	template <MathFloatingPoint T>
	[[nodiscard]] T Asin(T value) noexcept
	{
		return std::asin(value);
	}

	/// @brief 反余弦（运行期薄封装），返回弧度
	template <MathFloatingPoint T>
	[[nodiscard]] T Acos(T value) noexcept
	{
		return std::acos(value);
	}

	/// @brief 反正切（运行期薄封装），返回弧度
	template <MathFloatingPoint T>
	[[nodiscard]] T Atan(T value) noexcept
	{
		return std::atan(value);
	}

	/// @brief 双参数反正切（运行期薄封装），返回弧度
	template <MathFloatingPoint T>
	[[nodiscard]] T Atan2(T y, T x) noexcept
	{
		return std::atan2(y, x);
	}

	// ------------------------------------------------------------------------
	// 方程求根
	// ------------------------------------------------------------------------

	/// @brief 解一元二次方程 a·x² + b·x + c = 0（实数域）
	/// @param out 根的缓冲区；成功写入时满足 out[0] >= out[1]（重根时两者相等）
	/// @param a 二次项系数
	/// @param b 一次项系数
	/// @param c 常数项
	/// @return 实根个数（0 / 1 / 2）
	///
	/// @note 使用数值稳定的求根式：q = −(b + copysign(√Δ, b)) / 2，避免 b 与 √Δ 相消；
	///       旧实现直接代入 (−b ± √Δ) / 2a，当 b² ≫ 4ac 时会丢失精度
	/// @note a == 0 时退化为一次方程；a、b 同时为 0（c 亦为 0）时返回 0 ——
	///       该情形为无穷多解，需调用方在解方程前自行判定
	template <MathFloatingPoint T>
	[[nodiscard]] constexpr Usize SolveQuadratic(std::array<T, 2>& out, T a, T b, T c) noexcept
	{
		if (a == T(0))
		{
			if (b == T(0))
				return 0;

			out[0] = static_cast<T>(-c / b);
			out[1] = out[0];
			return 1;
		}

		const T discriminant = static_cast<T>(b * b - static_cast<T>(4) * a * c);

		if (discriminant < T(0))
			return 0;

		if (discriminant == T(0))
		{
			out[0] = static_cast<T>(-b / (static_cast<T>(2) * a));
			out[1] = out[0];
			return 1;
		}

		const T root = Sqrt(discriminant);
		const T q = static_cast<T>(-(b + CopySign(root, b)) / static_cast<T>(2));

		if (q == T(0))
		{
			// 只在 c == 0 且 b < 0 时出现：两根为 0 与 −b/a
			out[0] = static_cast<T>(-b / a);
			out[1] = T(0);
			return 2;
		}

		T first = static_cast<T>(q / a);
		T second = static_cast<T>(c / q);

		if (second > first)
		{
			const T swap = first;
			first = second;
			second = swap;
		}

		out[0] = first;
		out[1] = second;
		return 2;
	}
}
