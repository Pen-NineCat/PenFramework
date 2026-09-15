// File /Native/Engine/IO/Internal/PathNormalizer.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// NOTE: This is an internal framework file and should not be used in user's code externally.

#pragma once
#include "../../Core/Environment.h"

#include "../../String/StringView.hpp"

namespace PenEngine::Internal
{
	class PathNormalizer
	{
	public:
		enum class State : U8
		{
			AtStart,
			AtRootDir,
			AtRootName,
			AtBlock,
			AtEnd
		};

		struct Iterator
		{
			PathNormalizer* Normalizer;
			State CurrentState;
			StringView Block;


			Iterator& operator=(const Iterator&) noexcept = default;

			Iterator& operator++() noexcept
			{
				return *this = Normalizer->Search();
			}
		};

		PathNormalizer(StringView path, State state) noexcept : m_rawPath(path), m_state(state) {}

		Iterator Search() noexcept;
	private:
		const char* GetStartPointer() const noexcept;

		StringView m_rawPath;
		StringView m_processedPath;
		State m_state;
	};
}