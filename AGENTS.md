# PenFramework · Agent 工作须知

本文是接入本仓库的 agent 的工作须知。内容限于**稳定约束**：结构性事实、目录归属规则、编码约定、
构建与验证方式、禁区。**不包含会随代码漂移的统计数字**——需要这类信息时自行用命令查。

---

## 0. 冲突裁决与工作原则

**裁决顺序**（遇到规则冲突时）：

1. **作者的口头/书面裁定**（§1 记录的即是）
2. **仓库当前代码** —— 现状与本文不符时，除 §1 外**以代码为准**
3. `Docs/` 与 `PersonalWorkaround/README.md`
4. `PersonalWorkaround/PenFramework-old/` 下的历史文档（参考层，部分已明确作废）

**工作原则**（按重要性）：

1. **先判断要不要动工作区**：只有**检索 / 分析 / 研究**类任务，或作者 **明确要求"先在临时工作区工作"** 时，
   才在 `PersonalWorkaround/` 下新建文件夹；**其余情况直接把结果输出给作者**，不要凭空造目录（见 §2.1），如果 **不确定** 当前是否需要新建临时工作区，先询问作者
2. **报告"某处不存在 X"之前，先证明你的搜索能命中一个已知存在的 X。**
   搜索失败时**先怀疑查询本身**，不要归因于"文件在变"或"工具异常"。
3. **同一结论换一条无关的实现路径复核**（如字符串匹配 vs 逐字节/码点比较）。
4. **不要修改在途未提交改动涉及的文件**，除非作者明确授权（§9 Q6）。
5. **不要把统计值写进文档**——文件数、测试数、逐文件清单会立刻过期。需要时用命令查。
6. **改代码前先读相邻代码的注释**，尤其带编号不变量（如 `I1`–`I5`）的段落；改完同步更新注释。
7. **不要顺手做无关的格式化或重命名**——会把功能改动淹没在噪音里，难以 review。
8. ⚠️ 本文是"结论"，不是一手事实。据此**断言某处代码的行为**前，**自己打开那个文件确认**。

---

## 1. 作者已确认的裁定（优先级最高）

| 编号 | 议题 | 裁定 |
| --- | --- | --- |
| D1 | `PersonalWorkaround/` 定位 | **临时工作区**，**全体 agent 共用**。其 `README.md` 是**有效 README**，优先阅读。**仅当**任务是检索 / 分析 / 研究类，或作者明确要求"先在临时工作区工作"时，才在其中**新建子文件夹**存放产出；**其余情况直接输出结果**（见 §2.1 的闸门），如果 **不确定** 当前是否需要新建临时工作区，先询问作者。**判定规则见 §2.1**：属于**自己本次任务脉络**的产出（含自己派发的子 agent 产出）可直接修改 / 移动 / 重命名 / 删除，**无需请示**；**超出该脉络的，回报作者，不要自行处置** |
| D2 | 许可证与文件头 | **MIT，4 行头**。|
| D3 | agent 的任务域 | **不固定**，按当次任务决定 |
| D4 | 文档语言 | **中文** |

---

## 2. 工作区治理与禁区

### 2.1 三棵树

> ⚠️ **先看这条：要不要用临时工作区？**
>
> 只有满足以下任一条时，才在 `PersonalWorkaround/` 下**新建文件夹**并开始工作：
> - 任务是**检索 / 分析 / 研究**类（需要在工作区留下过程材料、多轮产出或供后续会话引用）；
> - 作者**明确要求**"先在临时工作区工作"（或等价表述）。
>
> **其余情况直接输出结果**——不要为了"留痕"而凭空建目录、写中间文件。
> 判据：**作者要的是结论，不是文件树。**

| 路径 | 性质 | 可否修改 |
| --- | --- | --- |
| `Code/`、`CMakeLists.txt`、`CMakePresets.json`、`vcpkg.json`、`Visualizers/`、`Docs/` | 正式代码与工程配置 | ✅ 可改（遵守 §5、§6） |
| `PersonalWorkaround/PenFramework-old/` | 旧框架参考树（**自带独立 `.git`**） | ❌ **只读** |
| `PersonalWorkaround/PenFramework-old/PenFramework-old/` | 更古老的第三层副本（**自带独立 `.git`**） | ❌ **只读**，基本不必打开 |

**判据**：路径里出现 `PersonalWorkaround/` 即为**参考区**，默认只读。
需要参考其中某段代码时，**拷贝到自己新建的子文件夹**，不要就地改。

