// File /Native/Engine/Utils/CommandLineParser.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "Internal/CommandLineArgument.hpp"
#include <expected>
#include <vector>

namespace PenEngine
{
	class CommandLineParser;

	namespace Internal
	{
		/// @brief 把命令行取值原文转换为算术类型 `T`
		/// @return 成功返回值；失败返回携带原因的错误（无异常）
		/// @note 定义在 .cpp，只由本头的 `Get<T>` 转发调用 —— 这样算术转换只有一份实现
		template <typename T>
		[[nodiscard]] std::expected<T, CommandLineError> ParseCommandLineArithmetic(StringView raw, StringView argumentName);
	}

	/// @brief 解析结果：一次解析的只读视图
	/// @note I1. 生命周期：本对象**不**持有数据，只引用其 `CommandLineParser`。
	///       必须保证解析器活得更久（典型用法：解析器与结果同在 `main` 的作用域内）。
	///       I2. 取值语义（`Has` / `IsProvided` / `Get` 三者的分工）：
	///       - `Has(name)`：是否**可取值**。显式提供、有缺省值、开关（默认假）、计数（默认 0）都为真；
	///       - `IsProvided(name)`：值是否**来自命令行**（缺省填充时为 false）；
	///       - `Get<T>()`：取最后一个值并转换；既未提供又无缺省 → `MissingRequired`。
	///       I3. 多次赋值（`Append` / `Count` / 重复出现的 `Store`）统一保留**全部**取值，
	///       由 `Get<T>()`（取最后一个）与 `All()`（取全部）分别暴露。
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
	/// @note I1. **声明期与解析期严格分离**：所有 `Add*` 必须在第一次 `Parse` 之前调用完；
	///       `Parse` 之后再 `Add*` 会抛异常。解析结果一经产生便不再变化。
	///       I2. 选项名归一化：`--input-file` → 键 `input_file`；短名 `-i` → 键 `i`。
	///       命名冲突（含与 `-h`/`--help`）在 `Add*` 时即抛异常，而不是留到解析期。
	///       I3. `-abc` 形式的合并短选项仅在**每个**字符都已声明为短名、且除最后一个外
	///       都不取值时才展开；否则整个 token 视作未声明的长名并报错。
	///       I4. `--` 之后不再识别任何选项，全部按位置参数处理。
	///       I5. 失败一律通过返回值（`std::expected`）报告，**不抛异常** —— 命令行是用户输入，
	///       不是内部错误；只有 `Add*` 的误用（编程错误）才抛。
	///       I6. 生命周期纪律：`CommandLineParseResult` 内部持有 `this` 指针，因此
	///       **解析之后不要再移动解析器**（解析之前随便移，成员都是值语义的 vector/String，
	///       且登记的参数下标与 `m_arguments` 同步搬迁）。典型用法是工厂函数按值返回解析器，
	///       再在同一个作用域内 `Parse` 并消费结果。
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

		/// @brief 声明一个选项（`--名字`）
		/// @param name 长名（不含 `--`）
		/// @param aliases 额外长名（不含 `--`）
		CommandLineParser& AddOption(StringView name, std::initializer_list<StringView> aliases = {});
		/// @brief 声明一个开关，出现即为真（`store_true`）
		CommandLineParser& AddFlag(StringView name, std::initializer_list<StringView> aliases = {});
		/// @brief 声明一个计数开关（`count`），每出现一次 +1
		CommandLineParser& AddCount(StringView name, std::initializer_list<StringView> aliases = {});
		/// @brief 声明一个可重复取值的选项（`append`）
		CommandLineParser& AddAppendOption(StringView name, std::initializer_list<StringView> aliases = {});
		/// @brief 声明一个位置参数
		/// @param minCount 最少取值个数，0 表示可缺省
		/// @param maxCount 最多取值个数，`Usize(-1)` 表示不限
		CommandLineParser& AddPositional(StringView name, Usize minCount = 1, Usize maxCount = 1);

		/// @brief 声明短名（可多个），作用于**最近一次** `Add*` 声明的参数
		CommandLineParser& ShortNames(std::initializer_list<StringView> aliases);

