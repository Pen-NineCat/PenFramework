// File /Native/Engine/Math/Vec3.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "MathConstant.hpp"

#include <cassert>
#include <type_traits>

namespace PenEngine
{
	/// @brief 3 分量浮点向量
	/// @tparam T 仅 float/double（见 MathFloatingPoint）
	///
	/// I1. 布局：标准布局、平凡可复制、无额外对齐，sizeof == 3 * sizeof(T)
	///     （刻意不做 16 字节对齐：Vec3F 是 12 字节，才能紧凑装进顶点缓冲）
	/// I2. 分量与下标一一对应：0 → X，1 → Y，2 → Z
	/// I3. 本阶段只提供 +、−（含一元 −）、标量 * /（含标量在左的 *）、复合赋值与 operator[]；
	///     叉积 / 点积 / 长度 / 归一化等高层运算留待后续
	template <MathFloatingPoint T>
	struct FloatVec3
	{
		/// @brief 分量个数
		static constexpr Usize Dimension = 3;

		T X{};
		T Y{};
		T Z{};

		constexpr FloatVec3() noexcept = default;
		constexpr FloatVec3(T x, T y, T z) noexcept : X(x), Y(y), Z(z) {}

		/// @brief 分量访问（可写）
		/// @param index 分量下标，必须 < Dimension
		/// @note 常量求值不允许跨成员的指针算术（(&X)[index] 不是常量表达式），
		///       因此这里用条件链而非数组下标；越界时 Debug 断言失败，Release 下返回最后一个分量
		[[nodiscard]] constexpr T& operator[](Usize index) noexcept
		{
			assert(index < Dimension);

			return index == 0 ? X : (index == 1 ? Y : Z);
		}

		[[nodiscard]] constexpr const T& operator[](Usize index) const noexcept
		{
			assert(index < Dimension);

			return index == 0 ? X : (index == 1 ? Y : Z);
		}

		[[nodiscard]] constexpr FloatVec3 operator+(const FloatVec3& rhs) const noexcept
		{
			return { X + rhs.X, Y + rhs.Y, Z + rhs.Z };
		}

		[[nodiscard]] constexpr FloatVec3 operator-(const FloatVec3& rhs) const noexcept
		{
			return { X - rhs.X, Y - rhs.Y, Z - rhs.Z };
		}

		[[nodiscard]] constexpr FloatVec3 operator-() const noexcept
		{
			return { static_cast<T>(-X), static_cast<T>(-Y), static_cast<T>(-Z) };
		}

		[[nodiscard]] constexpr FloatVec3 operator*(T scalar) const noexcept
		{
			return { X * scalar, Y * scalar, Z * scalar };
		}

		[[nodiscard]] constexpr FloatVec3 operator/(T scalar) const noexcept
		{
			return { X / scalar, Y / scalar, Z / scalar };
		}

		constexpr FloatVec3& operator+=(const FloatVec3& rhs) noexcept
		{
			X += rhs.X;
			Y += rhs.Y;
			Z += rhs.Z;
			return *this;
		}

		constexpr FloatVec3& operator-=(const FloatVec3& rhs) noexcept
		{
			X -= rhs.X;
			Y -= rhs.Y;
			Z -= rhs.Z;
			return *this;
		}

		constexpr FloatVec3& operator*=(T scalar) noexcept
		{
			X *= scalar;
			Y *= scalar;
			Z *= scalar;
			return *this;
		}

		constexpr FloatVec3& operator/=(T scalar) noexcept
		{
			X /= scalar;
			Y /= scalar;
			Z /= scalar;
			return *this;
		}

		/// @brief 标量在左的乘法（v * s 由成员提供，s * v 由此友元提供）
		[[nodiscard]] friend constexpr FloatVec3 operator*(T scalar, const FloatVec3& value) noexcept
		{
			return value * scalar;
		}

		[[nodiscard]] constexpr bool operator==(const FloatVec3&) const noexcept = default;
	};

	/// @brief 3 分量整型向量
	/// @tparam T 仅整型且非 bool/字符（见 MathIntegral）
	///
	/// I1. 布局同 FloatVec3 的 I1：sizeof == 3 * sizeof(T)，无额外对齐
	/// I2. 除法是整数除法（向零截断）；一元 − 对无符号类型是模运算，均为定义行为
	template <MathIntegral T>
	struct IntegralVec3
	{
		/// @brief 分量个数
		static constexpr Usize Dimension = 3;

		T X{};
		T Y{};
		T Z{};