> 📌 **例外：属于自己任务脉络的工作产出，可自由整理**
>
> `PersonalWorkaround/` 是**全体 agent 共用的临时工作区**，内容会不断新增。
> **不要试图维护"哪些是我做的"这类白名单**——它必然过期。
>
> 判断顺序如下（**先看第 1 条，命中即可直接行动**）：
>
> **1. 归属判断——是否属于自己这次任务的脉络？**
>
> 满足以下任一条，即为**自己的工作产出**，可**直接**修改 / 移动 / 重命名 / 删除，**无需请示作者**：
> - 由**本次任务中自己直接创建**的目录或文件；
> - 由**本次任务中自己派发的子 agent 创建**的目录或文件（含它们各自的中间产物）；
> - 汇总场景：主 agent 为本次任务建立了 `SomeThing/`，子 agent 分别建立了
>   `Windows-SomeThing/`、`Linux-SomeThing/`、`macOS-SomeThing/`——
>   主 agent **直接**把它们移动 / 合并 / 重命名进统一结构，**这是正常操作，不是越权**。
>
> **2. 必要性判断——改动是否真的必要？**
>
> 即使归属满足第 1 条，也要问：
> - 合并/移动是否**减少重复、消除分叉**，还是只是换个位置？
> - 是否**有下游引用**会因此失效（其他文档、索引、脚本里的路径）？
> - 删除这个中间产物，是否会让**作者无法复核结论**？
>
> 三者都无正面理由时，**保留原状**。
>
> **3. 归属不可判定 → 回报作者，不要猜**
>
> 内容归谁所有，**在磁盘上没有记录**（无归属标记、目录名不自述、时间戳不足以区分，
> 且跨会话后"谁建的"这一信息已不存在）。因此：
> - **属于自己任务脉络的**（第 1 条）→ 直接整理；
> - **超出自己任务脉络的** → **回报作者并说明意图，不要自行移动或删除**；
> - 不要用"看起来像是 agent 建的"作为判断依据——**这不是证据**。
>
> **4. 永久不可动项（与归属无关，任何来源都不得修改 / 移动 / 删除）**
>
> - `PersonalWorkaround/PenFramework-old/`（旧框架参考树，独立 git 仓库）
> - `PersonalWorkaround/jsoncpp/`（第三方库克隆，独立 git 仓库）
> - `PersonalWorkaround/tools/`（作者个人工具，含 `.token`）
> - `PersonalWorkaround/Asset/`（作者的调研资料）
> - `PersonalWorkaround/README.md`（有效 README 草稿）
>
> 这 5 项是**固定的**，不会随工作开展而增加——因此**不需要任何人维护名单**。
> 除此之外的目录，一律按第 1–3 条判断。

> 💡 **建议：给自己的产出留一个归属标记**
>
> 新建工作目录时，放一个单行 `OWNER.md`，写明是谁、什么时候、为哪个任务创建的：
>
> ```
> task: NotificationBox 跨平台调研 / 主 agent
> created: 2026-09-22
> role: 汇总目录（合并各平台子 agent 产出）
> ```
>
> 这让**后续会话的 agent 也能判断归属**，把"不可判定"变成"可查"。
> 这是建议而非强制，但能显著减少来回请示。
>
> 📌 本目录名 `PersonalWorkaround` 已统一为正字（此前误拼为 `PersonalWorkaroud`）。
> 若在旧文档或 `.vs/` 缓存中看到旧拼写，**以本文件的 `PersonalWorkaround` 为准**。

> ⚠️ 那两棵子树各有**独立的 git 仓库**（`PersonalWorkaround/` 下另有若干第三方克隆也带 `.git`）。
> 在其中执行 git 命令会作用于**另一个仓库**。开工前先确认位置：
> ```powershell
> git rev-parse --show-toplevel   # 应为仓库根
> ```

### 2.2 `PersonalWorkaround/README.md` 的正确读法

它是**仓库根 README 的重写草稿**。其中大量 `🚧**该条目不完整**` 是**待补齐标记，不是事实陈述**。

- 它给出的项目定位、依赖管理方式、环境要求、构建命令**可信**（根 `README.md` 目前只有标题）；
- 标 `🚧` 的段落当前**没有内容**，不要当成"作者已写好但没贴出来"，也不要自行填充后当作既定事实。

> 📌 §2.1 的"参考区只读"是针对**修改**的约束，不禁止**阅读**。这份草稿要读
> （它本身在「永久不可动项」里，**只能读，不要改**）。

### 2.3 其它禁改 / 慎改区域

下表是**固定的**项目级禁区。`PersonalWorkaround/` 内各目录的处置**不以此表为准**，
见 §2.1 的判据（归属 / 必要性 / 回报 / 永久不可动项）。

| 路径 | 说明 |
| --- | --- |
| `out/` | 构建产物，含 `vcpkg_installed/`（完整第三方源码）。**不要编辑**，也不要把它当本项目代码。用 glob/grep 搜代码时**必须排除 `out/`** |
| `.vs/` | Visual Studio 本地状态。不要手工编辑 |
| `vc140.pdb`（仓库根） | 遗留调试符号，来源不明。**不要删除**，也不必分析 |
| `ApplicationData/`、`Code/Native/Editor/`、`Code/Native/Engine/Asset/` | **已建好的空占位目录**（无隐藏文件，故 git 不跟踪、`git status` 看不到）。不要往里塞临时文件，也不要当作"没人用的空地" |
| `PersonalWorkaround/jsoncpp/` | 第三方库的完整独立克隆（自带 `.git`）。不要改、不要提交 |
| `PersonalWorkaround/tools/GitHubMCP/` | 作者个人工具，含 `.token`。**不要读取、不要输出 `.token` 内容** |
| `PersonalWorkaround/Asset/`、`PenFramework-old/Docs/`、`PenFramework-old/.research/` | 调研资料，只读参考 |

> 📌 `PersonalWorkaround/Asset/`（**调研文档**）与 `Code/Native/Engine/Asset/`（**引擎资源模块的空占位目录**）
> 是两个不同的东西，路径相近，勿混淆。

### 2.4 git 管理

- ❌ **不要** `git stash` / `git checkout --` / `git reset` / `git clean`，或任何丢弃改动、删除文件的操作；
- ❌ 若非指定 **commit** 或 **review** 任务，**不要**提交或切换分支；
- ✅ 查看改动前状态用只读命令：`git diff <file>`、`git show HEAD:<file>`；
- ✅ 需要改代码前，**先向作者确认**改哪个文件（本仓库常有在途未提交改动）。

---

## 3. agent 的授权范围

**可以做**

