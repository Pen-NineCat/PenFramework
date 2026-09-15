// File /Native/Engine/IO/Internal/PathAnalyzer.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// NOTE: This is an internal framework file and should not be used in user's code externally.

#include "PathAnalyzer.h"
#include "PathParseUtils.h"

namespace PenEngine::Internal
{
	PathAnalyzer::Iterator PathAnalyzer::Forward() noexcept
	{
		const char* start = GetForwardStartPointer();
		const char* end = m_rawPath.DataEnd();

		if (start == end)
		{
			m_state = State::AtEnd;
			m_processedPath = {};
			return Iterator{ .Analyzer = this, .State = m_state,.Path = m_processedPath };
		}

		switch (m_state)
		{
		case State::AtStart:
		{
			// 状态变化路径
			// AtStart -> AtRootName
			// AtStart -> AtRootName [[fallthrough]]

			const char* ret = ForwardTrySkipRootName(start, end);
			if (ret != nullptr)
			{
				// 成功找到根名称，状态更新为 AtRootName
				m_state = State::AtRootName;
				m_processedPath = StringView(start, ret);
				return Iterator{ .Analyzer = this, .State = m_state,.Path = m_processedPath };
			}

			// 会 fallthrough 的情况：
			// 1. Unix Like 系统：不存在RootName，直接尝试处理根目录分隔符
			// 2. 相对路径：无论是 Windows 还是 Unix，没有RootName，也直接进入下一步检查分隔符
			[[fallthrough]];
		}
		case State::AtRootName:
		{
			// 状态变化路径
			// AtRootName -> AtRootDir
			// AtRootName -> AtBlock

			// 尝试跳过所有连续的分隔符（例如 "/" 或 "\\"）
			// 此时 start 指向的是根名称之后（或者路径开头）的位置
			const char* ret = ForwardTrySkipAllSeparator(start, end);

			// 如果 ret != nullptr，代表发现了根目录分隔符
			// 场景包括：
			// 1. Windows: "C:/" -> 识别到 '/'，确立为绝对路径的根目录
			// 2. Unix Like: "/home" -> 识别到开头的 '/'，确立为绝对路径的根目录
			if (ret != nullptr)
			{
				m_state = State::AtRootDir;
				m_processedPath = StringView(start, ret);
				return Iterator{ .Analyzer = this, .State = m_state,.Path = m_processedPath };
			}

			// 如果没有找到根目录分隔符，说明不是绝对路径的根，或者是特殊形式的相对路径
			// 具体场景分析：
			// 1. Unix Like: 路径不以 '/' 开头 -> 确认为相对路径，直接解析第一个Block
			// 2. Windows:
			//    2.1 如果是从 AtStart fallthrough 过来的：没有盘符也没有根斜杠 -> 相对路径
			//    2.2 如果是从 AtRootName 进入的（即存在盘符 "C:" 但后面没有 "/"）：
			//        例如 "C:"。这是合法的 Windows 路径，含义是：
			//        - 指向 C 盘的当前工作目录
			//        - 如果当前进程不在 C 盘，则指向该驱动器上一次记录的工作目录
			//        - 如果该驱动器从未设置过工作目录，则隐式指向 "C:/"
			m_state = State::AtBlock;
			m_processedPath = StringView(start, ForwardTrySkipName(start, end));
			return Iterator{ .Analyzer = this, .State = m_state,.Path = m_processedPath };
		}

		case State::AtRootDir:
		{
			// 状态变化路径
			// AtRootDir -> AtBlock

			// 已经识别了根目录（如 "/" 或 "C:/"），接下来必然是第一个块

			m_state = State::AtBlock;
			m_processedPath = StringView(start, ForwardTrySkipName(start, end));

			return Iterator{ .Analyzer = this, .State = m_state,.Path = m_processedPath };
		}

		case State::AtBlock:
		{
			// 状态变化路径
			// AtBlock -> AtBlock
			// AtBlock -> AtEnd

			// 当前刚处理完一个文件名，接下来需要查找是否有后续的分隔符和文件名

			// 尝试跳过当前位置开始的所有分隔符
			const char* blockStart = ForwardTrySkipAllSeparator(start, end);
			DEBUG_VERIFY_REPORT(blockStart != nullptr, "Invalid Pointer Argument");
			// 如果跳过分隔符后没有到达末尾，说明后面可能还有内容
			if (blockStart != end)
			{
				// 跳过接下来的名称部分（文件或目录名）
				const char* blockEnd = ForwardTrySkipName(blockStart, end);

				// 如果跳过分隔符后到达了末尾，或者跳过分隔符后没有解析到有效的名称
				// 这意味着路径以分隔符结尾（例如 "path/to/dir/"），或者剩余部分全是无效字符
				// 通过Normalizer处理过的路径不可能以分隔符结尾，这说明有路径调用了BuildRawPath
				// 并且传入了一个无法被视作AtBlock解析的路径
				// 例如"BuildRawPath(xxx/xxx/)"
				DEBUG_VERIFY_REPORT(blockEnd != nullptr, "Normalized path string should not end with separator.");

				// 状态保持为 AtBlock，因为要连续返回路径中的各个层级名称
				// 
				// 注意，参照cppreference，filename只返回文件名部分
				// https://cppreference.cn/w/cpp/filesystem/path/filename

				m_state = State::AtBlock;
				m_processedPath = StringView(blockStart, blockEnd);
				return Iterator{ .Analyzer = this, .State = m_state,.Path = m_processedPath };
			}

		// 没有搜索到新的Block，直接返回空
			m_state = State::AtEnd;
			m_processedPath = StringView(start, blockStart);
			return Iterator{ this, m_state, m_processedPath };
		}
		case State::AtEnd:
			break;
		}
		std::unreachable();
	}

