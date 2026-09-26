// File /Native/Engine/Math/Color.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "MathConstant.hpp"
#include "MathFunction.hpp"

#include <array>
#include <type_traits>

namespace PenEngine
{
	/// @brief 打包颜色（0xAABBGGRR）中 R 分量的位移
	inline constexpr U8 COL32_R_SHIFT = 0;
	/// @brief 打包颜色（0xAABBGGRR）中 G 分量的位移
	inline constexpr U8 COL32_G_SHIFT = 8;
	/// @brief 打包颜色（0xAABBGGRR）中 B 分量的位移
	inline constexpr U8 COL32_B_SHIFT = 16;
	/// @brief 打包颜色（0xAABBGGRR）中 A 分量的位移
	inline constexpr U8 COL32_A_SHIFT = 24;

	/// @brief 命名颜色
	///
	/// I1. 取值连续（0 … Count−1），且**同时作为调色板表的索引**
	///     （见 Internal::ColorPalette32）——枚举与颜色数据只有这一份对应关系，
	///     不再像旧实现那样让 Color32 / ColorF 各写一张 switch 表
	enum class Color : U8
	{
		Black = 0,   // 黑色
		White,       // 白色
		Gray,        // 灰色
		Silver,      // 银色
		Red,         // 红色
		Maroon,      // 栗色 / 暗红色
		Pink,        // 粉色 / 浅红色
		Crimson,     // 深红 / 鲜红色
		Coral,       // 珊瑚红 / 橙红色
		Blue,        // 蓝色
		Navy,        // 海军蓝 / 深蓝色
		Cyan,        // 青色 / 蓝绿色
		Teal,        // 凫蓝 / 青蓝色
		SkyBlue,     // 天蓝色 / 浅蓝色
		Green,       // 绿色
		Lime,        // 柠檬绿 / 亮绿色
		Olive,       // 橄榄绿 / 黄绿色
		Emerald,     // 翡翠绿 / 鲜艳绿色
		Forest,      // 森林绿 / 深绿色
		Yellow,      // 黄色
		Orange,      // 橙色 / 橙黄色
		Gold,        // 金色 / 金属黄色
		Amber,       // 琥珀色 / 橙黄色
		Purple,      // 紫色 / 基础紫色
		Violet,      // 紫罗兰色 / 蓝紫色
		Magenta,     // 洋红色 / 红紫色
		Lavender,    // 薰衣草紫 / 淡紫色
		Brown,       // 棕色 / 基础棕色
		Beige,       // 米色 / 浅棕色
		Tan,         // 棕褐色 / 黄棕色
		Chocolate,   // 巧克力色 / 深棕色
		Transparent, // 透明色 / 无颜色

		/// @brief 哨兵：颜色数量（不可当作颜色使用）
		Count
	};

	/// @brief 0~255 颜色
	///
	/// I1. 布局：4 个 U8、标准布局、平凡可复制，sizeof == 4（可直接作为顶点属性 / 纹理数据）
	/// I2. 分量不做隐式夹取：构造函数按原样保存，需要夹取请显式 Clamp
	struct Color32
	{
		U8 R = 0;
		U8 G = 0;
		U8 B = 0;
		U8 A = 255;

		constexpr Color32() noexcept = default;
		constexpr Color32(U8 r, U8 g, U8 b, U8 a = 255) noexcept : R(r), G(g), B(b), A(a) {}

		/// @brief 由命名颜色构造（查 Internal::ColorPalette32）
		constexpr explicit Color32(Color color) noexcept;

		/// @brief 打包为 0xAABBGGRR
		[[nodiscard]] constexpr U32 PackU32() const noexcept { return PackU32(R, G, B, A); }

		/// @brief 把 4 个 8 位分量打包为 0xAABBGGRR
		[[nodiscard]] static constexpr U32 PackU32(U8 r, U8 g, U8 b, U8 a) noexcept
		{
			return static_cast<U32>(a) << COL32_A_SHIFT
				| static_cast<U32>(b) << COL32_B_SHIFT
				| static_cast<U32>(g) << COL32_G_SHIFT
				| static_cast<U32>(r) << COL32_R_SHIFT;
		}

		/// @brief 预乘 Alpha（RGB 三个通道各自乘 A）
		/// @note 旧实现把 B 通道也算成了 G * A，此处已修正
		[[nodiscard]] constexpr Color32 PreMulAlpha() const noexcept
		{
			return Color32(PreMulAlpha(R, A), PreMulAlpha(G, A), PreMulAlpha(B, A), A);
		}

		/// @brief 单通道预乘 Alpha：(color * alpha + 127) / 255，四舍五入
		[[nodiscard]] static constexpr U8 PreMulAlpha(U8 color, U8 alpha) noexcept
		{
			if (alpha == 0)
				return 0;

			if (alpha == 255)
				return color;

			return static_cast<U8>((static_cast<U32>(color) * alpha + 127u) / 255u);
		}

