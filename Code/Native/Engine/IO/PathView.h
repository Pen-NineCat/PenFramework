// File /Native/Engine/IO/PathView.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../String/String.hpp"

namespace PenEngine
{
	class PathView
	{
	public:
		template <typename CharType>
		BasicString<CharType> ToString() const;
		template <typename CharType>
		std::basic_string<CharType> ToStdString() const;

		Usize Size() const noexcept;
		const char* Data() const noexcept;
		StringView View() const noexcept;
		bool Empty() const noexcept;

		static PathView BuildRawPath(StringView path) noexcept;

		PathView RootName() const;
		PathView RootPath() const;
		PathView RootDirectory() const;
		PathView Filename() const;
		PathView Extension() const;
		PathView Stem() const;
		PathView ParentPath() const;
		PathView RelativePath() const;

		bool HasRootName() const noexcept;
		bool HasRootPath() const noexcept;
		bool HasRootDirectory() const noexcept;
		bool HasRelativePath() const noexcept;
		bool HasParentPath() const noexcept;
		bool HasFilename() const noexcept;
		bool HasStem() const noexcept;
		bool HasExtension() const noexcept;

		bool IsRelative() const noexcept;
		bool IsAbsolute() const noexcept;
	private:
		StringView m_path;
	};

	template <typename CharType>
	[[nodiscard]] BasicString<CharType> PathView::ToString() const
	{
		BasicString<CharType> tmp;
		tmp.ConvertFrom(m_path);
		return tmp;
	}

	template <typename CharType>
	[[nodiscard]] std::basic_string<CharType> PathView::ToStdString() const
	{
		BasicString<CharType> tmp;
		tmp.ConvertFrom(m_path);
		return tmp.ToStdString();
	}
}

template <>
struct std::hash<PenEngine::PathView>
{
	static PenEngine::Usize operator()(const PenEngine::PathView& path) noexcept
	{
		return std::hash<PenEngine::StringView>::operator()(path.View());
	}
};

template <>
struct std::formatter<PenEngine::PathView, char> : std::formatter<char, char>
{
	static auto format(const PenEngine::PathView& path, std::format_context& ctx)
	{
		return std::format_to(ctx.out(), "{}", path.Data());
	}
};

template <>
struct std::formatter<PenEngine::PathView, wchar_t> : std::formatter<wchar_t, wchar_t>
{
	static auto format(const PenEngine::PathView& path, std::wformat_context& ctx)
	{
		return std::format_to(ctx.out(), L"{}", path.ToString<wchar_t>().Data());
	}
};