		/// @brief 设置必填（作用于最近一次声明的参数）
		CommandLineParser& Required(bool required = true);
		/// @brief 设置缺省值原文（作用于最近一次声明的参数）
		CommandLineParser& Default(StringView defaultValue);
		/// @brief 限定取值集合（作用于最近一次声明的参数）
		CommandLineParser& Choices(std::initializer_list<StringView> choices);
		/// @brief 设置帮助说明（作用于最近一次声明的参数）
		CommandLineParser& Help(StringView help);
		/// @brief 设置帮助中的取值占位名（作用于最近一次声明的参数）
		CommandLineParser& Metavar(StringView metavar);
		/// @brief 设置取值转换器（作用于最近一次声明的参数）
		/// @note 转换器只承担「解析期早失败」；`Get<T>` 仍会独立转换一次，两者行为一致
		CommandLineParser& ValueParser(std::function<bool(const StringView&, String&)> parser);
		/// @brief 按内置规则声明转换器（`"Int"` / `"UInt"` / `"Float"` / `"Bool"`）
		CommandLineParser& Type(StringView typeName);

		/// @brief 按 argv 形态解析；`argumentVector[0]` 视作程序名
		/// @param argumentVector 形如 `argv`：第 0 项是程序名/路径，其余是参数。
		///       接收 `const std::vector&` 而非 `span`：后者对临时 vector 会立刻悬垂
		///       （`parser.Parse(BuildArgs())` 这种写法会静默读到已析构内存）
		/// @return 成功时给出结果视图；失败时给出 `CommandLineError`
		[[nodiscard]] std::expected<CommandLineParseResult, CommandLineError> Parse(const std::vector<String>& argumentVector);

		/// @brief 取完整帮助文本（对应 argparse 的 `format_help()`）
		[[nodiscard]] String HelpText() const;
		/// @brief 取用法行（`usage: ...`）
		[[nodiscard]] String UsageText() const;
		/// @brief 取错误文本（`error: ...`，含用法行）
		[[nodiscard]] String ErrorText(const CommandLineError& error) const;

		/// @brief 本解析器是否已经解析过
		[[nodiscard]] bool Parsed() const noexcept { return m_parsed; }
		/// @brief 未到达 `--` 之前的选项前缀字符，当前固定为 `'-'`
		[[nodiscard]] char PrefixChar() const noexcept { return m_prefixChar; }

	private:
		// 解析结果需要读取参数声明（名字归一化、缺省值、动作类别）与定位函数，
		// 但不修改解析器状态 —— 无 setter 语义，只开读取入口
		friend class CommandLineParseResult;

		/// @brief `--input-file` → `input_file`、`-i` → `i`；位置参数原样保留
		[[nodiscard]] String NormalizeName(StringView rawName, bool optional) const;
		/// @brief 归一化**查询**名字：带前缀的选项名走 `NormalizeName`，其余原样返回
		/// @note 声明期与查询期必须用同一套归一化，否则 `--input-file` 查不到 `input_file`
		[[nodiscard]] String LookupNormalize(StringView name) const;
		/// @brief 校验并登记一个名字，冲突时抛异常
		void RegisterName(Usize argumentIndex, StringView rawName, bool optional);
		/// @brief 解析期与声明期共用的定位函数；未声明返回 `Usize(-1)`
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
		/// @brief 判定 token 是否是一个选项（以 `PrefixChar` 开头且长度 > 1）
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

	// 显式特化声明：定义在 .cpp，必须在使用点之前可见
	template <> [[nodiscard]] std::expected<String, CommandLineError> CommandLineParseResult::Get<String>(StringView name) const;
	template <> [[nodiscard]] std::expected<bool, CommandLineError> CommandLineParseResult::Get<bool>(StringView name) const;

	template <typename T>
	std::expected<T, CommandLineError> CommandLineParseResult::Get(StringView name) const
	{
		static_assert(std::is_arithmetic_v<T>,
			"CommandLineParseResult::Get<T> 仅支持算术类型；String 与 bool 由显式特化提供");

		auto raw = ResolveRawValue(name);
		if (!raw.has_value())
			return std::unexpected(std::move(raw.error()));

		String argumentName(name);
		return Internal::ParseCommandLineArithmetic<T>(*raw, argumentName);
	}
}
