// File /Native/Engine/Core/Window/Win32Window.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "Win32Window.h"
#include "../CoreApplication.h"

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
		m_title = context.ApplicationTitle.ConvertToString<wchar_t>();
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
			// 关闭流程SFM
			// -> WM_CLOSE
			// 广播关闭事件，如果有对象监听并且捕获，则不关闭窗口，反之则转入下一阶段
			// -> WM_DESTROY
			// PostQuit，转入下一阶段
			// -> WM_QUIT
		switch (msg)
		{
			case WM_CLOSE:
				if (m_application->PostEvent(m_events.Close.get()))
					return 0;
				return DefWindowProc(hWnd, msg, wParam, lParam);
			case WM_DESTROY:
				PostQuitMessage(0);
				return 0;
			case WM_LBUTTONDOWN:
			{
				I32 x = LOWORD(lParam);
				I32 y = HIWORD(lParam);
				m_events.Mouse->SetX(x);
				m_events.Mouse->SetY(y);
				m_events.Mouse->SetState(MouseState::LeftClick);
				m_events.Mouse->Start();
				m_application->PostEvent(m_events.Mouse.get());
				return 0;
			}
			case WM_RBUTTONDOWN:
			{
				I32 x = LOWORD(lParam);
				I32 y = HIWORD(lParam);
				m_events.Mouse->SetX(x);
				m_events.Mouse->SetY(y);
				m_events.Mouse->SetState(MouseState::RightClick);
				m_events.Mouse->Start();
				m_application->PostEvent(m_events.Mouse.get());
				return 0;
			}
			case WM_MBUTTONDOWN:
			{
				I32 x = LOWORD(lParam);
				I32 y = HIWORD(lParam);
				m_events.Mouse->SetX(x);
				m_events.Mouse->SetY(y);
				m_events.Mouse->SetState(MouseState::MiddleClick);
				m_events.Mouse->Start();
				m_application->PostEvent(m_events.Mouse.get());
				return 0;
			}
			case WM_LBUTTONUP:
			{
				I32 x = LOWORD(lParam);
				I32 y = HIWORD(lParam);
				m_events.Mouse->SetX(x);
				m_events.Mouse->SetY(y);
				m_events.Mouse->SetState(MouseState::LeftRelease);
				m_events.Mouse->Start();
				m_application->PostEvent(m_events.Mouse.get());
				return 0;
			}
			case WM_RBUTTONUP:
			{
				I32 x = LOWORD(lParam);
				I32 y = HIWORD(lParam);
				m_events.Mouse->SetX(x);
				m_events.Mouse->SetY(y);
				m_events.Mouse->SetState(MouseState::RightRelease);
				m_events.Mouse->Start();
				m_application->PostEvent(m_events.Mouse.get());
				return 0;
			}
			case WM_MBUTTONUP:
			{
				I32 x = LOWORD(lParam);
				I32 y = HIWORD(lParam);
				m_events.Mouse->SetX(x);
				m_events.Mouse->SetY(y);
				m_events.Mouse->SetState(MouseState::MiddleRelease);
				m_events.Mouse->Start();
				m_application->PostEvent(m_events.Mouse.get());
				return 0;
			}
			case WM_MOUSEMOVE:
			{
				I32 x = LOWORD(lParam);
				I32 y = HIWORD(lParam);
				static I32 lastX = 0, lastY = 0;
				I32 deltaX = x - lastX;
				I32 deltaY = y - lastY;
				lastX = x;
				lastY = y;

				m_events.MouseMove->SetX(static_cast<U32>(x));
				m_events.MouseMove->SetY(static_cast<U32>(y));
				m_events.MouseMove->SetDeltaX(deltaX);
				m_events.MouseMove->SetDeltaY(deltaY);
				m_events.MouseMove->Start();
				m_application->PostEvent(m_events.MouseMove.get());
				return 0;
			}
			case WM_KEYDOWN:
			{
				auto key = static_cast<VKey>(wParam);
				bool autoRepeat = LOWORD(lParam) > 0;
				auto modifyKey = ModifyKey::None;
				if (GetKeyState(VK_SHIFT) & 0x8000)
					modifyKey = static_cast<ModifyKey>(static_cast<int>(modifyKey) | static_cast<int>(ModifyKey::Shift));
				if (GetKeyState(VK_CONTROL) & 0x8000)
					modifyKey = static_cast<ModifyKey>(static_cast<int>(modifyKey) | static_cast<int>(ModifyKey::Ctrl));
				if (GetKeyState(VK_MENU) & 0x8000)
					modifyKey = static_cast<ModifyKey>(static_cast<int>(modifyKey) | static_cast<int>(ModifyKey::Alt));

				m_events.KeyPress->SetKey(key);
				m_events.KeyPress->SetModifyKey(modifyKey);
				m_events.KeyPress->SetAutoRepeat(autoRepeat);
				m_events.KeyPress->Start();
				m_application->PostEvent(m_events.KeyPress.get());
				return 0;
			}
			case WM_KEYUP:
			{
				auto key = static_cast<VKey>(wParam);
				auto modifyKey = ModifyKey::None;
				if (GetKeyState(VK_SHIFT) & 0x8000)
					modifyKey = static_cast<ModifyKey>(static_cast<int>(modifyKey) | static_cast<int>(ModifyKey::Shift));
				if (GetKeyState(VK_CONTROL) & 0x8000)
					modifyKey = static_cast<ModifyKey>(static_cast<int>(modifyKey) | static_cast<int>(ModifyKey::Ctrl));
				if (GetKeyState(VK_MENU) & 0x8000)
					modifyKey = static_cast<ModifyKey>(static_cast<int>(modifyKey) | static_cast<int>(ModifyKey::Alt));
				m_events.KeyRelease->SetKey(key);
				m_events.KeyRelease->SetModifyKey(modifyKey);
				m_events.KeyRelease->Start();
				m_application->PostEvent(m_events.KeyRelease.get());
				return 0;
			}
			case WM_SIZE:
			{
				U32 width = LOWORD(lParam);
				U32 height = HIWORD(lParam);
				U32 sizeType = wParam;

				m_events.WindowResize->SetOldWidth(m_windowWidth);
				m_events.WindowResize->SetOldHeight(m_windowHeight);
				m_events.WindowResize->SetWidth(width);
				m_events.WindowResize->SetHeight(height);
				m_events.WindowResize->Start();
				m_application->PostEvent(m_events.WindowResize.get());

				if (sizeType == SIZE_MINIMIZED)
				{
					m_events.WindowMinimize->SetWidth(width);
					m_events.WindowMinimize->SetHeight(height);
					m_events.WindowMinimize->Start();
					m_application->PostEvent(m_events.WindowMinimize.get());
				}
				else if (sizeType == SIZE_MAXIMIZED)
				{
					m_events.WindowMaximize->SetWidth(width);
					m_events.WindowMaximize->SetHeight(height);
					m_events.WindowMaximize->Start();
					m_application->PostEvent(m_events.WindowMaximize.get());
				}

				m_windowWidth = static_cast<U16>(width);
				m_windowHeight = static_cast<U16>(height);
				return 0;
			}
			case WM_MOVE:
			{
				I32 x = LOWORD(lParam);
				I32 y = HIWORD(lParam);

				m_events.WindowMove->SetOldX(m_windowX);
				m_events.WindowMove->SetOldY(m_windowY);
				m_events.WindowMove->SetX(x);
				m_events.WindowMove->SetY(y);
				m_events.WindowMove->Start();
				m_application->PostEvent(m_events.WindowMove.get());

				m_windowX = x;
				m_windowY = y;
				return 0;
			}
			case WM_SETFOCUS:
			{
				m_events.WindowFocus->SetFocused(true);
				m_events.WindowFocus->Start();
				m_application->PostEvent(m_events.WindowFocus.get());
				return 0;
			}
			case WM_KILLFOCUS:
			{
				m_events.WindowFocus->SetFocused(false);
				m_events.WindowFocus->Start();
				m_application->PostEvent(m_events.WindowFocus.get());
				return 0;
			}
			case WM_DPICHANGED:
			{
				UINT newDPI = HIWORD(wParam);
				UINT oldDPI = GetDpiForWindow(m_hwnd);

				m_events.DPIChange->SetOldDPI(oldDPI);
				m_events.DPIChange->SetNewDPI(newDPI);
				m_events.DPIChange->Start();
				m_application->PostEvent(m_events.DPIChange.get());
				return 0;
			}
			case WM_IME_CHAR:
			case WM_CHAR:
			{
				auto ch = static_cast<wchar_t>(wParam);
				auto modifyKey = ModifyKey::None;
				if (GetKeyState(VK_SHIFT) & 0x8000)
					modifyKey = static_cast<ModifyKey>(static_cast<int>(modifyKey) | static_cast<int>(ModifyKey::Shift));
				if (GetKeyState(VK_CONTROL) & 0x8000)
					modifyKey = static_cast<ModifyKey>(static_cast<int>(modifyKey) | static_cast<int>(ModifyKey::Ctrl));
				if (GetKeyState(VK_MENU) & 0x8000)
					modifyKey = static_cast<ModifyKey>(static_cast<int>(modifyKey) | static_cast<int>(ModifyKey::Alt));

				m_events.CharInput->SetChar(WString(ch).ConvertToString<char>());
				m_events.CharInput->SetModifyKey(modifyKey);
				m_events.CharInput->Start();
				m_application->PostEvent(m_events.CharInput.get());
				return 0;
			}
			default:
				return DefWindowProc(hWnd, msg, wParam, lParam);
		}
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