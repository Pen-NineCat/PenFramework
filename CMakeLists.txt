cmake_minimum_required(VERSION 4.0)

project("PenFramework")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# 输出形态：决定进程入口与链接子系统（详见 Code/Native/Main.cpp 的入口分派）
#   Terminal - 控制台子系统 + int main   （默认，便于 std::println 直接输出到终端）
#   Native   - Windows 子系统 + WinMain   （无控制台窗口）
# 由 CMakePresets.json 的对应预设设置，也可用 -DOUTPUT_BUILD_TYPE=Native 覆盖。
set(OUTPUT_BUILD_TYPE "Terminal" CACHE STRING "Output build type: Terminal / Native")
set_property(CACHE OUTPUT_BUILD_TYPE PROPERTY STRINGS Terminal Native)

message(STATUS "Cmake version: ${CMAKE_VERSION}.")
message(STATUS "Cmake build type: ${CMAKE_BUILD_TYPE}.")

message(STATUS "Set compiler: ${CMAKE_CXX_COMPILER_ID} sepcific options.")

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC") 
    if (POLICY CMP0141)
        cmake_policy(SET CMP0141 NEW)
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
    endif()

    add_compile_options(/utf-8) 
    add_compile_options(/Zc:preprocessor)
    add_compile_options(/diagnostics:caret)
    add_compile_options(/FC)
else()
    message(STATUS "Compiler dont need sepcific options.")
endif()

message(STATUS "Compiler sepcific options ready.")
message(STATUS "Output build type: ${OUTPUT_BUILD_TYPE}.")

if(WIN32)
    add_compile_definitions(UNICODE _UNICODE)
endif()

# 包管理 
# ---------------------------------------------
# String Character Convert | boost::locale 支持
# CoroutineScheduler | boost::unordered_flat_map 支持
# Thread P/E | boost::dynamic_bitset 支持
# UUID | boost::uuid支持
find_package(Boost CONFIG REQUIRED COMPONENTS
	locale
    unordered
    dynamic_bitset
    uuid
)

# Coroutine System plf-hive支持
# 如果std::hive可用，可以使用std::hive替换
find_path(PLF_HIVE_INCLUDE_DIRS "plf_hive.h")

# enum -> string支持
find_package(magic_enum CONFIG REQUIRED)

# 字体支持
find_package(freetype CONFIG REQUIRED)

# 日志支持
find_package(spdlog CONFIG REQUIRED)

# 测试：唯一开关，统一覆盖 Engine / Memory / Editor 的单元测试与基准（约定见 Docs/Testing.md）
# 打开后：GTest + CMake/PenTest.cmake（pen_add_unit_test / pen_add_benchmark）+ enable_testing()。
# 测试目标是独立 exe（Engine 目录不能出现第二个 main，见 §7.2.1），不参与 PenFramework.exe。
option(PENFRAMEWORK_BUILD_TESTS "构建单元测试与基准，并注册 ctest" OFF)
if(PENFRAMEWORK_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)
    include(${CMAKE_SOURCE_DIR}/CMake/PenTest.cmake)
endif()

# Windows气泡通知框支持
if(WIN32)
    find_package(unofficial-wintoast CONFIG REQUIRED)
endif()
# ---------------------------------------------

# PenMemory 分配器：Code/Native/Memory → PenMemory.dll
# ---------------------------------------------
# 该目录是独立模块，由它自己的子 CMakeLists.txt 构建；主程序只链接它的导入库
# 它自己的测试与基准同样由 PENFRAMEWORK_BUILD_TESTS 控制（见 Code/Native/Memory/CMakeLists.txt）
add_subdirectory(Code/Native/Memory)

# 各层测试：Test/ 目录自带 CMakeLists.txt，为空时 helper 自动跳过
if(PENFRAMEWORK_BUILD_TESTS)
    add_subdirectory(Code/Native/Engine/Test)
    add_subdirectory(Code/Native/Editor/Test)
endif()
# ---------------------------------------------

