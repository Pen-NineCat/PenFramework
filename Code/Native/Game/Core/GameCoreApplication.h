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

	private:
		struct ApplicationData
		{
			String Title;
			U16 Width;
			U16 Height;
			bool Maximized;

#ifdef PENFRAMEWORK_OS_WIN32
			HINSTANCE HInstance;
#endif // PENFRAMEWORK_OS_WIN32
			WindowHandleData WindowHandle;
		};

		/// @brief 关闭所有子系统
		/// @return 关闭结果
		int Shutdown();

		void ParseCommandLine(int argc, char* argv[]);

		void PreLoad();
		void LoadWindow();
		bool DispatchWindowEvent();

		Window m_window;
		ApplicationData m_data;
	};
}
