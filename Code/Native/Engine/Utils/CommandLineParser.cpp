// File /Native/Engine/Utils/CommandLineParser.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "CommandLineParser.hpp"

#include "../Exception/Exception.hpp"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <limits>

namespace PenEngine
{
	namespace
	{
		/// @brief 内置转换器名，用于 Type()
		constexpr StringView BuiltinTypeInt = "Int";
		constexpr StringView BuiltinTypeUInt = "UInt";
		constexpr StringView BuiltinTypeFloat = "Float";
		constexpr StringView BuiltinTypeBool = "Bool";

		/// @brief 去掉首尾空白，供所有取值转换共用
		[[nodiscard]] StringView TrimView(StringView raw) noexcept
		{
			Usize begin = 0;
			Usize end = raw.Size();

			while (begin < end)
			{
				const char ch = raw.Data()[begin];
				if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
					break;
				++begin;
			}

			while (end > begin)
			{
				const char ch = raw.Data()[end - 1];
				if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
					break;
				--end;
			}

			return raw.Subview(begin, end - begin);
		}

		/// @brief 构造一个解析错误
		[[nodiscard]] CommandLineError MakeError(CommandLineErrorCode code, StringView argumentName,
			StringView token, String message)
		{
			CommandLineError error;
			error.Code = code;
			error.ArgumentName = String(argumentName);
			error.Token = String(token);
			error.Message = std::move(message);
			return error;
		}

		/// @brief 算术转换的实现体；失败返回携带原因的 error
		template <typename T>
		[[nodiscard]] std::expected<T, CommandLineError> ConvertArithmetic(StringView raw, StringView argumentName)
		{
			const StringView trimmed = TrimView(raw);

			if constexpr (std::is_floating_point_v<T>)
			{
				T value{};
				const char* begin = trimmed.Data();
				const char* end = begin + trimmed.Size();
				const auto [position, code] = std::from_chars(begin, end, value);
				if (code != std::errc() || position != end)
					return std::unexpected(MakeError(CommandLineErrorCode::InvalidValue, argumentName, trimmed,
						"无法解析为浮点数：" + String(trimmed)));
				return value;
			}
			else if constexpr (std::is_unsigned_v<T>)
			{
				// from_chars 对无符号类型会把 "-1" 静默回绕，因此先按有符号解析再判范围
				I64 signedValue = 0;
				const char* begin = trimmed.Data();
				const char* end = begin + trimmed.Size();
				const auto [position, code] = std::from_chars(begin, end, signedValue);
				if (code != std::errc() || position != end || signedValue < 0)
					return std::unexpected(MakeError(CommandLineErrorCode::InvalidValue, argumentName, trimmed,
						"无法解析为非负整数：" + String(trimmed)));

				if (static_cast<U64>(signedValue) > static_cast<U64>(std::numeric_limits<T>::max()))
					return std::unexpected(MakeError(CommandLineErrorCode::InvalidValue, argumentName, trimmed,
						"取值超出范围：" + String(trimmed)));
				return static_cast<T>(signedValue);
			}
			else
			{
				T value{};
				const char* begin = trimmed.Data();
				const char* end = begin + trimmed.Size();
				const auto [position, code] = std::from_chars(begin, end, value);
				if (code != std::errc() || position != end)
					return std::unexpected(MakeError(CommandLineErrorCode::InvalidValue, argumentName, trimmed,
						"无法解析为整数：" + String(trimmed)));
				return value;
			}
		}

		/// @brief "true"/"false"/"1"/"0"/"yes"/"no"/"on"/"off"（不区分大小写）→ bool
		[[nodiscard]] bool TryParseBool(StringView raw, bool& result) noexcept
		{
			const StringView trimmed = TrimView(raw);

			auto equalsIgnoreCase = [&trimmed](StringView candidate) noexcept
			{
				if (trimmed.Size() != candidate.Size())
					return false;
				for (Usize i = 0; i < trimmed.Size(); ++i)
				{
					char lhs = trimmed.Data()[i];
					char rhs = candidate.Data()[i];
					if (lhs >= 'A' && lhs <= 'Z')
						lhs = static_cast<char>(lhs - 'A' + 'a');
					if (rhs >= 'A' && rhs <= 'Z')
						rhs = static_cast<char>(rhs - 'A' + 'a');
					if (lhs != rhs)
						return false;
				}
				return true;
			};

			if (equalsIgnoreCase("true") || equalsIgnoreCase("1") || equalsIgnoreCase("yes") || equalsIgnoreCase("on"))
			{
				result = true;
				return true;
			}

			if (equalsIgnoreCase("false") || equalsIgnoreCase("0") || equalsIgnoreCase("no") || equalsIgnoreCase("off"))
			{
				result = false;
				return true;
			}

			return false;
		}

