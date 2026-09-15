// File /Native/Engine/Event/IEngineEvent.hpp
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
		MaxDefinedEvent = std::numeric_limits<U32>::max() // 最大事件定义范围
	};

	class IEngineEvent
	{
	public:
		explicit IEngineEvent(PredefinedEngineEventType type) noexcept :m_type(type) {}
		virtual ~IEngineEvent() noexcept = default;

		void Start() noexcept { m_finished = false; }
		void Finish() noexcept { m_finished = true; }

		PredefinedEngineEventType Type() const noexcept { return m_type; }
	private:
		bool m_finished = false;

		PredefinedEngineEventType m_type = PredefinedEngineEventType::Undefined;
	};
}