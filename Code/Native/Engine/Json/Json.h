// File /Native/Engine/Json/Json.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include <expected>
#include <string>
#include <variant>
#include <vector>

#include "../Core/Environment.h"
#include "../String/String.hpp"
#include "../String/StringUnorderedMap.hpp"
#include "../Utils/Concept.hpp"

namespace PenEngine
{
	class Json;

	namespace Internal
	{
		using JsonObject = StringUnorderedMap<Json>;
		using JsonArray = std::vector<Json>;
		using JsonValue = std::variant<bool, I64, U64, double, String, JsonObject, JsonArray>;

		// 解析与构造时允许的最大嵌套层数，避免恶意(或错误)的深层嵌套导致栈溢出
		constexpr static Usize MaxNestingDepth = 512;

		template <typename T>
		concept IsValidJsonValue = IsOneOf<T, bool, I64, U64, double, String, JsonObject, JsonArray>;

		// 注意两点：
		// 1. 判断方向是 String 能否由 T 构造，而非 T 能否由 String 构造；
		// 2. String 存在显式的容量构造(Usize)，std::is_constructible 会把它当成
		//    隐式转换，因此 String 对所有算术类型都"可构造"。真正的分派顺序必须
		//    先判定 bool / 整数 / 浮点，最后才落到字符串转换上。
		template <typename T>
		concept IsValidConstructibleJsonValue = std::is_constructible_v<String, T> || std::is_arithmetic_v<T> || std::is_same_v<T, JsonObject> || std::is_same_v<T, JsonArray>;

		// 解析时携带的上下文：源串 + 读取位置 + 当前嵌套深度
		class JsonParseContext;
		// 构造时携带的上下文：输出串 + 当前缩进层级
		class JsonBuildContext;
	}


	class BadJsonValue : public Exception
	{
	public:
		BadJsonValue() : Exception("BadJsonValue", "请求的类型不符合当前存储的类型") {}
	};

	class Json
	{
	public:
		template <typename T> requires Internal::IsValidConstructibleJsonValue<T>
		Json& operator=(T&& value)
		{
			using ValueType = std::remove_cvref_t<T>;

			if constexpr (std::is_same_v<ValueType, bool>)
				m_value = value;
			else if constexpr (std::is_integral_v<ValueType>)
			{
				if constexpr (std::is_signed_v<ValueType>)
					m_value = static_cast<I64>(value);
				else
					m_value = static_cast<U64>(value);
			}
			else if constexpr (std::is_floating_point_v<ValueType>)
				m_value = static_cast<double>(value);
			else if constexpr (std::is_same_v<ValueType, String>)
				m_value = String(std::forward<T>(value));
			else
				m_value = std::forward<T>(value);

			return *this;
		}

		template <typename T> requires Internal::IsValidJsonValue<T>
		T& GetValueAs()
		{
			auto ptr = TryGetValueAs<T>();

			if (ptr == nullptr)
				ThrowException(BadJsonValue());

			return *ptr;
		}

		template <typename T> requires Internal::IsValidJsonValue<T>
		const T& GetValueAs() const
		{
			auto ptr = TryGetValueAs<T>();

			if (ptr == nullptr)
				ThrowException(BadJsonValue());

			return *ptr;
		}

		template <typename T> requires Internal::IsValidJsonValue<T>
		T* TryGetValueAs() noexcept
		{
			return std::get_if<T>(&m_value);
		}

		template <typename T> requires Internal::IsValidJsonValue<T>
		const T* TryGetValueAs() const noexcept
		{
			return std::get_if<T>(&m_value);
		}

		Json& operator[](const String& key)
		{
			Internal::JsonObject* ptr = TryGetValueAs<Internal::JsonObject>();

			if (ptr == nullptr)
				ThrowException(BadJsonValue());

			return (*ptr)[key];
		}

		const Json& operator[](const String& key) const
		{
			const Internal::JsonObject* ptr = TryGetValueAs<Internal::JsonObject>();

			if (ptr == nullptr)
				ThrowException(BadJsonValue());

			if (auto it = ptr->find(key); it == ptr->end())
				ThrowException(BadJsonValue());
			else
				return it->second;
		}

		Json& operator[](Usize idx)
		{
			Internal::JsonArray* ptr = TryGetValueAs<Internal::JsonArray>();

			if (ptr == nullptr)
				ThrowException(BadJsonValue());

			return (*ptr)[idx];
		}

		const Json& operator[](Usize idx) const
		{
			const Internal::JsonArray* ptr = TryGetValueAs<Internal::JsonArray>();

			if (ptr == nullptr)
				ThrowException(BadJsonValue());

			return (*ptr)[idx];
		}

		struct ParseErrorData
		{
			Usize ErrorLine;
			char ErrorCharacter;
		};

		std::expected<void, ParseErrorData> Parse(StringView json);
		String Build(bool needFormat);
	private:
		// 两个上下文类需要直接读写 m_value，但它们的完整定义不能出现在 Json 内部
		// (JsonObject / JsonArray 以 Json 为元素类型，会形成不完整类型)，因此定义在
		// Json.cpp 的 Internal 命名空间中，这里仅授予访问权限
		friend class Internal::JsonParseContext;
		friend class Internal::JsonBuildContext;

		Internal::JsonValue m_value;
	};
}