1. **在满足 §2.1 闸门的前提下**，在 `PersonalWorkaround/` 下**新建子文件夹**：拷贝源码做实验、
   撰写研究/设计文档、写验证脚本。**不满足闸门时不要建**——直接输出结果。
2. 阅读仓库内任意文件（`.token` 除外，含 §2.2 的草稿 README）。
3. 在作者已下达任务的前提下修改 `Code/` 下的相关代码，并遵守 §5、§6。
4. 运行构建与测试（产物写入 `out/` 属正常）。

**不可以做**

1. 修改 / 移动 / 重命名 / 删除 §2.1 第 4 条的 **5 项永久不可动项**
   （`PenFramework-old/`、`jsoncpp/`、`tools/`、`Asset/`、`PersonalWorkaround/README.md`）。**无例外。**
2. 移动 / 删除**超出自己任务脉络**的工作区内容——应回报作者，说明意图与理由（§2.1 第 3 条）。
3. 丢弃、回滚、提交在途未提交的改动。
4. 把旧文档的约定（尤其 MPL-2.0 头）当作现行契约。
5. 在 §9 列出的事项上**自行拍板**——那些是作者未决的决策。

---

## 4. 项目定位与设计基线

### 4.1 定位

PenFramework 是一个**实验性游戏引擎**，**C++ 核心层 + C# 用户层**：

- 进程入口、主循环、渲染、资源、内存由 **C++ 持有**；
- 托管运行时（.NET / CLR）被**嵌入**，C# 用于用户层（脚本 / 业务逻辑）；
- 同时**仍允许**用纯 C++ 开发用户程序。

调用链的**目标形态**（非现状）：

```
cpp main
  └─ CoreApplication::Exec()            ← CoreLoop，C++ 完全拥有
       ├─ 计算 dt / 固定步长累加
       ├─ 泵窗口消息
       ├─ 更新场景树 → Entity::Update(dt) → [C# 函数指针] 进入托管环境
       ├─ 渲染提交
       └─ Present
```

当前 `Exec()` 只实现了「派发窗口消息」这一步，其余以 `// todo` 留位。

### 4.2 已确认的设计决策

| 议题 | 决策 |
| --- | --- |
| 对象模型 | 混合式：C++ Entity 壳 + 组件槽 + C# 脚本（可平滑演进到 ECS） |
| C++ / C# 边界 | **C ABI 边界 + 纯虚接口 table**（ABI 稳定、可版本化、支持程序集热更） |
| RHI 抽象层级 | **高层命令表，多后端各自 lowering**（D3D11 内部即时提交，D3D12/VK 走真实 command list） |
| 线程模型 | **先单线程，但架构预留拆分点**（命令录制与资源生命周期从第一版起即可跨线程） |
| .NET 承载 | **hostfxr 与 NativeAOT 作为两个编译选项**，由编译选项切换 |
| 编译工具链 | 对 CMake 与 .NET 工具链做**封装式调用**，不自己实现构建系统 |
| 首期范围 | 工程骨架 + 工具链 + 一个 RHI 后端 + 场景树 + 渲染闭环；**.NET 互操作在骨架稳定后接入** |

### 4.3 .NET 互操作的三条硬约束

做互操作前必须知道这三条**事实性限制**（完整论证见 §8 索引中的 .NET 调研文档）：

| # | 限制 | 结论 |
| --- | --- | --- |
| ① | `load_assembly_and_get_function_pointer` 总是把程序集加载进隔离的 ALC，且**无法卸载**或释放原生函数指针 | 要支持热更，必须用 `hdt_load_assembly` + `hdt_get_function_pointer`，由托管侧自持可回收 ALC |
| ② | 官方把"来自 ALC 内外的强 GC 句柄"（`Normal` / `Pinned`）列为**可卸载性的硬性阻碍** | 放弃"把 `GCHandle.ToIntPtr` 交给 C++"；改用整数 `{index, generation}` 句柄指向托管侧对象表（附带收益：陈旧句柄可被检测，而非 use-after-free） |
| ③ | `nethost` / `hostfxr` **只支持框架依赖部署** | `--self-contained` / `PublishSingleFile` / `PublishTrimmed` 全不可用。零安装需随包分发**框架布局**的私有运行时目录，并让 `hostfxr_initialize_parameters.dotnet_root` 指向它 |

**由此确定的脚本桥形态**：**三个稳定入口 + 整数句柄表**。

---

## 5. 目录结构与代码归属

### 5.1 代码区布局

```
PenFramework/
├─ CMakeLists.txt              ← 根构建脚本（唯一 exe + add_subdirectory(Memory)）
├─ CMakePresets.json           ← 构建预设
├─ vcpkg.json                  ← C++ 依赖清单（manifest 模式）
├─ Visualizers/                ← VS 调试可视化器
├─ Docs/                       ← 文档
├─ Code/
│  └─ Native/                  ← 顶层代码根
│     ├─ Main.cpp              ← 进程入口
│     ├─ Engine/               ← 主程序源码（GLOB_RECURSE 全量编入 exe）
│     ├─ Memory/               ← 独立模块 PenMemory.dll（不参与主程序编译）
│     └─ Editor/               ← 空占位（编辑器规划位）
├─ ApplicationData/            ← 空占位（运行时数据规划位）
├─ out/                        ← 构建产物（勿编辑）
└─ PersonalWorkaround/          ← 临时工作区（git 忽略）
```

`Engine/` 下的模块与职责：

