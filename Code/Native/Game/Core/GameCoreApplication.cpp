// File /Code/Native/Game/Core/GameCoreApplication.cpp
//
// SPDX - License - Identifier: MIT
// Copyright(c) 2026 - Present Pen - NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "GameCoreApplication.h"

namespace PenEngine
{
	GameCoreApplication::GameCoreApplication(int argc, char* argv[])
	{
		
	}

	int GameCoreApplication::Exec()
	{
		
	}

	void GameCoreApplication::CreateWindow()
	{
		
	}

	bool GameCoreApplication::DispatchWindowEvent()
	{
		if (m_window.TranslateWindowEvent() == false)
			return false;

		auto events = m_window.CollectWindowEvent();
		for (IEngineEvent* event : events)
		{

			// 如果事件是关闭事件且未完成，则返回 false，退出主循环
			if (event->Type() == PredefinedEngineEventType::Close && event->Finished() == false)
				return false;
		}

		return true;
	}
}
