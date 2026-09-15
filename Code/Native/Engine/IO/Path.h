// File /Native/Engine/IO/Path.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../String/String.hpp"
#include "PathView.h"
#include <filesystem>

namespace PenEngine
{
	class Path
	{
	public:
		using value_type = char;
		using pointer = value_type*;
		using const_pointer = const value_type*;
		using reference = value_type&;
		using const_reference = const value_type&;
		using size_type = Usize;
		using difference_type = PtrDiff;

		// @todo 长路径支持需要内部使用平台特化的separator，并且对于size >= MAX_PATH的路径自动加上\\?\ 目前不打算实现这一部分内容
		constexpr static char PreferredSeparator =
		{
			#ifdef LONG_PATH_SUPPORT 
			// 如果不启用长路径支持，windows api可以识别以'/'为分隔符的路径
			#ifdef PENFRAMEWORK_OS_WIN32
			'\\'
			#else // PENFRAMEWORK_OS_WIN32
			'/'
			#endif // PENFRAMEWORK_OS_WIN32

			#else // LONG_PATH_SUPPORT
			'/'
			#endif // LONG_PATH_SUPPORT 
		};

		Path() noexcept = default;

		Path(const Path&) = default;
		Path(Path&&) noexcept = default;
		Path& operator=(const Path& path);
		Path& operator=(Path&& path) noexcept;

		~Path() noexcept = default;

		/* implicit */ Path(const String& path);
		/* implicit */ Path(StringView path);
		/* implicit */ Path(const char* path);
		/* implicit */ Path(const std::filesystem::path& path);
		/* implicit */ Path(PathView path);

		Path& operator=(std::nullopt_t) noexcept;
		Path& operator=(const String& path);
		Path& operator=(StringView path);
		Path& operator=(const char* path);
		Path& operator=(const std::string& path);
		Path& operator=(std::string_view path);
		Path& operator=(PathView path);

		static Path BuildRawPath(StringView path);

		template <typename SourceCharType>
		void ConvertAndNormalize(const BasicString<SourceCharType>& path);
		template <typename SourceCharType>
		void ConvertAndNormalize(BasicStringView<SourceCharType> path);
		template <typename SourceCharType>
		void ConvertAndNormalize(const SourceCharType* path);
		template <typename SourceCharType>
		void ConvertAndNormalize(const SourceCharType* path, Usize len);

		template <typename SourceCharType>
		void ConvertDirect(const BasicString<SourceCharType>& path);
		template <typename SourceCharType>
		void ConvertDirect(BasicStringView<SourceCharType> path);
		template <typename SourceCharType>
		void ConvertDirect(const SourceCharType* path);
		template <typename SourceCharType>
		void ConvertDirect(const SourceCharType* path, Usize len);

		Path operator/(const Path& path) const;
		Path operator/(const String& path) const;
		Path operator/(StringView path) const;
		Path operator/(const char* path) const;
		Path operator/(const std::string& path) const;
		Path operator/(std::string_view path) const;
		Path operator/(PathView path) const;

		Path& operator/=(const Path& path);
		Path& operator/=(const String& path);
		Path& operator/=(StringView path);
		Path& operator/=(const char* path);
		Path& operator/=(const std::string& path);
		Path& operator/=(std::string_view path);
		Path& operator/=(PathView path);

		Path& operator+=(const Path& path);
		Path& operator+=(const String& path);
		Path& operator+=(StringView path);
		Path& operator+=(const char* path);
		Path& operator+=(const std::string& path);
		Path& operator+=(std::string_view path);
		Path& operator+=(PathView path);

		bool IsAbsolute() const noexcept;
		bool IsRelative() const noexcept;

		bool Empty() const noexcept;

		Path RootName() const;
		Path RootPath() const;
		Path RootDirectory() const;
		Path Filename() const;
		Path Extension() const;
		Path Stem() const;
		Path ParentPath() const;
		Path RelativePath() const;

		Path& ReplaceFilename(const Path& filename);
		Path& ReplaceExtension(const Path& extension);
		Path& RemoveFilename();

		template <typename CharType>
		BasicString<CharType> ToString() const;
		template <typename CharType>
		std::basic_string<CharType> ToStdString() const;

		Usize Size() const noexcept;
		const char* Data() const noexcept;

		void Clear() noexcept;

		StringView View() const noexcept;

		bool HasRootName() const noexcept;
		bool HasRootPath() const noexcept;
		bool HasRootDirectory() const noexcept;
		bool HasRelativePath() const noexcept;
		bool HasParentPath() const noexcept;
		bool HasFilename() const noexcept;
		bool HasStem() const noexcept;
		bool HasExtension() const noexcept;

