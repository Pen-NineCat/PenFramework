// File /Native/Engine/Core/ConfigSource.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../String/String.hpp"

namespace PenEngine
{
	/// @brief 配置取值的来源层，按优先级从高到低排列
	/// @note 顺序即优先级：命令行覆盖配置文件，配置文件覆盖默认值
	enum class ConfigLayer : U8
	{
		/// @brief 命令行参数（最高）
		CommandLine = 0,
		/// @brief 配置文件 `ApplicationConfiguration.json`
		File = 1,
		/// @brief 代码内的默认值（最低，由调用点显式给出）
		Default = 2,
	};

	/// @brief 层名，用于日志与诊断
	[[nodiscard]] constexpr StringView ConfigLayerName(ConfigLayer layer) noexcept
	{
		switch (layer)
		{
			case ConfigLayer::CommandLine:
				return "命令行";
			case ConfigLayer::File:
				return "配置文件";
			case ConfigLayer::Default:
				return "默认值";
		}
		return "未知";
	}
}
