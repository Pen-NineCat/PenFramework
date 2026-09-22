// File /Native/Engine/Object/PObject.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "PObject.h"

#include "../Core/DeferredDestroyQueue.h"
#include "../Coroutine/CoroutineScheduler.hpp"

namespace PenEngine
{
	PObject::~PObject() noexcept
	{
		// 接收者销毁路径：放弃全部登记
		// 必须在析构体内完成：成员析构阶段 m_connectedSignal 已不可用
		DestroySignal.Emit(this);
	}

	void PObject::SetProperty(StringView name, const std::any& property)
	{
		m_properties[name] = property;
	}

	void PObject::DestroyLater() noexcept
	{
		// 只有"所有权由外部决定"的对象才能进入延迟销毁协议。
		// Debug 下直接抓住误用；Release 下退化为明确的空操作（而不是 std::unreachable 那种未定义行为）
		DEBUG_VERIFY_REPORT_WITH_REL_OPERATION(CanBeDeferredDestroyed(),
			"计数所有权对象（如RefObject）不能使用DestroyLater：它的delete只能由强引用归零决定",
			return);

		DeferredDestroyQueue::GetInstance().PostDeferredObject(this);
		DestroyLaterSignal.Emit(this);
	}

	void PObject::InternalStartCoroutine(Detail::TaskBase* task)
	{
		CoroutineScheduler::GetInstance().PostCoroutineTask(this,task);
	}
}
