# PenFramework · Agent 工作须知

本文是接入本仓库的 agent 的工作须知。内容限于**稳定约束**：结构性事实、目录归属规则、编码约定、
构建与验证方式、禁区。**不包含会随代码漂移的统计数字**——需要这类信息时自行用命令查。

---

## 0. 冲突裁决与工作原则

### 0.1 裁决顺序

遇到规则冲突时：

1. **作者的口头/书面裁定**（§1 记录的即是）
2. **仓库当前代码** —— 现状与本文不符时，除 §1 外**以代码为准**
3. `Docs/` 与 `PersonalWorkaround/README.md`
4. `PersonalWorkaround/PenFramework-old-research/` 下的历史文档（参考层，部分已明确作废）

### 0.2 开工前置确认（默认生效）

> 📌 作者过去每次都要手动打一句「先给出示例及需要我同意的要点，我回答并简单审查后开始」。
> **这句话现在是默认流程**——除非属于下方的免提情形，**接到任务先做一轮前置确认，再动手**。

**判据不是"任务大不大"，而是"做错了要返工多少"。返工代价高 → 必须先确认。**

一轮前置确认 = **一次给出下面两样东西，然后停下等作者回答**。
⚠️ **不要交一份"计划书"**：作者要审的是**具体样例**与**少数几个分叉点**，一屏为限。

#### ① 示例（先给）——产出物的**真实样例**，不是意图描述

| 任务类型 | 示例应当是什么 |
| --- | --- |
| 写文档 / 文档改版 | **真实的正文片段**（含标题层级与表格样式），或改版后的完整目录 |
| 写代码 / 改代码 | 真实的**函数签名 + 关键实现片段**，或一段 before / after 对照 |
| 新建目录结构 | 真实的**目录树**（带文件名），不是"大概分成三类" |
| 命名 / 格式约定 | 至少 3 条**按新约定写出的真实例子** + 1 个反例 |
| 调研 / 分析 | **一条完整的真实结论**（含证据与出处），或最终提纲 |

- ⚠️ **必须是真的**——要写**将要落盘的那几行**，不要写"我将会…""计划采用…"。
- **一屏能审完**，但要**完整到能看出风格与取舍**；同类产物有多份时，只给**差异最大的两个**。
- 体量大的产出（如整份文档重写），示例**可以是临时工作区里的一份草稿**
  （这正属于 §2.1 允许的"多轮产出"）：给出**草稿路径 + 关键片段**即可，不必把全文贴进对话。

#### ② 需同意的要点（后给）——编号列表，每条都是**真分叉**

```
1. 目录名用 Engine/Render/（我倾向）而不是 Engine/RHI/ —— 理由：……
2. 是否允许改动 Xxx.cpp（当前在途未提交）
3. ……
```

- 只列**作者才有权决定**的事：§9 的议题、命名、范围、破坏性改动、越出自己任务脉络的整理。
  **不要把"我打算做什么"包装成问题**（你打算做的事，用①的示例来说明）。
- **有倾向就给出倾向与理由**，不要只抛问题。
- 每条要能被**一句话回答**（"1 同意，2 改成 B"）。
- **控制在 3–6 条**；超过说明你还没想清楚，先自己想。

#### ③ 给出之后

- **停下，等作者回答**——不要在给出示例的同一轮里开始落盘。
- 作者答完**直接开做，不要重复确认**，也不要换个说法再问一遍。
- 若答复**改变了示例的形态**，**先改示例再动手**（只就变化点再确认一次）。

#### ④ 提问方式——**列出问题、交出输入框**，不要用提问工具

**凡是需要作者回答的内容**（①的要点、§9 的议题、中途的澄清、任何二选一），一律：

1. **写进正文末尾**（编号列表，写法同②）；
2. **然后结束本轮对话**——把**完整的、空白的输入框**留给作者。

- ❌ **不要用「提问工具」**（会弹出选项按钮、要求作者从预设项里挑的那种，如 `ask_user_question`）：
  它把回答**压成选项**，作者既不能自由展开，也无法一次答完多条。
- ❌ 不要在同一轮里"边问边继续做"——作者还没答，后续动作就没有依据。
- ✅ **一次列全所有待答项**，不要一条一条挤牙膏；每条都要能被"1 同意，2 改成 B"答完（见②）。
- ✅ 若某条**不适合一句话回答**（需要作者给一段方向），明说"这条请你给个方向"，**不要预设选项**。

#### ⑤ 免提情形（可直接做，但用一行说明依据）

- 只读、不写盘的问答（"X 在哪""这段代码做什么"）；
- 改动**极小且无歧义**（错别字、与本文档不符的笔误）；
- 作者**已就同一件事在本轮会话里确认过**；
- 作者明确说"直接做 / 不用问"。

### 0.3 工作原则（按重要性）

1. **临时文件只放 `PersonalWorkaround/`**：
   - 任务**需要写临时文件**（研究 / 调查 / 分析的中间结果、编译脚本、一次性验证程序、草稿）
     → 在 `PersonalWorkaround/` 下**新建子文件夹**存放；
   - **不需要写临时文件** → **直接把结果输出给作者**，不要凭空造目录；
   - **正式产物**（源码、正式测试、文档）→ **直接写进正式位置**，不要塞进工作区；
   - 🚫 **只准落在 `PersonalWorkaround/` 里**——系统临时目录、仓库根、其它盘符都不行（见 §2.1）。
     **拿不准就算"需要"**，建在工作区。
   **开工前的前置确认见 §0.2**。
2. **报告"某处不存在 X"之前，先证明你的搜索能命中一个已知存在的 X。**
   搜索失败时**先怀疑查询本身**，不要归因于"文件在变"或"工具异常"。
3. **同一结论换一条无关的实现路径复核**（如字符串匹配 vs 逐字节/码点比较）。
4. **不要修改在途未提交改动涉及的文件**，除非作者明确授权（见 §9 Q5）。
5. **不要把统计值写进文档**——文件数、测试数、逐文件清单会立刻过期。需要时用命令查。
6. **改代码前先读相邻代码的注释**，尤其带编号不变量（如 `I1`–`I5`）的段落；改完同步更新注释。
7. **不要顺手做无关的格式化或重命名**——会把功能改动淹没在噪音里，难以 review。
8. ⚠️ 本文是"结论"，不是一手事实。据此**断言某处代码的行为**前，**自己打开那个文件确认**。

---

## 1. 作者已确认的裁定（优先级最高）

