// File /Native/Engine/Core/CoreApplication.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "CoreApplication.h"

namespace PenEngine
{
	CoreApplication::CoreApplication()
	{
		m_applicationData.WindowWidth = 1920;
		m_applicationData.WindowHeight = 1080;
		m_applicationData.ApplicationTitle = "hello";

		PreLoad();
		LoadWindow();
	}

	int CoreApplication::Exec()
	{
		while (true)
		{
			if (m_window.DispatchWindowMessage() == false)
				return 0;
		}
	}

	bool CoreApplication::PreLoad()
	{
		#ifdef PENFRAMEWORK_OS_WIN32
		// Platform PreInit
		m_applicationData.HInstance = GetModuleHandle(nullptr);
		SetProcessDPIAware();
		#endif 
		return true;
	}

	bool CoreApplication::LoadWindow()
	{
		WindowInitContext windowInitContext;
		windowInitContext.Width = m_applicationData.WindowWidth;
		windowInitContext.Height = m_applicationData.WindowHeight;
		windowInitContext.ApplicationTitle = m_applicationData.ApplicationTitle;
		windowInitContext.CoreApplication = this;

#ifdef PENFRAMEWORK_OS_WIN32
		windowInitContext.HInstance = m_applicationData.HInstance;
		m_applicationData.Hwnd = m_window.Create(windowInitContext);
#endif  // PENFRAMEWORK_OS_WIN32
		return true;
	}
}
