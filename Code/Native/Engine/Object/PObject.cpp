// File /Native/Engine/Object/PObject.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "PObject.h"

#include "../Core/DeferredDestroyQueue.h"

namespace PenEngine
{
	PObject::~PObject() noexcept
	{
		// 接收者销毁路径：放弃全部登记
		// 必须在析构体内完成：成员析构阶段 m_connectedSignal 已不可用
		DestroySignal.Emit(this);
		DisconnectAll();
	}

	void PObject::SetProperty(StringView name, const std::any& property)
	{
		m_properties[name] = property;
	}

	void PObject::Disconnect(SignalBase& signal) noexcept
	{
		// 信号侧声明是不完整类型，虚调用必须留在本翻译单元
		signal.Disconnect(this);
	}

	void PObject::DisconnectAll() noexcept
	{
		// 只让信号清理槽位，登记表由本对象自行维护：
		// 此时本对象可能正在析构，不能让信号回写 m_connectedSignal
		// 先遍历再清空，避免 ForgetReceiver 期间改动被遍历的容器
		for (SignalBase* signal : m_connectedSignal)
			signal->ForgetReceiver(this);

		m_connectedSignal.clear();
	}

	void PObject::DestroyLater() noexcept
	{
		// 只有"所有权由外部决定"的对象才能进入延迟销毁协议。
		// Debug 下直接抓住误用；Release 下退化为明确的空操作（而不是 std::unreachable 那种未定义行为）
		DEBUG_VERIFY_REPORT_WITH_REL_OPERATION(CanBeDeferredDestroyed(),
			"计数所有权对象（如RefObject）不能使用DestroyLater：它的delete只能由强引用归零决定",
			return);

		DeferredDestroyQueue::GetInstance().PostDeferredObject(this);
		DestroyLatersSignal.Emit(this);
	}

	void PObject::InternalAddConnection(SignalBase* signal)
	{
		DEBUG_VERIFY_REPORT(signal != nullptr, "Signal must not be null");

		m_connectedSignal.emplace_back(signal);
	}

	void PObject::InternalRemoveConnection(SignalBase* signal) noexcept
	{
		// 信号侧的槽位由调用方先行清理：无论是信号正在遍历槽位，还是刚刚摘出槽位，
		// 回头调用 signal->ForgetReceiver 都是多余的，且会重入同一份容器
		std::erase(m_connectedSignal, signal);
	}

	SignalBase::SignalBase(PObject* owner) noexcept : m_owner(owner)
	{
		// owner 是析构通知协议的唯一依据，为空则协议静默失效
		DEBUG_VERIFY_REPORT(m_owner != nullptr, "A signal must be owned by a PObject");
	}

	SignalBase::~SignalBase() noexcept
	{
		// 发送者销毁路径已由派生类的析构体完成，此处不重复通知：
		// 派生类析构体先于本函数执行，且虚调用在本函数内不会下派到派生类
	}
}
