// File /Native/Engine/IO/Path.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "Path.h"

#include "Internal/PathAnalyzer.h"
#include "Internal/PathNormalizer.h"
#include "Internal/PathParseUtils.h"
#include "Internal/PathComponentUtils.h"

#ifdef PENFRAMEWORK_OS_WIN32
#include "../OS/Windows/Windows.h"
#else // PENFRAMEWORK_OS_WIN32
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#endif // PENFRAMEWORK_OS_WIN32

namespace PenEngine
{
	Path& Path::operator=(const Path& path)
	{
		if (&path != this)
			m_path = path.m_path;

		return *this;
	}

	Path& Path::operator=(Path&& path) noexcept
	{
		if (&path != this)
			m_path = std::move(path.m_path);

		return *this;
	}

	Path::Path(const String& path) : Path(StringView(path)) {}

	Path::Path(StringView path) : m_path(Normalize(path)) {}

	Path::Path(const char* path) : Path(StringView(path)) {}

	Path::Path(const std::filesystem::path& path) : Path(StringView(path.string())) {}

	Path::Path(PathView path) : Path(path.View()) {}

	Path& Path::operator=(std::nullopt_t) noexcept
	{
		m_path.Clear();
		return *this;
	}

	Path& Path::operator=(const String& path)
	{
		return *this = StringView(path);
	}

	Path& Path::operator=(StringView path)
	{
		m_path = Normalize(path);
		return *this;
	}

	Path& Path::operator=(const char* path)
	{
		return *this = StringView(path);
	}

	Path& Path::operator=(const std::string& path)
	{
		return *this = StringView(path);
	}

	Path& Path::operator=(std::string_view path)
	{
		return *this = StringView(path);
	}

	Path& Path::operator=(PathView path)
	{
		return *this = path.View();
	}

	Path Path::BuildRawPath(StringView path)
	{
		Path tmp;
		tmp.m_path = path;
		return tmp;
	}

	Path& Path::operator/=(const Path& path)
	{
		// 实现参照 https://cppreference.cn/w/cpp/filesystem/path/append
		// 
		// 若 p.is_absolute() || (p.has_root_name() && p.root_name() != root_name())，则如同通过 operator=(p)，以 p 替换当前路径然后完成。
		//  * 否则，若 p.has_root_directory()，则从* this 的通用格式路径名移除任何根目录和整个相对路径。
		//	* 否则，若 has_filename() || (!has_root_directory() && is_absolute())，则追加 path::preferred_separator 到 * this 的通用格式。
		//	* 无论哪种方式，都接着追加 p 的原生格式路径名（从其通用格式中省去任何根名）到 * this 的原生格式。

		StringView pRootName = Internal::GetRootName(path.Data());

		if (path.IsAbsolute() || (!pRootName.Empty()) && (pRootName != Internal::GetRootName(m_path)))
			m_path.Clear();
		else if (path.HasRootDirectory())
			m_path = pRootName;
		else if (HasFilename() || (!HasRootDirectory() && IsAbsolute()))
			m_path += '/';

		m_path += path.m_path;
		return *this;
	}

	Path& Path::operator/=(const String& path)
	{
		return *this /= Path(StringView(path));
	}

	Path& Path::operator/=(StringView path)
	{
		return *this /= Path(path);
	}

	Path& Path::operator/=(const char* path)
	{
		return *this /= Path(StringView(path));
	}

	Path& Path::operator/=(const std::string& path)
	{
		return *this /= Path(StringView(path));
	}

	Path& Path::operator/=(std::string_view path)
	{
		return *this /= Path(StringView(path));
	}

	Path& Path::operator/=(PathView path)
	{
		return *this /= Path(path.View());
	}

	Path& Path::operator+=(const Path& path)
	{
		m_path += path.m_path;
		return *this;
	}

	Path& Path::operator+=(const String& path)
	{
		m_path += path;
		return *this;
	}

	Path& Path::operator+=(StringView path)
	{
		m_path += path;
		return *this;
	}

	Path& Path::operator+=(const char* path)
	{
		m_path += path;
		return *this;
	}

	Path& Path::operator+=(const std::string& path)
	{
		m_path += path;
		return *this;
	}

