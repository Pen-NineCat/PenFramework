// File /Native/Engine/Math/Vec4.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "MathConstant.hpp"

#include <cassert>
#include <type_traits>

namespace PenEngine
{
	/// @brief 4 分量浮点向量（常用于齐次坐标：W 是第 4 分量，不参与 XYZ 语义）
	/// @tparam T 仅 float/double（见 MathFloatingPoint）
	///
	/// I1. 布局：标准布局、平凡可复制、无额外对齐，sizeof == 4 * sizeof(T)
	///     （刻意不做 16 字节对齐：需要 SIMD 对齐的场合另立类型，不要偷偷改 sizeof）
	/// I2. 分量与下标一一对应：0 → X，1 → Y，2 → Z，3 → W
	/// I3. 本阶段只提供 +、−（含一元 −）、标量 * /（含标量在左的 *）、复合赋值与 operator[]；
	///     点积 / 叉积 / 长度 / 归一化等高层运算留待后续
	template <MathFloatingPoint T>
	struct FloatVec4
	{
		/// @brief 分量个数
		static constexpr Usize Dimension = 4;

		T X{};
		T Y{};
		T Z{};
		T W{};

		constexpr FloatVec4() noexcept = default;
		constexpr FloatVec4(T x, T y, T z, T w) noexcept : X(x), Y(y), Z(z), W(w) {}

		/// @brief 分量访问（可写）
		/// @param index 分量下标，必须 < Dimension
		/// @note 常量求值不允许跨成员的指针算术（(&X)[index] 不是常量表达式），
		///       因此这里用条件链而非数组下标；越界时 Debug 断言失败，Release 下返回最后一个分量
		[[nodiscard]] constexpr T& operator[](Usize index) noexcept
		{
			assert(index < Dimension);

			return index == 0 ? X : (index == 1 ? Y : (index == 2 ? Z : W));
		}

		[[nodiscard]] constexpr const T& operator[](Usize index) const noexcept
		{
			assert(index < Dimension);

			return index == 0 ? X : (index == 1 ? Y : (index == 2 ? Z : W));
		}

		[[nodiscard]] constexpr FloatVec4 operator+(const FloatVec4& rhs) const noexcept
		{
			return { X + rhs.X, Y + rhs.Y, Z + rhs.Z, W + rhs.W };
		}

		[[nodiscard]] constexpr FloatVec4 operator-(const FloatVec4& rhs) const noexcept
		{
			return { X - rhs.X, Y - rhs.Y, Z - rhs.Z, W - rhs.W };
		}

		[[nodiscard]] constexpr FloatVec4 operator-() const noexcept
		{
			return { static_cast<T>(-X), static_cast<T>(-Y), static_cast<T>(-Z), static_cast<T>(-W) };
		}

		[[nodiscard]] constexpr FloatVec4 operator*(T scalar) const noexcept
		{
			return { X * scalar, Y * scalar, Z * scalar, W * scalar };
		}

		[[nodiscard]] constexpr FloatVec4 operator/(T scalar) const noexcept
		{
			return { X / scalar, Y / scalar, Z / scalar, W / scalar };
		}

		constexpr FloatVec4& operator+=(const FloatVec4& rhs) noexcept
		{
			X += rhs.X;
			Y += rhs.Y;
			Z += rhs.Z;
			W += rhs.W;
			return *this;
		}

		constexpr FloatVec4& operator-=(const FloatVec4& rhs) noexcept
		{
			X -= rhs.X;
			Y -= rhs.Y;
			Z -= rhs.Z;
			W -= rhs.W;
			return *this;
		}

		constexpr FloatVec4& operator*=(T scalar) noexcept
		{
			X *= scalar;
			Y *= scalar;
			Z *= scalar;
			W *= scalar;
			return *this;
		}

		constexpr FloatVec4& operator/=(T scalar) noexcept
		{
			X /= scalar;
			Y /= scalar;
			Z /= scalar;
			W /= scalar;
			return *this;
		}

		/// @brief 标量在左的乘法（v * s 由成员提供，s * v 由此友元提供）
		[[nodiscard]] friend constexpr FloatVec4 operator*(T scalar, const FloatVec4& value) noexcept
		{
			return value * scalar;
		}

		[[nodiscard]] constexpr bool operator==(const FloatVec4&) const noexcept = default;
	};

	/// @brief 4 分量整型向量
	/// @tparam T 仅整型且非 bool/字符（见 MathIntegral）
	///
	/// I1. 布局同 FloatVec4 的 I1：sizeof == 4 * sizeof(T)，无额外对齐
	/// I2. 除法是整数除法（向零截断）；一元 − 对无符号类型是模运算，均为定义行为
	template <MathIntegral T>
	struct IntegralVec4
	{
		/// @brief 分量个数
		static constexpr Usize Dimension = 4;

		T X{};
		T Y{};
		T Z{};
		T W{};

