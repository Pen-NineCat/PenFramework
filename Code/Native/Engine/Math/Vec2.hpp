// File /Native/Engine/Math/Vec2.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "MathConstant.hpp"

#include <cassert>
#include <type_traits>

namespace PenEngine
{
	/// @brief 2 分量浮点向量
	/// @tparam T 仅 float/double（见 MathFloatingPoint）
	///
	/// I1. 布局：标准布局、平凡可复制、无额外对齐，sizeof == 2 * sizeof(T)；
	///     因此可以紧凑地装进顶点缓冲或数组，不会被 SIMD 对齐撑开
	/// I2. 分量与下标一一对应：operator[](0) → X，operator[](1) → Y；越界是调用方错误
	/// I3. 本阶段只提供 +、−（含一元 −）、标量 * /（含标量在左的 *）、复合赋值与 operator[]；
	///     点积 / 长度 / 归一化等高层运算留待后续
	template <MathFloatingPoint T>
	struct FloatVec2
	{
		/// @brief 分量个数
		static constexpr Usize Dimension = 2;

		T X{};
		T Y{};

		constexpr FloatVec2() noexcept = default;
		constexpr FloatVec2(T x, T y) noexcept : X(x), Y(y) {}

		/// @brief 分量访问（可写）
		/// @param index 分量下标，必须 < Dimension
		/// @note 常量求值不允许跨成员的指针算术（(&X)[index] 不是常量表达式），
		///       因此这里用条件链而非数组下标；越界时 Debug 断言失败，Release 下返回最后一个分量
		[[nodiscard]] constexpr T& operator[](Usize index) noexcept
		{
			assert(index < Dimension);

			return index == 0 ? X : Y;
		}

		[[nodiscard]] constexpr const T& operator[](Usize index) const noexcept
		{
			assert(index < Dimension);

			return index == 0 ? X : Y;
		}

		[[nodiscard]] constexpr FloatVec2 operator+(const FloatVec2& rhs) const noexcept { return { X + rhs.X, Y + rhs.Y }; }
		[[nodiscard]] constexpr FloatVec2 operator-(const FloatVec2& rhs) const noexcept { return { X - rhs.X, Y - rhs.Y }; }
		[[nodiscard]] constexpr FloatVec2 operator-() const noexcept { return { static_cast<T>(-X), static_cast<T>(-Y) }; }
		[[nodiscard]] constexpr FloatVec2 operator*(T scalar) const noexcept { return { X * scalar, Y * scalar }; }
		[[nodiscard]] constexpr FloatVec2 operator/(T scalar) const noexcept { return { X / scalar, Y / scalar }; }

		constexpr FloatVec2& operator+=(const FloatVec2& rhs) noexcept
		{
			X += rhs.X;
			Y += rhs.Y;
			return *this;
		}

		constexpr FloatVec2& operator-=(const FloatVec2& rhs) noexcept
		{
			X -= rhs.X;
			Y -= rhs.Y;
			return *this;
		}

		constexpr FloatVec2& operator*=(T scalar) noexcept
		{
			X *= scalar;
			Y *= scalar;
			return *this;
		}

		constexpr FloatVec2& operator/=(T scalar) noexcept
		{
			X /= scalar;
			Y /= scalar;
			return *this;
		}

		/// @brief 标量在左的乘法（v * s 由成员提供，s * v 由此友元提供）
		[[nodiscard]] friend constexpr FloatVec2 operator*(T scalar, const FloatVec2& value) noexcept
		{
			return value * scalar;
		}

		[[nodiscard]] constexpr bool operator==(const FloatVec2&) const noexcept = default;
	};

	/// @brief 2 分量整型向量
	/// @tparam T 仅整型且非 bool/字符（见 MathIntegral）
	///
	/// I1. 布局同 FloatVec2 的 I1：sizeof == 2 * sizeof(T)，无额外对齐
	/// I2. 除法是整数除法（向零截断）；一元 − 对无符号类型是模运算，均为定义行为
	template <MathIntegral T>
	struct IntegralVec2
	{
		/// @brief 分量个数
		static constexpr Usize Dimension = 2;

		T X{};
		T Y{};

