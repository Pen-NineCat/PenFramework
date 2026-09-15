// File /Native/Engine/Event/DPIChangeEvent.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "IEngineEvent.hpp"

namespace PenEngine
{
    class DPIChangeEvent : public IEngineEvent
    {
    public:
        DPIChangeEvent() noexcept : IEngineEvent(PredefinedEngineEventType::DPIChange),
            m_oldDPI(0), m_newDPI(0) {}
        DPIChangeEvent(U32 oldDPI, U32 newDPI) noexcept
            : IEngineEvent(PredefinedEngineEventType::DPIChange),
            m_oldDPI(oldDPI), m_newDPI(newDPI) {}

        virtual ~DPIChangeEvent() noexcept override = default;

        void SetOldDPI(U32 oldDPI) noexcept { m_oldDPI = oldDPI; }
        U32 GetOldDPI() const noexcept { return m_oldDPI; }

        void SetNewDPI(U32 newDPI) noexcept { m_newDPI = newDPI; }
        U32 GetNewDPI() const noexcept { return m_newDPI; }

    private:
        U32 m_oldDPI = 0;
        U32 m_newDPI = 0;
    };
}