		constexpr IntegralVec4() noexcept = default;
		constexpr IntegralVec4(T x, T y, T z, T w) noexcept : X(x), Y(y), Z(z), W(w) {}

		/// @brief 分量访问（可写）
		/// @param index 分量下标，必须 < Dimension
		/// @note 常量求值不允许跨成员的指针算术（(&X)[index] 不是常量表达式），
		///       因此这里用条件链而非数组下标；越界时 Debug 断言失败，Release 下返回最后一个分量
		[[nodiscard]] constexpr T& operator[](Usize index) noexcept
		{
			assert(index < Dimension);

			return index == 0 ? X : (index == 1 ? Y : (index == 2 ? Z : W));
		}

		[[nodiscard]] constexpr const T& operator[](Usize index) const noexcept
		{
			assert(index < Dimension);

			return index == 0 ? X : (index == 1 ? Y : (index == 2 ? Z : W));
		}

		// 说明：整型算术一律提升到 int，花括号初始化必须显式收回 T，否则是收缩转换（C2398）
		[[nodiscard]] constexpr IntegralVec4 operator+(const IntegralVec4& rhs) const noexcept
		{
			return { static_cast<T>(X + rhs.X), static_cast<T>(Y + rhs.Y), static_cast<T>(Z + rhs.Z), static_cast<T>(W + rhs.W) };
		}

		[[nodiscard]] constexpr IntegralVec4 operator-(const IntegralVec4& rhs) const noexcept
		{
			return { static_cast<T>(X - rhs.X), static_cast<T>(Y - rhs.Y), static_cast<T>(Z - rhs.Z), static_cast<T>(W - rhs.W) };
		}

		[[nodiscard]] constexpr IntegralVec4 operator-() const noexcept
		{
			return { static_cast<T>(-X), static_cast<T>(-Y), static_cast<T>(-Z), static_cast<T>(-W) };
		}

		[[nodiscard]] constexpr IntegralVec4 operator*(T scalar) const noexcept
		{
			return { static_cast<T>(X * scalar), static_cast<T>(Y * scalar), static_cast<T>(Z * scalar), static_cast<T>(W * scalar) };
		}

		[[nodiscard]] constexpr IntegralVec4 operator/(T scalar) const noexcept
		{
			return { static_cast<T>(X / scalar), static_cast<T>(Y / scalar), static_cast<T>(Z / scalar), static_cast<T>(W / scalar) };
		}

		constexpr IntegralVec4& operator+=(const IntegralVec4& rhs) noexcept
		{
			X += rhs.X;
			Y += rhs.Y;
			Z += rhs.Z;
			W += rhs.W;
			return *this;
		}

		constexpr IntegralVec4& operator-=(const IntegralVec4& rhs) noexcept
		{
			X -= rhs.X;
			Y -= rhs.Y;
			Z -= rhs.Z;
			W -= rhs.W;
			return *this;
		}

		constexpr IntegralVec4& operator*=(T scalar) noexcept
		{
			X *= scalar;
			Y *= scalar;
			Z *= scalar;
			W *= scalar;
			return *this;
		}

		constexpr IntegralVec4& operator/=(T scalar) noexcept
		{
			X /= scalar;
			Y /= scalar;
			Z /= scalar;
			W /= scalar;
			return *this;
		}

		/// @brief 标量在左的乘法
		[[nodiscard]] friend constexpr IntegralVec4 operator*(T scalar, const IntegralVec4& value) noexcept
		{
			return value * scalar;
		}

		[[nodiscard]] constexpr bool operator==(const IntegralVec4&) const noexcept = default;
	};

	using Vec4F = FloatVec4<float>;
	using Vec4D = FloatVec4<double>;

	using Vec4I8 = IntegralVec4<I8>;
	using Vec4I16 = IntegralVec4<I16>;
	using Vec4I32 = IntegralVec4<I32>;
	using Vec4I64 = IntegralVec4<I64>;
	using Vec4U8 = IntegralVec4<U8>;
	using Vec4U16 = IntegralVec4<U16>;
	using Vec4U32 = IntegralVec4<U32>;
	using Vec4U64 = IntegralVec4<U64>;

	// 布局不变量（I1）：任何破坏它们的改动都会在这里编译失败
	static_assert(std::is_standard_layout_v<Vec4F> && std::is_trivially_copyable_v<Vec4F>);
	static_assert(sizeof(Vec4F) == 4 * sizeof(float) && alignof(Vec4F) == alignof(float));
	static_assert(sizeof(Vec4D) == 4 * sizeof(double) && alignof(Vec4D) == alignof(double));
	static_assert(sizeof(Vec4I32) == 4 * sizeof(I32) && alignof(Vec4I32) == alignof(I32));
	static_assert(sizeof(Vec4U64) == 4 * sizeof(U64) && alignof(Vec4U64) == alignof(U64));
	static_assert(Vec4F::Dimension == 4);
}
