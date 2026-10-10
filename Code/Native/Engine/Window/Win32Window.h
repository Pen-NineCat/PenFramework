// File /Code/Native/Engine/Window/Win32Window.h
//
// SPDX - License - Identifier: MIT
// Copyright(c) 2026 - Present Pen - NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../Events/IEngineEvent.hpp"
#include "WindowEventData.hpp"
#include "WindowHandleData.h"
#include "WindowInitContext.h"
#include <span>
#include <vector>

namespace PenEngine
{
	class Win32Window
	{
	public:
		Win32Window() noexcept = default;

		WindowHandleData Create(const WindowInitContext& ctx);

		/// @brief 处理窗口事件
		/// @return 窗口是否接收到退出命令
		bool TranslateWindowEvent();

		/// @brief 收集窗口事件列表
		std::span<IEngineEvent*> CollectWindowEvent();

		/// @brief 清空窗口消息列表
		void ClearWindowEventList();
	private:
		std::vector<IEngineEvent*> m_currentMessageList;
		WindowEventData m_eventData;
	};
}