| 编号 | 议题 | 裁定 |
| --- | --- | --- |
| D1 | `PersonalWorkaround/` 定位 | **临时工作区**，**全体 agent 共用**。其 `README.md` 是**有效 README**，优先阅读。**判据只有一条：任务会不会产生临时文件**——会（研究 / 调查 / 分析结果、编译脚本、一次性验证程序、草稿）就在其中**新建子文件夹**存放；不会就**直接输出结果**。🚫 **临时文件只准落在 `PersonalWorkaround/`，别处一律不行**（见 §2.1） |
| D2 | 许可证与文件头 | **MIT，4 行头**。旧文档要求的「9 行 MPL-2.0 头」**已作废** |
| D3 | agent 的任务域 | **不固定**，按当次任务决定 |
| D4 | 文档语言 | **中文** |
| D5 | 开工协议 | **默认先给「示例 + 需同意的要点」，作者回答后再动手**——作者**不需要每次重申**这句话。细则与免提情形见 §0.2 |
| D6 | 提问方式 | **需要作者回答时，把问题列在正文末尾、然后结束本轮**，把**完整的输入框**交给作者；**不要用「提问工具」**（弹选项按钮的，如 `ask_user_question`）。见 §0.2 ④ |

---

## 2. 工作区治理与禁区

### 2.1 临时工作区

> ⚠️ **先看这条：要不要用临时工作区？** 判据只有一条：**这次任务会不会产生临时文件**。
>
> | 这次任务会产生的…… | 放哪 |
> | --- | --- |
> | **临时文件**：研究 / 调查 / 分析的中间结果、编译脚本、一次性验证程序、方案草稿 | 👉 `PersonalWorkaround/` 下**新建子文件夹** |
> | **正式仓库产物**：源码、正式测试（`<层>/Test/UnitTest/`）、文档、CMake 配置 | 👉 **直接写进正式位置**，**不要**放工作区 |
> | **什么都不写**（只回答问题、只给结论） | 👉 **直接输出**，不要凭空造目录 |
>
> - **判据**：作者要的是结论，不是文件树；但**真需要落盘的过程材料，必须落在 `PersonalWorkaround/`**。
> - **拿不准就算"需要"**——建在工作区。多一个目录的代价，远小于把材料写到别处。
> - 作者明确要求"先在临时工作区工作"时**照办**，不必再判断。
>
> 🚫 **临时文件不得落在 `PersonalWorkaround/` 之外的任何位置**：
> 系统临时目录（`%TEMP%` / `%LOCALAPPDATA%\Temp` / `/tmp`）、仓库根、`out/` 里手写的脚本、
> 桌面、其它盘符…… 都**不行**。
> 这**不是"遵守了规则"**，而是绕过本节——写到那里作者看不到、复核不了、也清理不掉。
> 临时文件在本仓库**只有一个约定位置**：`PersonalWorkaround/` 下你自己新建的子文件夹。

**归属与整理**（工作区内既有内容的处置）：

`PersonalWorkaround/` 是全体 agent 共用区，内容会不断新增。
**不要试图维护"哪些是我做的"这类白名单**——它必然过期。按以下顺序判断：

**① 归属判断——是否属于自己这次任务的脉络？**

满足以下任一条，即为**自己的工作产出**，可**直接**修改 / 移动 / 重命名 / 删除，**无需请示作者**：
- 由**本次任务中自己直接创建**的目录或文件；
- 由**本次任务中自己派发的子 agent 创建**的目录或文件（含它们各自的中间产物）；
- 汇总场景：主 agent 为本次任务建立了 `SomeThing/`，子 agent 分别建立了
  `Windows-SomeThing/`、`Linux-SomeThing/`、`macOS-SomeThing/`——
  主 agent **直接**把它们移动 / 合并 / 重命名进统一结构，**这是正常操作，不是越权**。

**② 必要性判断——改动是否真的必要？**

即使归属满足①，也要问：
- 合并/移动是否**减少重复、消除分叉**，还是只是换个位置？
- 是否**有下游引用**会因此失效（其他文档、索引、脚本里的路径）？
- 删除这个中间产物，是否会让**作者无法复核结论**？

三者都无正面理由时，**保留原状**。

**③ 归属不可判定 → 回报作者，不要猜**

内容归谁所有，**在磁盘上没有记录**（无归属标记、目录名不自述、时间戳不足以区分，
且跨会话后"谁建的"这一信息已不存在）。因此：
- 属于自己任务脉络的（①）→ 直接整理；
- **超出自己任务脉络的** → **回报作者并说明意图，不要自行移动或删除**；
- 不要用"看起来像是 agent 建的"作为判断依据——**这不是证据**。

**④ 永久不可动项**（与归属无关，任何来源都不得修改 / 移动 / 删除）：

- `PersonalWorkaround/PenFramework-old/`（**旧工作区**，独立 git 仓库）
- `PersonalWorkaround/PenFramework-old-research/`（旧工作区的**部分总结**文档）
- `PersonalWorkaround/tools/`（作者个人工具，含 `.token`）
- `PersonalWorkaround/README.md`（有效 README 草稿）

以上各项**固定不变**，不会随工作开展而增加——因此**不需要任何人维护名单**。
第三方库克隆（如 `PersonalWorkaround/msdfgen/`）**不属于**参考资产，可按 §2.1 ①②③ 正常处置。

> 💡 **建议：给自己的产出留归属标记**
>
> 新建工作目录时放一个 `OWNER.md`，写明是谁、什么时候、为哪个任务创建的：
>
> ```
> task: NotificationBox 跨平台调研 / 主 agent
> created: 2026-09-22
> role: 汇总目录（合并各平台子 agent 产出）
> ```
>
> 这让**后续会话的 agent 也能判断归属**，把"不可判定"变成"可查"。
> 这是建议而非强制，但能显著减少来回请示。

### 2.2 正式区与参考区

| 路径 | 性质 | 可否修改 |
| --- | --- | --- |
| `Code/`、`CMakeLists.txt`、`CMakePresets.json`、`CMake/`、`vcpkg.json`、`Visualizers/`、`Docs/` | 正式代码与工程配置 | ✅ 可改（遵守 §6、§7） |
| `PersonalWorkaround/PenFramework-old/` | **旧工作区**——旧框架的完整源码树（**自带独立 `.git`**） | ❌ **只读** |
| `PersonalWorkaround/PenFramework-old-research/` | **对旧工作区的部分总结**（调研文档 + 第三方接口摘录，**无 `.git`**） | ❌ **只读** |

**判据**：路径里出现 `PersonalWorkaround/` 即为**参考区**，默认只读。
需要参考其中某段代码时，**拷贝到自己新建的子文件夹**，不要就地改。

