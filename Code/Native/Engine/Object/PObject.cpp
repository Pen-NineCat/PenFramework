// File /Native/Engine/Object/PObject.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "PObject.h"

namespace PenEngine
{
	void PObject::SetProperty(StringView name, const std::any& property)
	{
		m_properties[name] = property;
	}
}