| 目录 | 职责 | 关键类型 |
| --- | --- | --- |
| `Core/`（含 `Window/`） | 应用核心、平台/编译器判定宏与基础类型别名、延迟销毁队列 | `CoreApplication`、`Environment.h`、`IApplicationHost`、`DeferredDestroyQueue` |
| `Event/` | 事件类型与输入常量 | `IEngineEvent`、各 `*Event.hpp` |
| `String/` | 字符串、字符串视图、SIMD 查找 | `String`、`StringView`、`StrSearchUtils` |
| `Object/`（含 `Internal/`） | 属性对象、对象信号、引用计数 | `PObject`、`SignalObject`、`RefObject` |
| `IO/`（含 `Filesystem/`、`Internal/`） | 路径、URI、缓冲接口、文件设备 | `Path`、`PathView`、`URI`、`Win32FileDevice` |
| `Json/` | JSON 解析与构建 | `Json` |
| `Coroutine/` | 协程任务与调度器 | `CoroutineTask`、`CoroutineScheduler` |
| `Utils/`（含 `NotificationBox/`） | 单例、作用域守卫、时间、位标志、字节数组、概念 | `Singleton`、`Flag`、`ByteArray` |
| `Memory/` | `PenMemory.dll` 的 C++ 封装与全局 `new/delete` 接管 | `PenEngine::Memory::*`、`MemoryOperator` |
| `Exception/` | 统一异常基类与抛掷入口 | `Exception`、`ThrowException` |
| `DebugTools/` | 调试断言/校验宏 | `DEBUG_VERIFY_REPORT` 系列 |
| `OS/Windows/` | Windows 头收敛与 COM 初始化 | `Windows.h`、`WindowsMinDef.h` |

> ⚠️ 事件枚举 `PredefinedEngineEventType` 定义在 **`Core/`**（`IEngineEvent.hpp` 内），**不在 `Event/`**。

### 5.2 归属规则

**R1 · 模块化目录**
`Code/Native/Engine/<模块>/`，每个模块一个目录，目录名 **PascalCase**。

**R2 · 私有实现放 `Internal/`**
只被同模块其它文件使用的辅助设施放 `<模块>/Internal/`，用子命名空间 **`PenEngine::Internal`**。
"平台实现的私有头"（如 `Win32Window.h`）**与 `.cpp` 同目录**，不要塞进 `Internal/`。

**R3 · 头文件与实现同目录**
`.h` / `.hpp` 与对应 `.cpp` 同目录。**不设** `include/` 与 `src/` 分离。

**R4 · 头文件扩展名**
与同目录既有文件保持一致；新模块若全为头文件实现，用 `.hpp`。

**R5 · `Memory` 目录（唯一例外）**

| 项 | 值 |
| --- | --- |
| 位置 | `Code/Native/Memory/`（**不在 `Engine/` 下**） |
| 命名空间 | **`PenMemory`**（不是 `PenEngine`） |
| 对外 ABI | 纯 C 的 `extern "C"`，声明在 **`Code/Native/Memory/Interface.h`**（实现 `Interface.cpp`） |
| 布局 | 文件**平铺**，**不使用 `Internal/`** |
| 构建 | 自己的 `CMakeLists.txt`，产出 `PenMemory.dll` + 导入库 |
| 编译范围 | **不参与主程序编译**（避免同一份分配器被编两遍） |
| 主程序侧入口 | `Engine/Memory/Memory.hpp`（C++ 封装）+ `Engine/Memory/MemoryOperator.cpp`（`new/delete` 接管） |
| 设计文档 | `Code/Native/Memory/DESIGN.md`、`README.md` |

**R6 · 包含路径**
**包含根是 `Code/Native/`**，写 `"Engine/Xxx/Yyy.h"`。
同模块内就近用相对路径（如 `"CoreApplication.h"`、`"../Core/Environment.h"`），避免 `../../..` 超过两级。

> 📌 实测现状：`Engine/` 内部**几乎全用相对路径**，`"Engine/..."` 前缀写法目前只出现在 `Main.cpp`。
> 新模块跨层引用时按此倾向处理。

**R7 · 空目录**
`Code/Native/Editor/`、`Code/Native/Engine/Asset/`、`ApplicationData/` 是**已建好的规划占位**。
需要新模块时**新建自己的目录**，不要占用这三处。

### 5.3 规划中但尚未创建的目录

| 目录 | 用途 |
| --- | --- |
| `Code/DotNet/` | C# 侧源码与 csproj |
| `Engine/Render/`（或 RHI 目录，名字未定） | 渲染硬件接口 |
| `Engine/Scene/` | 场景树 / Entity |
| `Engine/Math/` | 数学库 |

→ 开始这些模块前，先读 §8 的调研索引，再向作者确认目录名与首批范围（§9 Q2）。
**注意**：R1–R7 只覆盖 `Code/Native/`，对 `Code/DotNet/` 尚**无归属规则**。

---

## 6. 编码规范

### 6.1 文件头

每个新增源文件（`.h` / `.hpp` / `.cpp`）**第 1 行起**必须有这 4 行：

```cpp
// File /Native/Engine/Xxx/Yyy.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat
```

| 项 | 约定 |
| --- | --- |
| 第 1 行 | `// File /Native/` + **相对 `Code/Native/` 的路径**（不写 `Code/`） |
| 第 2 行 | 空的 `//`（只有两个斜杠，**无尾随空格**） |
| 第 3 行 | `// SPDX-License-Identifier: MIT` |
| 第 4 行 | 版权行，原文照抄 |
| 之后 | 空一行，再 `#pragma once` |

⚠️ **现状中第 1 行存在多种偏差，必须知道它们不构成规范**（实测类型）：

