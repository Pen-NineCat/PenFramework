// File /Native/Engine/IO/URI.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include <expected>

#include "../String/String.hpp"
#include "Path.h"

namespace PenEngine
{
	class URI
	{
	public:
		enum class OperationResult : U8
		{
			ProtocolNotMatch,
			EncodingFailure,
			DecodingFailure
		};

		std::expected<Path, OperationResult> ToLocalPath() const;
		void FromLocalPath(const Path& path);

		const char* Data() const noexcept;
		Usize Size() const noexcept;

		StringView ToView() const noexcept;
	private:
		static int HexCharToInt(char c);

		String m_uri;
	};
}

template <>
struct std::hash<PenEngine::URI>
{
	static PenEngine::Usize operator()(const PenEngine::URI& path) noexcept
	{
		return std::hash<PenEngine::StringView>::operator()(path.ToView());
	}
};

template <>
struct std::formatter<PenEngine::URI>
{
	bool EnableDecoder = false;

	constexpr auto parse(format_parse_context& ctx)
	{
		auto it = ctx.begin();
		auto end = ctx.end();

		// 检查是否有格式说明符
		if (it != end)
		{
			// 查找 'r'
			if (it != end && *it == 'r')
			{
				EnableDecoder = true;
				++it;
			}

			// 如果还有其他字符，抛出异常（除非是结束符 '}'）
			if (it != end && *it != '}')
			{
				throw std::format_error("Invalid format specifier");
			}
		}

		return it;
	}

	auto format(const PenEngine::URI& uri, std::format_context& ctx) const
	{
		const char* data = uri.Data();
		PenEngine::Usize size = uri.Size();

		if (!EnableDecoder)
			return std::format_to(ctx.out(), "{}", std::string_view(data, size));

		std::string decodedStr;
		decodedStr.reserve(size);

		for (PenEngine::Usize i = 0; i < size; ++i)
		{
			char c = data[i];
			if (c == '%')
			{
				// 检查是否有足够的字符进行解码
				if (i + 2 < size)
				{
					char high = data[i + 1];
					char low = data[i + 2];

					// 简单的十六进制校验
					auto HexToInt = [](char h) -> int
					{
						if (h >= '0' && h <= '9') return h - '0';
						if (h >= 'a' && h <= 'f') return h - 'a' + 10;
						if (h >= 'A' && h <= 'F') return h - 'A' + 10;
						return -1;
					};

					if (int valHigh = HexToInt(high), valLow = HexToInt(low); valHigh != -1 && valLow != -1)
					{
						// 还原为原始字节
						decodedStr.push_back(static_cast<char>((valHigh << 4) | valLow));
						i += 2; // 跳过两个十六进制字符
						continue;
					}
				}

				// 如果格式不正确（如 %G1 或 %1），保留原样
				decodedStr.push_back(c);
			}
			else
			{
				decodedStr.push_back(c);
			}
		}

		// 输出还原后的 UTF-8 字符串
		return std::format_to(ctx.out(), "{}", decodedStr);
	}
};