		/// @brief 把计数值转为字符串
		[[nodiscard]] String MakeCountText(Usize count)
		{
			String text;
			text.ConvertAndPushBack(count);
			return text;
		}
	}

	namespace Internal
	{
		template <typename T>
		std::expected<T, CommandLineError> ParseCommandLineArithmetic(StringView raw, StringView argumentName)
		{
			return ConvertArithmetic<T>(raw, argumentName);
		}

		// 显式实例化：实现留在本翻译单元，头文件只做转发声明
		template std::expected<I8, CommandLineError> ParseCommandLineArithmetic<I8>(StringView, StringView);
		template std::expected<I16, CommandLineError> ParseCommandLineArithmetic<I16>(StringView, StringView);
		template std::expected<I32, CommandLineError> ParseCommandLineArithmetic<I32>(StringView, StringView);
		template std::expected<I64, CommandLineError> ParseCommandLineArithmetic<I64>(StringView, StringView);
		template std::expected<U8, CommandLineError> ParseCommandLineArithmetic<U8>(StringView, StringView);
		template std::expected<U16, CommandLineError> ParseCommandLineArithmetic<U16>(StringView, StringView);
		template std::expected<U32, CommandLineError> ParseCommandLineArithmetic<U32>(StringView, StringView);
		template std::expected<U64, CommandLineError> ParseCommandLineArithmetic<U64>(StringView, StringView);
		template std::expected<float, CommandLineError> ParseCommandLineArithmetic<float>(StringView, StringView);
		template std::expected<double, CommandLineError> ParseCommandLineArithmetic<double>(StringView, StringView);
		template std::expected<long double, CommandLineError> ParseCommandLineArithmetic<long double>(StringView, StringView);
	}

	// ================================================================
	// CommandLineParseResult
	// ================================================================

	CommandLineParseResult::CommandLineParseResult(const CommandLineParser& parser,
		String programName,
		std::vector<std::optional<std::vector<String>>> values,
		std::vector<bool> provided)
		: m_parser(&parser), m_programName(std::move(programName)),
		  m_values(std::move(values)), m_provided(std::move(provided))
	{}

	Usize CommandLineParseResult::IndexOf(StringView name) const
	{
		return m_parser->IndexOf(name);
	}

	StringView CommandLineParseResult::ProgramName() const noexcept
	{
		return m_programName;
	}

	bool CommandLineParseResult::Has(StringView name) const
	{
		const Usize index = IndexOf(name);
		if (index == static_cast<Usize>(-1))
			return false;

		// 与 Get<T>() 能取到值保持一致：显式提供、有缺省值、
		// 以及恒有默认值的开关（假）与计数（0）都算「有值」
		if (m_values[index].has_value() || m_parser->m_arguments[index].DefaultValue.has_value())
			return true;

		const CommandLineAction action = m_parser->m_arguments[index].Action;
		return action == CommandLineAction::StoreTrue || action == CommandLineAction::Count;
	}

	bool CommandLineParseResult::IsProvided(StringView name) const
	{
		const Usize index = IndexOf(name);
		return index != static_cast<Usize>(-1) && index < m_provided.size() && m_provided[index];
	}

	Usize CommandLineParseResult::Count(StringView name) const
	{
		const Usize index = IndexOf(name);
		if (index == static_cast<Usize>(-1) || !m_values[index].has_value())
			return 0;
		return m_values[index]->size();
	}

	std::vector<StringView> CommandLineParseResult::All(StringView name) const
	{
		std::vector<StringView> values;

		const Usize index = IndexOf(name);
		if (index == static_cast<Usize>(-1) || !m_values[index].has_value())
			return values;

		const auto& stored = *m_values[index];
		values.reserve(stored.size());
		for (const String& value : stored)
			values.push_back(value);
		return values;
	}

