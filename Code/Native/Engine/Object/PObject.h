// File /Native/Engine/Object/PObject.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../Core/Environment.h"
#include "../String/StringUnorderedMap.hpp"
#include "Internal/MetaFunction.hpp"
#include "SignalObject.hpp"
#include <algorithm>
#include <any>
#include <concepts>
#include <expected>
#include <format>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>
#include "../Coroutine/CoroutineTask.hpp"

// 考虑在 C++26 标准后使用反射实现
// 待解决：现在会导致非同名命名空间的同名类冲突
#define DECL_P_OBJECT(ClassType,InheritClassType) \
	public: \
	using SelfClass = ClassType; \
	using InheritClass = InheritClassType; \
	constexpr static PenEngine::StringView ClassMetaType = #ClassType; \
	constexpr static PenEngine::HashID ClassMetaHash = PenEngine::Internal::CalculateClassMetaHash(#ClassType); \
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

	class PObject : public SignalObject
	{
	public:
		[[nodiscard]] constexpr virtual HashID GetMetaHash() const noexcept { return Internal::CalculateClassMetaHash("PObject"); }
		[[nodiscard]] constexpr virtual StringView GetMetaType() const noexcept { return "PObject"; }
		[[nodiscard]] constexpr virtual bool CanConvertToUpperType(HashID hash) const noexcept { return false; }
		constexpr static bool StaticCanConvertToUpperType(HashID hash) noexcept { return false; }

		NothrowObjectSignal<PObject*> DestroyLaterSignal{ this };

		PObject() noexcept = default;
		virtual ~PObject() noexcept override;

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

		template <typename T>
		void StartCoroutine(CoroutineTask<T>&& task);
	private:
		/// @brief 内部启动协程函数
		/// @note 为了解决双向依赖问题
		void InternalStartCoroutine(Detail::TaskBase* task);

		StringUnorderedMap<std::any> m_properties;
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

	template <typename T>
	void PObject::StartCoroutine(CoroutineTask<T>&& task)
	{
		InternalStartCoroutine(new CoroutineTask<T>(std::move(task)));
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
