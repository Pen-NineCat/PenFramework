// File /Native/Engine/Test/UnitTest/MathTypesTest.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// 说明：Engine 层单元测试（GTest），同 MathFunctionTest.cpp：只编入 PenEngineTest 目标，
//   用例由 gtest_discover_tests 注册进 ctest。

#include "Engine/Math/Color.hpp"
#include "Engine/Math/Mat4x4.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"

#include <gtest/gtest.h>

#include <type_traits>

using namespace PenEngine;

// ===========================================================================
// 一、编译期验证
// ===========================================================================

// 布局不变量：标准布局、平凡可复制、紧凑（无 SIMD 对齐撑开）
static_assert(std::is_standard_layout_v<Vec2F> && std::is_trivially_copyable_v<Vec2F>);
static_assert(std::is_standard_layout_v<Vec3F> && std::is_trivially_copyable_v<Vec3F>);
static_assert(std::is_standard_layout_v<Vec4D> && std::is_trivially_copyable_v<Vec4D>);
static_assert(std::is_standard_layout_v<Mat4x4F> && std::is_trivially_copyable_v<Mat4x4F>);
static_assert(std::is_standard_layout_v<Color32> && std::is_trivially_copyable_v<Color32>);
static_assert(std::is_standard_layout_v<ColorF> && std::is_trivially_copyable_v<ColorF>);
static_assert(sizeof(Vec2F) == 8 && alignof(Vec2F) == alignof(float));
static_assert(sizeof(Vec3F) == 12 && alignof(Vec3F) == alignof(float));
static_assert(sizeof(Vec3D) == 24 && sizeof(Vec4F) == 16 && sizeof(Vec4D) == 32);
static_assert(sizeof(Mat4x4F) == 64 && sizeof(Mat4x4D) == 128);
static_assert(sizeof(Color32) == 4 && sizeof(ColorF) == 16);
static_assert(Vec2F::Dimension == 2 && Vec3F::Dimension == 3 && Vec4F::Dimension == 4 && Mat4x4F::Dimension == 4);

// 向量运算
constexpr bool VerifyVectors()
{
	Vec3F a{ 1.0f, 2.0f, 3.0f };
	Vec3F b{ 0.5f, 0.5f, 0.5f };

	if (a[0] != 1.0f || a[1] != 2.0f || a[2] != 3.0f)
		return false;

	if ((a + b) != Vec3F{ 1.5f, 2.5f, 3.5f } || (a - b) != Vec3F{ 0.5f, 1.5f, 2.5f })
		return false;

	if ((-a) != Vec3F{ -1.0f, -2.0f, -3.0f })
		return false;

	if ((2.0f * a) != Vec3F{ 2.0f, 4.0f, 6.0f } || (a * 2.0f) != Vec3F{ 2.0f, 4.0f, 6.0f })
		return false;

	if ((a / 2.0f) != Vec3F{ 0.5f, 1.0f, 1.5f })
		return false;

	Vec4F v4{ 1.0f, 2.0f, 3.0f, 4.0f };
	v4 += Vec4F{ 1.0f, 1.0f, 1.0f, 1.0f };

	if (v4[3] != 5.0f || v4.W != 5.0f)
		return false;

	v4[3] = 0.0f;

	if (v4.W != 0.0f)
		return false;

	Vec2I32 i2{ -3, 7 };
	i2 /= 2;

	if (i2[0] != -1 || i2[1] != 3)
		return false;

	Vec2U8 u2{ 200u, 100u };
	u2 += Vec2U8{ 100u, 100u };

	// 无符号按模回绕是定义行为
	return u2[0] == 44u && u2[1] == 200u;
}
static_assert(VerifyVectors());

// 矩阵：列主序不变量 + 单位矩阵 + 两种访问符等价
constexpr bool VerifyMatrix()
{
	Mat4x4F matrix{};
	matrix[1, 2] = 7.0f;

	if (matrix.m_data[2 * 4 + 1] != 7.0f || matrix(1, 2) != 7.0f || matrix(2, 1) != 0.0f)
		return false;

	const Mat4x4F identity = Mat4x4F::Identity();

	for (Usize row = 0; row < Mat4x4F::Dimension; ++row)
	{
		for (Usize column = 0; column < Mat4x4F::Dimension; ++column)
		{
			const float expected = row == column ? 1.0f : 0.0f;

			if (identity[row, column] != expected)
				return false;
		}
	}

	return Mat4x4F::Zero() == Mat4x4F{} && identity != Mat4x4F{};
}
static_assert(VerifyMatrix());

// 颜色：调色板 / 预乘 / 反预乘 / 插值 / 混合
static_assert(Color32(Color::Red).PackU32() == 0xFF0000FFu);
static_assert(Color32(Color::Transparent).PackU32() == 0x00000000u);
static_assert(ColorF(Color::White).R == 1.0f && ColorF(Color::Transparent).A == 0.0f);