		void MoveToUpLayer() noexcept;

		Path ToAbsolutePath() const;

		static Path GetApplicationPath();

		String ToAbsolutePathString() const;
		WString ToAbsolutePathWString() const;

		/* implicit */ operator PathView() const noexcept;
	private:
		static String Normalize(StringView path);
		static void Normalize(String& path);

		String m_path;
	};

	template <typename SourceCharType>
	void Path::ConvertAndNormalize(const BasicString<SourceCharType>& path)
	{
		String tmp;
		tmp.ConvertFrom(path);
		Normalize(tmp);
		m_path = std::move(tmp);
	}

	template <typename SourceCharType>
	void Path::ConvertAndNormalize(BasicStringView<SourceCharType> path)
	{
		String tmp;
		tmp.ConvertFrom(path);
		Normalize(tmp);
		m_path = std::move(tmp);
	}

	template <typename SourceCharType>
	void Path::ConvertAndNormalize(const SourceCharType* path)
	{
		String tmp;
		tmp.ConvertFrom(path);
		Normalize(tmp);
		m_path = std::move(tmp);
	}

	template <typename SourceCharType>
	void Path::ConvertAndNormalize(const SourceCharType* path, Usize len)
	{
		String tmp;
		tmp.ConvertFrom(path, len);
		Normalize(tmp);
		m_path = std::move(tmp);
	}

	template <typename SourceCharType>
	void Path::ConvertDirect(const BasicString<SourceCharType>& path)
	{
		m_path.ConvertFrom(path);
	}

	template <typename SourceCharType>
	void Path::ConvertDirect(BasicStringView<SourceCharType> path)
	{
		m_path.ConvertFrom(path);
	}

	template <typename SourceCharType>
	void Path::ConvertDirect(const SourceCharType* path)
	{
		m_path.ConvertFrom(path);
	}

	template <typename SourceCharType>
	void Path::ConvertDirect(const SourceCharType* path, Usize len)
	{
		m_path.ConvertFrom(path, len);
	}

	template <typename CharType>
	BasicString<CharType> Path::ToString() const
	{
		return m_path.ConvertToString<CharType>();
	}

	template <typename CharType>
	std::basic_string<CharType> Path::ToStdString() const
	{
		return m_path.ConvertToStdString<CharType>();
	}

}

template <>
struct std::hash<PenEngine::Path>
{
	static PenEngine::Usize operator()(const PenEngine::Path& path) noexcept
	{
		return std::hash<PenEngine::StringView>::operator()(path.View());
	}
};

template <>
struct std::formatter<PenEngine::Path, char> : std::formatter<char, char>
{
	bool enableAbsolute = false;

	constexpr auto parse(std::format_parse_context& ctx)
	{
		auto it = ctx.begin();
		auto end = ctx.end();

		// 检查是否有 'a' 标志
		if (it != end && *it == 'a')
		{
			enableAbsolute = true;
			++it;
		}

		// 检查是否还有其他非法字符（除了结束符 '}'）
		if (it != end && *it != '}')
		{
			throw std::format_error("Invalid format specifier for Path");
		}

		return it;
	}

	auto format(const PenEngine::Path& path, std::format_context& ctx) const
	{
		if (enableAbsolute)
		{
			if (path.IsAbsolute())
				return std::format_to(ctx.out(), "{}", path.Data());

			PenEngine::String absStr = path.ToAbsolutePathString();
			return std::format_to(ctx.out(), "{}", absStr.Data());
		}
		return std::format_to(ctx.out(), "{}", path.Data());
	}
};

template <>
struct std::formatter<PenEngine::Path, wchar_t> : std::formatter<wchar_t, wchar_t>
{
	bool enableAbsolute = false;

	constexpr auto parse(std::wformat_parse_context& ctx)
	{
		auto it = ctx.begin();
		auto end = ctx.end();

		// 检查是否有 'a' 标志
		if (it != end && *it == L'a')
		{
			enableAbsolute = true;
			++it;
		}

		// 检查是否还有其他非法字符（除了结束符 '}'）
		if (it != end && *it != L'}')
		{
			throw std::format_error("Invalid format specifier for Path");
		}

		return it;
	}

	auto format(const PenEngine::Path& path, std::wformat_context& ctx) const
	{
		if (enableAbsolute)
		{
			if (path.IsAbsolute())
				return std::format_to(ctx.out(), L"{}", path.ToString<wchar_t>().Data());

			PenEngine::WString absStr = path.ToAbsolutePathWString();
			return std::format_to(ctx.out(), L"{}", absStr.Data());
		}
		return std::format_to(ctx.out(), L"{}", path.ToString<wchar_t>().Data());
	}
};