| 偏差类型 | 实例 |
| --- | --- |
| 多一段目录名 | `Engine/OS/Windows/*` 写作 `.../Engine/OSPlatform/Windows/...` |
| 少 `Engine/` 段 | `Code/Native/Memory/**` 写作 `// File /Native/Memory/...`（该模块自身一致，**不要为了"统一"去改**） |
| **省略扩展名** | `IO/IInputBuffer.h` → `.../IO/IInputBuffer`；`IOutputBuffer.h` 同 |
| **省略子目录** | `IO/Filesystem/{FileDevice.hpp, Win32FileDevice.h/.cpp}` → `.../IO/...` |
| **文件名与实况不符** | `Core/Window/WindowInitContext.hpp` → `.../WindowContext`；`OS/Windows/ComInitalizer.hpp` → `.../ComInitializer.hpp` |

→ **规则**：新文件**按实况写**（真实相对路径，含扩展名、含子目录）。
**不要照邻居文件抄**，也不要去"顺手修正"既有文件的路径行——除非任务就是统一它们。

### 6.2 缩进与排版

| 项 | 约定 |
| --- | --- |
| 缩进 | **Tab** |
| 大括号 | **Allman**（左括号独占一行）。例外：单行函数 `{ return ...; }`，以及构造函数初始化列表可同行 `{` |
| 文件编码 | **UTF-8**（构建固定 `/utf-8`） |

> ⚠️ **现状例外**：`Engine/Event/` 模块**整体使用 4 空格缩进**（该模块 13 个文件几乎全是空格）。
> `Utils/Flag.hpp`、`Memory/tests/MiniTest.h` 为混合缩进。
> **在 `Event/` 下新增文件时跟随该模块用空格**；其它模块用 Tab。拿不准就看同目录邻居。

### 6.3 命名

| 类别 | 约定 | 示例 |
| --- | --- | --- |
| 命名空间 | `PenEngine` | `namespace PenEngine` |
| 库内私有命名空间 | `PenEngine::Internal` | `IO/Internal/*` |
| 分配器命名空间 | `PenMemory` | `Code/Native/Memory/*` |
| 类型 / 类 / 结构 / 枚举 | **PascalCase** | `CoreApplication`、`SignalObject` |
| 成员函数（库自有 API） | **PascalCase** | `Size()`、`Emit()`、`Connect()` |
| 成员函数（STL 兼容重载） | **snake_case**，与 STL 同名并存 | `Size()`/`size()`、`Begin()`/`begin()` |
| 成员变量 | **`m_` + camelCase**（`m_` 后**小写**字母开头） | `m_size`、`m_window`、`m_liveTaskCount` |
| 局部变量 | PascalCase | `auto token = ...` |
| 模板参数 | PascalCase | `template <typename CharType>` |
| 概念 / requires | 标准风格小写 | `requires std::invocable<F&, Args...>` |

> ⚠️ 成员变量这条是**实测**结论：`m_` 后接小写字母是绝对主流；
> 仅 `MouseEvent` 的 `m_X`/`m_Y` 因构造函数形参同名而用大写，属局部动机，**不是约定**。
> **不要写 `m_Size` 这种形式。**
>
> 📌 「STL 兼容重载」是**有意为之**：自定义容器同时提供 PascalCase 原生 API 与 snake_case STL 风格 API，
> 以便与标准算法/范围互操作。新增容器类时**两套都要给**。

### 6.4 宏

| 前缀 | 适用域 |
| --- | --- |
| `PENFRAMEWORK_*` | **引擎层**（`Engine/`），平台/编译器/构建判定与属性包装 |
| `PEN_MEMORY_*` | **分配器模块**（`Code/Native/Memory/`） |

**新增宏一律带对应前缀。** 仓库中存在少量既有无前缀宏（如 `BIT_ENUM`、`DECL_P_OBJECT`、
`DEBUG_VERIFY_REPORT*`、`WindowsMinDef.h` 的 Win32 裁剪宏）——**不要模仿它们新增，也不要改名**。

**平台/构建判定统一走 `Core/Environment.h`**，不要直接写 `#ifdef _WIN32`。该头提供：

| 类别 | 宏 |
| --- | --- |
| OS | `PENFRAMEWORK_OS_WIN32` / `OS_WIN64` / `OS_LINUX` / `OS_MACOS` / `OS_IOS` / `OS_ANDROID` |
| 编译器 | `PENFRAMEWORK_COMPILER_MSVC` / `COMPILER_GCC` / `COMPILER_CLANG` |
| 架构 | `PENFRAMEWORK_ARCH_X86` / `ARCH_X64` / `ARCH_ARM` / `ARCH_ARM64` / `ARCH_RISCV` + `ARCH_32BIT` / `ARCH_64BIT` |
| 指令集 | `PENFRAMEWORK_INTRINSIC_SSE` / `INTRINSIC_NEON` |
| 构建类型 | `PENFRAMEWORK_BUILD_DEBUG` / `BUILD_RELEASE` |
| 属性包装 | `PENFRAMEWORK_NO_UNIQUE_ADDRESS` / `PENFRAMEWORK_NO_VTABLE` / `PENFRAMEWORK_FORCE_INLINE` |
| 导出 | `PENFRAMEWORK_EXPORT_API` / `PENFRAMEWORK_IMPORT_API` |
| 语言特性开关 | 见下方 ⚠️ |

