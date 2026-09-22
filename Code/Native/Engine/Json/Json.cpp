// File /Native/Engine/Json/Json.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "Json.h"

#include <charconv>
#include <cmath>
#include <utility>

#include <boost/locale/encoding_utf.hpp>

namespace PenEngine::Internal
{
	// ================================================================
	// JsonParseContext
	//
	// 单遍线性扫描的递归下降解析器：
	//   ParseValue -> ParseObject / ParseArray / ParseString / ParseNumber / 字面量
	// 所有失败路径都返回 std::unexpected(ParseErrorData)，不抛异常。
	// ================================================================
	class JsonParseContext
	{
	public:
		explicit JsonParseContext(StringView source) noexcept : m_source(source) {}

		std::expected<void, Json::ParseErrorData> Run(Json& result)
		{
			SkipWhitespace();

			// 空串或纯空白不构成一个 JSON 文本
			if (AtEnd())
				return Fail();

			// 先解析到临时对象，整份文本成功后才提交，失败时不破坏调用方原有内容
			Json parsed;

			if (auto parseResult = ParseValue(parsed); !parseResult)
				return parseResult;

			SkipWhitespace();

			// 根值之后不允许再出现任何非空白内容
			if (!AtEnd())
				return Fail();

			result = std::move(parsed);
			return {};
		}
	private:
		// 解析结果总是整体替换 out 的内容，因此这里构造新的 JsonValue 再赋值，
		// 不依赖 out 当前持有的备选项
		template <typename T>
		static void CommitJsonValue(Json& out, T&& value)
		{
			out.m_value = Internal::JsonValue(std::forward<T>(value));
		}

		// 码位 -> UTF-8 追加
		static void AppendCodePointAsUTF8(String& out, U32 codePoint)
		{
			const boost::locale::utf::code_point code = static_cast<boost::locale::utf::code_point>(codePoint);
			auto inserter = std::back_insert_iterator(out);
			boost::locale::utf::utf_traits<char>::encode(code, inserter);
		}

		static bool IsHexDigit(char ch) noexcept
		{
			return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
		}

		// 解析一个 \uXXXX 中的 4 位十六进制，失败时不推进读取位置
		bool ParseHex4(U32& out) noexcept
		{
			if (m_pos + 4 > m_source.Size())
				return false;

			U32 value = 0;
			for (Usize i = 0; i < 4; ++i)
			{
				const char ch = m_source[m_pos + i];
				if (!IsHexDigit(ch))
					return false;

				value <<= 4;
				if (ch >= '0' && ch <= '9')
					value |= static_cast<U32>(ch - '0');
				else if (ch >= 'a' && ch <= 'f')
					value |= static_cast<U32>(ch - 'a' + 10);
				else
					value |= static_cast<U32>(ch - 'A' + 10);
			}

			m_pos += 4;
			out = value;
			return true;
		}

		bool AtEnd() const noexcept
		{
			return m_pos >= m_source.Size();
		}

		void SkipWhitespace() noexcept
		{
			while (!AtEnd())
			{
				const char ch = m_source[m_pos];
				if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r')
					return;
				++m_pos;
			}
		}

		// 读取位置所在的行(从 1 开始)
		Usize CurrentLine() const noexcept
		{
			const Usize limit = m_pos < m_source.Size() ? m_pos : m_source.Size();

			Usize line = 1;
			for (Usize i = 0; i < limit; ++i)
			{
				if (m_source[i] == '\n')
					++line;
			}
			return line;
		}

		char CurrentCharacter() const noexcept
		{
			return AtEnd() ? '\0' : m_source[m_pos];
		}

		// 以当前读取位置构造错误，不推进位置
		std::expected<void, Json::ParseErrorData> Fail() const
		{
			return std::unexpected(Json::ParseErrorData{ CurrentLine(), CurrentCharacter() });
		}

		// 后续若匹配给定字面量则整体消费
		bool MatchLiteral(StringView literal)
		{
			if (m_pos + literal.Size() > m_source.Size())
				return false;

			for (Usize i = 0; i < literal.Size(); ++i)
			{
				if (m_source[m_pos + i] != literal[i])
					return false;
			}

			m_pos += literal.Size();
			return true;
		}

