# Engine 层测试（单元测试 + 基准）
# ----------------------------------------------------------------------------
# 由根 CMakeLists.txt 在 PENFRAMEWORK_BUILD_TESTS=ON 时 add_subdirectory 引入，
# 装配函数来自 CMake/PenTest.cmake（根 CMake 已 include）。
#
# Engine 源码不在这里重新收集：根 CMakeLists.txt 把它编成 PenEngine OBJECT 库，
# PenFramework.exe 与 PenEngineTest 共享同一份编译产物，编译标志完全一致。
#
# 新增一个测试：把 XxxTest.cpp 放进 UnitTest/ 即可（glob 带 CONFIGURE_DEPENDS，
# 不必手工重新 configure）。

pen_add_unit_test(PenEngineTest DIR ${CMAKE_CURRENT_SOURCE_DIR}/UnitTest LIBS PenEngine)
pen_add_benchmark(PenEngineBenchmark ${CMAKE_CURRENT_SOURCE_DIR}/Benchmark LIBS PenEngine)