	PathAnalyzer::Iterator PathAnalyzer::Backward() noexcept
	{
		const char* start = GetBackwardStartPointer() - 1;
		const char* end = m_rawPath.Data() - 1;

		if (start == end)
		{
			m_state = State::AtStart;
			m_processedPath = {};
			return Iterator{ .Analyzer = this,.State = m_state,.Path = m_processedPath };
		}

		switch (m_state)
		{
		case State::AtEnd:
		{
			// 对于normalize过的字符串，如果其最后为分隔符，其只能为
			// "/"
			// "C:/"
			// 两种绝对路径
			const char* separatorStart = BackwardTrySkipAllSeparator(start, end);
			if (separatorStart != nullptr)
			{
				if (separatorStart == end)
				{
					// 说明路径为"/"
					m_state = State::AtRootDir;
					m_processedPath = StringView(end + 1, start + 1);
					return Iterator{ .Analyzer = this, .State = m_state,.Path = m_processedPath };
				}

				const char* rootNameStart = BackwardTrySkipRootName(separatorStart, end);

				DEBUG_VERIFY_REPORT(rootNameStart != nullptr, "Invalid Pointer Argument");

				m_state = State::AtRootDir;
				m_processedPath = StringView(start, rootNameStart);
				return Iterator{ .Analyzer = this, .State = m_state,.Path = m_processedPath };
			}

			const char* rootNameStart = BackwardTrySkipRootName(start, end);
			if (rootNameStart == end)
			{
				m_state = State::AtRootName;
				m_processedPath = StringView(rootNameStart + 1, start + 1);
				return Iterator{ .Analyzer = this, .State = m_state,.Path = m_processedPath };
			}

			m_state = State::AtBlock;
			m_processedPath = StringView(BackwardTrySkipName(start, end) + 1, start + 1);
			return Iterator{ .Analyzer = this, .State = m_state,.Path = m_processedPath };
		}
		case State::AtBlock:
		{
			const char* separatorStart = BackwardTrySkipAllSeparator(start, end);
			if (separatorStart == end)
			{
				m_state = State::AtRootDir;
				m_processedPath = StringView(end + 1, start + 1);
				return Iterator{ .Analyzer = this, .State = m_state,.Path = m_processedPath };
			}

			const char* rootNameStart = nullptr;
			if (separatorStart != nullptr)
				rootNameStart = BackwardTrySkipRootName(separatorStart, end);
			else
				rootNameStart = BackwardTrySkipRootName(start, end);

			if (rootNameStart == end)
			{
				if (separatorStart != nullptr)
				{
					m_state = State::AtRootDir;
					m_processedPath = StringView(separatorStart + 1, start + 1);
					return Iterator{ .Analyzer = this, .State = m_state,.Path = m_processedPath };
				}

				m_state = State::AtRootName;
				m_processedPath = StringView(rootNameStart + 1, start + 1);
				return Iterator{ .Analyzer = this, .State = m_state,.Path = m_processedPath };
			}

			m_state = State::AtBlock;
			m_processedPath = StringView(BackwardTrySkipName(separatorStart, end) + 1, separatorStart + 1);
			return Iterator{ .Analyzer = this, .State = m_state,.Path = m_processedPath };
		}
		case State::AtRootDir:
		{
			m_state = State::AtRootName;
			m_processedPath = StringView(end + 1, start + 1);
			return Iterator{ .Analyzer = this, .State = m_state,.Path = m_processedPath };
		}
		case State::AtRootName:
		case State::AtStart:
			break;
		}

		std::unreachable();
	}

	const char* PathAnalyzer::GetForwardStartPointer() const noexcept
	{
		switch (m_state)
		{
		case State::AtStart:
			return m_rawPath.Data();
		case State::AtRootName:
		case State::AtRootDir:
		case State::AtBlock:
			return m_processedPath.DataEnd();
		case State::AtEnd:
			return m_rawPath.DataEnd();
		}

		std::unreachable();
	}

	const char* PathAnalyzer::GetBackwardStartPointer() const noexcept
	{
		switch (m_state)
		{
		case State::AtStart:
		case State::AtRootName:
			return m_rawPath.Data();
		case State::AtRootDir:
		case State::AtBlock:
			return m_processedPath.Data();
		case State::AtEnd:
			return m_rawPath.DataEnd();
		}

		std::unreachable();
	}

}