	Path& Path::operator+=(std::string_view path)
	{
		m_path += path;
		return *this;
	}

	Path& Path::operator+=(PathView path)
	{
		m_path += path.View();
		return *this;
	}

	bool Path::IsAbsolute() const noexcept
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
		#else  // //PENFRAMEWORK_OS_WIN32
		return HasRootDirectory();
		#endif //PENFRAMEWORK_OS_WIN32
	}

	bool Path::IsRelative() const noexcept
	{
		return !IsAbsolute();
	}

	bool Path::Empty() const noexcept
	{
		return m_path.Empty();
	}

	Path Path::RootName() const
	{
		return BuildRawPath(Internal::GetRootName(m_path));
	}

	Path Path::RootPath() const
	{
		return (BuildRawPath(Internal::GetRootName(m_path)) += Internal::GetRootDirectory(m_path));
	}

	Path Path::RootDirectory() const
	{
		return BuildRawPath(Internal::GetRootDirectory(m_path));
	}

	Path Path::Filename() const
	{
		return BuildRawPath(Internal::GetFilename(m_path));
	}

	Path Path::Extension() const
	{
		return BuildRawPath(Internal::GetExtension(m_path));
	}

	Path Path::Stem() const
	{
		return BuildRawPath(Internal::GetStem(m_path));
	}

	Path Path::ParentPath() const
	{
		return BuildRawPath(Internal::GetParentPath(m_path));
	}

	Path Path::RelativePath() const
	{
		return BuildRawPath(Internal::GetRelativePath(m_path));
	}

	Path& Path::ReplaceFilename(const Path& filename)
	{
		return RemoveFilename() /= filename;
	}

	Path& Path::ReplaceExtension(const Path& extension)
	{
		if (Usize currentExtensionSize = Internal::GetExtension(m_path).Size(); currentExtensionSize > 0)
			m_path = m_path.Substr(0, m_path.Size() - currentExtensionSize);

		if (extension.Empty())
			return *this;

		if (extension.Data()[0] != '.')
			m_path += '.';

		m_path += extension.m_path;

		return *this;
	}

	Path& Path::RemoveFilename()
	{
		if (Usize filenameSize = Internal::GetFilename(m_path).Size(); filenameSize > 0)
			m_path = m_path.Substr(0, m_path.Size() - filenameSize);
		return *this;
	}

	Usize Path::Size() const noexcept
	{
		return m_path.Size();
	}

	const char* Path::Data() const noexcept
	{
		return m_path.Data();
	}

	void Path::Clear() noexcept
	{
		m_path.Clear();
	}

	StringView Path::View() const noexcept
	{
		return m_path;
	}

	bool Path::HasRootName() const noexcept
	{
		return !Internal::GetRootName(m_path).Empty();
	}

	bool Path::HasRootPath() const noexcept
	{
		return !(Internal::GetRootName(m_path).Empty() && Internal::GetRootDirectory(m_path).Empty());
	}

	bool Path::HasRootDirectory() const noexcept
	{
		return !Internal::GetRootDirectory(m_path).Empty();
	}

	bool Path::HasRelativePath() const noexcept
	{
		return !Internal::GetRelativePath(m_path).Empty();
	}

	bool Path::HasParentPath() const noexcept
	{
		return !Internal::GetParentPath(m_path).Empty();
	}

	bool Path::HasFilename() const noexcept
	{
		return !Internal::GetFilename(m_path).Empty();
	}

	bool Path::HasStem() const noexcept
	{
		return !Internal::GetStem(m_path).Empty();
	}

	bool Path::HasExtension() const noexcept
	{
		return !Internal::GetExtension(m_path).Empty();
	}

	void Path::MoveToUpLayer() noexcept
	{
		if (StringView filename = Internal::GetFilename(m_path); !filename.Empty())
			m_path.Resize(filename.Size() + 1);
	}

	Path Path::ToAbsolutePath() const
	{
		if (IsAbsolute())
			return *this;

		Path path;
		path.m_path = ToAbsolutePathString();

		return path;
	}

	Path Path::GetApplicationPath()
	{
		#ifdef PENFRAMEWORK_OS_WIN32
		// 1. 获取当前可执行文件的完整路径
		wchar_t pathBuffer[MAX_PATH];
		// GetModuleFileNameW 是 Unicode 版本
		// 第一个参数为 NULL，表示获取当前进程的主模块（.exe）
		DWORD pathLength = GetModuleFileName(nullptr, pathBuffer, MAX_PATH);

		if (pathLength == 0)
			return {};

		// 2. 从完整路径中提取目录部分
		WString fullPath(pathBuffer);

		// 查找最后一个反斜杠 '\' 的位置

		if (Usize lastSlashPos = CharRFind(L'\\', pathBuffer, std::char_traits<wchar_t>::length(pathBuffer), StringView::NPos); lastSlashPos != WString::NPos)
		{
			// 截取反斜杠之前的部分，即为目录路径
			Path res;
			res.ConvertDirect(fullPath.Substr(0, lastSlashPos));
			return res;
		}

		// 理论上不会执行到这里
		return {};
		#else //PENFRAMEWORK_OS_WIN32

		char pathBuffer[PATH_MAX];
		// 读取 /proc/self/exe 符号链接指向的真实路径
		// readlink 不会在末尾添加 '\0'，需要手动添加
		ssize_t len = readlink("/proc/self/exe", pathBuffer, sizeof(pathBuffer) - 1);

		if (len == -1)
			return{};

		pathBuffer[len] = '\0'; // 手动添加字符串结束符

		// 从完整路径中提取目录部分
		if (Usize lastSlashPos = CharFindLastOf('\\', pathBuffer, std::char_traits<char>::length(pathBuffer), 0); lastSlashPos != String::npos)
		{
			return Path(fullPath.substr(0, lastSlashPos));
		}

		return {};

		#endif //PENFRAMEWORK_OS_WIN32
	}

	String Path::ToAbsolutePathString() const
	{
		if (IsAbsolute())
			return m_path;

		#ifdef PENFRAMEWORK_OS_WIN32
		WString absPath;

		WString oldPath = m_path.ConvertToString<wchar_t>();
		std::vector<wchar_t> buffer(std::max(static_cast<Usize>(MAX_PATH), m_path.Size() + 1));

		DWORD relLen = GetFullPathName(oldPath.Data(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
		if (relLen > buffer.size())
		{
			buffer.resize(relLen);
			relLen = GetFullPathName(oldPath.Data(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
		}
		if (relLen != 0)
			absPath.ConvertAndPushBack(buffer.data(), relLen);

		String res = absPath.ConvertToString<char>();
		res.Replace('\\', '/');
		return res;

		#else //PENFRAMEWORK_OS_WIN32
		char resolvedPath[MAX_PATH];
		if (realpath(m_path.Data(), resolvedPath) != nullptr)
		{
			WString res;
			res.ConvertAndPushBack(resolvedPath);
		}
		return {};
		#endif //PENFRAMEWORK_OS_WIN32
	}

	WString Path::ToAbsolutePathWString() const
	{
		if (IsAbsolute())
			return m_path.ConvertToString<wchar_t>();

		#ifdef PENFRAMEWORK_OS_WIN32
		WString absPath;

		WString oldPath = m_path.ConvertToString<wchar_t>();
		std::vector<wchar_t> buffer(std::max(static_cast<Usize>(MAX_PATH), m_path.Size() + 1));

		DWORD relLen = GetFullPathName(oldPath.Data(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
		if (relLen > buffer.size())
		{
			buffer.resize(relLen);
			relLen = GetFullPathName(oldPath.Data(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
		}
		if (relLen != 0)
			absPath.ConvertAndPushBack(buffer.data(), relLen);

		absPath.Replace(L'\\', L'/');

		return absPath;

		#else //PENFRAMEWORK_OS_WIN32
		char resolvedPath[MAX_PATH];
		if (realpath(m_path.Data(), resolvedPath) != nullptr)
		{
			WString res;
			res.ConvertAndPushBack(resolvedPath);
		}
		return {};
		#endif //PENFRAMEWORK_OS_WIN32
	}

	Path::operator PathView() const noexcept
	{
		return PathView::BuildRawPath(m_path);
	}

	Path Path::operator/(const Path& path) const
	{
		Path res(*this);
		res /= path;
		return res;
	}

	Path Path::operator/(const String& path) const
	{
		Path res(*this);
		res /= path;
		return res;
	}

	Path Path::operator/(StringView path) const
	{
		Path res(*this);
		res /= path;
		return res;
	}

	Path Path::operator/(const char* path) const
	{
		Path res(*this);
		res /= path;
		return res;
	}

	Path Path::operator/(const std::string& path) const
	{
		Path res(*this);
		res /= path;
		return res;
	}

	Path Path::operator/(std::string_view path) const
	{
		Path res(*this);
		res /= path;
		return res;
	}

	Path Path::operator/(PathView path) const
	{
		Path res(*this);
		res /= path;
		return res;
	}

	String Path::Normalize(StringView path)
	{
		// 实现参照 https://cppreference.cn/w/cpp/filesystem/path

		// 规范路径名具有以下语法

		//	1.root - name(可选)：在具有多个根的文件系统（例如 "C:" 或 "//myserver"）上标识根。在有歧义的情况下，形成有效 root - name 的最长字符序列被视为 root - name。标准库除了操作系统 API 所理解的根名之外，还可以定义额外的根名。
		//	2.root - directory(可选)：一个目录分隔符，如果存在，则将此路径标记为绝对路径。如果它不存在（且除根名之外的第一个元素是文件名），则该路径是相对路径，需要另一个路径作为起始位置才能解析为文件名。
		//	3.零个或多个以下内容
		//	file - name：非目录分隔符或首选目录分隔符的字符序列（操作系统或文件系统可能会施加额外的限制）。此名称可以标识文件、硬链接、符号链接或目录。识别两个特殊的 file - name
		//		dot：由一个点字符.组成的文件名是表示当前目录的目录名。
		//		dot - dot：由两个点字符 ..组成的文件名是表示父目录的目录名。
		//	directory - separator：正斜杠字符 / 或作为 path::preferred_separator 提供的替代字符。如果此字符重复，则将其视为单个目录分隔符： / usr///////lib 与 /usr/lib 相同。
		//	路径可以通过遵循以下算法进行规范化

		//	1.如果路径为空，则停止（空路径的规范形式是空路径）。
		//	2.将每个 directory - separator（可能由多个斜杠组成）替换为单个 path::preferred_separator。
		//	3.将 root - name 中的每个斜杠字符替换为 path::preferred_separator。
		//	4.删除每个 dot 及紧随其后的任何 directory - separator。
		//	5.删除每个非 dot - dot 文件名，其后紧跟一个 directory - separator 和一个 dot - dot，以及紧随其后的任何 directory - separator。
		//	6.如果存在 root - directory，则删除所有 dot - dot 和紧随其后的任何 directory - separator。
		//	7.如果最后一个文件名是 dot - dot，则删除任何尾随的 directory - separator。
		//	8.如果路径为空，则添加一个 dot（. / 的规范形式是.）。

		if (path.Empty()) // 1.如果路径为空，则停止（空路径的规范形式是空路径）。
			return {};

		String res(path.Size());

		std::vector<StringView> stack;

		Internal::PathNormalizer normalizer(path, Internal::PathNormalizer::State::AtStart);

		Internal::PathNormalizer::Iterator it = normalizer.Search();

		bool isAbsolute = false;

		// 第一次Search只会出现三种状态转移
		// 1.AtRootName 由 AtStart -> AtRootName
		// 2.AtRootDir 由 AtStart -> AtRootName [[fallthrough]] -> AtRootDir
		// 3.AtBlock 由 AtStart -> AtRootName [[fallthrough]] -> AtBlock
		if (it.CurrentState == Internal::PathNormalizer::State::AtRootName)
		{
			// Windows 绝对路径
			// "C:/(xxx)"

			// 为了避免根路径被错误弹出，直接将根插入到结果中
			res += it.Block;

			// 3.将 root - name 中的每个斜杠字符替换为 path::preferred_separator
			for (char& ch : res)
				if (Internal::IsValidSeparator(ch))
					ch = PreferredSeparator;

			isAbsolute = true;

			// 定向到第一个Block
			++it; // AtRootName -> AtRootDir
			++it; // AtRootDir -> AtBlock
		}
		else if (it.CurrentState == Internal::PathNormalizer::State::AtRootDir)
		{
			// Unix Like 绝对路径
			// "/(xxxx)"
			isAbsolute = true;

			// 定向到第一个Block
			++it; // AtRootDir -> AtBlock
		}
		// it.CurrentState == AtBlock 相对路径，直接分析路径
		// "xx/xxx"
		// "C:xx/xxx"

		while (it.CurrentState < Internal::PathNormalizer::State::AtEnd)
		{
			if (it.Block == "..")
			{
				if (isAbsolute)
				{
					// 6.如果存在 root - directory，则删除所有 dot - dot 和紧随其后的任何 directory - separator
					// 即如果是绝对路径，且栈为空，则不进行操作
					if (!stack.empty())
						stack.pop_back();
				}
				else
				{
					// 对于../../../../xxx 这种相对路径，需要保留".."
					// 不存在xxx/xxx/xx/../../../xx这种中间出现多个".."的情形，因为栈内不存在xx/..这种情形
					// 即如果back() == ".." 则前面的所有部分都是".."
					if (stack.empty() || stack.back() == "..")
						stack.push_back(it.Block);
					else
						// 5.删除每个非 dot - dot 文件名，其后紧跟一个 directory - separator 和一个 dot - dot，以及紧随其后的任何 directory - separator
						stack.pop_back();
				}
			}
			else if (it.Block != '.')
			{
				stack.push_back(it.Block);
			}

			// 4.删除每个 dot 及紧随其后的任何 directory - separator
			// 对于it.Block == '.' 直接continue

			++it;
		}

		for (auto& block : stack)
		{
			// 2.将每个 directory - separator（可能由多个斜杠组成）替换为单个 path::preferred_separator
			// 由于block就是按dir - sep切分的，故可直接按block + PreferredSeparator 拼接
			// 7.如果最后一个文件名是 dot - dot，则删除任何尾随的 directory - separator
			// 不需要特判，因为无论如何，最后的directory - separator都被舍去了
			// 如果isAbsolute == true，则在Windows环境下会补全"C:/"的PreferredSeparator，如果是Unix Like环境，则会补全第一个PreferredSeparator
			if (isAbsolute || !res.Empty())
				res += PreferredSeparator;
			res += block;
		}

		if (res.Empty())
		{
			if (isAbsolute)
				res += '/'; // 说明原路径就是"/......."unix like绝对路径，但是后续部分全部被舍去了 
			else
				res += '.'; // 8.如果路径为空，则添加一个 dot（. / 的规范形式是.）
		}

		res.ShrinkToFit();

		return res;
	}

	void Path::Normalize(String& path)
	{
		if (path.Empty())
			return;

		std::vector<String> stack;

		Internal::PathNormalizer normalizer(path, Internal::PathNormalizer::State::AtStart);
		auto it = normalizer.Search();

		bool isAbsolute = false;
		String rootName;

		if (it.CurrentState == Internal::PathNormalizer::State::AtRootName)
		{
			rootName = it.Block;
			for (char& ch : rootName)
				if (Internal::IsValidSeparator(ch))
					ch = PreferredSeparator;
			isAbsolute = true;
			++it;
			++it;
		}
		else if (it.CurrentState == Internal::PathNormalizer::State::AtRootDir)
		{
			isAbsolute = true;
			++it;
		}

		while (it.CurrentState < Internal::PathNormalizer::State::AtEnd)
		{
			if (it.Block == "..")
			{
				if (isAbsolute)
				{
					if (!stack.empty())
						stack.pop_back();
				}
				else
				{
					if (stack.empty() || stack.back() == "..")
						stack.emplace_back(it.Block);
					else
						stack.pop_back();
				}
			}
			else if (it.Block != ".")
			{
				stack.emplace_back(it.Block);
			}
			++it;
		}

		path.Clear();

		if (!rootName.Empty())
			path += rootName;

		for (auto& block : stack)
		{
			if (isAbsolute || !path.Empty())
				path += PreferredSeparator;
			path += block;
		}

		if (path.Empty())
		{
			if (isAbsolute)
				path += '/';
			else
				path += '.';
		}
	}
}