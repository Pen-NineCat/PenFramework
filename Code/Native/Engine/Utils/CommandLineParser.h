// File /Code/Native/Engine/Utils/CommandLineParser.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include <expected>
#include <functional>

#include "../String/String.hpp"

namespace PenEngine
{
	class CommandLineParser;

	/// @brief 参数的行为类别，对齐 Python argparse 的 action
	enum class CommandLineAction : U8
	{
		/// @brief 取值一次，重复出现以最后一次为准（argparse 默认行为）
		Store,
		/// @brief 不取值，出现即为真（action="store_true"）
		StoreTrue,
		/// @brief 出现次数累加（action="count"）
		Count,
		/// @brief 每次取值都追加（action="append"）
		Append,
	};

	/// @brief 一个参数的声明
	struct CommandLineArgument
	{
		/// @brief 主名 + 同义长名；选项（Optional == true）时全部渲染为 --名字
		std::vector<String> Names;
		/// @brief 短名，渲染为 -x；仅对选项有意义
		std::vector<String> Aliases;

		/// @brief true = 选项（以 PrefixChars 之一开头）；false = 位置参数
		bool Optional = false;
		/// @brief 是否需要取值；StoreTrue / Count 必须为 false
		bool TakesValue = true;
		CommandLineAction Action = CommandLineAction::Store;

		/// @brief 是否必填；位置参数缺省时按 DefaultValue 填充
		bool Required = false;

		/// @brief 位置参数的取值个数区间；仅 Optional == false 时有意义
		Usize MinCount = 1;
		/// @brief 上限；Usize(-1) 表示不限
		Usize MaxCount = 1;

		/// @brief 取值转换器（from_chars 家族）；为空表示不校验
		/// @note 它只承担「解析期早失败」；ParseResult::Get<T> 仍会独立转换一次，
		///       因此两种入口（有/无转换器）的取值行为一致
		std::function<bool(const StringView&, String&)> ValueParser;
		/// @brief 限定取值集合；空表示不限
		std::vector<String> Choices;

		/// @brief 缺省值原文（未经类型转换）；std::nullopt 表示无缺省
		std::optional<String> DefaultValue;

		/// @brief --help 中的说明
		String Help;
		/// @brief 取值在帮助里的占位名，如 FILE
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
		/// @brief 取值不在 Choices 内
		NotInChoices,
		/// @brief 位置参数超出声明的个数上限
		TooManyValues,
		/// @brief 必填参数缺失
		MissingRequired,
		/// @brief 没有可接收的位置参数却出现了位置 token
		UnexpectedPositional,
		/// @brief 同一个解析器被解析两次
		AlreadyParsed,
		/// @brief 用户显式请求帮助（-h / --help）；调用方应打印 HelpText() 并以成功码退出
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

	/// @brief 解析结果：一次解析的只读视图
	/// @note 本对象不持有数据。
	class CommandLineParseResult
	{
	public:
		/// @note 由 `CommandLineParser::Parse` 构造，调用方不需要直接使用
		/// @param provided 与 `values` 等长：标记该下标的值是「用户显式提供」还是「缺省填充」
		CommandLineParseResult(const CommandLineParser& parser,
			String programName,
			std::vector<std::optional<std::vector<String>>> values,
			std::vector<bool> provided);

		/// @brief 平台给出的程序名/路径（`argv[0]` 或等价物）
		[[nodiscard]] StringView ProgramName() const noexcept;

		/// @brief 该参数是否可用：显式提供过，或声明了缺省值（开关为假、计数为 0 也算有值）
		[[nodiscard]] bool Has(StringView name) const;

		/// @brief 取值是否来自命令行（false 表示取的是缺省值/默认假，或该参数根本未声明）
		[[nodiscard]] bool IsProvided(StringView name) const;

		/// @brief 该参数收到的取值个数（`Count` 动作下即出现次数）
		[[nodiscard]] Usize Count(StringView name) const;

