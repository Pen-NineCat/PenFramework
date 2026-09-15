// File /Native/Engine/Event/MouseEvent.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "IEngineEvent.hpp"
#include "InputConstant.h"

namespace PenEngine
{
    class MouseEvent : public IEngineEvent
    {
    public:
        MouseEvent() noexcept : IEngineEvent(PredefinedEngineEventType::Mouse) {}
        MouseEvent(MouseState state,U32 X,U32 Y) noexcept : IEngineEvent(PredefinedEngineEventType::Mouse),m_state(state), m_X(X), m_Y(Y) {}

        virtual ~MouseEvent() noexcept = default;

        void SetState(MouseState state) noexcept { m_state = state; }
        MouseState GetState() const noexcept { return m_state; }

        void SetX(U32 X) noexcept { m_X = X; }
        U32 GetX() const noexcept { return m_X; }

        void SetY(U32 Y) noexcept { m_Y = Y; }
        U32 GetY() const noexcept { return m_Y; }

    private:
        MouseState m_state = MouseState::None;
        U32 m_X = 0;
        U32 m_Y = 0;
    };
}
