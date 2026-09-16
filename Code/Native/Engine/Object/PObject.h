// File /Native/Engine/Object/PObject.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include <any>
#include <concepts>
#include <expected>
#include <type_traits>
#include "../Core/Environment.h"
#include "Internal/MetaFunction.hpp"
#include "../String/StringUnorderedMap.hpp"
 
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
	public:
		[[nodiscard]] constexpr virtual HashID GetMetaHash() const noexcept { return CalculateClassMetaHash("PObject"); }
		[[nodiscard]] constexpr virtual StringView GetMetaType() const noexcept { return "PObject"; }
		[[nodiscard]] constexpr virtual bool CanConvertToUpperType(HashID hash) const noexcept { return false; }
		constexpr static bool StaticCanConvertToUpperType(HashID hash) noexcept { return false; }

		PObject() noexcept = default;

		PObject(const PObject&) noexcept = delete;
		PObject& operator=(const PObject&) noexcept = delete;
		PObject(PObject&&) noexcept = default;
		PObject& operator=(PObject&&) noexcept = default;

		virtual ~PObject() noexcept = default;

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
	private:
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
}