		/// @brief 反预乘 Alpha
		/// @note 结果夹到 255：若 color > alpha（不是合法的预乘值），反预乘会超过 255，
		///       旧实现在这里发生 U8 回绕（如 508 → 252）
		[[nodiscard]] constexpr Color32 UnPreMulAlpha() const noexcept
		{
			return Color32(UnPreMulAlpha(R, A), UnPreMulAlpha(G, A), UnPreMulAlpha(B, A), A);
		}

		/// @brief 单通道反预乘 Alpha
		/// @return color * 255 / alpha（四舍五入），并夹到 255
		[[nodiscard]] static constexpr U8 UnPreMulAlpha(U8 color, U8 alpha) noexcept
		{
			if (alpha == 0)
				return 0;

			if (alpha == 255)
				return color;

			// 括号必须把整个分子括住：旧实现写成 `... + (alpha >> 1) / alpha + ...`，
			// 移位先被 alpha 除掉，恒为 0
			const U32 value = (static_cast<U32>(color) * 255u + (alpha >> 1)) / alpha;

			return static_cast<U8>(value > 255u ? 255u : value);
		}

		/// @brief 与另一颜色线性插值
		/// @param to 终点颜色
		/// @param t 插值参数，夹到 [0, 1]
		/// @return 在 0~255 域内插值并四舍五入的结果
		/// @note 旧实现的签名是 `Lerp(U8 target, float t)`（只能插一个通道），
		///       且公式写成 `R + (R + target) * t`（应为 `R + (target − R) * t`），此处已修正
		[[nodiscard]] constexpr Color32 Lerp(const Color32& to, float t) const noexcept
		{
			const float clamped = Clamp01(t);

			return Color32(
				LerpChannel(R, to.R, clamped),
				LerpChannel(G, to.G, clamped),
				LerpChannel(B, to.B, clamped),
				LerpChannel(A, to.A, clamped));
		}

		/// @brief 单通道插值辅助：升到 float 计算，四舍五入回 0~255
		/// @pre t 已被夹到 [0, 1]（否则结果可能超出 U8 范围）
		[[nodiscard]] static constexpr U8 LerpChannel(U8 from, U8 to, float t) noexcept
		{
			const float value = static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * t;
			return static_cast<U8>(value + 0.5f);
		}

		[[nodiscard]] constexpr bool operator==(const Color32&) const noexcept = default;
	};

	/// @brief 0.0~1.0 颜色
	///
	/// I1. 布局：4 个 float、标准布局、平凡可复制，sizeof == 16
	/// I2. 构造函数把每个分量夹到 [0, 1]（旧实现只夹上界，负值会漏进对象）
	/// I3. 所有分量语义都是「直通 Alpha（非预乘）」；预乘请用 Color32::PreMulAlpha
	struct ColorF
	{
		float R = 0.0f;
		float G = 0.0f;
		float B = 0.0f;
		float A = 1.0f;

		constexpr ColorF() noexcept = default;
		constexpr ColorF(float r, float g, float b, float a = 1.0f) noexcept
			: R(Clamp(r, 0.0f, 1.0f))
			, G(Clamp(g, 0.0f, 1.0f))
			, B(Clamp(b, 0.0f, 1.0f))
			, A(Clamp(a, 0.0f, 1.0f))
		{
		}

		/// @brief 由命名颜色构造（经调色板换算：分量 / 255）
		constexpr explicit ColorF(Color color) noexcept;

		/// @brief 8 位分量 → [0, 1]
		[[nodiscard]] static constexpr float ToUnit(U8 value) noexcept { return static_cast<float>(value) / 255.0f; }

		/// @brief [0, 1] → 8 位分量（先夹取再四舍五入）
		[[nodiscard]] static constexpr U8 ToByte(float value) noexcept
		{
			return static_cast<U8>(Clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
		}

		/// @brief 打包为 0xAABBGGRR（分量先夹到 [0,1] 并四舍五入）
		[[nodiscard]] constexpr U32 PackU32() const noexcept { return Color32::PackU32(ToByte(R), ToByte(G), ToByte(B), ToByte(A)); }

		/// @brief 把 4 个归一化分量打包为 0xAABBGGRR
		[[nodiscard]] static constexpr U32 PackU32(float r, float g, float b, float a) noexcept
		{
			return Color32::PackU32(ToByte(r), ToByte(g), ToByte(b), ToByte(a));
		}

		/// @brief 与另一颜色线性插值
		/// @param to 终点颜色
		/// @param t 插值参数，夹到 [0, 1]
		/// @note 旧实现的签名是 `Lerp(float target, float t)`（只能插一个通道），
		///       且公式写成 `R + (R + target) * t`，此处已修正为 `R + (to − R) * t`
		[[nodiscard]] constexpr ColorF Lerp(const ColorF& to, float t) const noexcept
		{
			const float clamped = Clamp01(t);

			return ColorF(
				PenEngine::Lerp(R, to.R, clamped),
				PenEngine::Lerp(G, to.G, clamped),
				PenEngine::Lerp(B, to.B, clamped),
				PenEngine::Lerp(A, to.A, clamped));
		}

		/// @brief source-over 混合
		/// @param src 前景色（直通 Alpha）
		/// @return 以 this 为背景（dst）、src 为前景的混合结果：
		///         `src * src.A + this * (1 − src.A)`，Alpha 为 `src.A + this.A * (1 − src.A)`
		/// @note 旧实现的 RGB 与 Alpha 用的是互为相反的权重（RGB 把 this 当 src、Alpha 把参数当 src），
		///       两者自相矛盾；此处统一为「this = dst、参数 = src」
		[[nodiscard]] constexpr ColorF Blend(const ColorF& src) const noexcept
		{
			const float inverse = 1.0f - src.A;

			return ColorF(
				src.R * src.A + R * inverse,
				src.G * src.A + G * inverse,
				src.B * src.A + B * inverse,
				src.A + A * inverse);
		}

		[[nodiscard]] constexpr bool operator==(const ColorF&) const noexcept = default;
	};

