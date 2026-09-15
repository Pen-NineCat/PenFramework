// File /Native/Engine/IO/Internal/PathComponentUtils.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// NOTE: This is an internal framework file and should not be used in user's code externally.
#include "PathAnalyzer.h"
#include "PathComponentUtils.h"

namespace PenEngine::Internal
{
	StringView GetRootName(StringView path) noexcept
	{
		#ifdef PENFRAMEWORK_OS_WIN32
		PathAnalyzer analyzer(path, PathAnalyzer::State::AtStart);
		PathAnalyzer::Iterator it = analyzer.Forward();

		if (it.State == PathAnalyzer::State::AtRootName)
			return it.Path;
		#endif
		return {};
	}

	StringView GetRootDirectory(StringView path) noexcept
	{
		PathAnalyzer analyzer(path, PathAnalyzer::State::AtStart);
		PathAnalyzer::Iterator it = analyzer.Forward();

		#ifdef PENFRAMEWORK_OS_WIN32
		if (it.State == PathAnalyzer::State::AtRootName)
			++it;
		#endif // PENFRAMEWORK_OS_WIN32

		if (it.State == PathAnalyzer::State::AtRootDir)
			return it.Path;

		return {};
	}

	StringView GetFilename(StringView path) noexcept
	{
		PathAnalyzer analyzer(path, PathAnalyzer::State::AtStart);
		PathAnalyzer::Iterator it = analyzer.Forward();
		while (it.State <= PathAnalyzer::State::AtRootDir)
			++it;

		if (it.State == PathAnalyzer::State::AtEnd)
			return {};

		return PathAnalyzer(path, PathAnalyzer::State::AtEnd).Backward().Path;
	}

	StringView GetExtension(StringView path) noexcept
	{
		// 实现参照 https://cppreference.cn/w/cpp/filesystem/path/extension

		// 如果通用格式路径的 filename() 部分包含一个句点（.），并且不是特殊文件系统元素 dot 或 dot - dot 之一，那么“扩展名”是从最右边的句点（包括句点）开始，直到路径名末尾的子字符串。
		// 如果文件名的第一个字符是句点，则该句点将被忽略（像“.profile”这样的文件名不被视为扩展名）。
		// 如果路径名是.或 ..，或者 filename() 不包含 '.' 字符，则返回空路径。
		// 对于将附加元素（如备用数据流或分区数据集名称）附加到扩展名的文件系统，实现可能会定义附加行为。

		StringView filename = GetFilename(path);

		if (Usize pos = filename.FindLastOf('.'); filename == ".." || filename == '.' || pos == 0 || pos == StringView::NPos)
			return {};
		else
			return filename.Subview(pos);
	}

	StringView GetStem(StringView path) noexcept
	{
		// 实现参照 https://cppreference.cn/w/cpp/filesystem/path/stem

		// 返回从 filename() 的开头开始，直到但不包括最后一个句点(.) 字符的子字符串，但有以下例外：
		// 如果文件名中的第一个字符是句点，则忽略该句点（像 ".profile" 这样的文件名不被视为扩展名）
		// 如果文件名是特殊文件系统组件 点 或 点 - 点 之一，或者它没有句点，则该函数返回整个 filename()

		StringView filename = GetFilename(path);

		if (Usize pos = filename.FindLastOf('.'); filename == ".." || filename == '.' || pos == 0 || pos == StringView::NPos)
			return filename;
		else
			return filename.Subview(0, pos);
	}

	StringView GetRelativePath(StringView path) noexcept
	{
		PathAnalyzer analyzer(path, PathAnalyzer::State::AtStart);
		auto it = analyzer.Forward();
		while (it.State <= PathAnalyzer::State::AtRootDir)
			++it;

		if (it.State == PathAnalyzer::State::AtEnd)
			return {};

		return StringView(it.Path.Data(), path.DataEnd() - 1);
	}

	StringView GetParentPath(StringView path) noexcept
	{
		if (path.Empty())
			return {};

		PathAnalyzer forwardAnalyzer(path, PathAnalyzer::State::AtStart);
		PathAnalyzer::Iterator forwardIt = forwardAnalyzer.Forward();
		while (forwardIt.State <= PathAnalyzer::State::AtRootDir)
			++forwardIt;

		if (forwardIt.State == PathAnalyzer::State::AtEnd)
			return path;

		PathAnalyzer backwardAnalyzer(path, PathAnalyzer::State::AtEnd);
		PathAnalyzer::Iterator backwardIt = backwardAnalyzer.Backward();

		if (backwardIt.Path.Data() == path.Data())
			return {};

		--backwardIt;
		return StringView(path.Data(), backwardIt.Path.DataEnd());
	}

}