> ⚠️ **语言特性开关的实况**（用前自查，不要假设可用）：
> - `PENFRAMEWORK_SUPPORT_PINVOKE` —— 已**无条件定义**（不是可切换的开关）
> - `PENFRAMEWORK_SUPPORT_CONSTEXPR_EXCEPTION` —— **只有 `#ifdef` 探测，从未被定义**，
>   因此 `PENFRAMEWORK_CONSTEXPR_EXCEPTION_FUNCTION` **恒为空**
> - `PENFRAMEWORK_CONSTEXPR_EXCEPTION_FUNCTION`、`PENFRAMEWORK_NO_VTABLE` —— 已定义

> ⚠️ **例外**：`Code/Native/Memory/` 是独立 C ABI 模块，**不依赖 `Environment.h`**，
> 其中直接使用 `_WIN32` 是既有正确做法。
> **规则简记**：`Engine/` 内禁止裸 `_WIN32`；`Memory/` 内允许。

### 6.5 基础类型别名

`Core/Environment.h` 在 `namespace PenEngine` 下定义：
`U8` / `U16` / `U32` / `U64`、`I8` / `I16` / `I32` / `I64`、`Usize`、`PtrDiff`、`HashID`、`B8`。

**引擎代码用这些别名**（`Usize` 而非 `size_t`，`U32` 而非 `unsigned`），不要写原生类型。

### 6.6 注释

- 用 **Doxygen 三斜线**：`/// @brief` / `@param` / `@return` / `@note` / `@tparam`
- **中文**描述，标签保留英文
- 新增**非平凡**的类型与函数（含不变量、锁序、生命周期约束的）**必须**写注释；一行 getter 可省
- **不变量/纪律用编号写**（范例：`CoroutineScheduler::Update()` 上方的 `I1`~`I5`）。
  修改这类代码前先读那段注释，改完同步更新

### 6.7 异常

统一入口 `Engine/Exception/Exception.hpp`：

```cpp
class Exception : public std::exception   // 携带 ExceptionType / Detail / Stacktrace / SourceLocation
template <typename E> [[noreturn]] void ThrowException(E e, ...);  // 自动采集调用点
```

- 抛异常一律走 `ThrowException(...)`，**不要裸 `throw`**（否则丢失调用点采集）
- 标准异常（如 `std::bad_alloc`）同样走它，会被包装成 `PenEngine::Exception`
- 异常类型需 `std::derived_from<E, PenEngine::Exception>`

### 6.8 调试校验宏

定义处：`Engine/DebugTools/DebugVerify.hpp`。**Debug 与 Release 语义不同，用前必读该文件**。要点：

- `DEBUG_VERIFY_REPORT*` / `DEBUG_REPORT_HANDLE*`：Debug 下报错并中断，**Release 下展开为空**
- `DEBUG_ALWAYS_REPORT` / `_L`：Debug 下无条件中断，**Release 下展开为 `std::unreachable()`**（执行到即 UB）
- `*_WITH_REL_*` 系列（`DEBUG_VERIFY_REPORT_WITH_REL_OPERATION` 等 4 个）定义在 Debug 开关**之外**，
  **两种配置下都存在**：Debug 下有报错提示，Release 下**静默执行后续动作 / 静默抛异常**

该文件头注明 *"internal framework file... should not be used in user's code externally"*——
**用户层代码不得使用这些宏**。

### 6.9 内存分配

**优先使用标准 `new` / `delete`**：主程序的全局 `operator new/delete` 已被
`Engine/Memory/MemoryOperator.cpp` 全套接管（含 sized / aligned / nothrow 变体），标准写法即走 PenMemory。

需要显式控制时用 `Engine/Memory/Memory.hpp` 的 `PenEngine::Memory`：`Allocate` / `AlignedAllocate` /
`Deallocate` / `AlignedDeallocate` / `GetAllocatedSize` / `GetStats` / `VerifyHeap` / `ReleaseFreeMemory`
/ `GetPageSize` / `GetAlignment`。

> ⚠️ `Memory::Allocate` 返回**未构造内存**——本层不做 placement new、不调用析构。
> 分配器的设计与验收标准见 `Code/Native/Memory/DESIGN.md`。

### 6.10 第三方依赖

- 依赖必须经 **vcpkg manifest（`vcpkg.json`）** 添加。**不得**往仓库塞源码，也不得绕开走系统 `find_package`
- 当前依赖：`boost-unordered`（flat_map）、`boost-locale`（编码转换）、`plf-hive`（稳定地址容器）、
  `wintoast`（仅 Windows）
- **全局 `new`/`delete` 已被接管**：不要引入自带全局分配器的第三方库
- Windows API 经 `Engine/OS/Windows/Windows.h` / `WindowsMinDef.h` 收敛，不要到处直接 `#include <windows.h>`

**平台 SDK（如 DirectX 系列）走系统 SDK**——即由 Visual Studio / Windows SDK 提供头与库，
**不加入 `vcpkg.json`**。这与"依赖经 vcpkg 管理"并不矛盾：vcpkg 管的是**可移植的第三方库**，
平台 SDK 由工具链提供。

> 📌 因此 `vcpkg.json` 中不含任何 DirectX 条目是**预期状态**，不是遗漏。
> 需要某个平台 SDK 组件时，在 CMake 里显式链接对应库（如 `d3d12`、`dxgi`、`dxguid`），
> 并注意**目标 Windows SDK 版本**由工具链决定——若要用较新的 Agility SDK，需另行与作者确认。

### 6.11 语言与编译器基线

**C++23**，MSVC 为主，编译固定加 `/utf-8`、`/Zc:preprocessor`、`/diagnostics:caret`、`/FC`。
写预处理器代码时必须符合 `/Zc:preprocessor` 的合规形式。

---

## 7. 构建与验证

