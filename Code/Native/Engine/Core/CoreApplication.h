// File /Native/Engine/Core/CoreApplication.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "ApplicationConfig.h"
#include "IApplicationHost.hpp"
#include "Window/Window.hpp"
#include "../Event/IEngineEvent.hpp"
#include "../Exception/Exception.hpp"
#include "../Utils/CommandLineParser.hpp"
#include <vector>

namespace PenEngine
{
	/// @brief 启动期**预期内**的失败：命令行非法，或用户请求了 `--help`
	/// @note I1. 只承载「调用方需要据此决定退出码」的失败；取参本身的异常
	///       （如非法 UTF-8 编码）不在此列，按普通异常直接向上传播。
	///       I2. `Text` 是可直接写进 stdout 的完整文本（含用法行），
	///       入口不需要再自己拼错误信息。
	///       I3. `Detail` 固定为类型名而非文本内容：`Exception::what()` 返回其内部
	///       `std::string` 的指针，若把临时 `String` 传进去会立刻悬垂。
	class CoreApplicationCommandLineException : public Exception
	{
	public:
		CoreApplicationCommandLineException(StringView type, String text, bool successExit)
			: Exception(type, type), m_text(std::move(text)), m_successExit(successExit)
		{}

		/// @brief 面向用户的完整文本（`--help` 是帮助，其余是 `usage + error:`）
		[[nodiscard]] const String& Text() const noexcept { return m_text; }
		/// @brief 该情况是否属于「正常结束」：`--help` 为 true（退出码 0），参数错误为 false（退出码 1）
		[[nodiscard]] bool SuccessExit() const noexcept { return m_successExit; }
	private:
		String m_text;
		bool m_successExit = false;
	};

	/// @brief 命令行参数非法（含未知选项、缺值、类型不符等）
	class CoreApplicationBadCommandLineException : public CoreApplicationCommandLineException
	{
	public:
		explicit CoreApplicationBadCommandLineException(String text)
			: CoreApplicationCommandLineException("CoreApplicationBadCommandLineException", std::move(text), false)
		{}
	};

	/// @brief 用户请求了 `--help` / `-h`：不是错误，正常退出并展示帮助
	class CoreApplicationHelpRequestedException : public CoreApplicationCommandLineException
	{
	public:
		explicit CoreApplicationHelpRequestedException(String text)
			: CoreApplicationCommandLineException("CoreApplicationHelpRequestedException", std::move(text), true)
		{}
	};

	class CoreApplication
	{
	public:
		/// @brief 构造应用并完成全部启动期初始化（取参 → 解析 → 装载配置 → 建窗口）
		/// @param argc 入口参数个数
		/// @param argv 入口参数数组
		/// @note I1. 入口一律把 `argc/argv` 交进来即可：是否真的使用它们由平台决定 ——
		///       Windows 忽略它们，改由 `CommandLineUtils` 走宽字符 API；
		///       其他平台直接采用。见 `CommandLineUtils` 的 I1。
		///       I2. 失败方式：命令行非法抛 `CoreApplicationBadCommandLineException`，
		///       `--help` 抛 `CoreApplicationHelpRequestedException`（两者都携带可直接输出的文本）。
		///       进程入口应捕获这两个并返回对应的退出码 —— 见 `RunApplication()`。
		///       I3. 配置项都有默认值，配置文件可缺省，因此构造失败**只**可能来自命令行。
		CoreApplication(int argc, char* argv[]);

		int Exec();

		bool PostEvent(IEngineEvent* event);

		/// @brief 启动配置的只读访问（窗口尺寸、标题、线程开关等）
		[[nodiscard]] const ApplicationConfig& Config() const noexcept { return m_config; }

		/// @brief 进程入口的完整启动流程：构造 → 主循环，并把启动期失败折成退出码
		/// @return 进程退出码
		/// @note I1. 这是进程入口**唯一**该调用的函数。它会捕获
		///       `CoreApplicationCommandLineException` 并打印其文本
		///       （`--help` → 0，参数错误 → 1）；其他异常按原样向上传播，不静默吞掉。
		[[nodiscard]] static int RunApplication(int argc, char* argv[]);

	private:
		/// @brief 把 `ApplicationConfigOptions()` 声明进一个解析器
		/// @param programName 帮助与错误信息里显示的程序名
		/// @note 全部为可选参数：命令行只是最高优先级的**来源**，不是必填项
		[[nodiscard]] static CommandLineParser BuildCommandLineParser(StringView programName);

		/// @brief 解析命令行；失败与 `--help` 都通过抛 `CoreApplicationCommandLineException` 表达
		/// @param parser 已声明完毕的解析器
		/// @param argumentVector 原始参数（含 `argv[0]`）
		/// @return 解析结果（自有数据，不引用 `parser`）
		[[nodiscard]] static CommandLineParseResult ParseCommandLine(CommandLineParser& parser,
			const std::vector<String>& argumentVector);

		/// @brief 装配三层配置：命令行层 + 配置文件层
		/// @param parser 已声明完毕的解析器；只在本次调用内使用
		/// @param argumentVector 原始参数（含 `argv[0]`）
		[[nodiscard]] static ApplicationConfig BuildConfiguration(CommandLineParser& parser,
			std::vector<String> argumentVector);

		/// @brief 把生效配置固化进 `m_applicationData`，并输出一次带来源层的摘要
		void ApplyConfiguration();

		/// @brief 启动期第一步：拉起日志器（可执行文件目录下的 `Logs/PenFramework.log`）
		/// @return 成功返回 true；失败返回 false（不阻断启动，Warn 以上仍有 stderr 兜底）
		/// @note 日志路径不是配置项，因此本步骤排在解析命令行**之前** ——
		///       这样连「命令行非法」这类启动期错误也能留在日志里
		/// @note 日志器是进程级单例：本函数只负责初始化，生命周期与冲刷由它自己承担
		bool InitializeLogger();

		struct CoreApplicationData
		{
			String ApplicationTitle;
			String ApplicationVersion;

			/// @brief 系统版本展示串，如 "Windows 10 Pro 25H2 (Build 26200.9457)"
			/// @note 仅 Windows 填充；判断 10/11 请看内核 Build 号，不要解析本字符串
			String OSVersion;

			U32 WindowWidth;
			U32 WindowHeight;
			bool Maximized;

			/// @brief 渲染后端名；RHI 尚未接入，当前只记录
			String RenderInterface;
			U32 MaxFPS;
			U32 MaxTPS;
			bool EnableRHIThread;
			bool EnableRenderThread;
			#ifdef PENFRAMEWORK_OS_WIN32
			HINSTANCE HInstance;
			HWND Hwnd;
			#endif // PENFRAMEWORK_OS_WIN32
		};

		bool PreLoad();
		bool LoadInstancePlugin();
		bool LoadWindow();

		/// @note 配置是自有数据，不含跨成员的生命周期约束
		ApplicationConfig m_config;

		CoreApplicationData m_applicationData;
		OSWindow m_window;

		std::unique_ptr<IApplicationHost> m_applicationHost;
	};
}
