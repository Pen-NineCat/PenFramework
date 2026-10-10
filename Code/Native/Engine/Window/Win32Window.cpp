// File /Code/Native/Engine/Window/Win32Window.cpp
//
// SPDX - License - Identifier: MIT
// Copyright(c) 2026 - Present Pen - NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "Win32Window.h"

#include "../OS/Win/Windows.h"

namespace
{
	LRESULT DefaultWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		#ifdef PENFRAMEWORK_OS_WIN64
		auto window = reinterpret_cast<PenEngine::Win32Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
		#else
		auto window = reinterpret_cast<PenEngine::Win32Window*>(GetWindowLong(hWnd, GWL_USERDATA));
		#endif
		return window ? window->WndProc(hWnd, msg, wParam, lParam) : DefWindowProc(hWnd, msg, wParam, lParam);
	}
}

namespace PenEngine
{
	WindowHandleData Win32Window::Create(const WindowInitContext& ctx)
	{
		m_title.ConvertFrom(ctx.Title);
		m_hInstance = ctx.HInstance;

		WNDCLASSEX desc =
		{
			.cbSize = sizeof(WNDCLASSEX),
			.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
			.lpfnWndProc = DefaultWndProc,
			.cbClsExtra = 0,
			.cbWndExtra = 0,
			.hInstance = ctx.HInstance,
			.hIcon = nullptr,
			.hCursor = nullptr,
			.hbrBackground = nullptr,
			.lpszMenuName = m_title.Data(),
			.lpszClassName = m_title.Data(),
			.hIconSm = nullptr
		};

		RegisterClassEx(&desc);

		m_hWnd = CreateWindowEx(
			0,
			m_title.Data(),
			m_title.Data(),
			WS_OVERLAPPEDWINDOW | WS_VISIBLE | (ctx.Maximized ? WS_MAXIMIZE : 0),
			0,
			0,
			ctx.Width,
			ctx.Height,
			nullptr,
			nullptr,
			ctx.HInstance,
			nullptr
		);

		#ifdef PENFRAMEWORK_OS_WIN64
		SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		#else
		SetWindowPtr(m_hWnd, GWL_USERDATA, reinterpret_cast<LONG>(this));
		#endif

		return { .Hwnd = m_hWnd };
	}

	bool Win32Window::TranslateWindowEvent()
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

	std::span<IEngineEvent*> Win32Window::CollectWindowEvent()
	{
		return m_currentWindowEventList;
	}

	void Win32Window::ClearWindowEventList()
	{
		m_currentWindowEventList.clear();
	}

	LRESULT Win32Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
	{
		return DefaultWndProc(hWnd, msg, wParam, lParam);
	}
}
