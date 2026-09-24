// File /Native/Engine/Core/CoreApplication.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "CoreApplication.h"

#include "CommandLineUtils.h"
#include "DeferredDestroyQueue.h"
#include "../Coroutine/CoroutineScheduler.hpp"
#include "../Utils/NotificationBox/NotificationBox.hpp"
#include <cstdio>
#include <format>
#include <print>

#ifdef PENFRAMEWORK_OS_WIN32
#include "../OS/Windows/SystemInfo.h"
#include "../OS/Windows/Windows.h"
#endif // PENFRAMEWORK_OS_WIN32

namespace PenEngine
{
	namespace
	{
		/// @brief 键名最后一段转大写，作为 `--help` 里的取值占位（`window.width` → `WIDTH`）
		[[nodiscard]] String MakeConfigMetavar(StringView key)
		{
			StringView last = key;
			if (const Usize dot = key.RFind('.'); dot != StringView::NPos)
				last = key.Subview(dot + 1);

			String metavar;
			metavar.Reserve(last.Size());
			for (Usize i = 0; i < last.Size(); ++i)
			{
				char ch = last.Data()[i];
				if (ch >= 'a' && ch <= 'z')
					ch = static_cast<char>(ch - 'a' + 'A');
				metavar.PushBack(ch);
			}
			return metavar;
		}
	}

	CommandLineParser CoreApplication::BuildCommandLineParser(StringView programName)
	{
		CommandLineParser parser(programName, "PenFramework —— 实验性游戏引擎");
		// 内置的 -h / --help 由解析器构造函数自动声明，这里不需要重复添加

		for (const ConfigOption& option : ApplicationConfigOptions())
		{
			if (option.TakesValue)
				parser.AddOption(option.Key).Help(option.Help).Metavar(MakeConfigMetavar(option.Key));
			else
				parser.AddFlag(option.Key).Help(option.Help);
		}

		return parser;
	}

	CommandLineParseResult CoreApplication::ParseCommandLine(CommandLineParser& parser,
		const std::vector<String>& argumentVector)
	{
		auto parseResult = parser.Parse(argumentVector);
		if (!parseResult.has_value())
		{
			const CommandLineError& error = parseResult.error();

			// `--help` 与参数错误都不是「程序出错」，而是「这次调用该怎么收场」，
			// 因此折成携带文本与退出码语义的异常交给入口（见 RunApplication）
			if (error.Code == CommandLineErrorCode::HelpRequested)
				ThrowException(CoreApplicationHelpRequestedException(parser.HelpText()));

			ThrowException(CoreApplicationBadCommandLineException(parser.ErrorText(error)));
		}

		return std::move(*parseResult);
	}

	ApplicationConfig CoreApplication::BuildConfiguration(CommandLineParser& parser,
		std::vector<String> argumentVector)
	{
		// 解析器活在本函数作用域内即可：解析结果与配置都是自有数据，
		// 不会留任何指向解析器的引用（见 ApplicationConfig 的 I5）
		const CommandLineParseResult parseResult = ParseCommandLine(parser, argumentVector);

		ApplicationConfig config;
		// 文件层可缺省：不存在或 JSON 非法都不中止启动，由默认值兜底（诊断见 ParseWarnings）
		config.LoadFile();
		config.LoadCommandLine(parseResult, std::move(argumentVector));
		return config;
	}

	CoreApplication::CoreApplication(int argc, char* argv[])
	{
		// 取参：是否真的用 argc/argv 由平台决定（Windows 忽略它们，走宽字符 API）。
		// 两个入口都传各自的入口参数即可，不需要在入口处做平台判断
		std::vector<String> argumentVector = CommandLineUtils::GetArgumentVector(argc, argv);

		// 程序名：优先用平台给出的首项，缺失时退回固定名，保证帮助文本始终可读
		const StringView programName = argumentVector.empty()
			? StringView("PenFramework")
			: StringView(argumentVector.front());

		CommandLineParser parser = BuildCommandLineParser(programName);

		// 命令行解析与配置装配都在这里完成；结果全部是自有数据，不留下跨成员的生命周期约束
		m_config = BuildConfiguration(parser, std::move(argumentVector));

		// 启动期的第一个动作：把三层配置固化进 CoreApplicationData
		ApplyConfiguration();

		PreLoad();
		LoadInstancePlugin();
		LoadWindow();
	}

	int CoreApplication::RunApplication(int argc, char* argv[])
	{
		try
		{
			CoreApplication application(argc, argv);
			return application.Exec();
		}
		catch (const CoreApplicationCommandLineException& e)
		{
			// `--help` → 0（正常结束）；参数错误 → 1
			std::println("{}", e.Text());
			return e.SuccessExit() ? 0 : 1;
		}
	}

