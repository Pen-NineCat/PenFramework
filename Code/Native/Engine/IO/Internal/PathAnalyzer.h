// File /Native/Engine/IO/Internal/PathAnalyzer.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// NOTE: This is an internal framework file and should not be used in user's code externally.

#pragma once

#include "../../String/String.hpp"

namespace PenEngine::Internal
{
	class PathAnalyzer
	{
	public:
		enum class State : U8
		{
			AtStart,
			AtRootName,
			AtRootDir,
			AtBlock,
			AtEnd,
		};
		struct Iterator
		{
			PathAnalyzer* Analyzer;
			State State;
			StringView Path;

			Iterator& operator++() noexcept
			{
				return *this = Analyzer->Forward();
			}

			Iterator& operator--() noexcept
			{
				return *this = Analyzer->Backward();
			}
		};

		PathAnalyzer(StringView path, State startState) noexcept : m_rawPath(path), m_state(startState) {}

		// 正向搜索
		Iterator Forward() noexcept;
		Iterator Backward() noexcept;

		const char* GetForwardStartPointer() const noexcept;
		const char* GetBackwardStartPointer() const noexcept;

		bool AtStart() const noexcept { return m_state == State::AtStart; }
		bool AtRootName() const noexcept { return m_state == State::AtRootName; }
		bool AtRootDir() const noexcept { return m_state == State::AtRootDir; }
		bool AtFilename() const noexcept { return m_state == State::AtBlock; }
		bool AtEnd() const noexcept { return m_state == State::AtEnd; }
	private:
		StringView m_rawPath;
		StringView m_processedPath;
		State m_state;
	};
}