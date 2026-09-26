// File /Native/Engine/Math/MathConstant.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../Core/Environment.h"
#include "../Utils/Concept.hpp"

#include <concepts>
#include <numbers>

namespace PenEngine
{
	/// @brief 数学库的浮点标量类型
	///
	/// I1. 只放行 float/double：位级工具（MathFunction.hpp 的 Internal::FloatTraits）
	///     依赖 IEEE-754 的二进制布局按 4/8 字节分派，`long double` 在部分平台上是 16 字节，
	///     故此处直接排除，而不是等到 static_assert 才报错
	template <typename T>
	concept MathFloatingPoint = IsOneOf<T, float, double>;

	/// @brief 数学库的整型标量类型
	///
	/// I1. 排除 bool 与全部字符类型：它们满足 std::integral，但参与算术没有意义
	template <typename T>
	concept MathIntegral = std::integral<T>
		&& !IsOneOf<T, bool, char, wchar_t, char8_t, char16_t, char32_t>;

	/// @brief 数学库的无符号整型标量类型（对齐/取整运算只对无符号成立）
	template <typename T>
	concept MathUnsignedIntegral = MathIntegral<T> && !std::signed_integral<T>;

	/// @brief 数学库的算术标量类型（整型或浮点）
	/// @note 标准库提供的是 std::is_arithmetic 类型特征（`std::is_arithmetic_v<T>`
	///       == `is_integral_v<T> || is_floating_point_v<T>`），并没有名为 `std::arithmetic`
	///       的概念，故这里把该特征包成本模块的概念；语义与特征完全一致（含 bool）
	template <typename T>
	concept MathArithmetic = std::is_arithmetic_v<T>;

	/// @brief 数学库默认容差（单精度基准值）
	/// @note NearAbs/NearRel 的 epsilon 形参类型是 T，双精度调用点由该值转换而来；
	///       需要双精度专用容差时显式传入
	inline constexpr float MathEpsilon = 1e-5f;

	/// @brief 圆周率 π
	template <MathFloatingPoint T> inline constexpr T Pi = std::numbers::pi_v<T>;
	/// @brief 自然常数 e
	template <MathFloatingPoint T> inline constexpr T E = std::numbers::e_v<T>;
	/// @brief 黄金比 φ
	template <MathFloatingPoint T> inline constexpr T Phi = std::numbers::phi_v<T>;
	/// @brief 2 的自然对数
	template <MathFloatingPoint T> inline constexpr T Ln2 = std::numbers::ln2_v<T>;
	/// @brief 10 的自然对数
	template <MathFloatingPoint T> inline constexpr T Ln10 = std::numbers::ln10_v<T>;
	/// @brief √2
	template <MathFloatingPoint T> inline constexpr T Sqrt2 = std::numbers::sqrt2_v<T>;
	/// @brief 2π（整圆）
	template <MathFloatingPoint T> inline constexpr T Tau = static_cast<T>(2) * Pi<T>;
	/// @brief π/2
	template <MathFloatingPoint T> inline constexpr T HalfPi = Pi<T> / static_cast<T>(2);
}
