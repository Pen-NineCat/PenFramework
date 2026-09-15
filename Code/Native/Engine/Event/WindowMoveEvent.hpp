// File /Native/Engine/Event/WindowMoveEvent.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "IEngineEvent.hpp"

namespace PenEngine
{
    class WindowMoveEvent : public IEngineEvent
    {
    public:
        WindowMoveEvent() noexcept : IEngineEvent(PredefinedEngineEventType::WindowMove) {}
        WindowMoveEvent(I32 x, I32 y, I32 oldX, I32 oldY) noexcept
            : IEngineEvent(PredefinedEngineEventType::WindowMove),
            m_x(x), m_y(y), m_oldX(oldX), m_oldY(oldY) {}

        virtual ~WindowMoveEvent() noexcept override = default;

        void SetX(I32 x) noexcept { m_x = x; }
        I32 GetX() const noexcept { return m_x; }

        void SetY(I32 y) noexcept { m_y = y; }
        I32 GetY() const noexcept { return m_y; }

        void SetOldX(I32 oldX) noexcept { m_oldX = oldX; }
        I32 GetOldX() const noexcept { return m_oldX; }

        void SetOldY(I32 oldY) noexcept { m_oldY = oldY; }
        I32 GetOldY() const noexcept { return m_oldY; }

    private:
        I32 m_x = 0;
        I32 m_y = 0;
        I32 m_oldX = 0;
        I32 m_oldY = 0;
    };
}