// File /Native/Engine/Utils/Flag.hpp
// 
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include <type_traits>

#define BIT_ENUM(bit) 1 << (bit - 1)

#define DECL_ENUM_FLAG_OPERATORS(EnumType) \
    constexpr PenEngine::Flag<EnumType> operator|(EnumType lhs, EnumType rhs) noexcept { \
        return PenEngine::Flag<EnumType>(lhs) | rhs; \
    } \
    constexpr PenEngine::Flag<EnumType> operator&(EnumType lhs, EnumType rhs) noexcept { \
        return PenEngine::Flag<EnumType>(lhs) & rhs; \
    } \
    constexpr PenEngine::Flag<EnumType> operator^(EnumType lhs, EnumType rhs) noexcept { \
        return PenEngine::Flag<EnumType>(lhs) ^ rhs; \
    } \
    constexpr PenEngine::Flag<EnumType> operator|(EnumType lhs, PenEngine::Flag<EnumType> rhs) noexcept { \
        return rhs | lhs; \
    } \
    constexpr PenEngine::Flag<EnumType> operator&(EnumType lhs, PenEngine::Flag<EnumType> rhs) noexcept { \
        return rhs & lhs; \
    } \
    constexpr PenEngine::Flag<EnumType> operator^(EnumType lhs, PenEngine::Flag<EnumType> rhs) noexcept { \
        return rhs ^ lhs; \
    } \
	constexpr PenEngine::Flag<EnumType> operator~(EnumType value) noexcept { \
		return static_cast<EnumType>(~static_cast<std::underlying_type_t<EnumType>>(value)); \
	} \
    constexpr bool operator!(EnumType value) noexcept { \
        return static_cast<std::underlying_type_t<EnumType>>(value) == 0; \
    }
#define DECL_ENUM_FLAG_TYPE(EnumType) \
		using EnumType##Flag = PenEngine::Flag<EnumType>;  \
		DECL_ENUM_FLAG_OPERATORS(EnumType)

#define DECL_ENUM_FLAG_TYPE_WITH_NAME(EnumType,FlagType) \
		using FlagType = PenEngine::Flag<EnumType>; \
		DECL_ENUM_FLAG_OPERATORS(EnumType)

#define DECL_ENUM_FLAG_FRIEND_OPERATORS(EnumType) \
    friend constexpr PenEngine::Flag<EnumType> operator|(EnumType lhs, EnumType rhs) noexcept { \
        return PenEngine::Flag<EnumType>(lhs) | rhs; \
    } \
    friend constexpr PenEngine::Flag<EnumType> operator&(EnumType lhs, EnumType rhs) noexcept { \
        return PenEngine::Flag<EnumType>(lhs) & rhs; \
    } \
    friend constexpr PenEngine::Flag<EnumType> operator^(EnumType lhs, EnumType rhs) noexcept { \
        return PenEngine::Flag<EnumType>(lhs) ^ rhs; \
    } \
    friend constexpr PenEngine::Flag<EnumType> operator|(EnumType lhs, PenEngine::Flag<EnumType> rhs) noexcept { \
        return rhs | lhs; \
    } \
    friend constexpr PenEngine::Flag<EnumType> operator&(EnumType lhs, PenEngine::Flag<EnumType> rhs) noexcept { \
        return rhs & lhs; \
    } \
    friend constexpr PenEngine::Flag<EnumType> operator^(EnumType lhs, PenEngine::Flag<EnumType> rhs) noexcept { \
        return rhs ^ lhs; \
    } \
	friend constexpr PenEngine::Flag<EnumType> operator~(EnumType value) noexcept { \
		return static_cast<EnumType>(~static_cast<std::underlying_type_t<EnumType>>(value)); \
	} \
    friend constexpr bool operator!(EnumType value) noexcept { \
        return static_cast<std::underlying_type_t<EnumType>>(value) == 0; \
    }
#define DECL_ENUM_FLAG_FRIEND_TYPE(EnumType) \
		using EnumType##Flag = Flag<EnumType>;  \
		DECL_ENUM_FLAG_FRIEND_OPERATORS(EnumType)

#define DECL_ENUM_FLAG_FRIEND_TYPE_WITH_NAME(EnumType,FlagType) \
		using FlagType = Flag<EnumType>; \
		DECL_ENUM_FLAG_FRIEND_OPERATORS(EnumType)

