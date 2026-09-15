// File /Native/Engine/IO/PathView.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "PathView.h"

#include "Internal/PathAnalyzer.h"
#include "Internal/PathComponentUtils.h"
#include "Internal/PathNormalizer.h"
#include "Internal/PathParseUtils.h"

namespace PenEngine
{
	bool PathView::IsRelative() const noexcept
	{
		return !IsAbsolute();
	}

	bool PathView::IsAbsolute() const noexcept
	{
		#ifdef PENFRAMEWORK_OS_WIN32
		StringView rootName = Internal::GetRootName(m_path);
		StringView rootDirectory = Internal::GetRootDirectory(m_path);

		// 1. 驱动器盘符路径 C:(/\)
		if (rootName.Size() == 2 && rootName[1] == ':')
			return !rootDirectory.Empty();

		// 2. 无根路径 \foo \foo\bar
		if (rootName.Empty())
			return false;

		// 3. 服务器/Unix Like路径 \\server\share //server/share
		if (rootName.Size() < 3)
			return false;
		if (rootName[0] != '/' && rootName[0] != '\\')
			return false;
		if (rootName[1] != '/' && rootName[1] != '\\')
			return false;

		return true;

		#endif //PENFRAMEWORK_OS_WIN32

		return HasRootDirectory();
	}

	Usize PathView::Size() const noexcept
	{
		return m_path.Size();
	}

	const char* PathView::Data() const noexcept
	{
		return m_path.Data();
	}

	StringView PathView::View() const noexcept
	{
		return m_path;
	}

	bool PathView::Empty() const noexcept
	{
		return m_path.Empty();
	}

	PathView PathView::BuildRawPath(StringView path) noexcept
	{
		PathView tmp;
		tmp.m_path = path;
		return tmp;
	}

	PathView PathView::RootName() const
	{
		return BuildRawPath(Internal::GetRootName(m_path));
	}

	PathView PathView::RootPath() const
	{
		Usize size = Internal::GetRootName(m_path).Size() + Internal::GetRootDirectory(m_path).Size();
		return BuildRawPath(m_path.Subview(size));
	}

	PathView PathView::RootDirectory() const
	{
		return BuildRawPath(Internal::GetRootDirectory(m_path));
	}

	PathView PathView::Filename() const
	{
		return BuildRawPath(Internal::GetFilename(m_path));
	}

	PathView PathView::Extension() const
	{
		return BuildRawPath(Internal::GetExtension(m_path));
	}

	PathView PathView::Stem() const
	{
		return BuildRawPath(Internal::GetStem(m_path));
	}

	PathView PathView::ParentPath() const
	{
		return BuildRawPath(Internal::GetParentPath(m_path));
	}

	PathView PathView::RelativePath() const
	{
		return BuildRawPath(Internal::GetRelativePath(m_path));
	}

	bool PathView::HasRootName() const noexcept
	{
		return !Internal::GetRootName(m_path).Empty();
	}

	bool PathView::HasRootPath() const noexcept
	{
		return !(Internal::GetRootName(m_path).Empty() && Internal::GetRootDirectory(m_path).Empty());
	}

	bool PathView::HasRootDirectory() const noexcept
	{
		return !Internal::GetRootDirectory(m_path).Empty();
	}

	bool PathView::HasRelativePath() const noexcept
	{
		return !Internal::GetRelativePath(m_path).Empty();
	}

	bool PathView::HasParentPath() const noexcept
	{
		return !Internal::GetParentPath(m_path).Empty();
	}

	bool PathView::HasFilename() const noexcept
	{
		return !Internal::GetFilename(m_path).Empty();
	}

	bool PathView::HasStem() const noexcept
	{
		return !Internal::GetStem(m_path).Empty();
	}

	bool PathView::HasExtension() const noexcept
	{
		return !Internal::GetExtension(m_path).Empty();
	}
}
