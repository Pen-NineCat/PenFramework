// File /Native/Engine/IO/Internal/PathNormalizer.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// NOTE: This is an internal framework file and should not be used in user's code externally.

#include "PathNormalizer.h"
#include "PathParseUtils.h"

namespace PenEngine::Internal
{
	PathNormalizer::Iterator PathNormalizer::Search() noexcept
	{
		const char* start = GetStartPointer();
		const char* end = m_rawPath.Data() + m_rawPath.Size();

		if (start == end)
		{
			m_state = State::AtEnd;
			m_processedPath = {};
			return Iterator{ .Normalizer = this,.CurrentState = m_state,.Block = m_processedPath };
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
					// 返回例如"C:"的块
					return Iterator{ .Normalizer = this,.CurrentState = m_state,.Block = m_processedPath };
				}

				// 会 fallthrough 的情况：
				// 1. Unix Like 系统：不存在根名称（RootName），直接尝试处理根目录分隔符
				// 2. 相对路径：无论是 Windows 还是 Unix，如果没有盘符，也直接进入下一步检查分隔符
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
					return Iterator{ this, m_state, m_processedPath };
				}

				// 如果没有找到根目录分隔符，说明不是绝对路径的根，或者是特殊形式的相对路径
				// 具体场景分析：
				// 1. Unix Like: 路径不以 '/' 开头 -> 确认为相对路径，直接解析第一个文件名
				// 2. Windows:
				//    2.1 如果是从 AtStart fallthrough 过来的：没有盘符也没有根斜杠 -> 相对路径
				//    2.2 如果是从 AtRootName 进入的（即存在盘符 "C:" 但后面没有 "/"）：
				//        例如 "C:"。这是合法的 Windows 路径，含义是：
				//        - 指向 C 盘的当前工作目录
				//        - 如果当前进程不在 C 盘，则指向该驱动器上一次记录的工作目录
				//        - 如果该驱动器从未设置过工作目录，则隐式指向 "C:/"
				m_state = State::AtBlock;
				m_processedPath = StringView(start, ForwardTrySkipName(start, end));
				return Iterator{ this, m_state, m_processedPath };
			}
			case State::AtRootDir:
			{
				// 状态流转：AtRootDir -> AtBlock

				// 已经识别了根目录（如 "/" 或 "C:/"），接下来必然是第一个块

				m_state = State::AtBlock;

				const char* blockStart = m_processedPath.Data() + m_processedPath.Size();
				const char* blockEnd = ForwardTrySkipName(start, end);

				m_processedPath = StringView(start, blockEnd);

				return Iterator{ this, m_state, StringView(blockStart,blockEnd) };
			}
			case State::AtBlock:
			{
				// 当前刚处理完一个块，接下来需要查找是否有后续的分隔符和块
				// 尝试跳过当前位置开始的所有分隔符
				const char* blockStart = ForwardTrySkipAllSeparator(start, end);
				DEBUG_VERIFY_REPORT(blockStart != nullptr, "Invalid Pointer Argument");
				// 如果跳过分隔符后没有到达末尾，说明后面可能还有内容
				if (blockStart != end)
				{
					// 尝试跳过接下来的名称部分（文件或目录名）
					const char* blockEnd = ForwardTrySkipName(blockStart, end);

					// 如果成功解析到名称
					if (blockEnd != nullptr)
					{
						m_state = State::AtBlock;
						m_processedPath = StringView(blockStart, blockEnd);
						return Iterator{ this, m_state, m_processedPath };
					}
				}

				// 如果跳过分隔符后到达了末尾，或者跳过分隔符后没有解析到有效的名称
				// 这意味着路径以分隔符结尾（例如 "path/to/dir/"），或者剩余部分全是无效字符
				// 直接视作到达末尾，因为path的normalize函数只会拼接block
				m_state = State::AtEnd;
				m_processedPath = StringView(start, blockStart);

				// 没有搜索到新的Block，直接返回空
				return Iterator{ this, m_state, m_processedPath };
			}
			case State::AtEnd:
				break;
		}

		std::unreachable();
	}

	const char* PathNormalizer::GetStartPointer() const noexcept
	{
		switch (m_state)
		{
			case State::AtStart:
				return m_rawPath.Data();
			case State::AtRootDir:
			case State::AtRootName:
			case State::AtBlock:
				return m_processedPath.DataEnd();
			case State::AtEnd:
				return m_rawPath.DataEnd();
		}

		std::unreachable();
	}
}