### 7.1 前置条件

| 项 | 要求 |
| --- | --- |
| 操作系统 | **Windows** |
| Visual Studio | 含 C++ 工作负载（提供 `cl.exe`） |
| CMake | **≥ 4.0**，且**在 PATH 中** |
| Ninja | 已安装且在 PATH |
| vcpkg | 已安装且设置 **`VCPKG_ROOT`** |
| `VSINSTALLDIR` | **已设置**（预设引用了它，为空则 configure 失败） |
| 网络 | 首次 configure 需下载依赖 |

> ⚠️ **先确认工具链真的可用**，否则构建命令连启动都做不到：
> ```powershell
> (Get-Command cmake).Source; (Get-Command ninja).Source
> $env:VCPKG_ROOT; $env:VSINSTALLDIR
> ```
> `cmake` / `ninja` / `cl.exe` 通常只在 **「Developer PowerShell for VS」** 里可用。
> 若在普通终端里它们都是 `NOT FOUND`，**不要试图自己安装或改用别的生成器**——
> 转到开发者终端，或向作者说明无法构建。

### 7.2 构建命令

```powershell
cmake --preset x64-debug                  # 配置
cmake --build out/build/x64-debug         # 构建（注意：直接给构建目录）
```

⚠️ **`CMakePresets.json` 只定义了 `configurePresets`，没有 `buildPresets`**。
因此下面这条命令**会失败**并报 `No such build preset`：

```powershell
cmake --build --preset x64-debug   # ❌ 不可用：本项目没有 build preset
```

**构建时一律直接指定构建目录** `out/build/<preset>/`。

预设：`x64-debug`、`x64-release`、`x86-debug`、`x86-release`（`windows-base` 为 hidden 基类）。
`--preset` 只在**配置**阶段有效。

> ⚠️ **只有 `x64-debug` 是实际验证过的路径**。`x86-*` 继承自 `windows-base` 而**没有
> `CMAKE_TOOLCHAIN_FILE`**，拿不到 vcpkg 工具链，**不要假设其可用**。

**产物**：`out/build/<preset>/` 下的 `PenFramework.exe` 与 `PenMemory.dll`。
后者的 `RUNTIME_OUTPUT_DIRECTORY` 被显式设为构建根目录，**必须与 exe 同目录才能加载**。

> 💡 若 `out/build/x64-debug/` 已有配置缓存，**可直接跳过配置、只跑构建**——
> 缓存里的工具链路径未必等于当前 `$env{VCPKG_ROOT}`，重新 configure 反而可能失败。
> 只有新增/删除 `Engine/` 下的源文件时才必须重新 configure（见 §7.5）。

> 📌 本机有一个构建辅助脚本（在临时工作区里，非仓库组成部分）：
> 它负责调用 `vcvars64.bat` 设置环境，然后执行上面两条命令。**脚本内的构建步骤同样是
> `cmake --build <构建目录>`**，可作为写法参照。用前请确认该路径仍存在。

### 7.3 运行时的预期现象

当前 `Exec()` 只做「派发窗口消息」，其余阶段以 `// todo` 留位。运行 `PenFramework.exe` 会看到：
弹出一个窗口、**不渲染、不响应输入**、点关闭即正常退出。

**这是当前实现状态，不是环境故障**——不要因此去改构建配置。

### 7.4 测试现状

| 目标 | 情况 |
| --- | --- |
| `Code/Native/Memory/`（PenMemory） | ✅ 有测试与基准。两条路径：① CMake 选项 `PEN_MEMORY_BUILD_TESTS`（默认 OFF）打开后用 `ctest`；② `Code/Native/Memory/build.ps1` 独立编译运行 |
| `Engine/**` | ❌ **没有任何测试框架或测试代码** |

→ 改动 Engine 层时，验证手段是**编译通过 + 运行验证 + 代码级不变量自检**。
**不要假设存在"跑一下单元测试"这条路径。** 要引入测试框架，先问作者（§9 Q3）。

**`build.ps1` 的参数**（不读脚本不会知道，故列在此）：

| 参数 | 作用 |
| --- | --- |
| （无） | Release：`/O2 /MD /DNDEBUG`，产物在 `build/` |
| `-Debug` | `/Od /Zi /MDd /D_DEBUG`，**断言开启**，产物在 `build-debug/` |
| `-NoRun` | 只编译，不运行测试 |
| `-Bench` | 测试通过后额外运行基准 |

该脚本用 `/W4 /WX`（警告视为错误），并自行定位 `vcvars64.bat`；**非 Windows 平台不可用**。

> 📌 PenMemory 的验收范式值得沿用到新模块：**每个用例结束做一次全堆不变量校验**
> （`PenEngine::Memory::VerifyHeap()`）、**多种编译配置各跑一遍**、**重复多轮**、并记录**量化数据**。

### 7.5 构建系统改动规则

| 位置 | 新文件是否自动编译 |
| --- | --- |
| `Code/Native/Engine/**` | ✅ 自动（根 CMake 用 `file(GLOB_RECURSE ...)`，**无 `CONFIGURE_DEPENDS`**） |
| `Code/Native/Memory/*.cpp` | ❌ **两处清单都要同步**：`Code/Native/Memory/CMakeLists.txt` 的 `PEN_MEMORY_SOURCES`，以及 `build.ps1` 的 `$libSources` |
| `Code/Native/Editor/**` 及其它新顶层目录 | ❌ **必须改根 `CMakeLists.txt`** |