	std::expected<String, CommandLineError> CommandLineParseResult::ResolveRawValue(StringView name) const
	{
		const Usize index = IndexOf(name);
		if (index == static_cast<Usize>(-1))
			return std::unexpected(MakeError(CommandLineErrorCode::UnknownOption, name, name,
				"未声明的参数：" + String(name)));

		const CommandLineArgument& argument = m_parser->m_arguments[index];

		if (const auto& slot = m_values[index]; slot.has_value() && !slot->empty())
			return slot->back();

		// 未显式提供：开关默认假、计数默认 0，其余按声明的缺省值
		if (argument.DefaultValue.has_value())
			return *argument.DefaultValue;

		if (argument.Action == CommandLineAction::StoreTrue)
			return String("False");

		if (argument.Action == CommandLineAction::Count)
			return MakeCountText(0);

		return std::unexpected(MakeError(CommandLineErrorCode::MissingRequired, name, name,
			"缺少取值：" + String(name)));
	}

	template <>
	std::expected<String, CommandLineError> CommandLineParseResult::Get<String>(StringView name) const
	{
		return ResolveRawValue(name);
	}

	template <>
	std::expected<bool, CommandLineError> CommandLineParseResult::Get<bool>(StringView name) const
	{
		auto raw = ResolveRawValue(name);
		if (!raw.has_value())
			return std::unexpected(std::move(raw.error()));

		bool value = false;
		if (!TryParseBool(*raw, value))
			return std::unexpected(MakeError(CommandLineErrorCode::InvalidValue, name, *raw,
				"无法解析为布尔值：" + *raw));
		return value;
	}

	// ================================================================
	// CommandLineParser · 声明期
	// ================================================================

	CommandLineParser::CommandLineParser(StringView programName, StringView description)
		: m_programName(programName), m_description(description)
	{
		// 内置开关：与 argparse 一致，任何解析器都自带 -h / --help
		AddFlag("help").ShortNames({ "h" }).Help("显示本帮助并退出");
	}

	String CommandLineParser::NormalizeName(StringView rawName, bool optional) const
	{
		if (!optional)
			return String(rawName);

		// I1. 只剥掉**开头**的前缀字符（`--input-file` → `input-file`、`-i` → `i`），
		//     之后串内的 '-' 一律转成 '_'。
		//     曾经的写法是「遇到前缀字符就 continue」，但 '-' 既是前缀又是 kebab 分隔符，
		//     两个分支都命中 '-'，结果所有连字符被静默删除（`input-file` → `inputfile`），
		//     键名与用户预期不符 —— 名字里的字符不允许被无声丢掉。
		Usize begin = 0;
		while (begin < rawName.Size() && rawName.Data()[begin] == m_prefixChar)
			++begin;

		String normalized;
		for (Usize i = begin; i < rawName.Size(); ++i)
		{
			const char ch = rawName.Data()[i];
			normalized.PushBack(ch == '-' ? '_' : ch);
		}

		return normalized;
	}

	Usize CommandLineParser::IndexOf(StringView name) const
	{
		// 选项查询要过一遍与声明相同的归一化：声明 `input-file` 登记的是 `input_file`，
		// 但 token 里读到的是 `input-file` / `--input-file`，不归一化就永远查不到。
		// 位置参数名不含前缀字符，LookupNormalize 会原样返回，行为不变。
		const String canonical = LookupNormalize(name);

		for (Usize i = 0; i < m_canonicalNames.size(); ++i)
		{
			if (m_canonicalNames[i] == canonical)
				return m_nameToIndex[i];
		}
		return static_cast<Usize>(-1);
	}

	String CommandLineParser::LookupNormalize(StringView name) const
	{
		// 查询可能带前缀（`--input-file`）也可能已经剥掉（解析期传进来的就是裸名 `input-file`），
		// 两种情况都要归一到同一个键：带前缀就先剥掉，再统一做 kebab → snake
		Usize begin = 0;
		while (begin < name.Size() && name.Data()[begin] == m_prefixChar)
			++begin;

		return NormalizeName(name.Subview(begin), true);
	}

	void CommandLineParser::VerifyDeclarable() const
	{
		if (m_parsed)
			ThrowException(Exception("CommandLineParser", "解析之后不允许再声明参数"));
	}

	CommandLineArgument& CommandLineParser::LastDeclared()
	{
		if (m_lastDeclared == static_cast<Usize>(-1))
			ThrowException(Exception("CommandLineParser", "尚未声明任何参数，修饰函数无处作用"));
		return m_arguments[m_lastDeclared];
	}

