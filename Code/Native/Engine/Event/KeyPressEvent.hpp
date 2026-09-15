// File /Native/Engine/Event/KeyPressEvent.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "IEngineEvent.hpp"
#include "InputConstant.h"

namespace PenEngine
{
	class KeyPressEvent : public IEngineEvent
	{
	public:
		KeyPressEvent() noexcept : IEngineEvent(PredefinedEngineEventType::KeyPress) {}
		KeyPressEvent(VKey key, ModifyKey modifyKey, bool autoRepeat) noexcept
			: IEngineEvent(PredefinedEngineEventType::KeyPress),
			m_key(key), m_modifyKey(modifyKey), m_autoRepeat(autoRepeat) {
		}

		virtual ~KeyPressEvent() noexcept override = default;

		void SetKey(VKey key) noexcept { m_key = key; }
		VKey GetKey() const noexcept { return m_key; }

		void SetModifyKey(ModifyKey modifyKey) noexcept { m_modifyKey = modifyKey; }
		ModifyKey GetModifyKey() const noexcept { return m_modifyKey; }

		void SetAutoRepeat(bool autoRepeat) noexcept { m_autoRepeat = autoRepeat; }
		bool IsAutoRepeat() const noexcept { return m_autoRepeat; }
	private:
		VKey m_key = {};
		ModifyKey m_modifyKey = {};
		bool m_autoRepeat = false;
	};
}