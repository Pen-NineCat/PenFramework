// File /Native/Engine/Test/UnitTest/MathFunctionTest.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// 说明：Engine 层单元测试（GTest）。本文件只编入 PenEngineTest 目标——根 CMakeLists.txt 把
//   Code/Native/Engine/Test/ 排除出主程序，所以这里可以有 TEST() 而不与 Main.cpp 撞入口。
//   用例由 gtest_discover_tests 逐个注册进 ctest：
//     ctest --test-dir out/build/x64-debug --output-on-failure -R MathFunction

#include "Engine/Math/MathFunction.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

using namespace PenEngine;

// ===========================================================================
// 一、编译期验证（consteval 路径）
// ===========================================================================

// 常量与概念
static_assert(MathFloatingPoint<float> && MathFloatingPoint<double>);
static_assert(!MathFloatingPoint<long double> && !MathFloatingPoint<int>);
static_assert(MathIntegral<I32> && MathIntegral<U64>);
static_assert(!MathIntegral<bool> && !MathIntegral<char> && !MathIntegral<float>);
static_assert(sizeof(Pi<float>) == sizeof(float), "Pi<T> 必须保持 T 的精度");
static_assert(Pi<float> == std::numbers::pi_v<float>);
static_assert(HalfPi<double> * 2.0 == Pi<double> && Tau<float> == 2.0f * Pi<float>);

// 特殊值判定
static_assert(IsNaN(std::numeric_limits<float>::quiet_NaN()));
static_assert(!IsNaN(1.0f) && !IsNaN(std::numeric_limits<double>::infinity()));
static_assert(IsInfinity(std::numeric_limits<float>::infinity()));
static_assert(IsInfinity(-std::numeric_limits<double>::infinity()));
static_assert(!IsInfinity(std::numeric_limits<double>::max()));
static_assert(SignBit(-0.0f) && !SignBit(0.0f) && SignBit(-1.5));
static_assert(Sign(7) == 1 && Sign(-7) == -1 && Sign(0) == 0 && Sign(-0.0f) == 0);
static_assert(CopySign(3.0f, -1.0f) == -3.0f && CopySign(-3.0f, 1.0f) == 3.0f);
static_assert(CopySign(3.0, -0.0) == -3.0);

// 绝对值
static_assert(Abs(-42) == 42 && Abs(42) == 42 && Abs(0) == 0);
static_assert(Abs(3.5f) == 3.5f && Abs(-3.5) == 3.5);
static_assert(!SignBit(Abs(-0.0f)));
static_assert(Abs(static_cast<U32>(7)) == 7u);

// 取整：两端、负数，以及「旧实现会跑数百万次循环」的大值
static_assert(Trunc(2.7f) == 2.0f && Trunc(-2.7f) == -2.0f);
static_assert(Floor(2.7f) == 2.0f && Floor(-2.7f) == -3.0f);
static_assert(Ceil(2.1f) == 3.0f && Ceil(-2.1f) == -2.0f);
static_assert(Floor(2.0f) == 2.0f && Ceil(2.0f) == 2.0f);
static_assert(SignBit(Trunc(-0.5f)) && Floor(-0.5f) == -1.0f);
static_assert(Floor(5000000.0f) == 5000000.0f, "旧实现在此要跑约 4.8e6 次迭代");
static_assert(Floor(16777216.0f) == 16777216.0f);
static_assert(Floor(1e300) == 1e300 && Ceil(-1e300) == -1e300);
static_assert(Floor(std::numeric_limits<float>::infinity()) == std::numeric_limits<float>::infinity());

// 定义式：floor(x) <= x < floor(x) + 1，ceil 对称（6001 个 float + 6001 个 double 样本）
constexpr bool VerifyFloorCeilDefinition()
{
	for (int i = -3000; i <= 3000; ++i)
	{
		const float value = static_cast<float>(i) * 0.37f;
		const float floorValue = Floor(value);
		const float ceilValue = Ceil(value);

		if (floorValue > value || value >= floorValue + 1.0f)
			return false;

		if (ceilValue < value || value > ceilValue + 1.0f)
			return false;
	}

	for (int i = -3000; i <= 3000; ++i)
	{
		const double value = static_cast<double>(i) * 0.371;
		const double floorValue = Floor(value);
		const double ceilValue = Ceil(value);

		if (floorValue > value || value >= floorValue + 1.0)
			return false;

		if (ceilValue < value || value > ceilValue + 1.0)
			return false;
	}

	return true;
}
static_assert(VerifyFloorCeilDefinition());

