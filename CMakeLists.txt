cmake_minimum_required(VERSION 4.0)

project("PenFramework")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

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

file(GLOB_RECURSE SRC ${CMAKE_SOURCE_DIR}/Code/Native/*)

add_executable(${PROJECT_NAME} ${SRC})

# Visual Studio Nativs 支持
#if(CMAKE_BUILD_TYPE STREQUAL "Debug")
#    message(STATUS "Nativs Visualizer ready.")
#    target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/Visualizers/PenFramework.natvis")
#endif()