	void CommandLineParser::VerifyArgument(const CommandLineArgument& argument) const
	{
		if (argument.Action == CommandLineAction::StoreTrue && argument.TakesValue)
			ThrowException(Exception("CommandLineParser", "StoreTrue 不接受取值"));

		if (argument.Action == CommandLineAction::Count && argument.TakesValue)
			ThrowException(Exception("CommandLineParser", "Count 不接受取值"));

		if (!argument.TakesValue && argument.Action == CommandLineAction::Store)
			ThrowException(Exception("CommandLineParser", "TakesValue 为假时动作只能是 StoreTrue / Count"));

		if (!argument.Optional && argument.Action != CommandLineAction::Store)
			ThrowException(Exception("CommandLineParser", "位置参数的动作只能是 Store，请用 MinCount 表达可选性"));

		if (!argument.Optional && argument.MaxCount != static_cast<Usize>(-1) && argument.MinCount > argument.MaxCount)
			ThrowException(Exception("CommandLineParser", "位置参数的 MinCount 不能大于 MaxCount"));

		if (!argument.Optional && argument.Required)
			ThrowException(Exception("CommandLineParser", "位置参数的必填性由 MinCount 表达，不要设置 Required"));

		if (argument.Optional && argument.Names.empty())
			ThrowException(Exception("CommandLineParser", "选项至少需要一个长名"));
	}

	void CommandLineParser::RegisterName(Usize argumentIndex, StringView rawName, bool optional)
	{
		const String canonical = NormalizeName(rawName, optional);
		if (canonical.Empty())
			ThrowException(Exception("CommandLineParser", "参数名不能为空：'" + String(rawName) + "'"));

		if (IndexOf(canonical) != static_cast<Usize>(-1))
			ThrowException(Exception("CommandLineParser", "参数名重复声明：'" + canonical + "'"));

		m_canonicalNames.push_back(canonical);
		m_nameToIndex.push_back(argumentIndex);
	}

	CommandLineParser& CommandLineParser::AddOption(StringView name, std::initializer_list<StringView> aliases)
	{
		VerifyDeclarable();

		CommandLineArgument argument;
		argument.Optional = true;
		argument.TakesValue = true;
		argument.Action = CommandLineAction::Store;
		argument.Names.emplace_back(name);
		for (StringView alias : aliases)
			argument.Names.emplace_back(alias);

		VerifyArgument(argument);

		m_arguments.push_back(std::move(argument));
		m_lastDeclared = m_arguments.size() - 1;
		for (const String& declaredName : m_arguments[m_lastDeclared].Names)
			RegisterName(m_lastDeclared, declaredName, true);
		return *this;
	}

	CommandLineParser& CommandLineParser::AddFlag(StringView name, std::initializer_list<StringView> aliases)
	{
		VerifyDeclarable();

		CommandLineArgument argument;
		argument.Optional = true;
		argument.TakesValue = false;
		argument.Action = CommandLineAction::StoreTrue;
		argument.Names.emplace_back(name);
		for (StringView alias : aliases)
			argument.Names.emplace_back(alias);

		VerifyArgument(argument);

		m_arguments.push_back(std::move(argument));
		m_lastDeclared = m_arguments.size() - 1;
		for (const String& declaredName : m_arguments[m_lastDeclared].Names)
			RegisterName(m_lastDeclared, declaredName, true);
		return *this;
	}

	CommandLineParser& CommandLineParser::AddCount(StringView name, std::initializer_list<StringView> aliases)
	{
		VerifyDeclarable();

		CommandLineArgument argument;
		argument.Optional = true;
		argument.TakesValue = false;
		argument.Action = CommandLineAction::Count;
		argument.Names.emplace_back(name);
		for (StringView alias : aliases)
			argument.Names.emplace_back(alias);

		VerifyArgument(argument);

		m_arguments.push_back(std::move(argument));
		m_lastDeclared = m_arguments.size() - 1;
		for (const String& declaredName : m_arguments[m_lastDeclared].Names)
			RegisterName(m_lastDeclared, declaredName, true);
		return *this;
	}

	CommandLineParser& CommandLineParser::AddAppendOption(StringView name, std::initializer_list<StringView> aliases)
	{
		VerifyDeclarable();

		CommandLineArgument argument;
		argument.Optional = true;
		argument.TakesValue = true;
		argument.Action = CommandLineAction::Append;
		argument.Names.emplace_back(name);
		for (StringView alias : aliases)
			argument.Names.emplace_back(alias);

		VerifyArgument(argument);

		m_arguments.push_back(std::move(argument));
		m_lastDeclared = m_arguments.size() - 1;
		for (const String& declaredName : m_arguments[m_lastDeclared].Names)
			RegisterName(m_lastDeclared, declaredName, true);
		return *this;
	}

