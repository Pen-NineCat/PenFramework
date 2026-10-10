// File /Code/Native/Game/Core/GameCoreApplication.cpp
//
// SPDX - License - Identifier: MIT
// Copyright(c) 2026 - Present Pen - NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "GameCoreApplication.h"

#include "../../Engine/OS/Win/Windows.h"
#include <shellapi.h>

#include "../../Engine/Utils/CommandLineParser.h"

namespace
{
	std::vector<PenEngine::String> GetArgumentVector(int argc, char* argv[])
	{
		std::vector<PenEngine::String> res;
		// 如果是Win32平台，则一致忽略arg，使用GetCommandLine + CommandLineToArg函数
		#ifdef PENFRAMEWORK_OS_WIN32

		struct LocalMemoryGuard
		{
			explicit LocalMemoryGuard(HLOCAL handle) noexcept : Handle(handle) {}

			LocalMemoryGuard(const LocalMemoryGuard&) = delete;
			LocalMemoryGuard(LocalMemoryGuard&&) = delete;
			LocalMemoryGuard& operator=(const LocalMemoryGuard&) = delete;
			LocalMemoryGuard& operator=(LocalMemoryGuard&&) = delete;

			~LocalMemoryGuard() noexcept
			{
				if (Handle != nullptr)
					LocalFree(Handle);
			}

			HLOCAL Handle;
		};

		LPWSTR commandLine = GetCommandLine();
		LPWSTR* arguments = CommandLineToArgvW(commandLine, &argc);

		LocalMemoryGuard guard(arguments);

		for (int i = 0; i < argc; ++i)
		{
			PenEngine::String tmp;
			tmp.ConvertFrom(arguments[i]);
			res.push_back(std::move(tmp));
		}

		#else
		for (int i = 0; i < argc; ++i)
			res.emplace_back(argv[i]);
		#endif // PENFRAMEWORK_OS_WIN32
		return res;
	}
}

namespace PenEngine
{
	GameCoreApplication::GameCoreApplication(int argc, char* argv[])
	{
		ParseCommandLine(argc, argv);

		LoadWindow();
	}

	int GameCoreApplication::Exec()
	{
		while (true)
		{
			if (DispatchWindowEvent() == false)
				return Shutdown();
		}
	}

	int GameCoreApplication::Shutdown()
	{
		return 0;
	}

	void GameCoreApplication::ParseCommandLine(int argc, char* argv[])
	{
		std::vector<String> arguments = GetArgumentVector(argc, argv);

		// 优先尝试命令行的首项
		CommandLineParser parser(arguments.empty() ? "Pen" : arguments.front());

	}

	void GameCoreApplication::PreLoad()
	{
#ifdef PENFRAMEWORK_OS_WIN32
		m_data.HInstance = GetModuleHandle(nullptr);
		SetProcessDPIAware();
#endif // PENFRAMEWORK_OS_WIN32
	}

	void GameCoreApplication::LoadWindow()
	{
		const WindowInitContext ctx =
		{
			.Title = m_data.Title,
			.Width = m_data.Width,
			.Height = m_data.Height,
			.HInstance = m_data.HInstance,
			.Maximized = m_data.Maximized
		};

		m_data.WindowHandle = m_window.Create(ctx);
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