		std::expected<void, Json::ParseErrorData> ParseValue(Json& out)
		{
			if (++m_depth > MaxNestingDepth)
				return Fail();

			std::expected<void, Json::ParseErrorData> result;

			switch (CurrentCharacter())
			{
			case '{':
				result = ParseObject(out);
				break;
			case '[':
				result = ParseArray(out);
				break;
			case '"':
			{
				String string;
				result = ParseString(string);
				if (result)
					CommitJsonValue(out, std::move(string));
				break;
			}
			case 't':
				result = MatchLiteral("true") ? std::expected<void, Json::ParseErrorData>{} : Fail();
				if (result)
					CommitJsonValue(out, true);
				break;
			case 'f':
				result = MatchLiteral("false") ? std::expected<void, Json::ParseErrorData>{} : Fail();
				if (result)
					CommitJsonValue(out, false);
				break;
			case 'n':
				// JsonValue 的第一个备选项是 bool，无法表示 null，因此 null 保留为默认值(即 false)
				result = MatchLiteral("null") ? std::expected<void, Json::ParseErrorData>{} : Fail();
				break;
			default:
				result = ParseNumber(out);
				break;
			}

			--m_depth;
			return result;
		}

		std::expected<void, Json::ParseErrorData> ParseObject(Json& out)
		{
			++m_pos; // 跳过 '{'

			JsonObject object;

			SkipWhitespace();

			if (CurrentCharacter() == '}')
			{
				++m_pos;
				CommitJsonValue(out, std::move(object));
				return {};
			}

			while (true)
			{
				SkipWhitespace();

				if (CurrentCharacter() != '"')
					return Fail();

				String key;
				if (auto keyResult = ParseString(key); !keyResult)
					return keyResult;

				SkipWhitespace();

				if (CurrentCharacter() != ':')
					return Fail();
				++m_pos;

				SkipWhitespace();

				Json value;
				if (auto valueResult = ParseValue(value); !valueResult)
					return valueResult;

				// 重复键沿用后出现的值，与常见实现的取舍一致
				object.insert_or_assign(std::move(key), std::move(value));

				SkipWhitespace();

				if (CurrentCharacter() == ',')
				{
					++m_pos;
					continue;
				}

				if (CurrentCharacter() == '}')
				{
					++m_pos;
					break;
				}

				return Fail();
			}

			CommitJsonValue(out, std::move(object));
			return {};
		}

		std::expected<void, Json::ParseErrorData> ParseArray(Json& out)
		{
			++m_pos; // 跳过 '['

			JsonArray array;

			SkipWhitespace();

			if (CurrentCharacter() == ']')
			{
				++m_pos;
				CommitJsonValue(out, std::move(array));
				return {};
			}

			while (true)
			{
				SkipWhitespace();

				Json element;
				if (auto elementResult = ParseValue(element); !elementResult)
					return elementResult;

				array.push_back(std::move(element));

				SkipWhitespace();

				if (CurrentCharacter() == ',')
				{
					++m_pos;
					continue;
				}

				if (CurrentCharacter() == ']')
				{
					++m_pos;
					break;
				}

				return Fail();
			}

			CommitJsonValue(out, std::move(array));
			return {};
		}

		// 进入时应指向起始的 '"'，返回时指向结尾 '"' 之后
		std::expected<void, Json::ParseErrorData> ParseString(String& out)
		{
			++m_pos; // 跳过起始 '"'

			out.Clear();

			while (true)
			{
				if (AtEnd())
					return Fail();

				const char ch = m_source[m_pos++];

				if (ch == '"')
					return {};

				// 未转义的控制字符在 JSON 字符串中非法
				if (static_cast<unsigned char>(ch) < 0x20)
				{
					--m_pos;
					return Fail();
				}

				if (ch != '\\')
				{
					out.PushBack(ch);
					continue;
				}

				if (AtEnd())
					return Fail();

				const char escape = m_source[m_pos++];
				switch (escape)
				{
				case '"':  out.PushBack('"');  break;
				case '\\': out.PushBack('\\'); break;
				case '/':  out.PushBack('/');  break;
				case 'b':  out.PushBack('\b'); break;
				case 'f':  out.PushBack('\f'); break;
				case 'n':  out.PushBack('\n'); break;
				case 'r':  out.PushBack('\r'); break;
				case 't':  out.PushBack('\t'); break;
				case 'u':
				{
					U32 codePoint = 0;
					if (!ParseHex4(codePoint))
						return Fail();

					// 高代理项必须紧跟一个低代理项，合成后按真实码位编码
					if (codePoint >= 0xD800 && codePoint <= 0xDBFF)
					{
						if (m_pos + 2 > m_source.Size() || m_source[m_pos] != '\\' || m_source[m_pos + 1] != 'u')
							return Fail();
						m_pos += 2;

						U32 lowSurrogate = 0;
						if (!ParseHex4(lowSurrogate))
							return Fail();

						if (lowSurrogate < 0xDC00 || lowSurrogate > 0xDFFF)
							return Fail();

						codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (lowSurrogate - 0xDC00);
					}
					else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF)
						return Fail(); // 孤立的低代理项

					AppendCodePointAsUTF8(out, codePoint);
					break;
				}
				default:
					--m_pos;
					return Fail();
				}
			}
		}