	void CoreApplication::ApplyConfiguration()
	{
		// 启动配置的三层取值（命令行 → 配置文件 → 默认值）由 ApplicationConfig 完成，
		// 这里只负责把结果固化进 CoreApplicationData。
		// 每一项的默认值都写在调用点上，因此「没有配置文件也能起来」是显然的。
#ifdef PENFRAMEWORK_OS_WIN32
		// 屏幕尺寸作为窗口尺寸的默认值：命令行与配置文件都没给时铺满屏幕
		const U32 screenWidth = static_cast<U32>(GetSystemMetrics(SM_CXSCREEN));
		const U32 screenHeight = static_cast<U32>(GetSystemMetrics(SM_CYSCREEN));
#else
		const U32 screenWidth = 1920;
		const U32 screenHeight = 1080;
#endif // PENFRAMEWORK_OS_WIN32

		m_applicationData.ApplicationTitle = m_config.GetString("application.title", "PenFramework");
		m_applicationData.ApplicationVersion = m_config.GetString("application.version", "Unknown");

		m_applicationData.WindowWidth = m_config.GetUInt("window.width", screenWidth);
		m_applicationData.WindowHeight = m_config.GetUInt("window.height", screenHeight);
		m_applicationData.Maximized = m_config.GetBool("window.maximized", false);

		m_applicationData.MaxTPS = m_config.GetUInt("timing.max_tps", 50);
		m_applicationData.MaxFPS = m_config.GetUInt("timing.max_fps", 0); // 0 = 不限帧

		m_applicationData.RenderInterface = m_config.GetString("render.interface", "D3D11");
		m_applicationData.EnableRHIThread = m_config.GetBool("render.enable_rhi_thread", false);
		m_applicationData.EnableRenderThread = m_config.GetBool("render.enable_render_thread", false);

#ifdef PENFRAMEWORK_OS_WIN32
		// 系统版本：注册表（人类可读名）+ RtlGetVersion（权威版本号），理由见 SystemInfo.h
		const SystemVersion systemVersion = QuerySystemVersion();
		m_applicationData.OSVersion = systemVersion.ToString();
#else
		m_applicationData.OSVersion = "Unknown OS Version";
#endif // PENFRAMEWORK_OS_WIN32

		// 把生效配置连同来源层输出一次：三层优先级的唯一可观察证据就是这里。
		// Terminal 构建走 stdout；Native 没有控制台，改用信息框（否则配置生效与否完全不可见）。
		if (!m_config.FileLoaded())
			std::printf("[Config] 配置文件未加载：%s\n", m_config.ConfigFilePath().Data());

		for (const String& warning : m_config.ParseWarnings())
			std::printf("[Config] %s\n", warning.Data());

		String summary;

		// 参数是**实际生效值**、`T` 必须与取值时一致：这样 GetLayer<T> 才会
		// 报出真正提供该值的层（命令行给了非法值时它会正确地指向文件层/默认值）
		auto appendLine = [&summary, this](StringView key, auto value)
		{
			using ValueType = std::remove_cvref_t<decltype(value)>;

			String text;
			if constexpr (std::is_same_v<ValueType, String>)
				text = value;
			else
				text = std::format("{}", value);

			summary += key;
			summary += " = ";
			summary += text;
			summary += "    [";

			if (auto layer = m_config.GetLayer<ValueType>(key); layer.has_value())
				summary += ConfigLayerName(*layer);
			else
				summary += "默认值";

			summary += "]\n";
		};

		appendLine("application.title", m_applicationData.ApplicationTitle);
		appendLine("application.version", m_applicationData.ApplicationVersion);
		appendLine("window.width", m_applicationData.WindowWidth);
		appendLine("window.height", m_applicationData.WindowHeight);
		appendLine("window.maximized", m_applicationData.Maximized);
		appendLine("timing.max_tps", m_applicationData.MaxTPS);
		appendLine("timing.max_fps", m_applicationData.MaxFPS);
		appendLine("render.interface", m_applicationData.RenderInterface);
		appendLine("render.enable_rhi_thread", m_applicationData.EnableRHIThread);
		appendLine("render.enable_render_thread", m_applicationData.EnableRenderThread);

		// 系统版本不是配置项，单独列一行（无来源层可标）
		summary += "os.version = ";
		summary += m_applicationData.OSVersion;
		summary += "    [系统]\n";

		std::printf("[Config] 生效配置（命令行 > 配置文件 > 默认值）：\n%s", summary.Data());

#ifdef PENFRAMEWORK_OS_WIN32
		// 仅 Native 构建需要弹窗：它没有控制台，stdout 无处可看
		#ifndef PENFRAMEWORK_BUILD_TERMINAL
		MessageBoxW(nullptr, summary.ConvertToString<wchar_t>().Data(), L"PenFramework 启动配置", MB_OK | MB_ICONINFORMATION);
		#endif // !PENFRAMEWORK_BUILD_TERMINAL
#endif // PENFRAMEWORK_OS_WIN32
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
			// 1.1 更新主机场景Fix状态 Host -> FixedUpdate()
			// 1.2 计算碰撞 // todo
			// 2.  收集并分发窗口事件
			// 2.1 处理平台窗口事件 Window.DispatchWindowMessage
			// 2.2 由窗口函数调用框架窗口事件函数 Window.DispatchWindowMessage -> this -> PostEvent
			// 2.3 发送到主机 Host -> PostEvent(event)
			// 
			// 可能的更改：
			// 这一块也可能是
			// 2.1 处理平台窗口事件 Window.TranslateWindowMessage
			// 2.2 收集框架事件 Window.GetAndClearWindowMessage
			// 2.3 发送到主机 while window_message_vector not empty | Host -> PostEvent(event)
			// 
			// 3.  更新主机场景状态 Host -> Update(dt) / CLR Host Manager -> Update() // todo
			// 4.  更新其他可调度子模块
			// 4.1 更新协程调度 CoroutineScheduler -> Update()
			// 4.2 更新延迟删除队列 DeferredDestroyQueue -> Update()
			// 4.3 更新通知框 NotificationBox -> Update()
			// 
			// 可能的更改：
			// 这一步可能需要引入中央单例调度模块，让这些子模块注册到调度模块内，这样不用在CoreApplication内写大量的Update
			// 但是也需要提供一个IUpdatableEnginePlugin接口类，让Coroutine等类继承
			// 也许cpp26反射属性可以解决这个问题，例如
			// template for CollectUpdatableEnginePlugin()
			//		plugin.Update() // 实际上展开为一组plugin的Update()
			// 
			// 5.  准备渲染 // todo
			// 5.1 收集渲染对象与渲染剔除 Host -> OnPreCull(), Cull, OnPostCull() // todo
			// 5.2 拷贝并交换双缓冲Proxy Host -> OnCreateProxy()
			// 事实上，拷贝并交换双缓冲交由RT线程是更好的。
			// 但是由于OnCreateProxy()是一个对象事件接口，所以不能让RT线程负责（也许可以，但是为了安全性最好别这样做）
			// Unity是采用MT调用Proxy，而UE将Cull,Proxy,Render等操作全都抽到了RT上处理
			// 目前来看，没有需要用到后者的必要
			// 5.3 准备渲染 // todo
			// 如果启用RT线程，向RT传递渲染对象
			// 如果未启用RT线程，则调度所有渲染对象
			// 5.4 如果启用严格线程同步，并且RT比MT延后了2帧则进行同步 // todo

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

			CoroutineScheduler::GetInstance().Update();
			DeferredDestroyQueue::GetInstance().Update();
			NotificationBox::GetInstance().Update();
		}
	}

	bool CoreApplication::PostEvent(IEngineEvent* event)
	{
		// return m_applicationHost->PostEvent(event);
		return false;
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

	bool CoreApplication::LoadInstancePlugin()
	{
		CoroutineScheduler::GetInstance();
		DeferredDestroyQueue::GetInstance();

		NotificationBoxConfig cfg = {.Organization = "Pen", .Application = "PenFramework", .Version = "0.1", .AllowShellRegistration = true, .IconPath = ""};

		auto exp = NotificationBox::GetInstance().Initialize(cfg);

		return exp.has_value();
	}

	bool CoreApplication::LoadWindow()
	{
		WindowInitContext windowInitContext;
		windowInitContext.Width = m_applicationData.WindowWidth;
		windowInitContext.Height = m_applicationData.WindowHeight;
		// Maximized 来自配置层，Win32Window::Create 已支持（WS_MAXIMIZE）
		windowInitContext.Maximize = m_applicationData.Maximized;
		windowInitContext.ApplicationTitle = m_applicationData.ApplicationTitle;
		windowInitContext.CoreApplication = this;

#ifdef PENFRAMEWORK_OS_WIN32
		windowInitContext.HInstance = m_applicationData.HInstance;
		m_applicationData.Hwnd = m_window.Create(windowInitContext);
#endif  // PENFRAMEWORK_OS_WIN32
		return true;
	}
}
