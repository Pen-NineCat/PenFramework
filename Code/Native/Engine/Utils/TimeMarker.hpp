// File /Native/Engine/Utils/TimeMarker.hpp
// 
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include <chrono>

namespace PenEngine
{
	class TimeMarker
	{
	public:
		TimeMarker() noexcept = default;
		TimeMarker(const TimeMarker&) = delete;
		TimeMarker(TimeMarker&&) = delete;
		TimeMarker& operator=(const TimeMarker&) = delete;
		TimeMarker& operator=(TimeMarker&&) = delete;
		~TimeMarker() noexcept = default;

		/// @brief 该时刻距离上次标记时的时间
		/// @return 该时刻距离上次标记时的时间
		float Peek() const
		{
			return std::chrono::duration<float>(std::chrono::steady_clock::now() - m_markTime).count();
		}
		/// @brief 返回该时刻距离上次标记时的时间，并将标记移动到现在
		/// @return 该时刻距离上次标记时的时间
		float Mark()
		{
			const auto temp = m_markTime;
			m_markTime = std::chrono::steady_clock::now();
			return std::chrono::duration<float>(m_markTime - temp).count();
		}

		/// @brief 将标记移动到现在
		void Set()
		{
			m_markTime = std::chrono::steady_clock::now();
		}

		/// @brief 获取现在距离上次重置/初始化的时间 
		/// @return 获取现在距离上次重置/初始化的时间 
		float GetStartTime() const
		{
			return std::chrono::duration<float>(std::chrono::steady_clock::now() - m_startTime).count();
		}

		/// @brief 重置计时器，包括开始时间与标记时间
		void Reset()
		{
			m_startTime = std::chrono::steady_clock::now();
			m_markTime = m_startTime;
		}
	private:
		std::chrono::time_point<std::chrono::steady_clock> m_startTime = std::chrono::steady_clock::now();
		std::chrono::time_point<std::chrono::steady_clock> m_markTime = m_startTime;
	};
}