		// 严格按 JSON 数字文法切出 token: -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
		std::expected<void, Json::ParseErrorData> ParseNumber(Json& out)
		{
			const Usize start = m_pos;

			if (CurrentCharacter() == '-')
				++m_pos;

			if (AtEnd())
				return Fail();

			if (m_source[m_pos] == '0')
				++m_pos;
			else if (m_source[m_pos] >= '1' && m_source[m_pos] <= '9')
			{
				while (!AtEnd() && m_source[m_pos] >= '0' && m_source[m_pos] <= '9')
					++m_pos;
			}
			else
				return Fail();

			bool isInteger = true;

			if (!AtEnd() && m_source[m_pos] == '.')
			{
				isInteger = false;
				++m_pos;

				if (AtEnd() || m_source[m_pos] < '0' || m_source[m_pos] > '9')
					return Fail();

				while (!AtEnd() && m_source[m_pos] >= '0' && m_source[m_pos] <= '9')
					++m_pos;
			}

			if (!AtEnd() && (m_source[m_pos] == 'e' || m_source[m_pos] == 'E'))
			{
				isInteger = false;
				++m_pos;

				if (!AtEnd() && (m_source[m_pos] == '+' || m_source[m_pos] == '-'))
					++m_pos;

				if (AtEnd() || m_source[m_pos] < '0' || m_source[m_pos] > '9')
					return Fail();

				while (!AtEnd() && m_source[m_pos] >= '0' && m_source[m_pos] <= '9')
					++m_pos;
			}

			const char* begin = m_source.Data() + start;
			const char* end = m_source.Data() + m_pos;

			if (isInteger)
			{
				if (*begin == '-')
				{
					I64 value = 0;
					if (auto [ptr, error] = std::from_chars(begin, end, value);
						error == std::errc() && ptr == end)
					{
						CommitJsonValue(out, value);
						return {};
					}
				}
				else
				{
					// 先尝试 I64，超出范围再落到 U64
					I64 signedValue = 0;
					if (auto [ptr, error] = std::from_chars(begin, end, signedValue);
						error == std::errc() && ptr == end)
					{
						CommitJsonValue(out, signedValue);
						return {};
					}

					U64 unsignedValue = 0;
					if (auto [ptr, error] = std::from_chars(begin, end, unsignedValue);
						error == std::errc() && ptr == end)
					{
						CommitJsonValue(out, unsignedValue);
						return {};
					}
				}
			}

			// 带小数或指数，以及无法用整数承载的超长整数，统一作为 double
			double doubleValue = 0.0;
			if (auto [ptr, error] = std::from_chars(begin, end, doubleValue);
				error == std::errc() && ptr == end)
			{
				CommitJsonValue(out, doubleValue);
				return {};
			}

			m_pos = start;
			return Fail();
		}