constexpr bool VerifyColor()
{
	// 预乘：三个通道都要乘 A（旧实现把 B 通道写成了 G * A）
	const Color32 premultiplied = Color32(0, 0, 255, 128).PreMulAlpha();

	if (premultiplied.R != 0 || premultiplied.G != 0 || premultiplied.B != 128 || premultiplied.A != 128)
		return false;

	// 反预乘：旧静态版括号错位，恒等于 color * 255
	if (Color32::UnPreMulAlpha(64, 128) != 128)
		return false;

	// 反预乘溢出必须夹到 255，而不是 U8 回绕
	if (Color32(200, 0, 0, 128).UnPreMulAlpha().R != 255)
		return false;

	// 合法预乘值的往返
	if (Color32(64, 32, 16, 128).PreMulAlpha().UnPreMulAlpha() != Color32(64, 32, 16, 128))
		return false;

	// 8 位插值：255 → 0 的中点是 128（四舍五入）
	if (Color32(Color::White).Lerp(Color32(Color::Black), 0.5f) != Color32(128, 128, 128, 255))
		return false;

	if (Color32(Color::Black).Lerp(Color32(Color::White), 0.0f) != Color32(Color::Black))
		return false;

	// 浮点插值：旧公式 R + (R + target) * t 在非零起点上会算错
	if (!NearAbs(ColorF(0.2f, 0.2f, 0.2f, 1.0f).Lerp(ColorF(0.6f, 0.6f, 0.6f, 1.0f), 0.5f).R, 0.4f))
		return false;

	// 构造夹到 [0, 1]（旧实现只夹上界）
	const ColorF clamped = ColorF(-0.5f, 2.0f, 0.5f, 1.0f);

	if (clamped.R != 0.0f || clamped.G != 1.0f || clamped.B != 0.5f)
		return false;

	// source-over：this = dst、参数 = src
	const ColorF blended = ColorF(1.0f, 0.0f, 0.0f, 1.0f).Blend(ColorF(0.0f, 0.0f, 1.0f, 0.5f));

	if (!NearAbs(blended.R, 0.5f) || !NearAbs(blended.B, 0.5f) || !NearAbs(blended.A, 1.0f))
		return false;

	return ColorF(Color::Red).PackU32() == 0xFF0000FFu && ColorF::PackU32(1.0f, 1.0f, 1.0f, 1.0f) == 0xFFFFFFFFu;
}
static_assert(VerifyColor());

// ===========================================================================
// 二、运行期用例
// ===========================================================================

TEST(MathTypes, VectorArithmetic)
{
	const Vec3F a{ 1.0f, 2.0f, 3.0f };
	const Vec3F b{ 0.5f, 0.5f, 0.5f };

	EXPECT_EQ(a + b, (Vec3F{ 1.5f, 2.5f, 3.5f }));
	EXPECT_EQ(a - b, (Vec3F{ 0.5f, 1.5f, 2.5f }));
	EXPECT_EQ(-a, (Vec3F{ -1.0f, -2.0f, -3.0f }));
	EXPECT_EQ(a * 2.0f, (Vec3F{ 2.0f, 4.0f, 6.0f }));
	EXPECT_EQ(2.0f * a, a * 2.0f);
	EXPECT_EQ(a / 2.0f, (Vec3F{ 0.5f, 1.0f, 1.5f }));

	Vec4F mutated{ 1.0f, 2.0f, 3.0f, 4.0f };
	mutated -= Vec4F{ 0.0f, 0.0f, 0.0f, 4.0f };
	mutated *= 2.0f;
	mutated /= 2.0f;

	EXPECT_EQ(mutated, (Vec4F{ 1.0f, 2.0f, 3.0f, 0.0f }));
	EXPECT_EQ(mutated[3], 0.0f);
}

TEST(MathTypes, VectorAliasesCoverBothFamilies)
{
	EXPECT_EQ(sizeof(Vec2I8), 2u);
	EXPECT_EQ(sizeof(Vec3U16), 6u);
	EXPECT_EQ(sizeof(Vec4I64), 32u);
	EXPECT_EQ(sizeof(Vec2D), 16u); // double 分量

	const Vec2I16 signedPair{ -1, 1 };
	const Vec2U16 unsignedPair{ 1u, 2u };

	EXPECT_EQ((signedPair + Vec2I16{ 1, 1 }), (Vec2I16{ 0, 2 }));
	EXPECT_EQ((unsignedPair * 2u), (Vec2U16{ 2u, 4u }));
}

