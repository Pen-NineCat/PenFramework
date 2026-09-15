// File /Native/Engine/Event/WindowMinimizeEvent.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat
#pragma once

#include "IEngineEvent.hpp"

namespace PenEngine
{
    class WindowMinimizeEvent : public IEngineEvent
    {
    public:
        WindowMinimizeEvent() noexcept : IEngineEvent(PredefinedEngineEventType::WindowMinimize) {}
        WindowMinimizeEvent(U32 width, U32 height) noexcept
            : IEngineEvent(PredefinedEngineEventType::WindowMinimize),
            m_width(width), m_height(height) {}

        virtual ~WindowMinimizeEvent() noexcept override = default;

        void SetWidth(U32 width) noexcept { m_width = width; }
        U32 GetWidth() const noexcept { return m_width; }

        void SetHeight(U32 height) noexcept { m_height = height; }
        U32 GetHeight() const noexcept { return m_height; }

    private:
        U32 m_width = 0;
        U32 m_height = 0;
    };
}