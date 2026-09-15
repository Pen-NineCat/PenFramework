// File /Native/Engine/Event/WindowFocusEvent.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat
#pragma once

#include "IEngineEvent.hpp"

namespace PenEngine
{
    class WindowFocusEvent : public IEngineEvent
    {
    public:
        WindowFocusEvent() noexcept : IEngineEvent(PredefinedEngineEventType::WindowFocus), m_focused(false) {}
        WindowFocusEvent(bool focused) noexcept
            : IEngineEvent(PredefinedEngineEventType::WindowFocus),
            m_focused(focused) {}

        virtual ~WindowFocusEvent() noexcept override = default;

        void SetFocused(bool focused) noexcept { m_focused = focused; }
        bool IsFocused() const noexcept { return m_focused; }

    private:
        bool m_focused = false;
    };
}