	CommandLineParser& CommandLineParser::AddPositional(StringView name, Usize minCount, Usize maxCount)
	{
		VerifyDeclarable();

		CommandLineArgument argument;
		argument.Optional = false;
		argument.TakesValue = true;
		argument.Action = CommandLineAction::Store;
		argument.Names.emplace_back(name);
		argument.MinCount = minCount;
		argument.MaxCount = maxCount;

		VerifyArgument(argument);

		m_arguments.push_back(std::move(argument));
		m_lastDeclared = m_arguments.size() - 1;
		m_positionalIndices.push_back(m_lastDeclared);
		RegisterName(m_lastDeclared, name, false);
		return *this;
	}

	CommandLineParser& CommandLineParser::ShortNames(std::initializer_list<StringView> aliases)
	{
		VerifyDeclarable();

		CommandLineArgument& argument = LastDeclared();
		if (!argument.Optional)
			ThrowException(Exception("CommandLineParser", "位置参数没有短名"));

		for (StringView alias : aliases)
		{
			argument.Aliases.emplace_back(alias);
			RegisterName(m_lastDeclared, alias, true);
		}
		return *this;
	}

	CommandLineParser& CommandLineParser::Required(bool required)
	{
		VerifyDeclarable();
		LastDeclared().Required = required;
		return *this;
	}

	CommandLineParser& CommandLineParser::Default(StringView defaultValue)
	{
		VerifyDeclarable();
		LastDeclared().DefaultValue = String(defaultValue);
		return *this;
	}

	CommandLineParser& CommandLineParser::Choices(std::initializer_list<StringView> choices)
	{
		VerifyDeclarable();

		CommandLineArgument& argument = LastDeclared();
		argument.Choices.clear();
		for (StringView choice : choices)
			argument.Choices.emplace_back(choice);
		return *this;
	}

	CommandLineParser& CommandLineParser::Help(StringView help)
	{
		VerifyDeclarable();
		LastDeclared().Help = String(help);
		return *this;
	}

	CommandLineParser& CommandLineParser::Metavar(StringView metavar)
	{
		VerifyDeclarable();
		LastDeclared().Metavar = String(metavar);
		return *this;
	}

	CommandLineParser& CommandLineParser::ValueParser(std::function<bool(const StringView&, String&)> parser)
	{
		VerifyDeclarable();
		LastDeclared().ValueParser = std::move(parser);
		return *this;
	}

	CommandLineParser& CommandLineParser::Type(StringView typeName)
	{
		VerifyDeclarable();

		if (typeName == BuiltinTypeInt)
		{
			return ValueParser([](const StringView& raw, String&)
			{
				return ConvertArithmetic<I64>(raw, "value").has_value();
			});
		}

		if (typeName == BuiltinTypeUInt)
		{
			return ValueParser([](const StringView& raw, String&)
			{
				return ConvertArithmetic<U64>(raw, "value").has_value();
			});
		}

		if (typeName == BuiltinTypeFloat)
		{
			return ValueParser([](const StringView& raw, String&)
			{
				return ConvertArithmetic<double>(raw, "value").has_value();
			});
		}

		if (typeName == BuiltinTypeBool)
		{
			return ValueParser([](const StringView& raw, String&)
			{
				bool value = false;
				return TryParseBool(raw, value);
			});
		}

		ThrowException(Exception("CommandLineParser", "未知的内置类型名：'" + String(typeName) + "'"));
	}

	// ================================================================
	// CommandLineParser · 解析期
	// ================================================================

	bool CommandLineParser::LooksLikeOption(StringView token) const noexcept
	{
		return token.Size() > 1 && token.Data()[0] == m_prefixChar;
	}