> ⚠️ **旧工作区只有一层，不要去找"旧工作区的旧工作区"。**
> `PersonalWorkaround/PenFramework-old/` 本身就是旧工作区，它下面**没有**再嵌套一个
> `PenFramework-old/`。与之**并列**的 `PenFramework-old-research/` 是**另一个独立目录**、
> 不是它的子目录，且只是对旧工作区的**部分**总结——旧工作区的全貌看 `PenFramework-old/` 本体，
> **不要拿这份总结当旧工作区的全部**。

> ⚠️ `PenFramework-old/` 有**独立的 git 仓库**（`PersonalWorkaround/` 下另有若干第三方克隆也带 `.git`）。
> 在其中执行 git 命令会作用于**另一个仓库**。开工前先确认位置：
> ```powershell
> git rev-parse --show-toplevel   # 应为仓库根
> ```

#### 2.2.1 `PersonalWorkaround/README.md` 的正确读法

它是**仓库根 README 的重写草稿**。其中大量 `🚧**该条目不完整**` 是**待补齐标记，不是事实陈述**。

- 它给出的项目定位、依赖管理方式、环境要求、构建命令**可信**（根 `README.md` 目前只有标题）；
- 标 `🚧` 的段落当前**没有内容**，不要当成"作者已写好但没贴出来"，也不要自行填充后当作既定事实。

> 📌 "参考区只读"是针对**修改**的约束，不禁止**阅读**。这份草稿要读
> （它本身在「永久不可动项」里，**只能读，不要改**）。

### 2.3 其它禁改 / 慎改区域

下表是**固定的**项目级禁区。`PersonalWorkaround/` 内各目录的处置**不以此表为准**，
见 §2.1 的归属判据。

| 路径 | 说明 |
| --- | --- |
| `out/` | 构建产物，含 `vcpkg_installed/`（完整第三方源码）。**不要编辑**，也不要把它当本项目代码。用 glob/grep 搜代码时**必须排除 `out/`** |
| `.vs/` | Visual Studio 本地状态。不要手工编辑 |
| `PersonalWorkaround/tools/GitHubMCP/` | 作者个人工具，含 `.token`。**不要读取、不要输出 `.token` 内容** |
| `PersonalWorkaround/Asset/`、`PersonalWorkaround/PenFramework-old-research/`（含其 `research/`、`.research/`） | 调研资料与接口摘录，只读参考 |
| `PersonalWorkaround/` 之外的任何位置（系统临时目录 `%TEMP%`、仓库根、其它盘符……） | **禁止作为临时文件落点**——临时文件一律放 `PersonalWorkaround/`（见 §2.1） |

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

1. **按 §2.1 的判据**在 `PersonalWorkaround/` 下**新建子文件夹**：拷贝源码做实验、撰写研究/设计文档、
   写验证脚本、放编译脚本与一次性验证程序。**该建就建**——不要为了"少建目录"而把临时文件写到别处。
2. 阅读仓库内任意文件（`.token` 除外，含 §2.2.1 的草稿 README）。
3. 在作者已下达任务的前提下修改 `Code/` 下的相关代码，并遵守 §6、§7。
4. 运行构建与测试（产物写入 `out/` 属正常）。

**不可以做**

