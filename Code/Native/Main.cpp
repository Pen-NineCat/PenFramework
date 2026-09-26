// File /Native/Main.cpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "Engine/Core/CoreApplication.h"
#include "Engine/IO/Filesystem/FileDevice.hpp"

// 进程入口只做三件事：认领入口签名、把 `argc/argv` 交给 CoreApplication、返回它的退出码。
// 取参、解析、装载配置、构造应用、主循环都在 CoreApplication 内（见其 I1）。
//
// 入口形态由 CMake 变量 OUTPUT_BUILD_TYPE 决定：
//   Terminal —— 控制台子系统（/SUBSYSTEM:CONSOLE），入口为 int main
//   Native   —— Windows 子系统（/SUBSYSTEM:WINDOWS），入口为 wWinMain
//
// I1. 两个入口严格互斥：给定一次构建，只能出现其中一个。
//     注意 PENFRAMEWORK_OS_WIN32 在 Windows 上恒为已定义（见 Core/Environment.h），
//     因此判定 Native 入口必须用「非 Terminal 且 Windows」，
//     若只写 #ifdef PENFRAMEWORK_OS_WIN32，控制台构建里会同时编出 wWinMain（死代码，
//     并且与 main 一起暴露为两个入口符号）。
//
// I2. 两个入口都把各自的入口参数**原样**交给 CoreApplication，不在入口处做平台判断：
//     Windows 侧由 CommandLineUtils 忽略它们并改走宽字符 API（原因见其 I1），
//     其他平台直接采用。这样两种构建的解析结果一致，入口也不必关心差异。
//
// I3. 入口用窄字符 `main` 而非 `wmain`：参数只是转交，`main` 已经够用，
//     用窄入口就不必改链接器入口点
//     （/SUBSYSTEM:CONSOLE 默认寻找 `main`，`wmain` 需显式指定 wmainCRTStartup）。

#if defined(PENFRAMEWORK_BUILD_TERMINAL) || !defined(PENFRAMEWORK_OS_WIN32)

/// @brief 控制台入口（Terminal 构建，或任何非 Windows 平台）
int main(int argc, char* argv[])
{
	return PenEngine::CoreApplication::RunApplication(argc, argv);
}

#endif // PENFRAMEWORK_BUILD_TERMINAL || !PENFRAMEWORK_OS_WIN32

#if !defined(PENFRAMEWORK_BUILD_TERMINAL) && defined(PENFRAMEWORK_OS_WIN32)

/// @brief 窗口入口（Native 构建，仅 Windows）
/// @note `wWinMain` 没有 `argc/argv`（只有不含 exe 路径的未拆分 `lpCmdLine`），
///       因此这里传 `(0, nullptr)`：Windows 侧本就会忽略它们，行为与其他入口一致
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	(void)hInstance;     // CoreApplication 内部自行取 GetModuleHandle
	(void)hPrevInstance; // 16 位残留，恒为 nullptr
	(void)lpCmdLine;     // 不含 exe 路径且未拆分，见 I2
	(void)nShowCmd;      // 窗口显示方式由配置层决定

	return PenEngine::CoreApplication::RunApplication(0, nullptr);
}

#endif // !PENFRAMEWORK_BUILD_TERMINAL && PENFRAMEWORK_OS_WIN32
