// File /Native/Engine/Event/KeyReleaseEvent.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "IEngineEvent.hpp"
#include "InputConstant.h"

namespace PenEngine
{
    class KeyReleaseEvent : public IEngineEvent
    {
    public:
        KeyReleaseEvent() noexcept : IEngineEvent(PredefinedEngineEventType::KeyRelease) {}
        KeyReleaseEvent(VKey key, ModifyKey modifyKey) noexcept 
            : IEngineEvent(PredefinedEngineEventType::KeyRelease), 
              m_key(key), m_modifyKey(modifyKey) {}

        virtual ~KeyReleaseEvent() noexcept override = default;

        void SetKey(VKey key) noexcept { m_key = key; }
        VKey GetKey() const noexcept { return m_key; }

        void SetModifyKey(ModifyKey modifyKey) noexcept { m_modifyKey = modifyKey; }
        ModifyKey GetModifyKey() const noexcept { return m_modifyKey; }

    private:
        VKey m_key = VKey::None;
        ModifyKey m_modifyKey = ModifyKey::None;
    };
}