1. 修改 / 移动 / 重命名 / 删除 §2.1 ④ 的 **永久不可动项**。**无例外。**
2. 重命名 / 移动 / 删除**超出自己任务脉络**的工作区内容——应回报作者，说明意图与理由。
3. 丢弃、回滚、提交在途未提交的改动。
4. 在 §9 列出的事项上**自行拍板**——那些是作者未决的决策。
5. **把临时文件写到 `PersonalWorkaround/` 之外的任何位置**（系统临时目录 `%TEMP%`、仓库根、
   其它盘符……），用来规避 §2.1。临时文件只有一个约定位置：`PersonalWorkaround/` 下自己新建的子文件夹。

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
  └─ CoreApplication                       ← CoreLoop，C++ 完全拥有
       ├─ 认领入口签名、解析命令行、装载配置
       ├─ 计算 dt / 固定步长累加
       ├─ 泵窗口消息
       ├─ 更新场景树 → Entity::Update(dt) → [C# 函数指针] 进入托管环境
       ├─ 渲染提交
       └─ Present
```

当前 `CoreApplication` 已实现：入口分派、命令行解析、配置装载、窗口创建与消息泵。
**场景树、渲染提交、Present 尚未实现**（以 `// todo` 留位，见其 `I1`）。

`Main.cpp` 只做三件事：认领入口签名、把 `argc/argv` 交给 `CoreApplication`、返回它的退出码。
**入口形态由 CMake 变量 `OUTPUT_BUILD_TYPE` 决定**（见 §7.2）。

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
├─ CMakeLists.txt              ← 根构建脚本（PenEngine OBJECT 库 + 唯一 exe + add_subdirectory(Memory)）
├─ CMakePresets.json           ← 构建预设（含测试开关，见 §7.5）
├─ CMake/PenTest.cmake         ← 测试装配函数 pen_add_unit_test / pen_add_benchmark（见 §7.5）
├─ vcpkg.json                  ← C++ 依赖清单（manifest 模式）
├─ Visualizers/                ← VS 调试可视化器
├─ Docs/                       ← 文档（测试约定见 Docs/Testing.md）
├─ ApplicationData/            ← 运行时数据（见 §7.3）
├─ Code/
│  └─ Native/                  ← 顶层代码根
│     ├─ Main.cpp              ← 进程入口（入口形态由 OUTPUT_BUILD_TYPE 决定）
│     ├─ Engine/               ← 主程序源码（GLOB_RECURSE 编成 PenEngine OBJECT 库）+ Test/ 测试
│     ├─ Memory/               ← 独立模块 PenMemory.dll（不参与主程序编译）+ Test/ 测试
│     └─ Editor/               ← 空占位（编辑器规划位）+ Test/ 测试骨架
├─ out/                        ← 构建产物（勿编辑）
└─ PersonalWorkaround/         ← 临时工作区（git 忽略）
```

`Engine/` 下的模块与职责：

| 目录 | 职责 |
| --- | --- |
| `Core/`（含 `Window/`） | 应用核心与主循环、入口分派、命令行解析、应用配置装载、平台/编译器判定宏与基础类型别名、延迟销毁队列 |
| `Event/` | 事件类型与输入常量 |
| `String/` | 字符串、字符串视图、SIMD 查找 |
| `Object/`（含 `Internal/`） | 属性对象、对象信号、引用计数 |
| `IO/`（含 `Filesystem/`、`Internal/`） | 路径、URI、缓冲接口、文件设备 |
| `Json/` | JSON 解析与构建 |
| `Math/` | 向量、矩阵、颜色、数学常量与函数 |
| `Thread/`（含 `Internal/`） | 线程与线程类型抽象 |
| `Coroutine/` | 协程任务与调度器 |
| `Utils/`（含 `NotificationBox/`、`Internal/`） | 单例、作用域守卫、时间、位标志、字节数组、概念、命令行解析器、通知框 |
| `Memory/` | `PenMemory.dll` 的 C++ 封装与全局 `new/delete` 接管 |
| `Exception/` | 统一异常基类与抛掷入口 |
| `DebugTools/` | 调试断言/校验宏 |
| `OS/Windows/` | Windows 头收敛、COM 初始化、系统信息 |
| `Asset/`、`Resource/` | **空占位**，尚未实现 |

> 📌 **测试不在模块目录里**：各层测试集中在 `<层>/Test/UnitTest/`（单元测试）与
> `<层>/Test/Benchmark/`（基准），由 CMake + `ctest` 统一跑（见 §7.5、`Docs/Testing.md`）。

> ⚠️ 事件枚举 `PredefinedEngineEventType` 定义在 **`Core/`**（`IEngineEvent.hpp` 内），**不在 `Event/`**。

### 5.2 归属规则

**R1 · 模块化目录**
`Code/Native/Engine/<模块>/`，每个模块一个目录，目录名 **PascalCase**。

**R2 · 私有实现放 `Internal/`**
只被同模块其它文件使用的辅助设施放 `<模块>/Internal/`，用子命名空间 **`PenEngine::Internal`**。
"平台实现的私有头"（如 `Win32Window.h`、`Win32NotificationBox.h`）**与 `.cpp` 同目录**，不要塞进 `Internal/`。

**R3 · 模块级子目录**
`Filesystem/`（同模块的次级分类）等按需嵌套，保持与同模块既有布局一致。
**测试不放在模块目录**：各层测试集中在 `<层>/Test/UnitTest/`（单元测试）与
`<层>/Test/Benchmark/`（基准），另有该层 `Test/CMakeLists.txt` 装配（见 §7.5）。
放回 `<模块>/tests/` 会被根 CMake 编进主程序，测试里的 `main()`/`TEST()` 会撞入口。

**R4 · 头文件与实现同目录**
`.h` / `.hpp` 与对应 `.cpp` 同目录。**不设** `include/` 与 `src/` 分离。

**R5 · 头文件扩展名**
与同目录既有文件保持一致；新模块若全为头文件实现，用 `.hpp`。

**R6 · `Memory` 目录（唯一例外）**

| 项 | 值 |
| --- | --- |
| 位置 | `Code/Native/Memory/`（**不在 `Engine/` 下**） |
| 命名空间 | **`PenMemory`**（不是 `PenEngine`） |
| 对外 ABI | 纯 C 的 `extern "C"`，声明在 **`Code/Native/Memory/Interface.h`**（实现 `Interface.cpp`） |
| 布局 | 源文件**平铺**，**不使用 `Internal/`** |
| 构建 | 自己的 `CMakeLists.txt`，产出 `PenMemory.dll` + 导入库 |
| 编译范围 | **不参与主程序编译**（避免同一份分配器被编两遍） |
| 主程序侧入口 | `Engine/Memory/Memory.hpp`（C++ 封装）+ `Engine/Memory/MemoryOperator.cpp`（`new/delete` 接管） |
| 设计文档 | `Code/Native/Memory/DESIGN.md`、`README.md` |

**R7 · 包含路径**
**包含根是 `Code/Native/`**，跨模块写 `"Engine/Xxx/Yyy.h"`；同模块内就近用相对路径
（如 `"CoreApplication.h"`、`"../Core/Environment.h"`），避免 `../../..` 超过两级。

**R8 · 空目录**
`Code/Native/Editor/`、`Code/Native/Engine/Asset/`、`Code/Native/Engine/Resource/` 是**已建好的规划占位**。
需要新模块时**新建自己的目录**，不要占用这三处。
（例外：`Code/Native/Editor/Test/` 已是 Editor 层的测试骨架，见 §7.5。）

### 5.3 规划中但尚未创建/未实现的模块

| 目录 | 用途 |
| --- | --- |
| `Code/DotNet/` | C# 侧源码与 csproj（**不存在**） |
| `Engine/Render/`（或 RHI 目录，名字未定） | 渲染硬件接口（**不存在**，名字未定） |
| `Engine/Scene/` | 场景树 / Entity（**不存在**） |
| `Engine/Asset/`、`Engine/Resource/` | 资源系统（**空占位**，尚未实现） |

→ 开始这些模块前，先读 §8 的调研索引，再向作者确认目录名与首批范围（§9 Q2）。
**注意**：R1–R8 只覆盖 `Code/Native/`，对 `Code/DotNet/` 尚**无归属规则**。

---

## 6. 编码规范

### 6.1 文件头

每个源文件（`.h` / `.hpp` / `.cpp`）**第 1 行起**必须有这 4 行：

```cpp
// File /Native/Engine/Xxx/Yyy.h
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat
```

> ✅ **当前仓库已全量合规**：无 MPL-2.0 残留、无第 2 行尾随空格、无缺头文件。
> 这条是**已收敛的规范**，新文件照上面模板写即可。

### 6.2 缩进与排版

| 项 | 约定 |
| --- | --- |
| 缩进 | **Tab** |
| 大括号 | **Allman**（左括号独占一行）。例外：单行函数 `{ return ...; }`，以及构造函数初始化列表可同行 `{` |
| 文件编码 | **UTF-8**（构建固定 `/utf-8`） |

> ⚠️ **现状例外**：`Engine/Event/` 模块**整体使用 4 空格缩进**（该模块绝大多数文件是空格）。
> `Utils/Flag.hpp`、`Memory/Test/UnitTest/MiniTest.h` 为混合缩进。
> **在 `Event/` 下新增文件时跟随该模块用空格**；其它模块用 Tab。拿不准就看同目录邻居。

### 6.3 命名

| 类别 | 约定 | 示例 |
| --- | --- | --- |
| 命名空间 | `PenEngine` | `namespace PenEngine` |
| 库内私有命名空间 | `PenEngine::Internal` | `IO/Internal/*` |
| 分配器命名空间 | `PenMemory` | `Code/Native/Memory/*` |
| 类型 / 类 / 结构 / 枚举 | **PascalCase** | `CoreApplication`、`SignalObject` |
| 非成员函数 / Lambda | **PascalCase** | `PackU32(...)`、`PreMulAlpha(...)` |
| 成员函数（库自有 API） | **PascalCase** | `Size()`、`Emit()`、`Connect()` |
| 成员函数（STL 兼容重载） | **snake_case**，与 STL 同名并存 | `Size()`/`size()`、`Begin()`/`begin()` |
| 概念 / requires | **PascalCase** | `IsOneOf<T, ...>`、`MathFloatingPoint<T>` |
| 私有成员变量 | **`m_` + camelCase** | `m_size`、`m_window`、`m_liveTaskCount` |
| 公开成员变量 / 信号 | **PascalCase** | `Width`、`Height`、`DestroyLater` |
| 局部变量 | **camelCase** | `auto token = ...` |
| 模板参数 | **PascalCase** | `template <typename CharType>` |
| 宏 | 见 §6.4 | |

> ⚠️ **私有/公开成员用不同前缀策略**，这是实测结论：
> - 私有成员一律 `m_` + 小写开头（`m_size`）；**不要写 `m_Size`**；
> - 公开成员**不带 `m_`**，直接 PascalCase（`WindowInitContext::Width`、
>   `ApplicationConfig::Text`）；信号槽名同理（`DestroyLater`）。
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
>
> ⚠️ **另有构建期宏**（由 CMake 定义，不在 `Environment.h` 里）：
> `PENFRAMEWORK_BUILD_TERMINAL`（见 §7.2）、`PENFRAMEWORK_BUILD_TESTS`（见 §7.5；**是 CMake 选项名，
> 不再有 `PENFRAMEWORK_BUILD_GTEST` 这个宏**）。

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
（**测试目标不接管**——gtest 是 DLL，两套堆互 free 会 `__fastfail`，见 §7.5。）

需要显式控制时用 `Engine/Memory/Memory.hpp` 的 `PenEngine::Memory`：`Allocate` / `AlignedAllocate` /
`Deallocate` / `AlignedDeallocate` / `GetAllocatedSize` / `GetStats` / `VerifyHeap` / `ReleaseFreeMemory`
/ `GetPageSize` / `GetAlignment`。

> ⚠️ `Memory::Allocate` 返回**未构造内存**——本层不做 placement new、不调用析构。
> 分配器的设计与验收标准见 `Code/Native/Memory/DESIGN.md`。

### 6.10 第三方依赖

- 依赖必须经 **vcpkg manifest（`vcpkg.json`）** 添加。**不得**往仓库塞源码，也不得绕开走系统 `find_package`
- 当前 vcpkg 依赖：`boost-unordered`、`boost-locale`、`boost-dynamic-bitset`、`plf-hive`、
  `magic-enum`、`freetype`、`gtest`、`wintoast`（仅 Windows）
- **全局 `new`/`delete` 已被接管**：不要引入自带全局分配器的第三方库
- Windows API 经 `Engine/OS/Windows/Windows.h` / `WindowsMinDef.h` 收敛，不要到处直接 `#include <windows.h>`

> ⚠️ **有已声明但暂未使用的依赖**：`magic_enum`、`freetype` 已在 `CMakeLists.txt` 中
> `find_package` + `target_link_libraries`，但当前**代码里还没有引用点**。
> 这是**预留**（enum→string、字体），**不要当作冗余删除**。

**平台 SDK（如 DirectX 系列）走系统 SDK**——即由 Visual Studio / Windows SDK 提供头与库，
**不加入 `vcpkg.json`**。这与"依赖经 vcpkg 管理"并不矛盾：vcpkg 管的是**可移植的第三方库**，
平台 SDK 由工具链提供。

> 📌 因此 `vcpkg.json` 中不含任何 DirectX 条目是**预期状态**，不是遗漏。
> 当前已在 CMake 中链接的平台库：`shell32.lib`（`CommandLineToArgvW`）、
> `Advapi32.lib`（注册表读系统版本）。
> 需要其它平台 SDK 组件时，在 CMake 里显式链接，并注意**目标 Windows SDK 版本**由工具链决定——
> 若要用较新的 Agility SDK，需另行与作者确认。

### 6.11 语言与编译器基线

**C++23**，MSVC 为主，编译固定加 `/utf-8`、`/Zc:preprocessor`、`/diagnostics:caret`、`/FC`。
写预处理器代码时必须符合 `/Zc:preprocessor` 的合规形式。

---

## 7. 构建与验证

### 7.1 前置条件

| 项 | 要求 |
| --- | --- |
| 操作系统 | **Windows**（非 Windows 也能配置，但无 `WinMain`，只有控制台入口） |
| Visual Studio | 含 C++ 工作负载（提供 `cl.exe`） |
| CMake | **≥ 4.0**，且**在 PATH 中** |
| Ninja | 已安装且在 PATH |
| vcpkg | 已安装且设置 **`VCPKG_ROOT`** |
| `VSINSTALLDIR` | **需要**（预设引用了它，为空则 configure 失败）。⚠️ **不保证已设置**，见下方自举方法 |
| 网络 | 首次 configure 需下载依赖 |

> ⚠️ **先确认工具链真的可用**，否则构建命令连启动都做不到：
> ```powershell
> (Get-Command cmake).Source; (Get-Command ninja).Source
> $env:VCPKG_ROOT; $env:VSINSTALLDIR
> ```
> `cmake` / `ninja` / `cl.exe` 通常只在 **「Developer PowerShell for VS」** 里可用。
>
> ⚠️ **但 agent 的工具调用跑在普通 PowerShell 里，没有"切换到开发者终端"这个动作**——
> 正确做法是**在命令内部自举开发者环境**（`Code/Native/Memory/build.ps1` 就是这么做的）：

**自举步骤**

1. **找 VS 安装根**：优先用 `$env:VSINSTALLDIR`；**该变量可能为空**，此时回退 `vswhere`：
   ```powershell
   $vw = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
   & $vw -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
   ```
2. **`call "<VS根>\VC\Auxiliary\Build\vcvars64.bat"`** —— 它同时设置 `PATH` / `INCLUDE` / `LIB` 与 `VSINSTALLDIR`
3. 之后才能执行 `cmake --preset ...` / `cmake --build <构建目录>`

> ⚠️ **两个实测坑**：
> - **环境变量有值 ≠ PATH 有工具链**：本机出现过 `VSINSTALLDIR` / `VCPKG_ROOT` **都有值**、
>   而 `cmake`/`ninja`/`cl.exe` **全部 `NOT FOUND`** 的状态。所以第 1 步的 `Get-Command` 检查才是判据，
>   不要只看环境变量就认为可以构建。
> - **全新构建目录第一次 configure 还需要 Ninja 在 PATH**（`call vcvars64.bat` 之后 `where ninja`
>   实测为空）：VS 自带的在 `<VS根>\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja`，需要自己加进
>   `PATH`。另外 manifest install 阶段**要联网**拉 vcpkg 注册表，断网时表现为每个依赖都
>   `does not exist` + `vcpkg install failed`（该失败发生在 `CMakeLists.txt:3 (project)`，
>   与项目自身的 CMake 改动无关）。已配置过的 `out/build/<preset>/` 两条都不需要。
> - **`call` 与后续命令要写进 `.bat` 文件、再 `cmd /c "<该 .bat 路径>"` 执行**：
>   `cmd /c "call "X.bat" && cmake ..."` 这种嵌套引号会**静默失败**（实测只得到 `exit code: 1`、无任何输出）。
>   并且 **`.bat` 文件必须是 ASCII-only**：`cmd.exe` 按 **OEM 代码页**解析它，
>   中文注释会造成语法乱码（实测报 `'preset]' is not recognized`、`'vcvars64.bat。' is not recognized` 之类）。

**不要试图自己安装工具链，也不要改用别的生成器。**

### 7.2 构建命令与输出形态

```powershell
cmake --preset x64-debug                  # 配置
cmake --build out/build/x64-debug         # 构建（注意：直接给构建目录）
```

⚠️ **`CMakePresets.json` 只定义了 `configurePresets`，没有 `buildPresets`**。
因此下面这条命令**会失败**并报 `No such build preset`：

```powershell
cmake --build --preset x64-debug   # ❌ 不可用：本项目没有 build preset
```

**构建时一律直接指定构建目录** `out/build/<preset>/`。`--preset` 只在**配置**阶段有效。

**输出形态 `OUTPUT_BUILD_TYPE`**（决定进程入口与链接子系统）：

| 取值 | 子系统 | 入口 | 说明 |
| --- | --- | --- | --- |
| `Terminal`（默认） | 控制台 | `int main` | 便于直接看到 `std::println` 输出 |
| `Native` | Windows | `wWinMain` | 无控制台窗口 |

预设对应关系（**预设名带空格**，命令行里需要引号）：

| 预设 | OUTPUT_BUILD_TYPE |
| --- | --- |
| `x64-debug` / `x64-release` | `Terminal` |
| `"x64-debug test"` | `Terminal`，并置 `PENFRAMEWORK_BUILD_TESTS=ON`（独立构建目录，见 §7.5） |
| `"x64-debug native"` / `"x64-release native"` | `Native` |
| `x86-debug` / `x86-release` | 未设置（回落到默认 `Terminal`） |

也可用 `-DOUTPUT_BUILD_TYPE=Native` 直接覆盖。`x64-debug` 预设已默认 `PENFRAMEWORK_BUILD_TESTS=ON`。

> ⚠️ **只有 x64 系列是实际验证过的路径**。`x86-*` 继承自 `windows-base` 而**没有
> `CMAKE_TOOLCHAIN_FILE`**，拿不到 vcpkg 工具链，**不要假设其可用**。

**产物**：`out/build/<preset>/` 下的 `PenFramework.exe` 与 `PenMemory.dll`。
后者的 `RUNTIME_OUTPUT_DIRECTORY` 被显式设为构建根目录，**必须与 exe 同目录才能加载**。

> 💡 若 `out/build/x64-debug/` 已有配置缓存，**可直接跳过配置、只跑构建**——
> 缓存里的工具链路径未必等于当前 `$env{VCPKG_ROOT}`，重新 configure 反而可能失败。
> 只有新增/删除 `Engine/` 下的源文件时才必须重新 configure（见 §7.6）。

### 7.2.1 临时测试如何编译（**不进 CMake**）

> 📌 **先读 §7.5**：仓库内的正式测试已有统一入口（`<层>/Test/UnitTest/` + `ctest`）。
> 本节只适用于**一次性实验**——验证完就丢、不该留在仓库里的那种。

**带 `main()` 的代码不能放进 `Code/Native/Engine/**`（`Test/` 除外）或 `Code/Native/Main.cpp`**——
这是硬约束，不是偏好：

- 根 `CMakeLists.txt` 用 `file(GLOB_RECURSE ... Code/Native/Engine/*)` 把 Engine 编成 `PenEngine` 库
  （`Test/` 已被 `list(FILTER ...)` 排除），交给 `PenFramework.exe` 链接；
- `Code/Native/Main.cpp` 独占唯一的进程入口。

→ 任何落进这两处的 `main()` 都会与 `Main.cpp` **撞重复入口符号**。
所以临时测试**独立编译**，源码留在临时工作区（§2.1），一个字都不进 CMake。

**模板**（ASCII-only 的 `.bat`；与 `Memory/build.ps1` 一样自行 `call vcvars64.bat`）：

```bat
call "%VSINSTALLDIR%VC\Auxiliary\Build\vcvars64.bat"
cl /nologo /std:c++latest /EHsc /O2 /MD /utf-8 /DUNICODE /D_UNICODE ^
   /I "<仓库>\out\build\x64-debug\vcpkg_installed\x64-windows\include" ^
   Test.cpp /Fe:Test.exe ^
   /link /LIBPATH:"<仓库>\out\build\x64-debug\vcpkg_installed\x64-windows\lib" Foo.lib
```

**要点**（都是实测踩出来的）：

- ⚠️ 模板里的 `%VSINSTALLDIR%` **可能为空**——空则先按 §7.1 用 `vswhere` 取到 VS 根再拼路径，
  否则这一行会静默失败（`call` 一个不存在的文件，后面 `cl` 就是 `NOT FOUND`）。
- **头/库用工程内的 `out\build\<preset>\vcpkg_installed\x64-windows\`**，而不是 `$env{VCPKG_ROOT}`
  的全局树——保证与主程序用的是同一份 triplet 产物（两处通常都存在，别混）。
- **CRT 与库必须同配置配对**：`/MD` 配 `lib\`，`/MDd` 配 `debug\lib\`。混用是 ABI 不匹配。
- **Windows SDK 头由 `vcvars64.bat` 设的 `INCLUDE` 带进来**，不需要额外 `/I`。
- 若依赖库对系统 DLL 是**动态加载**的（如 `wintoast` 对 SHELL32 / PROPSYS / COMBASE 用
  `LoadLibraryW` + `GetProcAddress`），则**不需要**为它们加导入库；
  拿不准某个 WinRT 入口归哪个导入库时（`runtimeobject.lib` 还是 `windowsapp.lib`？），
  **用同样的动态加载规避猜测**，而不是试错链接。
- 跑过测试后若 exe 仍在运行，下一次链接会报 `LNK1168 无法打开 ... 进行写入`——
  先 `Get-Process <名字> | Stop-Process`。

### 7.3 运行时数据（ApplicationData）

`ApplicationData/` 是**运行时数据根**（当前只有 `ApplicationConfiguration.json`）。

`ApplicationConfig` 按「可执行文件目录 / ApplicationData / ...」查找配置，
因此根 `CMakeLists.txt` 有一条 **POST_BUILD 规则**把 `ApplicationData/` 的**内容**
（不是顶层目录名）拷到 exe 同级。

含义：
- **改配置不需要重新编译**，但改了仓库里的那份后需要重新构建（或手工拷贝）才能生效；
- 若新增配置文件，放进 `ApplicationData/` 即会被自动拷贝，**不要写进 `Code/`**。

### 7.4 运行时的预期现象

`CoreApplication` 已实现入口分派、命令行解析、配置装载、窗口创建与消息泵；
**场景树与渲染尚未实现**。运行 `PenFramework.exe` 会看到一个窗口，
**不渲染、不响应游戏逻辑**，点关闭即正常退出。

**这是当前实现状态，不是环境故障**——不要因此去改构建配置。

### 7.5 测试现状

**三层都有统一入口**：源码在 `<层>/Test/UnitTest/`（单元测试）与 `<层>/Test/Benchmark/`（基准），
由根 CMake 的 `PENFRAMEWORK_BUILD_TESTS` 统一构建、`ctest` 统一运行。
完整约定与命令见 **`Docs/Testing.md`**，本节只记"是什么、怎么跑、改之前要知道什么"。

| 目标 | 情况 |
| --- | --- |
| `Engine/Test/` | ✅ GTest 用例（`PenEngineTest`），每个 `TEST()` 注册成一个 ctest 用例 |
| `Memory/Test/` | ✅ MiniTest 框架（**不链 GTest**），7 个测试 exe 注册进 ctest + 1 个基准 |
| `Editor/Test/` | 🟡 只有骨架（Editor 本身尚无源码），目录为空时 CMake 自动跳过 |

**开关与预设**：

- 唯一开关 `option(PENFRAMEWORK_BUILD_TESTS "…" OFF)`，**默认 OFF**；
- 预设 `x64-debug`（agent 路径）与 `x64-debug test`（手工路径，独立构建目录）已置 `ON`；
- 旧名 `PEN_MEMORY_BUILD_TESTS`、`PENFRAMEWORK_BUILD_GTEST` **已废除**，写旧名不会生效；
- 从旧状态切过来时**至少要重新 configure 一次**（预设变量与目标结构都变了）。

**跑法**（agent 路径，一条命令到底）：

```powershell
cmake --preset x64-debug
cmake --build out/build/x64-debug
ctest --test-dir out/build/x64-debug --output-on-failure
# 等价：ctest --preset x64-debug；过滤：-R MathTypes；列出：-N
```

**关键实现事实**（改这里之前必须知道）：

- `Engine/**` 编成 **`PenEngine` OBJECT 库**，`PenFramework.exe` 与 `PenEngineTest` 共享同一份编译产物；
- 根 CMake 用 `list(FILTER ENGINE_SRC EXCLUDE REGEX "/Test/")` 把测试排除出主程序——
  **不要**把带 `main()`/`TEST()` 的文件放回模块目录（`Engine/**` 会被编进 exe）；
- ⚠️ **全局 `new/delete` 接管（`Engine/Memory/MemoryOperator.cpp`）只属于主程序，不在 `PenEngine` 里**：
  vcpkg 的 `x64-windows` 只提供 **gtest.dll / gtest_main.dll**（没有静态库），
  若测试 exe 也接管分配，就会与 gtest.dll 的 CRT 堆互相释放对方内存，
  触发 `_CrtIsValidHeapPointer` → `__fastfail`（实测：测试 exe 静默 `exit code 3`）。
  测试目标用 CRT 堆即可；要验证分配器接管，用 `Memory/Test/UnitTest/test_override.cpp` 那条路径；
- 依赖（Boost / freetype / wintoast / 平台库 / `PenMemory`）挂在 `PenEngine` 上（`PUBLIC`），
  exe 与测试目标都通过链接它拿到头目录与导入库；
- 测试 exe 一律落**构建根目录**（与 `PenMemory.dll` 同级），否则 `test_override` 之类加载不到 DLL；
- 装配函数在 `CMake/PenTest.cmake`（`pen_add_unit_test` / `pen_add_benchmark`），
  用例经 `gtest_discover_tests(... DISCOVERY_MODE PRE_TEST)` 注册，**基准不注册 ctest**。
- `Memory/Test/**` 的源码同时被 `Memory/build.ps1`（不经 CMake 的独立 MSVC 路径）引用，路径改动两处同步（§7.6）。

**`build.ps1` 的参数**（PenMemory 专用）：

| 参数 | 作用 |
| --- | --- |
| （无） | Release：`/O2 /MD /DNDEBUG`，产物在 `build/` |
| `-Debug` | `/Od /Zi /MDd /D_DEBUG`，**断言开启**，产物在 `build-debug/` |
| `-NoRun` | 只编译，不运行测试 |
| `-Bench` | 测试通过后额外运行基准 |

该脚本用 `/W4 /WX`（警告视为错误），并自行定位 `vcvars64.bat`；**非 Windows 平台不可用**。

> 📌 PenMemory 的验收范式值得沿用到新模块：**每个用例结束做一次全堆不变量校验**
> （`PenEngine::Memory::VerifyHeap()`）、**多种编译配置各跑一遍**、**重复多轮**、并记录**量化数据**。

### 7.6 构建系统改动规则

| 位置 | 新文件是否自动编译 |
| --- | --- |
| `Code/Native/Engine/**` | ✅ 自动（根 CMake 用 `file(GLOB_RECURSE ...)`，**无 `CONFIGURE_DEPENDS`**；`Test/` 与 `MemoryOperator.cpp` 不在 `PenEngine` 里） |
| `Code/Native/*/Test/UnitTest/**`、`Test/Benchmark/**` | ✅ 自动（helper 的 glob 带 `CONFIGURE_DEPENDS`，增删测试文件**不必**手工重新 configure） |
| `Code/Native/Memory/*.cpp` | ❌ **两处清单都要同步**：`Code/Native/Memory/CMakeLists.txt` 的 `PEN_MEMORY_SOURCES`，以及 `build.ps1` 的 `$libSources` |
| `Code/Native/Memory/Test/UnitTest/**`、`Test/Benchmark/**` | ❌ **两处清单都要同步**：`Memory/CMakeLists.txt` 的测试清单，以及 `build.ps1` 的 `$tests` / `$benchmarks` |
| `Code/Native/Editor/**` 及其它新顶层目录 | ❌ **必须改根 `CMakeLists.txt`** |
| `ApplicationData/**` | ✅ 无需改 CMake（POST_BUILD 整目录拷贝） |

> ⚠️ **GLOB 的坑**：新增/删除 `Engine/` 下的文件后**必须重新 configure**，只 `--build` 发现不了。
>
> ⚠️ 两条测试路径的覆盖**不完全一致**（`build.ps1` 的清单与 CMake 注册的测试目标不同，
> 例如它不编译 `test_dll`）。需要全量验证时用 CMake/ctest 路径。

### 7.7 警告等级（两个目标不同）

| 目标 | 策略 |
| --- | --- |
| `PenMemory`（经 `build.ps1`） | `/W4 /WX` —— 最高等级 + **警告视为错误** |
| `PenFramework.exe`（经根 CMake） | **未设 `/W4` / `/WX`** —— 警告**不阻断构建** |

→ 主程序"构建成功"**不代表无警告**。要么自己在输出里确认，要么引入 `/WX`（需先问作者）。

### 7.8 调试支持

- **Natvis**：`Visualizers/PenFramework.natvis` 仅在 **MSVC + Debug** 下加入构建。
  新增/重命名会在调试器里查看的核心类型时**需同步更新**，否则 VS 中显示为裸结构体。
- **即时中断**：Debug 下 `DEBUG_*` 宏会调用 `_CrtDbgReportW` + `_CrtDbgBreak` + `__fastfail(5)`。
  调试器直接崩到 `__fastfail` 时，**先怀疑这些宏**，而不是代码崩溃。
- **Edit and Continue**：Debug / RelWithDebInfo 走 `/ZI`（受 CMP0141 控制）。
  注意 **ASan 与 EditAndContinue 互斥**，临时开 ASan 需同时改 `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT`。

### 7.9 当前没有的东西

无 CI / 无 clang-format / 无 clang-tidy / 无静态分析与格式检查脚本 /
除 `Docs/Testing.md`（测试约定）与 `Docs/README_English.md` 外无开发者入口文档。

→ 要引入上述任一，**先问作者**。

---

## 8. 调研资料索引

⚠️ 下列文档**全部位于只读参考区**，且**全部是旧工作区的产出**。它们位于
`PersonalWorkaround/PenFramework-old-research/`，是对**旧工作区的部分总结**（**不是全貌**）。
下表中**不以 `PersonalWorkaround/` 开头**的路径均相对该目录。
用法：**读思路、抄结论、不要抄代码、不要抄约定**。（`PersonalWorkaround/README.md` 是例外，见 §2.2.1。）

| 你要做的事 | 先读 |
| --- | --- |
| 项目整体设计与已确认决策 | `Architecture.md`（§1–§3 有效；§5–§8、§10 为旧框架历史；§9 以本仓库实测为准） |
| 判断旧组件能否复用 | `OldFramework-Reusability-Survey.md` |
| 按件移植旧代码 | `Port-Reconciliation.md`（⚠️ 其"MPL-2.0 头""包含路径"两项要求已作废） |
| 接入 .NET / C# 互操作 | `Research-DotNet-Embedding-2026.md`（§7 含一份 `DotNetHost/CMakeLists.txt` 草案） |
| 设计 RHI | `RHI-Architecture-Research-2026.md` + 同目录 `research/rhi-interface-validation-testing.md`、`research/RHI-ResourceBinding-Swapchain.md` |
| 设计资源系统与打包 | `PersonalWorkaround/Asset/AssetPipeline_Survey.md`（§8 是给本项目的落地建议） |
| 看 RHI 的真实接口形态 | `PersonalWorkaround/PenFramework-old-research/.research/` 下的第三方接口摘录（含 `RHI-Architecture-Survey.md`） |

> 📌 这些文档的**章节号、行数、体量会随更新而变化**，引用时以文件内实际标题为准。
> **引用前先确认文件存在**——`PersonalWorkaround/` 下的参考资料可能被整理或移动。
> 旧工作区本体（`PersonalWorkaround/PenFramework-old/`）里的**源码**仍可查阅，
> 但其**文档未必还在原处**——找文档先来 `PenFramework-old-research/`。

**仅移植任务需要**：旧框架是半成品单二进制引擎（MSVC-only、Windows-only、D3D11-only）。
其数学库有真 bug（新仓库的 `Engine/Math` 是重写的，**不要参考旧实现**）、
对象/事件模型是桩、资源管线与延迟渲染命令缓冲未实现、无任何 .NET 互操作能力；
其协程调度器有 use-after-free 与 header 全局变量的 ODR 问题
（新仓库已重写，但改 `CoroutineScheduler` 前先读其 `I1`–`I5` 不变量注释）。
逐条缺陷清单见 Reusability Survey。

---

## 9. 待作者决定的开放问题

遇到以下任一事项时**停下来问作者，不要自行拍板**，也不要把自己的选择写进文档当作既定事实。

| 编号 | 事项 | 前置 |
| --- | --- | --- |
| **Q1** | **下一个里程碑是什么？**（RHI / 资源 / .NET 互操作 / 编辑器，四条路的前置工作完全不同） | — |
| **Q2** | **新模块的目录名与划分**（如 RHI 放 `Engine/Render/` 还是 `Engine/RHI/`？后端实现放哪？） | Q1 |
| **Q3** | **`Engine/Asset/`、`Engine/Resource/` 空占位的首批范围** | Q1 |
| **Q4** | **`Engine/` 跨层 include 用相对路径还是 `Engine/...` 前缀**（现状：`Main.cpp` 用 `Engine/...`，模块内用相对） | Q2 |
| **Q5** | **是否允许修改当前在途未提交的文件**（本仓库常有并行改动，任何改代码的任务都会撞上） | — |
| **Q6** | **后续依赖能否自带上游全局分配器**（`new`/`delete` 已被接管，会冲突） | — |

**推荐提问顺序**：先问 **Q1** 与 **Q5**（无前置、且阻塞其余）。

**提问格式**（与 §0.2 ② 的「需同意的要点」是同一套写法）

1. 说明**你要做的事**与**卡在哪一条编号**；
2. 给出**你倾向的选项与理由**（不要只抛问题）；
3. 若涉及在途未提交文件，**同时列出你要改的文件**。

⚠️ **写完就结束本轮**，把输入框交给作者——**不要用「提问工具」**（见 §0.2 ④）。
