// File /Engine/Utils/TimeAccumulator.hpp
// 
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include <chrono>
#include "../Core/Environment.h"

namespace PenEngine
{
	class TimeAccumulator
	{
	public:
		TimeAccumulator() noexcept = default;
		TimeAccumulator(const TimeAccumulator&) noexcept = delete;
		TimeAccumulator(TimeAccumulator&&) noexcept = default;
		TimeAccumulator& operator=(const TimeAccumulator&) noexcept = delete;
		TimeAccumulator& operator=(TimeAccumulator&&) noexcept = default;

		explicit TimeAccumulator(float step) noexcept :m_step(step) {}

		void ResetStep(float step) noexcept
		{
			m_step = step;
		}

		U8 Update(float dt) noexcept
		{
			m_accumulator += dt;
			U8 res = 0;
			while (m_accumulator >= m_step)
			{
				m_accumulator -= m_step;
				res++;
			}
			return res;
		}
	private:
		float m_accumulator = 0;
		float m_step = 0;
	};
}
