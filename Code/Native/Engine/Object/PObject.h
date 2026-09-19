// File /Native/Engine/Object/PObject.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../Core/Environment.h"
#include "../String/StringUnorderedMap.hpp"
#include "Internal/MetaFunction.hpp"
#include "Internal/ObjectSignal.hpp"
#include <algorithm>
#include <any>
#include <concepts>
#include <expected>
#include <format>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

// 考虑在 C++26 标准后使用反射实现
#define DECL_P_OBJECT(ClassType,InheritClassType) \
	public: \
	using SelfClass = ClassType; \
	using InheritClass = InheritClassType; \
	constexpr static PenEngine::StringView ClassMetaType = #ClassType; \
	constexpr static PenEngine::HashID ClassMetaHash = PenEngine::CalculateClassMetaHash(#ClassType); \
	constexpr virtual PenEngine::HashID GetMetaHash() const noexcept override { return ClassMetaHash; } \
	constexpr virtual PenEngine::StringView GetMetaType() const noexcept override { return ClassMetaType; } \
	constexpr virtual bool CanConvertToUpperType(PenEngine::HashID hash) const noexcept override { return StaticCanConvertToUpperType(hash); } \
	constexpr static bool StaticCanConvertToUpperType(PenEngine::HashID hash) noexcept { if(hash == ClassMetaHash) return true;return InheritClass::StaticCanConvertToUpperType(hash); } \

namespace PenEngine
{
	class PObject;

	template <typename T>
	concept IsDerivedFromPObject = std::is_same_v<T, PObject> || std::derived_from<T, PObject>;

	template <typename T>
	concept IsDefinedPObject = std::is_same_v<T, typename T::SelfClass>&& IsDerivedFromPObject<T>;

	class PObject
	{
		template <typename... Args>
		friend class ObjectSignal;
		template <typename... Args>
		friend class NothrowObjectSignal;
	public:
		[[nodiscard]] constexpr virtual HashID GetMetaHash() const noexcept { return CalculateClassMetaHash("PObject"); }
		[[nodiscard]] constexpr virtual StringView GetMetaType() const noexcept { return "PObject"; }
		[[nodiscard]] constexpr virtual bool CanConvertToUpperType(HashID hash) const noexcept { return false; }
		constexpr static bool StaticCanConvertToUpperType(HashID hash) noexcept { return false; }

		NothrowObjectSignal<PObject*> DestroySignal{ this };
		NothrowObjectSignal<PObject*> DestroyLatersSignal{ this };

		PObject() noexcept = default;
		virtual ~PObject() noexcept;

		PObject(const PObject&) noexcept = delete;
		PObject& operator=(const PObject&) noexcept = delete;

		/// @brief 禁止移动
		/// @note 对象持有以自身地址为身份的信号与双向表登记，移动会使这些身份错位
		PObject(PObject&&) noexcept = delete;
		PObject& operator=(PObject&&) noexcept = delete;

		template <IsDefinedPObject Derived>
		Derived* TryCastTo() noexcept
		{
			if (CanConvertToUpperType(Derived::ClassMetaHash))
				return static_cast<Derived*>(this);
			return nullptr;
		}

		void SetProperty(StringView name, const std::any& property);

		template <typename T, typename... Args>
		void EmplaceProperty(StringView name, Args&&... args);

		enum class GetPropertyError : U8
		{
			PropertyNotFound,
			PropertyIsEmpty,
			PropertyTypeIncompatible
		};

		/// @brief 获得对象属性
		/// @tparam T 期望的属性类型
		/// @param name 属性名
		template <typename T>
		std::expected<T, GetPropertyError> TryGetProperty(StringView name) noexcept(noexcept(std::expected<T, GetPropertyError>(std::declval<T>())));

		/// @brief 显式连接：本对象作为接收者，把自己的槽位挂到发送者的信号上
		/// @param signal 发送者对象持有的信号，句法上即"谁发的信号"
		/// @param f 槽位，其可调用性由信号自身的参数列表约束
		/// @note 连接持续到任一方销毁，或显式调用 Disconnect
		/// @note 接收者就是本对象，因此参数里不再需要 sender 一栏
		template <typename F, typename... Args> requires std::invocable<F&, Args...>
		void Connect(ObjectSignal<Args...>& signal, F&& f)
		{
			signal.Connect(this, std::forward<F>(f));
		}

		template <typename F, typename... Args> requires std::is_nothrow_invocable_v<F&, Args...>
		void Connect(NothrowObjectSignal<Args...>& signal, F&& f)
		{
			signal.Connect(this, std::forward<F>(f));
		}

		/// @brief 断开本对象在指定信号上的连接
		void Disconnect(SignalBase& signal) noexcept;

		/// @brief 断开本对象在所有信号上的全部连接
		/// @note 析构亦复用此路径
		void DisconnectAll() noexcept;

		/// @brief 本对象的生命周期是否允许交给延迟销毁队列（即所有权是否由外部决定）
		/// @note 计数所有权对象（RefObject/RefPtr）返回 false：它的 delete 时机只能由强引用归零决定，
		///       与延迟队列的"计数归零"是两个互斥的最后删除者，共存只会双删或谁都不删。
		///       子类要屏蔽延迟销毁，覆盖这个纯查询即可；不要在 DestroyLater 上做文章
		[[nodiscard]] virtual bool CanBeDeferredDestroyed() const noexcept { return true; }

		/// @brief 通知对象将会被销毁
		/// @note 非虚：先入队建计数、再发 DestroyLatersSignal 是延迟销毁的唯一协议顺序
		///       （监听者在 Catch 时该计数必须已经存在），交给子类保管会被"忘了调基类"破坏。
		///       扩展点只有两个：覆盖 CanBeDeferredDestroyed()（策略）或监听 DestroyLatersSignal（通知）
		void DestroyLater() noexcept;
	private:
		/// 内部接口：由信号调用，不属于用户 API
		/// @brief 在接收者侧登记一条连接；信号侧保证同一信号只调用一次
		void InternalAddConnection(SignalBase* signal);

		/// @brief 移除本对象在指定信号上的登记
		/// @note 只动本对象的登记表；信号侧的槽位由调用方先行清理
		void InternalRemoveConnection(SignalBase* signal) noexcept;

		StringUnorderedMap<std::any> m_properties;

		/// 自身监听的信号：连接关系的唯一真相来源在信号自己的槽位表里，本表是接收者侧的清理路径
		std::vector<SignalBase*> m_connectedSignal;
	};

	template <typename T, typename ... Args>
	void PObject::EmplaceProperty(StringView name, Args&&... args)
	{
		if (auto it = m_properties.find(name); it == m_properties.end())
			m_properties.emplace(String(name), std::any(std::in_place_type<T>, std::forward<Args>(args)...));
		else
			it->second = std::any(std::in_place_type<T>, std::forward<Args>(args)...);
	}

	template <typename T>
	std::expected<T, PObject::GetPropertyError> PObject::TryGetProperty(StringView name) noexcept(noexcept(std::expected<T, GetPropertyError>(std::declval<T>())))
	{
		auto it = m_properties.find(name);
		if (it == m_properties.end())
			return std::unexpected(GetPropertyError::PropertyNotFound);

		if (!it->second.has_value())
			return std::unexpected(GetPropertyError::PropertyIsEmpty);

		T* ptr = std::any_cast<T>(&(it->second));

		if (ptr == nullptr)
			return std::unexpected(GetPropertyError::PropertyTypeIncompatible);

		return *ptr;
	}

	template <typename... Args>
	ObjectSignal<Args...>::~ObjectSignal() noexcept
	{
		NotifyOwnerGoneImpl();
	}

	template <typename... Args>
	template <typename F> requires std::invocable<F&, Args...>
	void ObjectSignal<Args...>::Connect(PObject* receiver, F&& f)
	{
		DEBUG_VERIFY_REPORT(receiver != nullptr, "Signal receiver must not be null");
		DEBUG_VERIFY_REPORT(receiver != m_owner, "An object must not connect to its own signal");

		for (auto& slot : m_slot)
		{
			if (slot.first == receiver)
			{
				slot.second = std::move_only_function<void(Args...)>(std::forward<F>(f));
				return;
			}
		}

		m_slot.emplace_back(receiver, std::move_only_function<void(Args...)>(std::forward<F>(f)));

		if (receiver != m_owner)
			receiver->InternalAddConnection(this);
	}

	template <typename... Args>
	void ObjectSignal<Args...>::Disconnect(PObject* receiver)
	{
		ForgetReceiver(receiver);
		if (receiver != m_owner)
			receiver->InternalRemoveConnection(this);
	}

	template <typename... Args>
	void ObjectSignal<Args...>::ForgetReceiver(PObject* receiver) noexcept
	{
		std::erase_if(m_slot, [receiver](const auto& slot) { return slot.first == receiver; });
	}

	template <typename... Args>
	void ObjectSignal<Args...>::DisconnectAll() noexcept
	{
		// 先摘出槽位表：接收者的清理回调会反向操作 m_slot
		std::vector<std::pair<PObject*, std::move_only_function<void(Args...)>>> slots = std::move(m_slot);
		m_slot.clear();

		for (auto& slot : slots)
		{
			if (slot.first && slot.first != m_owner)
				slot.first->InternalRemoveConnection(this);
		}
	}

	template <typename... Args>
	void ObjectSignal<Args...>::NotifyOwnerGoneImpl() noexcept
	{
		// 发送者销毁路径：通知每个接收者放弃对本信号的登记
		// 槽位表已被摘出，接收者侧的清理不会回到这里
		std::vector<std::pair<PObject*, std::move_only_function<void(Args...)>>> slots = std::move(m_slot);
		m_slot.clear();

		for (auto& slot : slots)
		{
			if (slot.first && slot.first != m_owner)
				slot.first->InternalRemoveConnection(this);
		}
	}

	template <typename... Args>
	NothrowObjectSignal<Args...>::~NothrowObjectSignal() noexcept
	{
		NotifyOwnerGoneImpl();
	}

	template <typename... Args>
	template <typename F> requires std::is_nothrow_invocable_v<F&, Args...>
	void NothrowObjectSignal<Args...>::Connect(PObject* receiver, F&& f)
	{
		DEBUG_VERIFY_REPORT(receiver != nullptr, "Signal receiver must not be null");

		// 同一接收者只占一个槽位，重复 Connect 视为替换
		for (auto& slot : m_slot)
		{
			if (slot.first == receiver)
			{
				slot.second = std::move_only_function<void(Args...)>(std::forward<F>(f));
				return;
			}
		}

		m_slot.emplace_back(receiver, std::move_only_function<void(Args...)>(std::forward<F>(f)));

		if (receiver != m_owner)
			receiver->InternalAddConnection(this);
	}

	template <typename... Args>
	void NothrowObjectSignal<Args...>::Disconnect(PObject* receiver)
	{
		ForgetReceiver(receiver);
		if (receiver != m_owner)
			receiver->InternalRemoveConnection(this);
	}

	template <typename... Args>
	void NothrowObjectSignal<Args...>::ForgetReceiver(PObject* receiver) noexcept
	{
		std::erase_if(m_slot, [receiver](const auto& slot) { return slot.first == receiver; });
	}

	template <typename... Args>
	void NothrowObjectSignal<Args...>::DisconnectAll() noexcept
	{
		std::vector<std::pair<PObject*, std::move_only_function<void(Args...)>>> slots = std::move(m_slot);
		m_slot.clear();

		for (auto& slot : slots)
		{
			if (slot.first && slot.first != m_owner)
				slot.first->InternalRemoveConnection(this);
		}
	}

	template <typename... Args>
	void NothrowObjectSignal<Args...>::NotifyOwnerGoneImpl() noexcept
	{
		std::vector<std::pair<PObject*, std::move_only_function<void(Args...)>>> slots = std::move(m_slot);
		m_slot.clear();

		for (auto& slot : slots)
		{
			if (slot.first && slot.first != m_owner)
				slot.first->InternalRemoveConnection(this);
		}
	}
}

template <>
struct std::formatter<PenEngine::PObject::GetPropertyError, char>
{
	constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

	auto format(PenEngine::PObject::GetPropertyError error, std::format_context& ctx) const
	{
		using Error = PenEngine::PObject::GetPropertyError;

		PenEngine::StringView name = "Unknown";

		switch (error)
		{
			case Error::PropertyNotFound:
				name = "PropertyNotFound";
				break;
			case Error::PropertyIsEmpty:
				name = "PropertyIsEmpty";
				break;
			case Error::PropertyTypeIncompatible:
				name = "PropertyTypeIncompatible";
				break;
		}

		return std::format_to(ctx.out(), "{}", name);
	}
};
