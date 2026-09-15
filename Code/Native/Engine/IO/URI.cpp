// File /Native/Engine/IO/URI.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "URI.h"

int PenEngine::URI::HexCharToInt(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

std::expected<PenEngine::Path, PenEngine::URI::OperationResult> PenEngine::URI::ToLocalPath() const
{
	const char* data = m_uri.Data();
	Usize size = m_uri.Size();

	constexpr char protocol[] = "file://";
	constexpr Usize protocolSize = std::size(protocol);

	if (size < protocolSize)
		return std::unexpected(OperationResult::ProtocolNotMatch);

	for (Usize i = 0;i < protocolSize;++i)
		if (std::tolower(static_cast<unsigned char>(data[i])) != protocol[i])
			return std::unexpected(OperationResult::ProtocolNotMatch);

	String tmp;
	tmp.Reserve(size - protocolSize);

	for (Usize i = protocolSize;i < size;)
	{
		if (char c = data[i]; c == '%')
		{
			// 处理百分号编码 %XX
			// 检查是否有足够的字符
			if (i + 2 >= size)
				return std::unexpected(OperationResult::EncodingFailure);

			int high = HexCharToInt(data[i + 1]);
			int low = HexCharToInt(data[i + 2]);

			if (high == -1 || low == -1)
				return std::unexpected(OperationResult::EncodingFailure);

			// 转换回字符
			char decodedChar = static_cast<char>((high << 4) | low);
			tmp += decodedChar;
			i += 3;
		}
		else
		{
			// 普通字符直接追加
			tmp += c;
			++i;
		}
	}

	return Path(tmp);
}

void PenEngine::URI::FromLocalPath(const Path& path)
{
	const char* data = path.Data();
	Usize size = path.Size();

	if (size == 0)
	{
		m_uri += "file://.";
		return;
	}

	String tmp;

	bool isWindowsAbsolute = (size >= 2 && std::isalpha(static_cast<unsigned char>(data[0])) && data[1] == ':');

	if (isWindowsAbsolute)
	{
		tmp += "file:///";
		tmp += data[0];
		tmp += data[1];
	}
	else
		tmp += "file://";

	for (Usize i = isWindowsAbsolute ? 2 : 0;i < size;++i)
	{
		char c = data[i];
		bool isSafeCharacter =
			(c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			c == '-' || c == '.' || c == '_' || c == '~' || c == '/';

		if (isSafeCharacter)
			tmp += c;
		else
		{
			auto uch = static_cast<U8>(c);
			std::vformat_to(std::back_insert_iterator(tmp), "%{:02X}", std::make_format_args(uch));
		}
	}

	m_uri = std::move(tmp);
}

const char* PenEngine::URI::Data() const noexcept
{
	return m_uri.Data();
}

PenEngine::Usize PenEngine::URI::Size() const noexcept
{
	return m_uri.Size();
}

PenEngine::StringView PenEngine::URI::ToView() const noexcept
{
	return m_uri;
}
