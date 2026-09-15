// File /Native/Engine/Event/WindowResizeEvent.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "IEngineEvent.hpp"

namespace PenEngine
{
    class WindowResizeEvent : public IEngineEvent
    {
    public:
        WindowResizeEvent() noexcept : IEngineEvent(PredefinedEngineEventType::WindowResize) {}
        WindowResizeEvent(U32 width, U32 height, U32 oldWidth, U32 oldHeight) noexcept
            : IEngineEvent(PredefinedEngineEventType::WindowResize), 
              m_width(width), m_height(height), m_oldWidth(oldWidth), m_oldHeight(oldHeight) {}

        virtual ~WindowResizeEvent() noexcept override = default;

        void SetWidth(U32 width) noexcept { m_width = width; }
        U32 GetWidth() const noexcept { return m_width; }

        void SetHeight(U32 height) noexcept { m_height = height; }
        U32 GetHeight() const noexcept { return m_height; }

        void SetOldWidth(U32 oldWidth) noexcept { m_oldWidth = oldWidth; }
        U32 GetOldWidth() const noexcept { return m_oldWidth; }

        void SetOldHeight(U32 oldHeight) noexcept { m_oldHeight = oldHeight; }
        U32 GetOldHeight() const noexcept { return m_oldHeight; }

    private:
        U32 m_width = 0;
        U32 m_height = 0;
        U32 m_oldWidth = 0;
        U32 m_oldHeight = 0;
    };
}