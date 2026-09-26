// File /Native/Engine/Math/Mat4x4.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "MathConstant.hpp"

#include <type_traits>

namespace PenEngine
{
	/// @brief 4×4 浮点矩阵（仅元素访问；变换 / 求逆 / 投影等高层运算留待后续）
	/// @tparam T 仅 float/double（见 MathFloatingPoint）
	///
	/// I1. 存储序为列主序：数学元素 (row, column) 存放在 m_data[column * 4 + row]，
	///     即同一列在内存中连续 —— 便于按列直接上传到图形 API 的常量缓冲
	/// I2. 布局：标准布局、平凡可复制、无额外对齐，sizeof == 16 * sizeof(T)
	/// I3. 元素访问同时提供 C++23 多维下标的 operator[](row, column) 与 operator()(row, column)，
	///     两者语义完全一致（行列均为 0 起始）
	/// I4. 默认构造是全零矩阵；单位矩阵用 Identity()
	template <MathFloatingPoint T>
	struct Mat4x4
	{
		/// @brief 行数与列数
		static constexpr Usize Dimension = 4;

		/// 元素存储（列主序，见 I1）
		T m_data[16]{};

		constexpr Mat4x4() noexcept = default;

		/// @brief 元素访问（可写）
		/// @param row 行号，0 ~ 3
		/// @param column 列号，0 ~ 3
		[[nodiscard]] constexpr T& operator[](Usize row, Usize column) noexcept
		{
			return m_data[column * Dimension + row];
		}

		/// @brief 元素访问（只读）
		[[nodiscard]] constexpr const T& operator[](Usize row, Usize column) const noexcept
		{
			return m_data[column * Dimension + row];
		}

		/// @brief 元素访问（可写，与 operator[] 等价）
		[[nodiscard]] constexpr T& operator()(Usize row, Usize column) noexcept
		{
			return m_data[column * Dimension + row];
		}

		/// @brief 元素访问（只读，与 operator[] 等价）
		[[nodiscard]] constexpr const T& operator()(Usize row, Usize column) const noexcept
		{
			return m_data[column * Dimension + row];
		}

		/// @brief 全零矩阵
		[[nodiscard]] static constexpr Mat4x4 Zero() noexcept
		{
			return Mat4x4{};
		}

		/// @brief 单位矩阵
		[[nodiscard]] static constexpr Mat4x4 Identity() noexcept
		{
			Mat4x4 result{};

			for (Usize index = 0; index < Dimension; ++index)
				result[index, index] = static_cast<T>(1);

			return result;
		}

		[[nodiscard]] constexpr bool operator==(const Mat4x4&) const noexcept = default;
	};

	using Mat4x4F = Mat4x4<float>;
	using Mat4x4D = Mat4x4<double>;

	// 布局不变量（I2）：任何破坏它们的改动都会在这里编译失败
	static_assert(std::is_standard_layout_v<Mat4x4F> && std::is_trivially_copyable_v<Mat4x4F>);
	static_assert(sizeof(Mat4x4F) == 16 * sizeof(float) && alignof(Mat4x4F) == alignof(float));
	static_assert(sizeof(Mat4x4D) == 16 * sizeof(double) && alignof(Mat4x4D) == alignof(double));
	static_assert(Mat4x4F::Dimension == 4);
}
