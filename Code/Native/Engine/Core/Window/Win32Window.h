// File /Native/Engine/Core/Window/Win32Window.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "WindowInitContext.hpp"
#include "../CoreEvent.hpp"
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
		WString m_title;
		CoreApplication* m_application = nullptr;
		U32 m_windowX = 0;
		U32 m_windowY = 0;
		U32 m_windowWidth;
		U32 m_windowHeight;
		HWND m_hwnd = nullptr;
		HINSTANCE m_hInstance = nullptr;
		CoreEvent m_events;
		bool m_alreadyClose = false;
	};
}