// 平方根：consteval 路径（Newton）精度（4000 个样本）
constexpr bool VerifySqrtConstexprAccuracy()
{
	for (int i = 1; i <= 2000; ++i)
	{
		const double exact = static_cast<double>(i) * 0.5;
		const double root = Sqrt(exact * exact);

		if (!NearRel(root, exact, 1e-13))
			return false;
	}

	for (int i = 1; i <= 2000; ++i)
	{
		const float exact = static_cast<float>(i) * 0.5f;
		const float root = Sqrt(exact * exact);

		if (!NearRel(root, exact, 1e-5f))
			return false;
	}

	return true;
}
static_assert(VerifySqrtConstexprAccuracy());

// 平方根：特殊值语义（负值含 −inf → NaN；−0.0 保留符号）
static_assert(IsNaN(Sqrt(-1.0f)) && IsNaN(Sqrt(-std::numeric_limits<float>::infinity())));
static_assert(IsNaN(Sqrt(std::numeric_limits<double>::quiet_NaN())));
static_assert(SignBit(Sqrt(-0.0f)) && Sqrt(-0.0f) == 0.0f);
static_assert(Sqrt(4.0f) == 2.0f);
static_assert(Sqrt(std::numeric_limits<double>::infinity()) == std::numeric_limits<double>::infinity());

// 幂（平方-乘，指数限定为非负整数）
static_assert(Pow(2.0f, 10u) == 1024.0f && Pow(3.0f, 3u) == 27.0f);
static_assert(Pow(2.0, 0u) == 1.0 && Pow(0.0, 0u) == 1.0);
static_assert(Pow(-2.0f, 3u) == -8.0f && Pow(-2.0f, 4u) == 16.0f);

// 区间、插值、近似比较
static_assert(Clamp(5, 0, 3) == 3 && Clamp(-5, 0, 3) == 0 && Clamp(2, 0, 3) == 2);
static_assert(Clamp01(1.5f) == 1.0f && Clamp01(-0.5f) == 0.0f && Clamp01(0.25f) == 0.25f);
static_assert(Lerp(0.0f, 10.0f, 0.5f) == 5.0f && Lerp(10.0f, 20.0f, 0.0f) == 10.0f);
static_assert(Lerp(0.0f, 10.0f, 2.0f) == 10.0f, "t 必须被夹到 [0, 1]");
static_assert(Lerp(0, 100, 0.5f) == 50);
static_assert(NearAbs(1.0f, 1.0f) && NearAbs(1.0f, 1.0f + MathEpsilon / 2.0f) && !NearAbs(1.0f, 1.1f));
static_assert(NearAbs(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()));
static_assert(!NearAbs(std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()));
static_assert(!NearAbs(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()));
static_assert(NearRel(1000.0f, 1000.001f) && !NearRel(1e-5f, 1.1e-5f));
static_assert(NearRel(0.0f, 0.0f) && !NearRel(0.0f, MathEpsilon));

// 整数对齐（旧 CeilPow2 / CalculateComMultiple 的正名与修正）
static_assert(IsPow2(1u) && IsPow2(16u) && IsPow2(1ull << 40));
static_assert(!IsPow2(0u) && !IsPow2(6u) && !IsPow2(3u));
static_assert(AlignUp(17u, 16u) == 32u && AlignUp(16u, 16u) == 16u && AlignUp(0u, 16u) == 0u);
static_assert(AlignDown(17u, 16u) == 16u && AlignDown(16u, 16u) == 16u && AlignDown(15u, 16u) == 0u);
static_assert(DivRoundUp(17u, 16u) == 2u && DivRoundUp(16u, 16u) == 1u && DivRoundUp(0u, 16u) == 0u);
static_assert(DivRoundUp(17ull, 1ull) == 17ull && DivRoundUp(1u, 64u) == 1u);