	namespace Internal
	{
		/// @brief 调色板表的类型
		using ColorPaletteTable = std::array<Color32, static_cast<Usize>(Color::Count)>;

		/// @brief 命名颜色的唯一数据源（索引 == Color 的值，见 Color 的 I1）
		inline constexpr ColorPaletteTable ColorPalette32 = { {
			Color32(0, 0, 0, 255),       // Black
			Color32(255, 255, 255, 255), // White
			Color32(128, 128, 128, 255), // Gray
			Color32(192, 192, 192, 255), // Silver
			Color32(255, 0, 0, 255),     // Red
			Color32(128, 0, 0, 255),     // Maroon
			Color32(255, 192, 203, 255), // Pink
			Color32(220, 20, 60, 255),   // Crimson
			Color32(255, 127, 80, 255),  // Coral
			Color32(0, 0, 255, 255),     // Blue
			Color32(0, 0, 128, 255),     // Navy
			Color32(0, 255, 255, 255),   // Cyan
			Color32(0, 128, 128, 255),   // Teal
			Color32(135, 206, 235, 255), // SkyBlue
			Color32(0, 128, 0, 255),     // Green
			Color32(0, 255, 0, 255),     // Lime
			Color32(128, 128, 0, 255),   // Olive
			Color32(80, 200, 120, 255),  // Emerald
			Color32(34, 139, 34, 255),   // Forest
			Color32(255, 255, 0, 255),   // Yellow
			Color32(255, 165, 0, 255),   // Orange
			Color32(255, 215, 0, 255),   // Gold
			Color32(255, 191, 0, 255),   // Amber
			Color32(128, 0, 128, 255),   // Purple
			Color32(143, 0, 255, 255),   // Violet
			Color32(255, 0, 255, 255),   // Magenta
			Color32(230, 230, 250, 255), // Lavender
			Color32(165, 42, 42, 255),   // Brown
			Color32(245, 245, 220, 255), // Beige
			Color32(210, 180, 140, 255), // Tan
			Color32(210, 105, 30, 255),  // Chocolate
			Color32(0, 0, 0, 0),         // Transparent
		} };
	}

	constexpr Color32::Color32(Color color) noexcept
		: Color32(Internal::ColorPalette32[static_cast<Usize>(color)])
	{
	}

	constexpr ColorF::ColorF(Color color) noexcept
		: ColorF(
			ToUnit(Internal::ColorPalette32[static_cast<Usize>(color)].R),
			ToUnit(Internal::ColorPalette32[static_cast<Usize>(color)].G),
			ToUnit(Internal::ColorPalette32[static_cast<Usize>(color)].B),
			ToUnit(Internal::ColorPalette32[static_cast<Usize>(color)].A))
	{
	}

	// 布局不变量（Color32 的 I1 / ColorF 的 I1）
	static_assert(std::is_standard_layout_v<Color32> && std::is_trivially_copyable_v<Color32>);
	static_assert(sizeof(Color32) == 4 * sizeof(U8) && alignof(Color32) == alignof(U8));
	static_assert(std::is_standard_layout_v<ColorF> && std::is_trivially_copyable_v<ColorF>);
	static_assert(sizeof(ColorF) == 4 * sizeof(float) && alignof(ColorF) == alignof(float));

	// 调色板表与枚举的对应关系抽查（表的索引是 Color 的值，错位在这里就会失败）
	static_assert(Internal::ColorPalette32[static_cast<Usize>(Color::Black)] == Color32(0, 0, 0, 255));
	static_assert(Internal::ColorPalette32[static_cast<Usize>(Color::Red)] == Color32(255, 0, 0, 255));
	static_assert(Internal::ColorPalette32[static_cast<Usize>(Color::SkyBlue)] == Color32(135, 206, 235, 255));
	static_assert(Internal::ColorPalette32[static_cast<Usize>(Color::Transparent)] == Color32(0, 0, 0, 0));
}