	std::expected<CommandLineParseResult, CommandLineError> CommandLineParser::Parse(const std::vector<String>& argumentVector)
	{
		if (m_parsed)
			return std::unexpected(MakeError(CommandLineErrorCode::AlreadyParsed, {}, {},
				"同一个解析器只能解析一次"));

		if (argumentVector.empty())
			return std::unexpected(MakeError(CommandLineErrorCode::UnexpectedPositional, {}, {},
				"缺少程序名（argv[0]）"));

		m_parsed = true;

		std::vector<std::optional<std::vector<String>>> values(m_arguments.size());
		// 与 values 平行：只有「用户显式提供」才置位，缺省填充不置位，
		// 否则 IsProvided() 无法区分 `--level 3` 与「level 的缺省值恰好是 3」
		std::vector<bool> provided(m_arguments.size(), false);
		std::vector<std::vector<String>> positionalTokens(m_arguments.size());

		auto appendValue = [&values, &provided](Usize index, StringView value)
		{
			if (!values[index].has_value())
				values[index].emplace();
			values[index]->emplace_back(value);
			provided[index] = true;
		};

		const Usize tokenCount = argumentVector.size();
		Usize cursor = 1;
		bool optionsTerminated = false;

		while (cursor < tokenCount)
		{
			const StringView token(argumentVector[cursor]);

			// I4：`--` 之后一律按位置参数处理
			if (!optionsTerminated && token.Size() == 2
				&& token.Data()[0] == m_prefixChar && token.Data()[1] == m_prefixChar)
			{
				optionsTerminated = true;
				++cursor;
				continue;
			}

			if (optionsTerminated || !LooksLikeOption(token))
			{
				// 位置 token：交给第一个尚未填满的位置参数
				bool consumed = false;
				for (Usize positionalIndex : m_positionalIndices)
				{
					const CommandLineArgument& argument = m_arguments[positionalIndex];
					if (argument.MaxCount != static_cast<Usize>(-1)
						&& positionalTokens[positionalIndex].size() >= argument.MaxCount)
					{
						continue;
					}
					positionalTokens[positionalIndex].emplace_back(token);
					consumed = true;
					break;
				}

				if (!consumed)
					return std::unexpected(MakeError(CommandLineErrorCode::UnexpectedPositional, {}, token,
						"多余的位置参数：" + String(token)));

				++cursor;
				continue;
			}

			// 选项 token：剥前缀，再判断是否带 `=` 内联取值
			Usize prefixLength = 0;
			while (prefixLength < token.Size() && token.Data()[prefixLength] == m_prefixChar)
				++prefixLength;

			const bool isLongForm = prefixLength >= 2;
			StringView nameView = token.Subview(prefixLength);
			std::optional<StringView> inlineValue;

			if (const Usize equalsPosition = nameView.Find('='); equalsPosition != StringView::NPos)
			{
				inlineValue = nameView.Subview(equalsPosition + 1);
				nameView = nameView.Subview(0, equalsPosition);
			}

			Usize index = IndexOf(nameView);

			// I3：`-abc` 仅在每个字符都是已声明短名、且除最后一个外都不取值时展开
			if (index == static_cast<Usize>(-1) && !isLongForm && !inlineValue.has_value() && nameView.Size() > 1)
			{
				bool expandable = true;
				for (Usize i = 0; i < nameView.Size(); ++i)
				{
					const Usize letterIndex = IndexOf(nameView.Subview(i, 1));
					const bool letterTakesValue = letterIndex != static_cast<Usize>(-1)
						&& m_arguments[letterIndex].TakesValue;

					// 只有最后一个字母允许取值
					if (letterIndex == static_cast<Usize>(-1) || (letterTakesValue && i + 1 < nameView.Size()))
					{
						expandable = false;
						break;
					}
				}

				if (expandable)
				{
					bool helpRequested = false;
					// 末位短名可能需要取值：它不在簇内取，而要吃掉下一个 token，
					// 因此这里只记下目标，走下面与「普通选项取值」相同的通道
					Usize clusterValueTarget = static_cast<Usize>(-1);

					for (Usize i = 0; i < nameView.Size(); ++i)
					{
						const Usize letterIndex = IndexOf(nameView.Subview(i, 1));
						const CommandLineArgument& argument = m_arguments[letterIndex];

						if (argument.Names.front() == "help")
							helpRequested = true;

						if (argument.Action == CommandLineAction::Count)
						{
							const Usize current = values[letterIndex].has_value() ? values[letterIndex]->size() : 0;
							appendValue(letterIndex, MakeCountText(current + 1));
							continue;
						}

						if (argument.TakesValue)
						{
							clusterValueTarget = letterIndex;
							continue;
						}

						appendValue(letterIndex, "True");
					}

					if (helpRequested)
						return std::unexpected(MakeError(CommandLineErrorCode::HelpRequested, "help", token, {}));

					// 簇内没有取值项：本 token 处理完毕
					if (clusterValueTarget == static_cast<Usize>(-1))
					{
						++cursor;
						continue;
					}

					// 有取值项：复用下面的取值路径（此时 index 必须是取值项本身）
					index = clusterValueTarget;
				}
			}

			if (index == static_cast<Usize>(-1))
				return std::unexpected(MakeError(CommandLineErrorCode::UnknownOption, nameView, token,
					"未声明的选项：'-" + String(nameView) + "'"));

			const CommandLineArgument& argument = m_arguments[index];

			if (!argument.TakesValue)
			{
				if (inlineValue.has_value())
					return std::unexpected(MakeError(CommandLineErrorCode::InvalidValue, argument.Names.front(), token,
						"开关不接受取值：" + String(token)));

				if (argument.Action == CommandLineAction::Count)
				{
					const Usize current = values[index].has_value() ? values[index]->size() : 0;
					appendValue(index, MakeCountText(current + 1));
				}
				else
				{
					appendValue(index, "True");
				}

				if (argument.Names.front() == "help")
					return std::unexpected(MakeError(CommandLineErrorCode::HelpRequested, "help", token, {}));

				++cursor;
				continue;
			}

			// 需要取值：优先 `=` 内联，其次下一个 token
			StringView value;
			if (inlineValue.has_value())
			{
				value = *inlineValue;
				++cursor;
			}
			else
			{
				if (cursor + 1 >= tokenCount)
					return std::unexpected(MakeError(CommandLineErrorCode::MissingValue, argument.Names.front(), token,
						"选项缺少取值：" + String(token)));

				value = StringView(argumentVector[cursor + 1]);
				cursor += 2;
			}

			// 取值校验：声明的转换器 + Choices
			String normalized;
			if (argument.ValueParser && !argument.ValueParser(value, normalized))
				return std::unexpected(MakeError(CommandLineErrorCode::InvalidValue, argument.Names.front(), value,
					"取值非法：" + String(value)));

			if (!argument.Choices.empty())
			{
				const bool matched = std::any_of(argument.Choices.begin(), argument.Choices.end(),
					[&value](const String& choice) { return choice == value; });
				if (!matched)
					return std::unexpected(MakeError(CommandLineErrorCode::NotInChoices, argument.Names.front(), value,
						"取值不在允许集合内：" + String(value)));
			}

			// Store 与 Append 都保留全部取值，差别只体现在 Get<T>()（取最后一个）与 All()
			appendValue(index, value);
		}

		// ---- 收尾阶段 1：位置参数落位（个数校验 + 缺省填充） ----
		for (Usize positionalIndex : m_positionalIndices)
		{
			const CommandLineArgument& argument = m_arguments[positionalIndex];
			const std::vector<String>& collected = positionalTokens[positionalIndex];

			if (collected.empty())
			{
				if (argument.MinCount > 0)
					return std::unexpected(MakeError(CommandLineErrorCode::MissingRequired, argument.Names.front(), {},
						"缺少位置参数：" + argument.Names.front()));

				if (argument.DefaultValue.has_value())
					values[positionalIndex] = std::vector<String>{ *argument.DefaultValue };
				continue;
			}

			if (collected.size() < argument.MinCount)
				return std::unexpected(MakeError(CommandLineErrorCode::MissingRequired, argument.Names.front(), {},
					"位置参数个数不足：" + argument.Names.front()));

			values[positionalIndex] = collected;
			provided[positionalIndex] = true;
		}

		// ---- 收尾阶段 2：选项必填校验 ----
		// 注意顺序：必填判定必须在「缺省填充」之前，否则 `Required()` + `Default()` 会被缺省值蒙混过关
		for (Usize i = 0; i < m_arguments.size(); ++i)
		{
			const CommandLineArgument& argument = m_arguments[i];
			if (!argument.Optional || !argument.Required || values[i].has_value())
				continue;

			return std::unexpected(MakeError(CommandLineErrorCode::MissingRequired, argument.Names.front(), {},
				"缺少必填选项：" + argument.Names.front()));
		}

		// ---- 收尾阶段 3：未被显式提供的选项写入缺省值 ----
		for (Usize i = 0; i < m_arguments.size(); ++i)
		{
			const CommandLineArgument& argument = m_arguments[i];
			if (!argument.Optional || values[i].has_value() || !argument.DefaultValue.has_value())
				continue;

			values[i] = std::vector<String>{ *argument.DefaultValue };
		}

		// ---- 收尾阶段 4：Count 落位为最终出现次数 ----
		for (Usize i = 0; i < m_arguments.size(); ++i)
		{
			if (m_arguments[i].Action != CommandLineAction::Count || !values[i].has_value())
				continue;

			values[i] = std::vector<String>{ MakeCountText(values[i]->size()) };
		}

		return CommandLineParseResult(*this, String(argumentVector[0]), std::move(values), std::move(provided));
	}

