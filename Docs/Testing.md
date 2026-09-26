# 测试：单元测试与基准

本文是**测试的唯一入口约定**。全部测试源码集中在各层的 `Test/` 目录，由 CMake 统一构建、
`ctest` 统一运行——**不需要自己找 Visual Studio、也不要手写 `cl` 命令**。
根 `CMakeLists.txt` 提供唯一开关 `PENFRAMEWORK_BUILD_TESTS`，装配函数在 `CMake/PenTest.cmake`。

---

## 1. 目录约定

```
Code/Native/
├─ Engine/Test/                  ← Engine 层测试
│  ├─ CMakeLists.txt
│  ├─ UnitTest/                  ← GTest 用例（一个 TEST() = 一个 ctest 用例）
│  └─ Benchmark/                 ← 基准（只构建，不注册 ctest）
├─ Memory/Test/                  ← PenMemory 层测试（MiniTest 框架，不用 GTest）
│  ├─ UnitTest/
│  └─ Benchmark/
└─ Editor/Test/                  ← Editor 层测试骨架（Editor 尚无源码，暂为空）
   ├─ CMakeLists.txt
   ├─ UnitTest/
   └─ Benchmark/
```

| 目录 | 放什么 | 构建/运行方式 |
| --- | --- | --- |
| `<层>/Test/UnitTest/` | 单元测试源码 | 编成测试 exe，用例逐个注册进 `ctest` |
| `<层>/Test/Benchmark/` | 基准源码 | 只构建 exe，**不注册 `ctest`**，手工运行 |
| `<层>/Test/CMakeLists.txt` | 调用 `pen_add_unit_test` / `pen_add_benchmark` | 空目录时自动跳过，不阻塞 configure |

⚠️ **测试文件不要放回 `<模块>/`（例如 `Engine/Math/tests/`）**：`Engine/**` 会被根 CMake 编入
`PenFramework.exe`，测试里的 `TEST()` 与 gtest 符号会和 `Main.cpp` 的唯一入口冲突
（根 CMake 已用 `list(FILTER ENGINE_SRC EXCLUDE REGEX "/Test/")` 排除 `/Test/` 目录）。

---

## 2. 两条运行路径

### 2.1 agent 路径（默认，一条命令到底）

`x64-debug` 预设已带 `PENFRAMEWORK_BUILD_TESTS=ON`：

```powershell
cmake --preset x64-debug
cmake --build out/build/x64-debug
ctest --test-dir out/build/x64-debug --output-on-failure
# 等价写法：ctest --preset x64-debug
```

### 2.2 手工路径（作者用）

 `x64-debug test` 预设与 `x64-debug` **配置相同、构建目录独立**（`out/build/x64-debug test`），
手工构建/调试/单跑时不干扰 agent 用的构建目录：

```powershell
cmake --preset "x64-debug test"
cmake --build "out/build/x64-debug test"
ctest --test-dir "out/build/x64-debug test" --output-on-failure
```

> ⚠️ **全新构建目录的第一次配置**（`x64-debug test` 首次、或任何新克隆）另有两个环境前提：
> ① **Ninja 要在 PATH** —— `vcvars64.bat` 在部分安装下**不会**把它加进来；
>    VS 自带的在 `<VS根>\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja`；
> ② **vcpkg 要能联网**（manifest install 阶段拉注册表）。断网时会看到
>    `Fetching registry information from https://github.com/microsoft/vcpkg (HEAD)...` 之后
>    每个依赖都 `does not exist`、最后 `vcpkg install failed`；
>    该失败发生在 `CMakeLists.txt:3 (project)`，与本项目的 CMake 改动无关。
> 已经配置过一次的 `out/build/x64-debug/` 两条都不需要（生成器路径已进缓存、依赖已装好）。

### 2.3 常用 ctest 命令

| 目的 | 命令 |
| --- | --- |
| 跑全部 | `ctest --test-dir out/build/x64-debug --output-on-failure` |
| 只跑某一组 | `ctest --test-dir out/build/x64-debug -R MathTypes` |
| 列出用例名 | `ctest --test-dir out/build/x64-debug -N` |
| 看单个用例细节 | `ctest --test-dir out/build/x64-debug -R Vec3 -V` |
| 失败重跑 | `ctest --test-dir out/build/x64-debug --rerun-failed` |
| 跑基准 exe | `out/build/x64-debug/PenEngineBenchmark.exe`、`out/build/x64-debug/benchmark.exe` |

也可以直接跑测试 exe（GTest 支持 `--gtest_filter=`）：

```powershell
out/build/x64-debug/PenEngineTest.exe --gtest_filter=MathTypes.*
```

> ⚠️ 测试 exe 一律落在**构建根目录**（与 `PenMemory.dll` 同级）。`test_override` 这类
> 接管了全局 `new/delete` 的用例需要加载 DLL，放到子目录会因找不到 DLL 而直接启动失败。

