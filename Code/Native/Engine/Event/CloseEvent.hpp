// File /Native/Engine/Event/CloseEvent.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "IEngineEvent.hpp"

namespace PenEngine
{
    class CloseEvent : public IEngineEvent
    {
    public:
        CloseEvent() noexcept : IEngineEvent(PredefinedEngineEventType::Close) {}
        virtual ~CloseEvent() noexcept override = default;
    };
}
