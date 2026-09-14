// File /Native/Engine/Core/Window/Win32Window.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "Win32Window.h"


static PenEngine::Win32Window* g_win32Window = nullptr;

extern "C"
{
	static LRESULT DefaultWndProc(HWND hWnd, UINT msg, WPARAM wParam,
													   LPARAM lParam)
	{
		return g_win32Window ? g_win32Window->WndProc(hWnd, msg, wParam, lParam) : DefWindowProc(hWnd, msg, wParam, lParam);
	}
}

namespace PenEngine
{
	Win32Window::~Win32Window() noexcept
	{
		Close();
	}

	HWND Win32Window::Create(const WindowInitContext& context)
	{
		m_title = context.ApplicationTitle;
		m_hInstance = context.HInstance;
		m_application = context.CoreApplication;

		WNDCLASSEX desc =
		{
			.cbSize = sizeof(WNDCLASSEX),
			.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
			.lpfnWndProc = DefaultWndProc,
			.cbClsExtra = 0,
			.cbWndExtra = 0,
			.hInstance = context.HInstance,
			.hIcon = nullptr,
			.hCursor = nullptr,
			.hbrBackground = nullptr,
			.lpszMenuName = m_title.Data(),
			.lpszClassName = m_title.Data(),
			.hIconSm = nullptr
		};

		RegisterClassEx(&desc);

		m_hwnd = CreateWindowEx(
			0,
			m_title.Data(),
			m_title.Data(),
			WS_OVERLAPPEDWINDOW | WS_VISIBLE | (context.Maximize ? WS_MAXIMIZE : 0),
			0,
			0,
			context.Width,
			context.Height,
			nullptr,
			nullptr,
			context.HInstance,
			nullptr
		);

		return m_hwnd;
	}

	void Win32Window::Close() noexcept
	{
		if (m_alreadyClose)
			return;

		m_alreadyClose = true;

		UnregisterClass(m_title.Data(), m_hInstance);
		DestroyWindow(m_hwnd);
	}

	LRESULT Win32Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
	{
		return DefWindowProc(hWnd,msg,wParam,lParam);
	}

	bool Win32Window::DispatchWindowMessage()
	{
		MSG msg = {};
		
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				return false;

			if (msg.message == WM_KEYDOWN)
				TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		return true;
	}
}