	// ================================================================
	// CommandLineParser · 帮助与错误文本
	// ================================================================

	String CommandLineParser::RenderMetavar(const CommandLineArgument& argument) const
	{
		if (!argument.Metavar.Empty())
			return argument.Metavar;
		if (argument.Optional)
			return String("VALUE");
		return argument.Names.front();
	}

	String CommandLineParser::RenderInvocation(const CommandLineArgument& argument) const
	{
		String rendered;
		const String metavar = RenderMetavar(argument);

		for (const String& alias : argument.Aliases)
		{
			rendered += m_prefixChar;
			rendered += alias;
			if (argument.TakesValue)
			{
				rendered += " ";
				rendered += metavar;
			}
			rendered += ", ";
		}

		if (argument.Optional)
		{
			for (Usize i = 0; i < argument.Names.size(); ++i)
			{
				rendered += m_prefixChar;
				rendered += m_prefixChar;
				rendered += argument.Names[i];
				if (argument.TakesValue)
				{
					rendered += " ";
					rendered += metavar;
				}
				if (i + 1 < argument.Names.size())
					rendered += ", ";
			}
		}
		else
		{
			rendered += argument.Names.front();
			if (argument.MaxCount == static_cast<Usize>(-1) || argument.MaxCount > 1)
			{
				rendered += " [";
				rendered += argument.Names.front();
				rendered += " ...]";
			}
		}

		return rendered;
	}

