// File /Code/Native/Game/Core/GameCoreApplication.h
//
// SPDX - License - Identifier: MIT
// Copyright(c) 2026 - Present Pen - NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../../Engine/Window/Window.h"

namespace PenEngine
{
	/// @brief 游戏模式核心应用程序类，托管一切调度逻辑
	class GameCoreApplication
	{
	public:
		GameCoreApplication(int argc, char* argv[]);

		/// @brief 开始执行主循环
		/// @return 运行结果
		int Exec();

		/// @brief 终止所有子系统
		/// @return 终止结果
		int Terminate();
	private:
		void CreateWindow();
		bool DispatchWindowEvent();


		Window m_window;
	};
}
