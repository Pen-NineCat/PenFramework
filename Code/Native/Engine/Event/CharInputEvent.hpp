// File /Native/Engine/Event/CharInputEvent.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "IEngineEvent.hpp"
#include "InputConstant.h"
#include "../String/String.hpp"

namespace PenEngine
{
    /// @brief 这些code代表可能会被转译的字符输入信息转译后的结果
	enum SpecificCharCode
	{
        Base = 'A' - 1,
        CtrlA = 'A' - Base, // Ctrl+A
        CtrlC = 'C' - Base, // Ctrl+C
        CtrlX = 'X' - Base, // Ctrl+X
        CtrlV = 'V' - Base, // Ctrl+V
        CtrlZ = 'Z' - Base, // Ctrl+Z
        CtrlY = 'Y' - Base, // Ctrl+Y
	};

    class CharInputEvent : public IEngineEvent
    {
    public:
        CharInputEvent() noexcept : IEngineEvent(PredefinedEngineEventType::CharInput) {}
        CharInputEvent(StringView ch, ModifyKey modifyKey) noexcept
            : IEngineEvent(PredefinedEngineEventType::CharInput), 
              m_char(ch), m_modifyKey(modifyKey) {}

        virtual ~CharInputEvent() noexcept override = default;

        void SetChar(StringView str) noexcept { m_char = str; }
        StringView GetChar() const noexcept { return m_char; }

        void SetModifyKey(ModifyKey modifyKey) noexcept { m_modifyKey = modifyKey; }
        ModifyKey GetModifyKey() const noexcept { return m_modifyKey; }

    private:
        String m_char = {};
        ModifyKey m_modifyKey = {};
    };
}