	String CommandLineParser::UsageText() const
	{
		String usage;
		usage += "usage: ";
		usage += m_programName;
		usage += " [-h]";

		for (Usize index : m_positionalIndices)
		{
			const CommandLineArgument& argument = m_arguments[index];
			const bool hasEllipsis = argument.MaxCount == static_cast<Usize>(-1) || argument.MaxCount > 1;
			const String metavar = RenderMetavar(argument);

			if (argument.MinCount == 0)
			{
				usage += " [";
				usage += metavar;
				if (hasEllipsis)
					usage += " ...";
				usage += "]";
			}
			else
			{
				usage += " ";
				usage += metavar;
				if (hasEllipsis)
				{
					usage += " [";
					usage += metavar;
					usage += " ...]";
				}
			}
		}

		for (const CommandLineArgument& argument : m_arguments)
		{
			if (!argument.Optional || argument.Names.front() == "help")
				continue;

			usage += " [";
			usage += RenderInvocation(argument);
			usage += "]";
		}

		return usage;
	}

	String CommandLineParser::HelpText() const
	{
		String help;
		help += UsageText();
		help += "\n";

		if (!m_description.Empty())
		{
			help += "\n";
			help += m_description;
			help += "\n";
		}

		help += "\n位置参数：\n";
		bool hasPositional = false;
		for (Usize index : m_positionalIndices)
		{
			const CommandLineArgument& argument = m_arguments[index];
			hasPositional = true;
			help += "  ";
			help += RenderInvocation(argument);
			if (!argument.Help.Empty())
			{
				help += "\n      ";
				help += argument.Help;
			}
			help += "\n";
		}
		if (!hasPositional)
			help += "  （无）\n";

		help += "\n选项：\n";
		for (const CommandLineArgument& argument : m_arguments)
		{
			if (!argument.Optional)
				continue;

			help += "  ";
			help += RenderInvocation(argument);
			if (argument.Required)
				help += "（必填）";
			if (argument.DefaultValue.has_value())
			{
				help += "（默认：";
				help += *argument.DefaultValue;
				help += "）";
			}
			if (!argument.Choices.empty())
			{
				help += "（可选值：";
				for (Usize i = 0; i < argument.Choices.size(); ++i)
				{
					if (i > 0)
						help += " / ";
					help += argument.Choices[i];
				}
				help += "）";
			}
			if (!argument.Help.Empty())
			{
				help += "\n      ";
				help += argument.Help;
			}
			help += "\n";
		}

		return help;
	}

	String CommandLineParser::ErrorText(const CommandLineError& error) const
	{
		String text;
		text += UsageText();
		text += "\n";
		text += m_programName;
		text += ": error: ";
		text += error.Message.Empty() ? String("参数错误") : error.Message;
		text += "\n";
		return text;
	}
}