> ⚠️ **GLOB 的坑**：新增/删除 `Engine/` 下的文件后**必须重新 configure**，只 `--build` 发现不了。
>
> ⚠️ 两条测试路径的覆盖**不完全一致**（`build.ps1` 的清单与 CMake 注册的测试目标不同，
> 例如它不编译 `test_dll`）。需要全量验证时用 CMake/ctest 路径。

### 7.6 警告等级（两个目标不同）

| 目标 | 策略 |
| --- | --- |
| `PenMemory`（经 `build.ps1`） | `/W4 /WX` —— 最高等级 + **警告视为错误** |
| `PenFramework.exe`（经根 CMake） | **未设 `/W4` / `/WX`** —— 警告**不阻断构建** |

→ 主程序"构建成功"**不代表无警告**。要么自己在输出里确认，要么引入 `/WX`（需先问作者）。

### 7.7 调试支持

- **Natvis**：`Visualizers/PenFramework.natvis` 仅在 **MSVC + Debug** 下加入构建。
  新增/重命名会在调试器里查看的核心类型时**需同步更新**，否则 VS 中显示为裸结构体。
- **即时中断**：Debug 下 `DEBUG_*` 宏会调用 `_CrtDbgReportW` + `_CrtDbgBreak` + `__fastfail(5)`。
  调试器直接崩到 `__fastfail` 时，**先怀疑这些宏**，而不是代码崩溃。
- **Edit and Continue**：Debug / RelWithDebInfo 走 `/ZI`（受 CMP0141 控制）。
  注意 **ASan 与 EditAndContinue 互斥**，临时开 ASan 需同时改 `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT`。

### 7.8 当前没有的东西

无 CI / 无 clang-format / 无 clang-tidy / 除 PenMemory 外无测试框架 / 无静态分析与格式检查脚本 /
根 `README.md` 之外无开发者入口文档。

→ 要引入上述任一，**先问作者**。

---

## 8. 调研资料索引

⚠️ 下列文档**全部位于只读参考区**，是**旧框架**的产出。
用法：**读思路、抄结论、不要抄代码、不要抄约定**。（`PersonalWorkaround/README.md` 是例外，见 §2.2。）

| 你要做的事 | 先读 |
| --- | --- |
| 项目整体设计与已确认决策 | `PenFramework-old/Docs/Architecture.md`（§1–§3 有效；§5–§8、§10 为旧框架历史；§9 以本仓库实测为准） |
| 判断旧组件能否复用 | `PenFramework-old/Docs/OldFramework-Reusability-Survey.md` |
| 按件移植旧代码 | `PenFramework-old/Docs/Port-Reconciliation.md`（⚠️ 其"MPL-2.0 头""包含路径"两项要求已作废） |
| 接入 .NET / C# 互操作 | `PenFramework-old/Docs/Research-DotNet-Embedding-2026.md`（§7 含一份 `DotNetHost/CMakeLists.txt` 草案） |
| 设计 RHI | `PenFramework-old/Docs/RHI-Architecture-Research-2026.md` + 同目录 `research/rhi-interface-validation-testing.md`、`research/RHI-ResourceBinding-Swapchain.md` |
| 设计资源系统与打包 | `PersonalWorkaround/Asset/AssetPipeline_Survey.md`（§8 是给本项目的落地建议） |
| 看 RHI 的真实接口形态 | `PenFramework-old/.research/` 下的第三方接口摘录 |

> 📌 这些文档的**章节号、行数、体量会随更新而变化**，引用时以文件内实际标题为准。
> **引用前先确认文件存在**——`PersonalWorkaround/` 下的参考资料可能被整理或移动。

**仅移植任务需要**：旧框架是半成品单二进制引擎（MSVC-only、Windows-only、D3D11-only）。
其数学库有真 bug（**不要参考，必须重写**）、对象/事件模型是桩、资源管线与延迟渲染命令缓冲未实现、
无任何 .NET 互操作能力；其协程调度器有 use-after-free 与 header 全局变量的 ODR 问题
（新仓库已重写，但改 `CoroutineScheduler` 前先读其 `I1`–`I5` 不变量注释）。
逐条缺陷清单见 Reusability Survey。

---

## 9. 待作者决定的开放问题

遇到以下任一事项时**停下来问作者，不要自行拍板**，也不要把自己的选择写进文档当作既定事实。

| 编号 | 事项 | 前置 |
| --- | --- | --- |
| **Q1** | **下一个里程碑是什么？**（RHI / 资源 / .NET 互操作 / 编辑器，四条路的前置工作完全不同） | — |
| **Q2** | **新模块的目录名与划分**（如 RHI 放 `Engine/Render/` 还是 `Engine/RHI/`？后端实现放哪？） | Q1 |
| **Q3** | **是否引入测试框架**（Catch2 / GoogleTest，还是沿用自研 `MiniTest`）；是否给主程序加 `/WX` | Q1 |
| **Q4** | **数学库自研还是引第三方**（如 glm） | Q1 |
| **Q5** | **`Engine/` 跨层 include 用相对路径还是 `Engine/...` 前缀** | Q2 |
| **Q6** | **是否允许修改当前在途未提交的文件**（本仓库常有并行改动，任何改代码的任务都会撞上） | — |
| **Q7** | **后续依赖能否自带上游全局分配器**（`new`/`delete` 已被接管，会冲突） | — |

**推荐提问顺序**：先问 **Q1** 与 **Q6**（无前置、且阻塞其余）。

**提问格式**

1. 说明**你要做的事**与**卡在哪一条编号**；
2. 给出**你倾向的选项与理由**（不要只抛问题）；
3. 若涉及在途未提交文件，**同时列出你要改的文件**。