		StringView m_source;
		Usize m_pos = 0;
		Usize m_depth = 0;
	};

	// ================================================================
	// JsonBuildContext
	//
	// needFormat 为 false 时输出紧凑单行文本；为 true 时成员与元素逐个换行
	// 并按层级缩进。空对象与空数组始终写作 {} 与 []。
	// ================================================================
	class JsonBuildContext
	{
	public:
		explicit JsonBuildContext(bool needFormat) noexcept : m_needFormat(needFormat) {}

		void Run(const Json& value)
		{
			WriteValue(value);
		}

		String TakeResult()
		{
			return std::move(m_out);
		}
	private:
		static constexpr char IndentCharacter = '\t';

		void WriteIndent()
		{
			if (!m_needFormat)
				return;

			m_out.PushBack('\n');
			for (Usize i = 0; i < m_indentLevel; ++i)
				m_out.PushBack(IndentCharacter);
		}

		void WriteString(const String& str)
		{
			m_out.PushBack('"');

			for (Usize i = 0; i < str.Size(); ++i)
			{
				const char ch = str[i];

				switch (ch)
				{
				case '"':  m_out.PushBack('\\'); m_out.PushBack('"');  break;
				case '\\': m_out.PushBack('\\'); m_out.PushBack('\\'); break;
				case '\b': m_out.PushBack('\\'); m_out.PushBack('b');  break;
				case '\f': m_out.PushBack('\\'); m_out.PushBack('f');  break;
				case '\n': m_out.PushBack('\\'); m_out.PushBack('n');  break;
				case '\r': m_out.PushBack('\\'); m_out.PushBack('r');  break;
				case '\t': m_out.PushBack('\\'); m_out.PushBack('t');  break;
				default:
					if (static_cast<unsigned char>(ch) < 0x20)
					{
						constexpr char HexDigits[] = "0123456789abcdef";
						const unsigned char code = static_cast<unsigned char>(ch);

						m_out.PushBack('\\');
						m_out.PushBack('u');
						m_out.PushBack('0');
						m_out.PushBack('0');
						m_out.PushBack(HexDigits[(code >> 4) & 0x0F]);
						m_out.PushBack(HexDigits[code & 0x0F]);
					}
					else
						m_out.PushBack(ch); // 其余字节(含 UTF-8 多字节序列)原样输出
					break;
				}
			}

			m_out.PushBack('"');
		}

		// 整型与浮点的 to_chars 重载集不同，先用 constexpr 分流
		template <typename Number>
		void WriteNumber(Number value)
		{
			char buffer[64];

			std::to_chars_result result{};

			if constexpr (std::is_floating_point_v<Number>)
			{
				// JSON 没有 Infinity / NaN 字面量，非有限值统一写作 null
				if (!std::isfinite(value))
				{
					m_out.PushBack("null");
					return;
				}

				result = std::to_chars(buffer, buffer + std::size(buffer), value, std::chars_format::general);
			}
			else
				result = std::to_chars(buffer, buffer + std::size(buffer), value);

			if (result.ec == std::errc())
			{
				m_out.PushBack(buffer, static_cast<Usize>(result.ptr - buffer));
				return;
			}

			// to_chars 对 64 位整数与双精度在 64 字节内必定成功，这里的兜底只为不产生非法文本
			m_out.PushBack('0');
		}

		void WriteObject(const JsonObject& object)
		{
			if (object.empty())
			{
				m_out.PushBack('{');
				m_out.PushBack('}');
				return;
			}

			m_out.PushBack('{');
			++m_indentLevel;

			bool isFirst = true;
			for (const auto& [key, value] : object)
			{
				if (!isFirst)
					m_out.PushBack(',');
				isFirst = false;

				WriteIndent();
				WriteString(key);

				m_out.PushBack(':');
				if (m_needFormat)
					m_out.PushBack(' ');

				WriteValue(value);
			}

			--m_indentLevel;
			WriteIndent();

			m_out.PushBack('}');
		}

		void WriteArray(const JsonArray& array)
		{
			if (array.empty())
			{
				m_out.PushBack('[');
				m_out.PushBack(']');
				return;
			}

			m_out.PushBack('[');
			++m_indentLevel;

			bool isFirst = true;
			for (const Json& element : array)
			{
				if (!isFirst)
					m_out.PushBack(',');
				isFirst = false;

				WriteIndent();
				WriteValue(element);
			}

			--m_indentLevel;
			WriteIndent();

			m_out.PushBack(']');
		}

		void WriteValue(const Json& value)
		{
			// 字符串与其余备选项写法不同，先单独分流
			if (const String* str = value.TryGetValueAs<String>())
			{
				WriteString(*str);
				return;
			}

			std::visit(
				[this](const auto& alternative)
				{
					using T = std::remove_cvref_t<decltype(alternative)>;

					if constexpr (std::is_same_v<T, bool>)
					{
						m_out.PushBack(alternative ? "true" : "false");
					}
					else if constexpr (std::is_same_v<T, I64>)
					{
						WriteNumber(alternative);
					}
					else if constexpr (std::is_same_v<T, U64>)
					{
						WriteNumber(alternative);
					}
					else if constexpr (std::is_same_v<T, double>)
					{
						WriteNumber(alternative);
					}
					else if constexpr (std::is_same_v<T, JsonObject>)
					{
						WriteObject(alternative);
					}
					else if constexpr (std::is_same_v<T, JsonArray>)
					{
						WriteArray(alternative);
					}
				},
				value.m_value);
		}

		String m_out;
		Usize m_indentLevel = 0;
		bool m_needFormat = true;
	};
}

namespace PenEngine
{
	std::expected<void, Json::ParseErrorData> Json::Parse(StringView json)
	{
		Internal::JsonParseContext context(json);

		return context.Run(*this);
	}

	String Json::Build(bool needFormat)
	{
		Internal::JsonBuildContext context(needFormat);

		context.Run(*this);

		return context.TakeResult();
	}
}
