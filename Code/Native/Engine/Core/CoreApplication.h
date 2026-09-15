// File /Native/Engine/Core/CoreApplication.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "IApplicationHost.hpp"
#include "Window/Window.hpp"
#include "../Event/IEngineEvent.hpp"

namespace PenEngine
{
	class CoreApplication
	{
	public:
		CoreApplication();

		int Exec();

		bool PostEvent(IEngineEvent* event);
	private:
		struct CoreApplicationData
		{
			String ApplicationTitle;
			U32 WindowWidth;
			U32 WindowHeight;
			#ifdef PENFRAMEWORK_OS_WIN32
			HINSTANCE HInstance;
			HWND Hwnd;
			#endif // PENFRAMEWORK_OS_WIN32
		};

		bool PreLoad();
		bool LoadWindow();

		CoreApplicationData m_applicationData;
		OSWindow m_window;

		std::unique_ptr<IApplicationHost> m_applicationHost;
	};
}
