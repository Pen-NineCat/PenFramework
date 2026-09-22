// File /Native/Engine/OSPlatform/Windows/ComInitializer.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../../Core/Environment.h"
#include "../../DebugTools/DebugVerify.hpp"
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