TEST(MathTypes, MatrixIsColumnMajor)
{
	Mat4x4F matrix{};

	matrix[0, 3] = 1.0f; // 第 3 列第 0 行

	EXPECT_EQ(matrix.m_data[12], 1.0f);
	EXPECT_EQ(matrix(0, 3), 1.0f);
	EXPECT_EQ(matrix(3, 0), 0.0f);

	const Mat4x4F identity = Mat4x4F::Identity();

	EXPECT_EQ((identity[0, 0]), 1.0f);
	EXPECT_EQ((identity[1, 1]), 1.0f);
	EXPECT_EQ((identity[2, 2]), 1.0f);
	EXPECT_EQ((identity[3, 3]), 1.0f);
	EXPECT_EQ((identity[3, 0]), 0.0f);
	EXPECT_EQ(Mat4x4F::Zero(), Mat4x4F{});
}

TEST(MathTypes, PaletteAndPack)
{
	EXPECT_EQ(Color32(Color::SkyBlue).PackU32(), 0xFFEBCE87u);
	EXPECT_EQ(Color32(Color::Green).PackU32(), 0xFF008000u);
	EXPECT_EQ(Color32(Color::Gold).PackU32(), 0xFF00D7FFu);
	EXPECT_EQ(ColorF(Color::Gray).R, ColorF::ToUnit(128));
	EXPECT_EQ(ColorF(Color::Red).PackU32(), Color32(Color::Red).PackU32());
}

TEST(MathTypes, PreMultipliedAlphaRegression)
{
	// 旧实现：B 通道被写成 G * A（这里 G == 0，B 会变成 0）
	const Color32 premultiplied = Color32(0, 0, 255, 128).PreMulAlpha();

	EXPECT_EQ(premultiplied.B, 128);
	EXPECT_EQ(premultiplied.A, 128);

	// 旧静态版：括号错位，结果与 alpha 无关
	EXPECT_EQ(Color32::UnPreMulAlpha(64, 128), 128);
	EXPECT_EQ(Color32::UnPreMulAlpha(64, 255), 64);
	EXPECT_EQ(Color32::UnPreMulAlpha(64, 0), 0);

	// 非法预乘值：夹到 255 而不是回绕
	EXPECT_EQ(Color32(200, 0, 0, 128).UnPreMulAlpha().R, 255);

	// 往返：8 位域上的预乘是有损的（alpha 很小时必然丢位），
	// 但在 alpha >= 128 的区间内误差不超过 1
	// 注意：循环变量用 int —— 用 U8 会在 alpha = 247 + 17 处回绕到 8，静默测到小 alpha
	for (int alpha = 128; alpha < 255; alpha += 17)
	{
		for (int channel = 0; channel <= alpha; channel += 13)
		{
			const Color32 premul = Color32(static_cast<U8>(channel), static_cast<U8>(channel), static_cast<U8>(channel),
				static_cast<U8>(alpha)).PreMulAlpha();

			EXPECT_NEAR(premul.UnPreMulAlpha().R, channel, 1);
		}
	}
}

TEST(MathTypes, ColorInterpolationAndBlend)
{
	EXPECT_EQ(Color32(Color::White).Lerp(Color32(Color::Black), 0.5f), Color32(128, 128, 128, 255));
	EXPECT_EQ(Color32(Color::Black).Lerp(Color32(Color::White), 0.25f), Color32(64, 64, 64, 255));

	// 旧公式在非零起点上会算错（0.2 → 0.6 的中点必须是 0.4）
	EXPECT_NEAR(ColorF(0.2f, 0.2f, 0.2f, 1.0f).Lerp(ColorF(0.6f, 0.6f, 0.6f, 1.0f), 0.5f).R, 0.4f, 1e-6f);

	// 不透明前景完全覆盖背景：a.Blend(b) 表示 b 盖在 a 上，故结果等于 b
	EXPECT_EQ(ColorF(Color::Green).Blend(ColorF(Color::Red)), ColorF(Color::Red));

	// 半透明蓝色盖在红色上，结果应为 (0.5, 0, 0.5, 1)
	const ColorF blended = ColorF(1.0f, 0.0f, 0.0f, 1.0f).Blend(ColorF(0.0f, 0.0f, 1.0f, 0.5f));

	EXPECT_NEAR(blended.R, 0.5f, 1e-6f);
	EXPECT_NEAR(blended.B, 0.5f, 1e-6f);
	EXPECT_NEAR(blended.A, 1.0f, 1e-6f);
}

TEST(MathTypes, ColorConstructionClamps)
{
	const ColorF clamped = ColorF(-1.0f, 2.0f, 0.5f, 3.0f);

	EXPECT_EQ(clamped.R, 0.0f);
	EXPECT_EQ(clamped.G, 1.0f);
	EXPECT_EQ(clamped.B, 0.5f);
	EXPECT_EQ(clamped.A, 1.0f);
}