# 源码范围：Engine/** 归 PenEngine；Memory 目录不参与主程序编译，避免同一份分配器被编两遍
# Engine/** 编成 PenEngine OBJECT 库，而不是直接塞给 exe：测试目标（Code/Native/*/Test）
# 要与 exe 共享同一份编译产物与编译标志，既不重复编译一遍全引擎，也不会两边标志漂移。
# Test/ 必须排除——测试文件带 TEST() 与 gtest 符号，进 exe 会撞 Main.cpp 的入口/重复符号。
file(GLOB_RECURSE ENGINE_SRC ${CMAKE_SOURCE_DIR}/Code/Native/Engine/*)
list(FILTER ENGINE_SRC EXCLUDE REGEX "/Test/")

# 全局 new/delete 接管（Engine/Memory/MemoryOperator.cpp）**只给主程序**，不进 PenEngine：
#   - 它把本模块的分配全转到 PenMemory.dll，而 gtest 是以 DLL 形式链接的（vcpkg 的
#     x64-windows 只提供 gtest.dll/gtest_main.dll），两边堆不同，
#     一旦跨模块释放对方的内存就会触发 _CrtIsValidHeapPointer -> __fastfail；
#   - 该文件自己的注释也是这个原则：「只替换主程序自己的分配，不去接管其它模块的堆」。
list(FILTER ENGINE_SRC EXCLUDE REGEX "/Engine/Memory/MemoryOperator\\.cpp$")

add_library(PenEngine OBJECT ${ENGINE_SRC})

# 主程序独占的源码：进程入口 + 全局 new/delete 接管
set(PEN_APP_SRC
    ${CMAKE_SOURCE_DIR}/Code/Native/Main.cpp
    ${CMAKE_SOURCE_DIR}/Code/Native/Engine/Memory/MemoryOperator.cpp
)

# 入口与子系统
# ---------------------------------------------
# Main.cpp 依据 PENFRAMEWORK_BUILD_TERMINAL 在 int main 与 WinMain 之间二选一，
# 该宏只定义在主程序目标上，必须与 add_executable 的 WIN32（即 /SUBSYSTEM:WINDOWS）保持一致：
#   Terminal - 定义 PENFRAMEWORK_BUILD_TERMINAL，走控制台子系统，入口为 main
#   Native   - 不定义该宏，走 WIN32 可执行（Windows 子系统），入口为 WinMain
if(WIN32)
    if(OUTPUT_BUILD_TYPE STREQUAL "Terminal")
        message(STATUS "Add Compile Target: ${PROJECT_NAME} (Terminal, console subsystem)")
        add_executable(${PROJECT_NAME} ${PEN_APP_SRC})
        target_compile_definitions(${PROJECT_NAME} PRIVATE PENFRAMEWORK_BUILD_TERMINAL)
    elseif(OUTPUT_BUILD_TYPE STREQUAL "Native")
        message(STATUS "Add Compile Target: ${PROJECT_NAME} (Native, windows subsystem)")
        add_executable(${PROJECT_NAME} WIN32 ${PEN_APP_SRC})
    else()
        message(FATAL_ERROR "Unknown OUTPUT_BUILD_TYPE: '${OUTPUT_BUILD_TYPE}'. Expected Terminal or Native.")
    endif()
else()
    # 非 Windows 平台没有 WinMain，只有控制台入口
    add_executable(${PROJECT_NAME} ${PEN_APP_SRC})
endif()
# ---------------------------------------------

# 依赖：挂 PenEngine（PUBLIC）而不是 exe —— exe 与各测试目标都链接 PenEngine，
# 头目录与导入库只维护这一份，避免两边漂移。
# ---------------------------------------------
target_link_libraries(PenEngine PUBLIC 
    Boost::locale 
)
target_link_libraries(PenEngine PUBLIC 
    Boost::unordered
)
target_link_libraries(PenEngine PUBLIC
    magic_enum::magic_enum
)
target_link_libraries(PenEngine PUBLIC
    Boost::dynamic_bitset
)
target_link_libraries(PenEngine PUBLIC 
    Freetype::Freetype
)
target_link_libraries(PenEngine PUBLIC
    Boost::uuid
)
target_include_directories(PenEngine PUBLIC ${PLF_HIVE_INCLUDE_DIRS})
if(WIN32)
    target_link_libraries(PenEngine PUBLIC unofficial::wintoast::wintoast)
endif()
# ---------------------------------------------

# 日志后端：spdlog（门面 Code/Native/Engine/Utils/Logger，spdlog 只出现在 Logger.cpp 一个 TU 里）
# ---------------------------------------------
# 用 header-only target：不引入 spdlog.dll，且 spdlog 内部走 std::allocator → 全局 new/delete
# → 落到 PenMemory，能被 GetStats / VerifyHeap 看见（换成 spdlog::spdlog 的 DLL 会绕过接管）。
target_link_libraries(PenEngine PUBLIC spdlog::spdlog_header_only)

# ⚠️ 必须是**目标级定义**，绝不能写在头文件里：旧 Logger.hpp 把 SPDLOG_USE_STD_FORMAT 写在
# 公开头里，任何先含 <spdlog/...> 的 TU 都会拿到另一套 fmt_lib → ODR 隐患。
# 用 PUBLIC：exe 与各测试目标共享同一份定义。
target_compile_definitions(PenEngine PUBLIC SPDLOG_USE_STD_FORMAT)

# 日志等级的编译期下限：0=Trace .. 6=Off。留空 = 由 Logger.hpp 按构建配置取默认
# （Debug=0 / Release=2）。设为 0 可让 Release 也保留 Debug/Trace 日志。
set(PENFRAMEWORK_LOG_MIN_LEVEL "" CACHE STRING "日志编译期下限（0=Trace .. 6=Off，空=按构建配置）")
if(NOT PENFRAMEWORK_LOG_MIN_LEVEL STREQUAL "")
    target_compile_definitions(PenEngine PUBLIC PENFRAMEWORK_LOG_MIN_LEVEL=${PENFRAMEWORK_LOG_MIN_LEVEL})
endif()

# Logger 编在 PenEngine 里，而输出形态宏原本只定义在 exe 目标上（见上面「入口与子系统」段）：
# LoggerConfig::EnableConsoleSink 的默认值要据此判断（Native 无控制台），故在 PenEngine 上补一份同值定义。
if(OUTPUT_BUILD_TYPE STREQUAL "Terminal")
    target_compile_definitions(PenEngine PUBLIC PENFRAMEWORK_BUILD_TERMINAL)
endif()
# ---------------------------------------------

# 平台 SDK（系统提供，不进 vcpkg）
# ---------------------------------------------
if(WIN32)
    # CommandLineUtils 用 CommandLineToArgvW 取权威命令行，它属于 shell32
    # （WindowsMinDef.h 定义了 WIN32_LEAN_AND_MEAN，Windows.h 不会再隐式带入）
    target_link_libraries(PenEngine PUBLIC "shell32.lib")
    # SystemInfo 读注册表取系统版本（ProductName / DisplayVersion / UBR）
    target_link_libraries(PenEngine PUBLIC "Advapi32.lib")
endif()
# ---------------------------------------------

# PenMemory 分配器（PenMemory.dll + 导入库）：Engine/Memory/MemoryOperator.cpp 接管了全局 new/delete
# ---------------------------------------------
target_link_libraries(PenEngine PUBLIC PenMemory)

# 主程序：Main.cpp + PenEngine 的编译产物（OBJECT 库直接链接即带入对象文件）
# ---------------------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE PenEngine)
# ---------------------------------------------

# 运行时数据
# ---------------------------------------------
# ApplicationData/ 是运行时数据根（当前只有 ApplicationConfiguration.json）。
# ApplicationConfig 按「可执行文件目录 / ApplicationData / ...」查找配置，
# 因此仓库里的这份必须出现在构建输出目录，否则每次都要手工拷。
# 注意是拷贝目录内容而非顶层目录名：目标已按 exe 同级布局。
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/ApplicationData" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/ApplicationData"
    COMMENT "复制 ApplicationData 到构建输出目录"
)
# ---------------------------------------------

# Visual Studio Nativs 支持
# ---------------------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Nativs Visualizer ready.")
    target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/Visualizers/PenFramework.natvis")
endif()
# ---------------------------------------------