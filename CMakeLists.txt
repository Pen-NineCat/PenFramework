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
# Char convert boost::locale 支持
# boost::unordered_flat_map 支持
find_package(Boost CONFIG REQUIRED COMPONENTS
	locale
    unordered
)

# Coroutine System plf-hive支持
# 如果std::hive可用，可以使用std::hive替换
find_path(PLF_HIVE_INCLUDE_DIRS "plf_hive.h")

find_package(magic_enum CONFIG REQUIRED)

# Windows气泡通知框支持
if(WIN32)
    find_package(unofficial-wintoast CONFIG REQUIRED)
endif()
# ---------------------------------------------

# PenMemory 分配器：Code/Native/Memory → PenMemory.dll
# ---------------------------------------------
# 该目录是独立模块，由它自己的子 CMakeLists.txt 构建；主程序只链接它的导入库
option(PEN_MEMORY_BUILD_TESTS "构建 PenMemory 的测试与基准程序" OFF)
if(PEN_MEMORY_BUILD_TESTS)
    enable_testing()
endif()
add_subdirectory(Code/Native/Memory)
# ---------------------------------------------

# 主程序源码：Main.cpp 与 Engine/**；Memory 目录不参与主程序编译，避免同一份分配器被编两遍
file(GLOB_RECURSE SRC ${CMAKE_SOURCE_DIR}/Code/Native/Engine/*)
list(APPEND SRC ${CMAKE_SOURCE_DIR}/Code/Native/Main.cpp)

# 入口与子系统
# ---------------------------------------------
# Main.cpp 依据 PENFRAMEWORK_BUILD_TERMINAL 在 int main 与 WinMain 之间二选一，
# 该宏只定义在主程序目标上，必须与 add_executable 的 WIN32（即 /SUBSYSTEM:WINDOWS）保持一致：
#   Terminal - 定义 PENFRAMEWORK_BUILD_TERMINAL，走控制台子系统，入口为 main
#   Native   - 不定义该宏，走 WIN32 可执行（Windows 子系统），入口为 WinMain
if(WIN32)
    if(OUTPUT_BUILD_TYPE STREQUAL "Terminal")
        message(STATUS "Add Compile Target: ${PROJECT_NAME} (Terminal, console subsystem)")
        add_executable(${PROJECT_NAME} ${SRC})
        target_compile_definitions(${PROJECT_NAME} PRIVATE PENFRAMEWORK_BUILD_TERMINAL)
    elseif(OUTPUT_BUILD_TYPE STREQUAL "Native")
        message(STATUS "Add Compile Target: ${PROJECT_NAME} (Native, windows subsystem)")
        add_executable(${PROJECT_NAME} WIN32 ${SRC})
    else()
        message(FATAL_ERROR "Unknown OUTPUT_BUILD_TYPE: '${OUTPUT_BUILD_TYPE}'. Expected Terminal or Native.")
    endif()
else()
    # 非 Windows 平台没有 WinMain，只有控制台入口
    add_executable(${PROJECT_NAME} ${SRC})
endif()
# ---------------------------------------------

# 包管理 
# ---------------------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Boost::locale 
)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Boost::unordered
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    magic_enum::magic_enum
)
target_include_directories(${PROJECT_NAME} PRIVATE ${PLF_HIVE_INCLUDE_DIRS})
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::wintoast::wintoast)
endif()
# ---------------------------------------------

# 平台 SDK（系统提供，不进 vcpkg）
# ---------------------------------------------
if(WIN32)
    # CommandLineUtils 用 CommandLineToArgvW 取权威命令行，它属于 shell32
    # （WindowsMinDef.h 定义了 WIN32_LEAN_AND_MEAN，Windows.h 不会再隐式带入）
    target_link_libraries(${PROJECT_NAME} PRIVATE "shell32.lib")
    # SystemInfo 读注册表取系统版本（ProductName / DisplayVersion / UBR）
    target_link_libraries(${PROJECT_NAME} PRIVATE "Advapi32.lib")
endif()
# ---------------------------------------------

# PenMemory 分配器（PenMemory.dll + 导入库）
# ---------------------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE PenMemory)
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