namespace PenEngine
{
	template <typename EnumType> requires std::is_enum_v<EnumType>
	class Flag
	{
	public:
		using EnumUnderlyingType = std::underlying_type_t<EnumType>;

		constexpr Flag() noexcept = default;
		constexpr Flag(const Flag&) noexcept = default;
		constexpr Flag(Flag&&) noexcept = default;
		constexpr Flag& operator=(const Flag&) noexcept = default;
		constexpr Flag& operator=(Flag&&) noexcept = default;
		constexpr ~Flag() noexcept = default;

		/* implicit */ constexpr Flag(EnumType flag) noexcept :
			m_value(flag) {
		}

		constexpr Flag& operator=(EnumType flag) noexcept
		{
			m_value = flag;
			return *this;
		}

		constexpr Flag operator|(EnumType flag) const noexcept
		{
			return Flag(static_cast<EnumUnderlyingType>(m_value) | static_cast<EnumUnderlyingType>(flag));
		}

		constexpr Flag operator|(Flag other) const noexcept
		{
			return Flag(m_value | other.m_value);
		}

		constexpr Flag& operator|=(EnumType flag) noexcept
		{
			m_value = static_cast<EnumType>(static_cast<EnumUnderlyingType>(m_value) | static_cast<EnumUnderlyingType>(flag));
			return *this;
		}

		constexpr Flag& operator|=(Flag other) noexcept
		{
			m_value = static_cast<EnumType>(static_cast<EnumUnderlyingType>(m_value) | static_cast<EnumUnderlyingType>(other.m_value));
			return *this;
		}

		constexpr Flag operator&(EnumType flag) const noexcept
		{
			return Flag(static_cast<EnumType>(static_cast<EnumUnderlyingType>(m_value) & static_cast<EnumUnderlyingType>(flag)));
		}

		constexpr Flag operator&(Flag other) const noexcept
		{
			return Flag(m_value & other.m_value);
		}

		constexpr Flag& operator&=(EnumType flag) noexcept
		{
			m_value = static_cast<EnumType>(static_cast<EnumUnderlyingType>(m_value) & static_cast<EnumUnderlyingType>(flag));
			return *this;
		}

		constexpr Flag& operator&=(Flag other) noexcept
		{
			m_value = static_cast<EnumType>(static_cast<EnumUnderlyingType>(m_value) & static_cast<EnumUnderlyingType>(other.m_value));
			return *this;
		}

		constexpr Flag operator^(EnumType flag) const noexcept
		{
			return Flag(static_cast<EnumUnderlyingType>(m_value) ^ static_cast<EnumUnderlyingType>(flag));
		}

		constexpr Flag operator^(Flag other) const noexcept
		{
			return Flag(m_value ^ other.m_value);
		}

		constexpr Flag& operator^=(EnumType flag) noexcept
		{
			m_value = static_cast<EnumType>(static_cast<EnumUnderlyingType>(m_value) ^ static_cast<EnumUnderlyingType>(flag));
			return *this;
		}

		constexpr Flag& operator^=(Flag other) noexcept
		{
			m_value = static_cast<EnumType>(static_cast<EnumUnderlyingType>(m_value) ^ static_cast<EnumUnderlyingType>(other.m_value));
			return *this;
		}

		constexpr Flag operator~() const noexcept
		{
			return Flag(~static_cast<EnumUnderlyingType>(m_value));
		}

		constexpr bool Test(EnumType flag) const noexcept
		{
			return (static_cast<EnumUnderlyingType>(m_value) & static_cast<EnumUnderlyingType>(flag)) != 0;
		}

		constexpr bool TestAll(Flag flag) const noexcept
		{
			return (m_value & flag.m_value) == flag.m_value;
		}

		constexpr bool Any() const noexcept
		{
			return static_cast<EnumUnderlyingType>(m_value) != 0;
		}

		constexpr bool operator==(const Flag&) const noexcept = default;
		constexpr bool operator==(EnumType flag) const noexcept
		{
			return static_cast<EnumUnderlyingType>(m_value) == static_cast<EnumUnderlyingType>(flag);
		}

		constexpr EnumType EnumValue() const noexcept
		{
			return m_value;
		}

		explicit operator bool() const noexcept
		{
			return Any();
		}
	private:
		explicit constexpr Flag(EnumUnderlyingType value) noexcept :
			m_value(static_cast<EnumType>(value)) {
		}

		EnumType m_value = EnumType(0);
	};
}
