// File /Native/Engine/IO/Internal/PathComponentUtils.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// NOTE: This is an internal framework file and should not be used in user's code externally.

#pragma once

#include "../../Core/Environment.h"
#include "../../String/StringView.hpp"

#pragma once

namespace PenEngine::Internal
{
	StringView GetRootName(StringView path) noexcept;
	StringView GetRootDirectory(StringView path) noexcept;
	StringView GetFilename(StringView path) noexcept;
	StringView GetExtension(StringView path) noexcept;
	StringView GetStem(StringView path) noexcept;
	StringView GetRelativePath(StringView path) noexcept;
	StringView GetParentPath(StringView path) noexcept;
}