// 三角：sin² + cos² == 1 与已知值
constexpr bool VerifySinCosIdentity()
{
	const float samples[] = { 0.0f, 0.1f, 0.5f, 1.0f, Pi<float> / 6.0f, Pi<float> / 4.0f, Pi<float> / 3.0f,
		Pi<float> / 2.0f, 2.0f, 3.0f, -0.5f, -1.5f, -2.5f, 10.0f, -10.0f };

	for (const float value : samples)
	{
		float sinValue = 0.0f;
		float cosValue = 0.0f;
		ScalarSinCos(sinValue, cosValue, value);

		if (!NearAbs(sinValue * sinValue + cosValue * cosValue, 1.0f, 1e-4f))
			return false;
	}

	return true;
}
static_assert(VerifySinCosIdentity());

constexpr bool VerifySinCosKnownValues()
{
	float sinValue = 0.0f;
	float cosValue = 0.0f;

	ScalarSinCos(sinValue, cosValue, 0.0f);

	if (sinValue != 0.0f || cosValue != 1.0f)
		return false;

	ScalarSinCos(sinValue, cosValue, Pi<float> / 6.0f);

	if (!NearAbs(sinValue, 0.5f, 1e-5f) || !NearAbs(cosValue, 0.8660254f, 1e-5f))
		return false;

	ScalarSinCos(sinValue, cosValue, Pi<float> / 3.0f);

	return NearAbs(sinValue, 0.8660254f, 1e-5f) && NearAbs(cosValue, 0.5f, 1e-5f);
}
static_assert(VerifySinCosKnownValues());

// 一元二次方程
constexpr bool VerifySolveQuadratic()
{
	std::array<float, 2> roots{};

	// x² − 3x + 2 = 0 → {2, 1}
	if (SolveQuadratic(roots, 1.0f, -3.0f, 2.0f) != 2 || !NearAbs(roots[0], 2.0f) || !NearAbs(roots[1], 1.0f))
		return false;

	// x² + 1 = 0 → 无实根
	if (SolveQuadratic(roots, 1.0f, 0.0f, 1.0f) != 0)
		return false;

	// (x − 3)² → 重根 3
	if (SolveQuadratic(roots, 1.0f, -6.0f, 9.0f) != 1 || !NearAbs(roots[0], 3.0f) || roots[1] != roots[0])
		return false;

	// 2x − 4 = 0 → 一次方程
	if (SolveQuadratic(roots, 0.0f, 2.0f, -4.0f) != 1 || !NearAbs(roots[0], 2.0f))
		return false;

	// x² − x = 0 → {1, 0}（q == 0 分支）
	if (SolveQuadratic(roots, 1.0f, -1.0f, 0.0f) != 2 || !NearAbs(roots[0], 1.0f) || !NearAbs(roots[1], 0.0f))
		return false;

	return true;
}
static_assert(VerifySolveQuadratic());

// 数值稳定性：b² ≫ 4ac 时小根不能丢光（旧的朴素公式会）
constexpr bool VerifySolveQuadraticStability()
{
	std::array<double, 2> roots{};

	if (SolveQuadratic(roots, 1.0, -1e8, 1.0) != 2)
		return false;

	return NearRel(roots[0], 1e8, 1e-12) && NearRel(roots[1], 1e-8, 1e-6) && roots[0] >= roots[1];
}
static_assert(VerifySolveQuadraticStability());

// ===========================================================================
// 二、运行期用例
// ===========================================================================

TEST(MathFunction, SpecialValues)
{
	EXPECT_TRUE(IsNaN(std::numeric_limits<double>::quiet_NaN()));
	EXPECT_FALSE(IsNaN(0.0));
	EXPECT_TRUE(IsInfinity(std::numeric_limits<double>::infinity()));
	EXPECT_TRUE(IsInfinity(-std::numeric_limits<double>::infinity()));
	EXPECT_FALSE(IsInfinity(1.0));

	EXPECT_EQ(Sign(-3.5), -1);
	EXPECT_EQ(Sign(3.5), 1);
	EXPECT_EQ(Sign(-0.0), 0);
	EXPECT_EQ(Sign(std::numeric_limits<double>::quiet_NaN()), 1);
}

