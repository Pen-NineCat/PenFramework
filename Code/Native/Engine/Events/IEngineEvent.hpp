// File /Code/Native/Engine/Event/IEngineEvent.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include <limits>
#include "../Core/Environment.h"

namespace PenEngine
{
	enum class PredefinedEngineEventType : U32
	{
		Undefined, // 未定义事件
		All, // 不指代任何实际的事件，只是用于表示所有事件
		// 基本事件
		Mouse,
		MouseMove,
		MouseWheel,
		KeyPress,
		KeyRelease,
		CharInput,
		WindowResize,
		WindowMove,
		WindowFocus,
		WindowClose,
		WindowMinimize,
		WindowMaximize,
		DPIChange,
		SignalCall,
		Close,
		WidgetResize,
		TransformChanged,
		// 编辑器事件
		Focus,
		UserDefinedStart, // 用户定义事件开始
		MaxUserDefinedEvent = std::numeric_limits<U32>::max() // 最大事件定义范围
	};

	class IEngineEvent
	{
	public:
		explicit IEngineEvent(PredefinedEngineEventType type) noexcept :m_type(type) {}
		virtual ~IEngineEvent() noexcept = default;

		/// @brief 开始新的事件处理周期
		void Start() noexcept { m_finished = false; }

		/// @brief 接受该事件的处理
		void Accept() noexcept { m_finished = true; }

		bool Finished() const noexcept {return m_finished;}

		[[nodiscard]] PredefinedEngineEventType Type() const noexcept { return m_type; }
	private:
		bool m_finished = false;

		PredefinedEngineEventType m_type = PredefinedEngineEventType::Undefined;
	};
}