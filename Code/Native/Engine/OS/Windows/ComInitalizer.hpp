// File /Engine/OSPlatform/Windows/ComInitializer.hpp
// This file is a part of PenFramework Project
// https://github.com/Pen-NineCat/PenFramework
// 
// Copyright (C) 2025 - Present PenNineCat. All rights reserved
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "../../Common/Environment.h"
#include "../../DebugTools/TerminalVerify.hpp"
#include "WindowsMinDef.h"
#include <objbase.h>

namespace PenEngine
{
	class ComInitializer
	{
	public:
		explicit ComInitializer(COINIT concurrencyModel = COINIT_APARTMENTTHREADED)
		{
			m_initResult = CoInitializeEx(nullptr, concurrencyModel | COINIT_DISABLE_OLE1DDE);

			DEBUG_VERIFY_REPORT(SUCCEEDED(m_initResult), "Failed to initialize COM library");
		}
		bool IsReady() const noexcept { return SUCCEEDED(m_initResult); }
		~ComInitializer() noexcept
		{
			if (SUCCEEDED(m_initResult))
				CoUninitialize();
		}
		ComInitializer(const ComInitializer&) = delete;
		ComInitializer(ComInitializer&&) = delete;
		ComInitializer& operator=(const ComInitializer&) = delete;
		ComInitializer& operator=(ComInitializer&&) = delete;
	private:
		HRESULT m_initResult;
	};
}