		/// @brief 取全部取值；未提供时为空
		[[nodiscard]] std::vector<StringView> All(StringView name) const;

		/// @brief 取单个取值（多次出现时取最后一个）并转换为 `T`
		/// @tparam T 支持：`String`、`bool`、以及全部算术类型
		/// @return 转换成功返回值；失败返回 `CommandLineError`（含参数名与原因）
		template <typename T>
		[[nodiscard]] std::expected<T, CommandLineError> Get(StringView name) const;

	private:
		/// @brief 按名字定位参数下标；未声明返回 `Usize(-1)`
		[[nodiscard]] Usize IndexOf(StringView name) const;

		/// @brief 取出用于 `Get<T>` 的取值原文：显式提供优先，其次缺省值，最后开关/计数的默认值
		/// @return 按值返回 —— 计数为 0 这类取值是**临时构造**的，返回 view 会立刻悬垂
		[[nodiscard]] std::expected<String, CommandLineError> ResolveRawValue(StringView name) const;

		const CommandLineParser* m_parser = nullptr;
		String m_programName;
		std::vector<std::optional<std::vector<String>>> m_values;
		/// @brief 与 `m_values` 等长；true 表示值来自命令行，false 表示来自缺省填充
		std::vector<bool> m_provided;
	};

	/// @brief 命令行解析器：Python argparse 风格的声明式规则 + 执行器
	class CommandLineParser
	{
	public:
		/// @param programName 出现在错误信息与帮助首行的程序名（通常是短名，不是完整路径）
		/// @param description 出现在帮助首行之后的描述；可为空
		explicit CommandLineParser(StringView programName, StringView description = {});
		~CommandLineParser() noexcept = default;

		CommandLineParser(const CommandLineParser&) = delete;
		CommandLineParser(CommandLineParser&&) = default;
		CommandLineParser& operator=(const CommandLineParser&) = delete;
		CommandLineParser& operator=(CommandLineParser&&) = default;

		/// @brief 声明一个选项（--名字）
		/// @param name 长名（不含 --）
		/// @param aliases 额外长名（不含 --）
		CommandLineParser& AddOption(StringView name, std::initializer_list<StringView> aliases = {});
		/// @brief 声明一个开关，出现即为真（store_true）
		CommandLineParser& AddFlag(StringView name, std::initializer_list<StringView> aliases = {});
		/// @brief 声明一个计数开关（count），每出现一次 +1
		CommandLineParser& AddCount(StringView name, std::initializer_list<StringView> aliases = {});
		/// @brief 声明一个可重复取值的选项（append）
		CommandLineParser& AddAppendOption(StringView name, std::initializer_list<StringView> aliases = {});
		/// @brief 声明一个位置参数
		/// @param name 长名（不含 --）
		/// @param minCount 最少取值个数，0 表示可缺省
		/// @param maxCount 最多取值个数，Usize(-1) 表示不限
		CommandLineParser& AddPositional(StringView name, Usize minCount = 1, Usize maxCount = 1);

		/// @brief 声明短名（可多个），作用于**最近一次** Add* 声明的参数
		CommandLineParser& ShortNames(std::initializer_list<StringView> aliases);

		/// @brief 设置必填（作用于最近一次声明的参数）
		CommandLineParser& Required(bool required = true);
		/// @brief 设置缺省值原文（作用于最近一次声明的参数）
		CommandLineParser& Default(StringView defaultValue);
		/// @brief 限定取值集合（作用于最近一次声明的参数）
		/// @note 选项与**位置参数**都生效（I7）；位置参数一次收到多个 token 时逐个校验
		CommandLineParser& Choices(std::initializer_list<StringView> choices);
		/// @brief 设置帮助说明（作用于最近一次声明的参数）
		CommandLineParser& Help(StringView help);
		/// @brief 设置帮助中的取值占位名（作用于最近一次声明的参数）
		CommandLineParser& Metavar(StringView metavar);
		/// @brief 设置取值转换器（作用于最近一次声明的参数）
		/// @note 转换器只承担「解析期早失败」；Get<T> 仍会独立转换一次，两者行为一致
		/// @note 选项与**位置参数**都生效（I7）
		CommandLineParser& ValueParser(std::function<bool(const StringView&, String&)> parser);
		/// @brief 按内置规则声明转换器（"Int" / "UInt" / "Float" / "Bool"）
		/// @note 选项与**位置参数**都生效（I7）
		CommandLineParser& Type(StringView typeName);

