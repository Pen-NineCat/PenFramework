// File /Native/Engine/Event/MouseMoveEvent.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "IEngineEvent.hpp"

namespace PenEngine
{
    class MouseMoveEvent : public IEngineEvent
    {
    public:
        MouseMoveEvent() noexcept : IEngineEvent(PredefinedEngineEventType::MouseMove) {}
        MouseMoveEvent(U32 X, U32 Y, U32 deltaX, U32 deltaY) noexcept 
            : IEngineEvent(PredefinedEngineEventType::MouseMove), 
              m_X(X), m_Y(Y), m_deltaX(deltaX), m_deltaY(deltaY) {}

        virtual ~MouseMoveEvent() noexcept override = default;

        void SetX(U32 X) noexcept { m_X = X; }
        U32 GetX() const noexcept { return m_X; }

        void SetY(U32 Y) noexcept { m_Y = Y; }
        U32 GetY() const noexcept { return m_Y; }

        void SetDeltaX(U32 deltaX) noexcept { m_deltaX = deltaX; }
        U32 GetDeltaX() const noexcept { return m_deltaX; }

        void SetDeltaY(U32 deltaY) noexcept { m_deltaY = deltaY; }
        U32 GetDeltaY() const noexcept { return m_deltaY; }

    private:
        U32 m_X = 0;
        U32 m_Y = 0;
        U32 m_deltaX = 0;
        U32 m_deltaY = 0;
    };
}