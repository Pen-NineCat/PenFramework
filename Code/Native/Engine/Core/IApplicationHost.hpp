// File /Native/Engine/Core/IApplicationHost.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Event/IEngineEvent.hpp"

namespace PenEngine
{
	class IApplicationHost
	{
	public:
		bool PostEvent(IEngineEvent* event);
		void Update(float dt);
		void FixedUpdate(float dt);
	};
}