		constexpr IntegralVec3() noexcept = default;
		constexpr IntegralVec3(T x, T y, T z) noexcept : X(x), Y(y), Z(z) {}

		/// @brief 分量访问（可写）
		/// @param index 分量下标，必须 < Dimension
		/// @note 常量求值不允许跨成员的指针算术（(&X)[index] 不是常量表达式），
		///       因此这里用条件链而非数组下标；越界时 Debug 断言失败，Release 下返回最后一个分量
		[[nodiscard]] constexpr T& operator[](Usize index) noexcept
		{
			assert(index < Dimension);

			return index == 0 ? X : (index == 1 ? Y : Z);
		}

		[[nodiscard]] constexpr const T& operator[](Usize index) const noexcept
		{
			assert(index < Dimension);

			return index == 0 ? X : (index == 1 ? Y : Z);
		}

		// 说明：整型算术一律提升到 int，花括号初始化必须显式收回 T，否则是收缩转换（C2398）
		[[nodiscard]] constexpr IntegralVec3 operator+(const IntegralVec3& rhs) const noexcept
		{
			return { static_cast<T>(X + rhs.X), static_cast<T>(Y + rhs.Y), static_cast<T>(Z + rhs.Z) };
		}

		[[nodiscard]] constexpr IntegralVec3 operator-(const IntegralVec3& rhs) const noexcept
		{
			return { static_cast<T>(X - rhs.X), static_cast<T>(Y - rhs.Y), static_cast<T>(Z - rhs.Z) };
		}

		[[nodiscard]] constexpr IntegralVec3 operator-() const noexcept
		{
			return { static_cast<T>(-X), static_cast<T>(-Y), static_cast<T>(-Z) };
		}

		[[nodiscard]] constexpr IntegralVec3 operator*(T scalar) const noexcept
		{
			return { static_cast<T>(X * scalar), static_cast<T>(Y * scalar), static_cast<T>(Z * scalar) };
		}

		[[nodiscard]] constexpr IntegralVec3 operator/(T scalar) const noexcept
		{
			return { static_cast<T>(X / scalar), static_cast<T>(Y / scalar), static_cast<T>(Z / scalar) };
		}

		constexpr IntegralVec3& operator+=(const IntegralVec3& rhs) noexcept
		{
			X += rhs.X;
			Y += rhs.Y;
			Z += rhs.Z;
			return *this;
		}

		constexpr IntegralVec3& operator-=(const IntegralVec3& rhs) noexcept
		{
			X -= rhs.X;
			Y -= rhs.Y;
			Z -= rhs.Z;
			return *this;
		}

		constexpr IntegralVec3& operator*=(T scalar) noexcept
		{
			X *= scalar;
			Y *= scalar;
			Z *= scalar;
			return *this;
		}

		constexpr IntegralVec3& operator/=(T scalar) noexcept
		{
			X /= scalar;
			Y /= scalar;
			Z /= scalar;
			return *this;
		}

		/// @brief 标量在左的乘法
		[[nodiscard]] friend constexpr IntegralVec3 operator*(T scalar, const IntegralVec3& value) noexcept
		{
			return value * scalar;
		}

		[[nodiscard]] constexpr bool operator==(const IntegralVec3&) const noexcept = default;
	};

	using Vec3F = FloatVec3<float>;
	using Vec3D = FloatVec3<double>;

	using Vec3I8 = IntegralVec3<I8>;
	using Vec3I16 = IntegralVec3<I16>;
	using Vec3I32 = IntegralVec3<I32>;
	using Vec3I64 = IntegralVec3<I64>;
	using Vec3U8 = IntegralVec3<U8>;
	using Vec3U16 = IntegralVec3<U16>;
	using Vec3U32 = IntegralVec3<U32>;
	using Vec3U64 = IntegralVec3<U64>;

	// 布局不变量（I1）：任何破坏它们的改动都会在这里编译失败
	static_assert(std::is_standard_layout_v<Vec3F> && std::is_trivially_copyable_v<Vec3F>);
	static_assert(sizeof(Vec3F) == 3 * sizeof(float) && alignof(Vec3F) == alignof(float));
	static_assert(sizeof(Vec3D) == 3 * sizeof(double) && alignof(Vec3D) == alignof(double));
	static_assert(sizeof(Vec3I32) == 3 * sizeof(I32) && alignof(Vec3I32) == alignof(I32));
	static_assert(sizeof(Vec3U64) == 3 * sizeof(U64) && alignof(Vec3U64) == alignof(U64));
	static_assert(Vec3F::Dimension == 3);
}
