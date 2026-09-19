// File /Native/Engine/Object/Internal/ObjectSignal.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../../Core/Environment.h"
#include "../../DebugTools/DebugVerify.hpp"
#include <concepts>
#include <functional>
#include <utility>
#include <vector>

namespace PenEngine
{
	class PObject;

	/// 信号机制的总协议
	/// @note 设计前提：对象地址即身份。信号自身持有 owner 指针（全系统唯一的 owner 记录），
	///       因此不存在"信号属于谁"的第二种表达，也就不需要维护双向登记表的同步。
	///
	/// 三条生命周期路径：
	/// 1. 接收者销毁 -> PObject::~PObject 遍历 m_connectedSignal 并调用 ForgetReceiver
	/// 2. 发送者销毁 -> ~ObjectSignal 通知每个接收者移除对本信号的登记
	/// 3. 显式断开   -> SignalBase::Disconnect：槽位表与登记表各删一个指针
	///
	/// @note 连接关系的唯一真相来源是信号自己的槽位表，接收者侧的 m_connectedSignal 只是清理路径
	/// @note 各信号的成员模板实现在 PObject.h 末尾：那里 PObject 才是完整类型，
	///       而 MSVC 不允许在 PObject 不完整时从非模板函数体中调用其成员z
	class SignalBase
	{
	public:
		SignalBase(const SignalBase&) noexcept = delete;
		SignalBase& operator=(const SignalBase&) noexcept = delete;

		/// 禁止移动：m_owner 缓存了 owner 地址，移动会让 owner 身份错位
		SignalBase(SignalBase&&) noexcept = delete;
		SignalBase& operator=(SignalBase&&) noexcept = delete;

		/// @brief 断开指定接收者：同时清理本信号的槽位与该接收者的登记
		virtual void Disconnect(PObject* receiver) = 0;

		/// @brief 仅清理本信号的槽位，交由接收者自己维护其登记表
		/// @note 供接收者析构路径使用；此时接收者的登记表正处于销毁过程中，不能再被回写
		virtual void ForgetReceiver(PObject* receiver) noexcept = 0;

		[[nodiscard]] PObject* GetOwner() const noexcept { return m_owner; }

		/// @note 必须是 public 且 virtual：派生类的析构需要能以 override 声明
		virtual ~SignalBase() noexcept;
	protected:
		/// @brief 由信号的构造函数显式指定 owner
		/// @note owner 为空说明信号没有归属，析构通知协议会静默失效，因此在此拦截
		explicit SignalBase(PObject* owner) noexcept;

		/// @brief 发送者销毁时，通知各接收者放弃对本信号的登记
		/// @note 由派生类在析构体中显式调用（~SignalBase 期间虚调用不会下派到派生类）
		virtual void NotifyOwnerGoneImpl() noexcept = 0;

		PObject* m_owner = nullptr;
	};

	/// @brief 对象信号
	/// @tparam Args 参数列表，例如 ObjectSignal<int, const char*>
	/// @note 成员声明形态：ObjectSignal<int> OnHit{ this };
	/// @note 只有 owner 能发出自己的信号：由 EmitFrom 在 Debug 下校验发出者身份
	/// @note 类外定义的模板头必须写成两段（类模板包 + 成员模板 F），
	///       合并成一段时 MSVC 不报错，而是静默丢弃该成员并让类外定义报 C2039
	template <typename... Args>
	class ObjectSignal final : public SignalBase
	{
	public:
		/// @note 推荐用默认成员初始化器传入 this，派生类构造函数无需写初始化列表
		explicit ObjectSignal(PObject* owner) noexcept : SignalBase(owner) {}

		/// 禁止移动：信号以自身地址为身份，接收者侧的登记记的就是这个地址
		ObjectSignal(const ObjectSignal&) noexcept = delete;
		ObjectSignal& operator=(const ObjectSignal&) noexcept = delete;
		ObjectSignal(ObjectSignal&&) noexcept = delete;
		ObjectSignal& operator=(ObjectSignal&&) noexcept = delete;

		~ObjectSignal() noexcept override;

		/// @brief 把接收者的槽位挂到本信号上，并在接收者侧登记一条反向记录
		/// @param receiver 接收者，之后可作为断开依据
		/// @param f 槽位，必须能以 Args... 调用
		template <typename F> requires std::invocable<F&, Args...>
		void Connect(PObject* receiver, F&& f);

		/// @brief 发出信号
		void Emit(Args... args)
		{
			for (auto& slot : m_slot)
				slot.second(args...);
		}

		void Disconnect(PObject* receiver) override;
		void ForgetReceiver(PObject* receiver) noexcept override;

		/// @brief 断开本信号上的全部接收者
		void DisconnectAll() noexcept;
	protected:
		void NotifyOwnerGoneImpl() noexcept override;

		std::vector<std::pair<PObject*, std::move_only_function<void(Args...)>>> m_slot;
	};

	/// @brief 不抛异常的对象信号，用于析构等不允许抛出的路径
	/// @tparam Args 参数列表，例如 NothrowObjectSignal<int>
	template <typename... Args>
	class NothrowObjectSignal final : public SignalBase
	{
	public:
		explicit NothrowObjectSignal(PObject* owner) noexcept : SignalBase(owner) {}

		NothrowObjectSignal(const NothrowObjectSignal&) noexcept = delete;
		NothrowObjectSignal& operator=(const NothrowObjectSignal&) noexcept = delete;
		NothrowObjectSignal(NothrowObjectSignal&&) noexcept = delete;
		NothrowObjectSignal& operator=(NothrowObjectSignal&&) noexcept = delete;

		~NothrowObjectSignal() noexcept override;

		template <typename F> requires std::is_nothrow_invocable_v<F&, Args...>
		void Connect(PObject* receiver, F&& f);

		void Emit(Args... args) noexcept
		{
			for (auto& slot : m_slot)
				slot.second(args...);
		}

		virtual void Disconnect(PObject* receiver) override;
		virtual void ForgetReceiver(PObject* receiver) noexcept override;

		void DisconnectAll() noexcept;
	protected:
		virtual void NotifyOwnerGoneImpl() noexcept override;

		std::vector<std::pair<PObject*, std::move_only_function<void(Args...)>>> m_slot;
	};
}
