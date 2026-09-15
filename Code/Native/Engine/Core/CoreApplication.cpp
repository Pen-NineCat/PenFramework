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
			// 参见
			// Unity 渲染模型 以及 Unite Shanghai 2025 | 团结引擎的并行渲染架构
			// https://developer.unity.cn/projects/6916d5c9edbc2a8f5f698a0d
			// 以及
			// UE 渲染模型 以及 UE5并行渲染架构
			// https://aimspike.notion.site/UOD2022-Parallel-Rendering-In-UE5-6c77420abe8547a5a0e00cbb9f86bcfc

			// 标准线程模型
			// 1. MainThread MT 主线程
			// 2. RenderThread RT渲染线程
			// 3. RHI(RenderHardwareInterface)Thread RHIT渲染硬件接口线程

			// 对于工作帧N，根据SFB渲染流程
			// MT工作第N帧
			// RT工作第N - 1帧或N帧
			// RHIT工作第N - 1帧或N帧

			// MT和RT使用双缓冲机制交换N/N-1的渲染结果
			// 默认启用严格线程同步
			// 同步后MT和RT的顺序严格符合N/N-1的渲染流程，但是会导致MT和RT的渲染延迟增加。
			// 不启用同步可能会出现MT已经开始处理N帧，RT还在处理N - X帧的情况，导致出现跳变。
			// 或者反过来RT已经渲染完N - 1帧，开始渲染N帧，但是MT还在处理第N帧，渲染数据未就绪的状态。

			// MT核心调度逻辑
			// 1.  更新场景物理 // todo
			// 1.1 Host -> FixedUpdate()；
			// 1.2 计算碰撞 // todo
			// 2.  收集并分发窗口事件
			// 2.1 处理平台窗口事件 Window.DispatchWindowMessage
			// 2.2 由窗口函数调用框架窗口事件函数 Window.DispatchWindowMessage -> this->PostEvent
			// 2.3 发送到主机 Host -> PostEvent(event);
			// 3.  更新主机场景状态 Host -> Update(dt); // todo
			// 4.  更新协程调度 CoroutineScheduler -> Update(); // todo
			// 5.  更新延迟删除队列 DeferredDestroyQueue -> Update(); // todo
			// 6.  准备渲染 // todo
			// 6.1 收集渲染对象与渲染剔除 Host -> OnPreCull(), Cull, OnPostCull(); // todo
			// 6.2 拷贝并交换双缓冲Proxy Host -> OnCreateProxy();
			// 事实上，拷贝并交换双缓冲交由RT线程是更好的。
			// 但是由于OnCreateProxy()是一个对象事件接口，所以不能让RT线程负责（也许可以，但是为了安全性最好别这样做）
			// Unity是采用MT调用Proxy，而UE将Cull,Proxy,Render等操作全都抽到了RT上处理
			// 目前来看，没有需要用到后者的必要
			// 6.3 准备渲染 // todo
			// 如果启用RT线程，向RT传递渲染对象
			// 如果未启用RT线程，则调度所有渲染对象
			// 6.4 如果启用严格线程同步，并且RT比MT延后了2帧则进行同步 // todo

			// RT核心调度逻辑
			// 1. 收集从MT传递的需要渲染的对象 // todo
			// 2. 更新渲染状态，生成渲染命令 // todo
			// 3. 等待时间到达/队列超长，提交渲染命令列表到RHIT // todo
			// 4. 如果有同步命令或者RHIT队列过长，则同步 // todo
			// 5. 等待MT同步

			// RHIT核心调度逻辑
			// 1. 从队列接收渲染命令 // todo
			// 2. 解析为VK/DX等底层命令 // todo

			if (m_window.DispatchWindowMessage() == false)
				return 0;
		}
	}

	bool CoreApplication::PostEvent(IEngineEvent* event)
	{
		return m_applicationHost->PostEvent(event);
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