		/// @brief 按 argv 形态解析；argumentVector[0] 视作程序名
		/// @param argumentVector 形如 argv：第 0 项是程序名/路径，其余是参数。
		///       接收 const std::vector& 而非 span：后者对临时 vector 会立刻悬垂
		///       （parser.Parse(BuildArgs()) 这种写法会静默读到已析构内存）
		/// @return 成功时给出结果视图；失败时给出 CommandLineError
		[[nodiscard]] std::expected<CommandLineParseResult, CommandLineError> Parse(const std::vector<String>& argumentVector);

		/// @brief 取完整帮助文本（对应 argparse 的 format_help()）
		[[nodiscard]] String HelpText() const;
		/// @brief 取用法行（usage: ...）
		[[nodiscard]] String UsageText() const;
		/// @brief 取错误文本（error: ...，含用法行）
		[[nodiscard]] String ErrorText(const CommandLineError& error) const;

		/// @brief 本解析器是否已经解析过
		[[nodiscard]] bool Parsed() const noexcept { return m_parsed; }
		/// @brief 未到达 -- 之前的选项前缀字符，当前固定为 '-'
		[[nodiscard]] char PrefixChar() const noexcept { return m_prefixChar; }

	private:
		// 解析结果需要读取参数声明（名字归一化、缺省值、动作类别）与定位函数，
		// 但不修改解析器状态 —— 无 setter 语义，只开读取入口
		friend class CommandLineParseResult;

		/// @brief --input-file → input_file、-i → i；位置参数原样保留
		[[nodiscard]] String NormalizeName(StringView rawName, bool optional) const;
		/// @brief 归一化**查询**名字：带前缀的选项名走 NormalizeName，其余原样返回
		/// @note 声明期与查询期必须用同一套归一化，否则 --input-file 查不到 input_file
		[[nodiscard]] String LookupNormalize(StringView name) const;
		/// @brief 校验并登记一个名字，冲突时抛异常
		void RegisterName(Usize argumentIndex, StringView rawName, bool optional);
		/// @brief 解析期与声明期共用的定位函数；未声明返回 Usize(-1)
		[[nodiscard]] Usize IndexOf(StringView name) const;
		/// @brief 检查此时是否允许继续声明
		void VerifyDeclarable() const;
		/// @brief 校验动作与参数组合是否自洽
		void VerifyArgument(const CommandLineArgument& argument) const;
		/// @brief 取最近一次声明的参数；没有则抛异常
		[[nodiscard]] CommandLineArgument& LastDeclared();
		/// @brief 渲染帮助中的取值占位
		[[nodiscard]] String RenderMetavar(const CommandLineArgument& argument) const;
		/// @brief 渲染帮助中的声明列
		[[nodiscard]] String RenderInvocation(const CommandLineArgument& argument) const;
		/// @brief 判定 token 是否是一个选项（以 PrefixChar 开头且长度 > 1）
		[[nodiscard]] bool LooksLikeOption(StringView token) const noexcept;

		std::vector<CommandLineArgument> m_arguments;
		std::vector<String> m_canonicalNames;
		std::vector<Usize> m_nameToIndex;
		std::vector<Usize> m_positionalIndices;
		Usize m_lastDeclared = static_cast<Usize>(-1);

		String m_programName;
		String m_description;
		char m_prefixChar = '-';
		bool m_parsed = false;
	};
}
