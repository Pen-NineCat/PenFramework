# PenFramework

## 注意

本README未完工，可能有部分内容不准确，或者被标记为🚧**该条目不完整**

如果对此有任何疑问，欢迎提出Issue

## 介绍

这是一个实验性的游戏引擎项目，由 C++ 与 C# 混合编译，使用 CLRHost 托管 C# 代码，同时仍然允许使用 C++ 开发用户程序。

## 第三方依赖

本项目使用 vcpkg 与 nuget 进行第三方依赖管理。

### vcpkg

本项目使用 vcpkg 管理 C++ 库。

前置条件 

- 未安装 vcpkg ，请前往 [vcpkg github](https://github.com/microsoft/vcpkg) 并参照 vcpkg 教程完成安装，随后进行下述检查。
- 已经安装了 vcpkg ，请检查是否设置了 VCPKG_ROOT 环境变量，或者修改 [CMakePresent.json](CMakePresets.json) 中的 CMAKE_TOOLCHAIN_FILE 至您的 vcpkg 目录
- 请确认您的当前网络可以正常访问 vcpkg 仓库。

- 可以参照 vcpkg.json 来了解本项目使用哪些 C++ 库

### Nuget

本项目使用 Nuget 管理 C# 库

🚧**该条目不完整，原因：目前未进行 C# 的开发任务**

## 环境要求

- 操作系统：Windows X86 提供完整支持，其他平台只提供接口，不保证实现可用
- C++版本：23或更高
- .NET / CLR：🚧**该条目不完整**
- 构建工具：CMake ≥ 4.0、Ninja、.NET环境


## 快速开始

🚧**该条目不完整，原因：目前不是可发布状态**

### 获取代码

```bash
git clone https://github.com/Pen-NineCat/PenFramework.git
cd PenFramework
```

## 许可证

本项目遵循 MIT 许可证 [License](LICENSE.txt)

## 致谢

- 感谢本项目用到的所有上游开源项目