TEST(MathFunction, RoundingMatchesStandardLibrary)
{
	for (int i = -200000; i <= 200000; ++i)
	{
		const double value = static_cast<double>(i) * 0.017;

		EXPECT_EQ(Floor(value), std::floor(value));
		EXPECT_EQ(Ceil(value), std::ceil(value));
		EXPECT_EQ(Trunc(value), std::trunc(value));
	}
}

TEST(MathFunction, SqrtMatchesStandardLibraryAtRuntime)
{
	for (int i = 1; i <= 100000; ++i)
	{
		const double value = static_cast<double>(i) * 1e-3;

		EXPECT_EQ(Sqrt(value), std::sqrt(value));
	}
}

TEST(MathFunction, ScalarSinCosAccuracy)
{
	double maxSinError = 0.0;
	double maxCosError = 0.0;

	for (int i = -100000; i <= 100000; ++i)
	{
		const float angle = static_cast<float>(i) * 1e-4f;
		float sinValue = 0.0f;
		float cosValue = 0.0f;

		ScalarSinCos(sinValue, cosValue, angle);

		maxSinError = std::max(maxSinError, static_cast<double>(std::abs(sinValue - std::sin(angle))));
		maxCosError = std::max(maxCosError, static_cast<double>(std::abs(cosValue - std::cos(angle))));
	}

	EXPECT_LT(maxSinError, 1e-5);
	EXPECT_LT(maxCosError, 1e-5);
}

TEST(MathFunction, TrigWrappers)
{
	EXPECT_FLOAT_EQ(Sin(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(Cos(0.0f), 1.0f);
	EXPECT_NEAR(Asin(1.0f), HalfPi<float>, 1e-6f);
	EXPECT_NEAR(Acos(1.0f), 0.0f, 1e-6f);
	EXPECT_NEAR(Atan2(1.0f, 1.0f), Pi<float> / 4.0f, 1e-6f);
	EXPECT_NEAR(Tan(Pi<float> / 4.0f), 1.0f, 1e-6f);
}

TEST(MathFunction, Alignment)
{
	constexpr Usize alignment = 4096;

	EXPECT_EQ(AlignUp(static_cast<Usize>(0), alignment), 0u);
	EXPECT_EQ(AlignUp(static_cast<Usize>(1), alignment), alignment);
	EXPECT_EQ(AlignUp(alignment, alignment), alignment);
	EXPECT_EQ(AlignUp(alignment + 1, alignment), alignment * 2);
	EXPECT_EQ(AlignDown(alignment + 1, alignment), alignment);
	EXPECT_EQ(DivRoundUp(static_cast<Usize>(1), alignment), 1u);
	EXPECT_EQ(DivRoundUp(alignment, alignment), 1u);
	EXPECT_EQ(DivRoundUp(alignment + 1, alignment), 2u);

	// 旧 CalculateComMultiple 的语义（向上对齐）与新 AlignUp 一致
	EXPECT_EQ(AlignUp(static_cast<Usize>(17), static_cast<Usize>(16)), 32u);
}

TEST(MathFunction, SolveQuadraticRuntime)
{
	std::array<double, 2> roots{};

	EXPECT_EQ(SolveQuadratic(roots, 1.0, -3.0, 2.0), 2u);
	EXPECT_NEAR(roots[0], 2.0, 1e-12);
	EXPECT_NEAR(roots[1], 1.0, 1e-12);

	EXPECT_EQ(SolveQuadratic(roots, 1.0, 0.0, 1.0), 0u);
	EXPECT_EQ(SolveQuadratic(roots, 0.0, 0.0, 0.0), 0u);
	EXPECT_EQ(SolveQuadratic(roots, 0.0, 4.0, -2.0), 1u);
	EXPECT_NEAR(roots[0], 0.5, 1e-12);
}