		constexpr IntegralVec2() noexcept = default;
		constexpr IntegralVec2(T x, T y) noexcept : X(x), Y(y) {}

		/// @brief 分量访问（可写）
		/// @param index 分量下标，必须 < Dimension
		/// @note 常量求值不允许跨成员的指针算术（(&X)[index] 不是常量表达式），
		///       因此这里用条件链而非数组下标；越界时 Debug 断言失败，Release 下返回最后一个分量
		[[nodiscard]] constexpr T& operator[](Usize index) noexcept
		{
			assert(index < Dimension);

			return index == 0 ? X : Y;
		}

		[[nodiscard]] constexpr const T& operator[](Usize index) const noexcept
		{
			assert(index < Dimension);

			return index == 0 ? X : Y;
		}

		// 说明：整型算术一律提升到 int，花括号初始化必须显式收回 T，否则是收缩转换（C2398）
		[[nodiscard]] constexpr IntegralVec2 operator+(const IntegralVec2& rhs) const noexcept
		{
			return { static_cast<T>(X + rhs.X), static_cast<T>(Y + rhs.Y) };
		}

		[[nodiscard]] constexpr IntegralVec2 operator-(const IntegralVec2& rhs) const noexcept
		{
			return { static_cast<T>(X - rhs.X), static_cast<T>(Y - rhs.Y) };
		}

		[[nodiscard]] constexpr IntegralVec2 operator-() const noexcept
		{
			return { static_cast<T>(-X), static_cast<T>(-Y) };
		}

		[[nodiscard]] constexpr IntegralVec2 operator*(T scalar) const noexcept
		{
			return { static_cast<T>(X * scalar), static_cast<T>(Y * scalar) };
		}

		[[nodiscard]] constexpr IntegralVec2 operator/(T scalar) const noexcept
		{
			return { static_cast<T>(X / scalar), static_cast<T>(Y / scalar) };
		}

		constexpr IntegralVec2& operator+=(const IntegralVec2& rhs) noexcept
		{
			X += rhs.X;
			Y += rhs.Y;
			return *this;
		}

		constexpr IntegralVec2& operator-=(const IntegralVec2& rhs) noexcept
		{
			X -= rhs.X;
			Y -= rhs.Y;
			return *this;
		}

		constexpr IntegralVec2& operator*=(T scalar) noexcept
		{
			X *= scalar;
			Y *= scalar;
			return *this;
		}

		constexpr IntegralVec2& operator/=(T scalar) noexcept
		{
			X /= scalar;
			Y /= scalar;
			return *this;
		}

		/// @brief 标量在左的乘法
		[[nodiscard]] friend constexpr IntegralVec2 operator*(T scalar, const IntegralVec2& value) noexcept
		{
			return value * scalar;
		}

		[[nodiscard]] constexpr bool operator==(const IntegralVec2&) const noexcept = default;
	};

	using Vec2F = FloatVec2<float>;
	using Vec2D = FloatVec2<double>;

	using Vec2I8 = IntegralVec2<I8>;
	using Vec2I16 = IntegralVec2<I16>;
	using Vec2I32 = IntegralVec2<I32>;
	using Vec2I64 = IntegralVec2<I64>;
	using Vec2U8 = IntegralVec2<U8>;
	using Vec2U16 = IntegralVec2<U16>;
	using Vec2U32 = IntegralVec2<U32>;
	using Vec2U64 = IntegralVec2<U64>;

	// 布局不变量（I1）：任何破坏它们的改动都会在这里编译失败
	static_assert(std::is_standard_layout_v<Vec2F> && std::is_trivially_copyable_v<Vec2F>);
	static_assert(sizeof(Vec2F) == 2 * sizeof(float) && alignof(Vec2F) == alignof(float));
	static_assert(sizeof(Vec2D) == 2 * sizeof(double) && alignof(Vec2D) == alignof(double));
	static_assert(sizeof(Vec2I32) == 2 * sizeof(I32) && alignof(Vec2I32) == alignof(I32));
	static_assert(sizeof(Vec2U64) == 2 * sizeof(U64) && alignof(Vec2U64) == alignof(U64));
	static_assert(Vec2F::Dimension == 2);
}
