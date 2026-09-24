// File /Native/Engine/Utils/Internal/CommandLineArgument.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../../String/String.hpp"
#include <functional>
#include <optional>
#include <vector>

namespace PenEngine
{
	/// @brief 参数的行为类别，对齐 Python argparse 的 `action`
	enum class CommandLineAction : U8
	{
		/// @brief 取值一次，重复出现以最后一次为准（argparse 默认行为）
		Store,
		/// @brief 不取值，出现即为真（`action="store_true"`）
		StoreTrue,
		/// @brief 出现次数累加（`action="count"`）
		Count,
		/// @brief 每次取值都追加（`action="append"`）
		Append,
	};

	/// @brief 一个参数的声明
	/// @note I1. 命名规则对齐 Python argparse：
	///       `Names` / `Aliases` 中都**不含**前缀字符（`-` / `--`）；
	///       短名为单字符（`i` 对应 `-i`），长名可多字符（`input` 对应 `--input`）；
	///       `Names` 的第 0 项是「主名」，也是取值时使用的键。
	///       I2. `Required == false` 的**位置参数**语义是"可缺省"，而非"可跳过"：
	///       缺省时按 `DefaultValue` 填充，出现时必须占据当前位置。
	struct CommandLineArgument
	{
		/// @brief 主名 + 同义长名；选项（`Optional == true`）时全部渲染为 `--名字`
		std::vector<String> Names;
		/// @brief 短名，渲染为 `-x`；仅对选项有意义
		std::vector<String> Aliases;

		/// @brief true = 选项（以 `PrefixChars` 之一开头）；false = 位置参数
		bool Optional = false;
		/// @brief 是否需要取值；`StoreTrue` / `Count` 必须为 false
		bool TakesValue = true;
		CommandLineAction Action = CommandLineAction::Store;

		/// @brief 是否必填；位置参数缺省时按 `DefaultValue` 填充
		bool Required = false;

		/// @brief 位置参数的取值个数区间；仅 `Optional == false` 时有意义
		Usize MinCount = 1;
		/// @brief 上限；`Usize(-1)` 表示不限
		Usize MaxCount = 1;

		/// @brief 取值转换器（`from_chars` 家族）；为空表示不校验
		/// @note 它只承担「解析期早失败」；`ParseResult::Get<T>` 仍会独立转换一次，
		///       因此两种入口（有/无转换器）的取值行为一致
		std::function<bool(const StringView&, String&)> ValueParser;
		/// @brief 限定取值集合；空表示不限
		std::vector<String> Choices;

		/// @brief 缺省值原文（未经类型转换）；`std::nullopt` 表示无缺省
		std::optional<String> DefaultValue;

		/// @brief `--help` 中的说明
		String Help;
		/// @brief 取值在帮助里的占位名，如 `FILE`
		String Metavar;
	};

	/// @brief 解析失败的分类
	enum class CommandLineErrorCode : U8
	{
		None = 0,
		/// @brief 出现了未声明的选项
		UnknownOption,
		/// @brief 选项要求取值但下一个 token 不是值或已耗尽
		MissingValue,
		/// @brief 取值未通过声明的转换器
		InvalidValue,
		/// @brief 取值不在 `Choices` 内
		NotInChoices,
		/// @brief 位置参数超出声明的个数上限
		TooManyValues,
		/// @brief 必填参数缺失
		MissingRequired,
		/// @brief 没有可接收的位置参数却出现了位置 token
		UnexpectedPositional,
		/// @brief 同一个解析器被解析两次
		AlreadyParsed,
		/// @brief 用户显式请求帮助（`-h` / `--help`）；调用方应打印 `HelpText()` 并以成功码退出
		HelpRequested,
	};

	/// @brief 解析失败的详情
	struct CommandLineError
	{
		CommandLineErrorCode Code = CommandLineErrorCode::None;
		/// @brief 触发失败的原始 token（选项名或取值）
		String Token;
		/// @brief 涉及的参数名
		String ArgumentName;
		/// @brief 面向用户的一句话说明
		String Message;
	};
}