> ⚠️ **测试 exe 不接管全局 `new/delete`**：`Engine/Memory/MemoryOperator.cpp` **只编入主程序**，
> 不在 `PenEngine` 里。原因：vcpkg 的 `x64-windows` 只提供 **`gtest.dll` / `gtest_main.dll`**
> （`lib/gtest.lib` 只是导入库，没有静态库），若测试 exe 也把分配转到 PenMemory，
> 就会与 gtest.dll 的 CRT 堆互相释放对方的内存，触发
> `_CrtIsValidHeapPointer` → `__fastfail`（实测表现：测试 exe **静默 `exit code 3`、无任何输出**）。
> 要验证分配器接管本身，走 `Memory/Test/UnitTest/test_override.cpp` 那条路径。

---

## 3. 产物与开关

| 项 | 值 |
| --- | --- |
| 开关 | `option(PENFRAMEWORK_BUILD_TESTS "…" OFF)` |
| 打开方式 | `-DPENFRAMEWORK_BUILD_TESTS=ON`，或直接用已置 ON 的 `x64-debug` / `x64-debug test` 预设 |
| 已废除的旧名 | `PEN_MEMORY_BUILD_TESTS`（→ 本开关）、`PENFRAMEWORK_BUILD_GTEST`（→ 测试不再编入主程序） |
| 测试目标 | `PenEngineTest`、`PenEditorTest`（空则跳过）；PenMemory 侧 `test_sizemap` … `test_dll` |
| 基准目标 | `PenEngineBenchmark`、`PenEditorBenchmark`、`benchmark`（PenMemory） |
| 装配函数 | `CMake/PenTest.cmake` 的 `pen_add_unit_test` / `pen_add_benchmark` |

> ⚠️ 关闭开关时测试源码**完全不参与构建**（不再有"被编译但没人执行"的中间态）。
> 新增/删除 `Engine/**` 下的源文件仍需重新 configure（§7.6 的 GLOB 规则）；
> `Test/UnitTest/` 与 `Test/Benchmark/` 的 glob 带 `CONFIGURE_DEPENDS`，增删测试文件**不必**手工 configure。

---

## 4. 新增一个单元测试

1. 在 `<层>/Test/UnitTest/` 新建 `XxxTest.cpp`，文件头 4 行、Tab 缩进、Allman 大括号（§6）；
2. include 用**包含根 `Code/Native/`** 的全路径，例如 `#include "Engine/Math/Vec3.hpp"`；
3. **不要写 `main()`**——`GTest::gtest_main` 已经提供；
4. 构建后按名字跑：`ctest --test-dir out/build/x64-debug -R Xxx -V`。

真实样例（`Code/Native/Engine/Test/UnitTest/MathTypesTest.cpp` 节选）：

```cpp
#include "Engine/Math/Color.hpp"

#include <gtest/gtest.h>

using namespace PenEngine;

TEST(MathTypes, ColorConstructionClamps)
{
	const ColorF clamped = ColorF(-1.0f, 2.0f, 0.5f, 3.0f);

	EXPECT_EQ(clamped.R, 0.0f);
	EXPECT_EQ(clamped.G, 1.0f);
	EXPECT_EQ(clamped.B, 0.5f);
	EXPECT_EQ(clamped.A, 1.0f);
}
```

**PenMemory 侧不同**：它的单元测试直接用 `Code/Native/Memory/Test/UnitTest/MiniTest.h`
（三宏框架，自身不从被测分配器取堆），由 `Memory/CMakeLists.txt` 的局部函数
`pen_memory_internal_test` / `pen_memory_dll_test` 建目标，因此**不链 GTest**。
每个用例结束做一次全堆不变量校验（`PenEngine::Memory::VerifyHeap()`）是它的验收范式。

---

## 5. 新增一个基准

- 源码放 `<层>/Test/Benchmark/`（PenMemory 现在叫 `benchmark`，Engine/Editor 叫 `Pen*Benchmark`）；
- 用 `std::chrono` 手写计时即可，**目前不引入 google-benchmark 依赖**（要引入先问作者）；
- 基准**只构建、不注册 ctest**——它耗时，且结果需要人看，不能混进"全绿"信号里；
- 跑法：`out/build/x64-debug/<目标名>.exe`。

---

## 6. 与「独立脚本」路径的关系

- `Code/Native/Memory/build.ps1` 是不经 CMake 的 **MSVC 独立路径**（自带 `vcvars64.bat` 定位、
  `/W4 /WX`）；它用的测试源码就是 `Memory/Test/UnitTest/`、`Memory/Test/Benchmark/` 下的同一批文件。
  **改测试文件路径时两处都要同步**（CMake 清单 + `build.ps1` 的 `$tests` / `$benchmarks`）。
- 两条路径覆盖不完全一致（`build.ps1` 不编译 `test_dll`）。要全量验证走 CMake/ctest 路径。
- 一次性实验（不属于本仓库的临时验证）仍按 AGENTS.md §7.2.1：源码留在 `PersonalWorkaround/`，
  用 ASCII-only `.bat` 自行 `call vcvars64.bat` 独立编译，**不进 CMake、不进 `Code/Native/`**。
