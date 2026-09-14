// File /Native/Engine/Core/Window/Win32Window.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "WindowInitContext.hpp"
#include "../../OS/Windows/Windows.h"

namespace PenEngine
{
	class CoreApplication;

	class Win32Window
	{
	public:
		Win32Window() noexcept = default;
		~Win32Window() noexcept;

		HWND Create(const WindowInitContext& context);
		void Close() noexcept;

		LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept;

		bool DispatchWindowMessage();
	private:
		String m_title;
		CoreApplication* m_application;
		HWND m_hwnd = nullptr;
		HINSTANCE m_hInstance = nullptr;
		bool m_alreadyClose;
	};
}
