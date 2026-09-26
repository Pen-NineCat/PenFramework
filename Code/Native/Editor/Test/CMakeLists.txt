# Editor 层测试（单元测试 + 基准）
# ----------------------------------------------------------------------------
# Editor 目前是规划占位：没有源码、也没有自己的 CMakeLists.txt（见 AGENTS.md §5.3、R8）。
# 这里先把目录骨架和装配入口建好——UnitTest/ 与 Benchmark/ 为空时，helper 会打印一行
# STATUS 并跳过，不阻塞 configure；等 Editor 有源码后把测试文件放进去即可生效。
#
# 注意：Editor 源码仍未参与构建，等 Editor 模块落地后需要在这里补上它自己的
# 源码收集/目标（届时 Editor 的测试目标才真正测得到东西）。

pen_add_unit_test(PenEditorTest DIR ${CMAKE_CURRENT_SOURCE_DIR}/UnitTest)
pen_add_benchmark(PenEditorBenchmark ${CMAKE_CURRENT_SOURCE_DIR}/Benchmark)
