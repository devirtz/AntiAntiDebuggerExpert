cmake_minimum_required(VERSION 3.15)
project(antidebugger)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(X64DBG_DOWNLOAD_SDK "Auto-download x64dbg plugin SDK if missing" ON)

set(X64DBG_SDK_URL "https://github.com/x64dbg/x64dbg/releases/download/2025.08.19/x64dbg-pluginsdk.zip")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This plugin targets x64 only. Configure with -DCMAKE_GENERATOR_PLATFORM=x64")
endif()

set(DEPS "${CMAKE_SOURCE_DIR}/deps")
file(MAKE_DIRECTORY "${DEPS}")

if(NOT EXISTS "${DEPS}/bridgemain.h")
    if(NOT X64DBG_DOWNLOAD_SDK)
        message(FATAL_ERROR "x64dbg SDK missing. Set -DX64DBG_DOWNLOAD_SDK=ON or extract into deps/")
    endif()
    set(_zip "${CMAKE_BINARY_DIR}/x64dbg-pluginsdk.zip")
    file(DOWNLOAD "${X64DBG_SDK_URL}" "${_zip}" SHOW_PROGRESS STATUS _dl)
    if(NOT _dl EQUAL 0)
        message(FATAL_ERROR "SDK download failed")
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${_zip}" WORKING_DIRECTORY "${DEPS}")
endif()

file(GLOB_RECURSE X64DBG_LIBS CONFIGURE_DEPENDS "${DEPS}/*_x64.lib" "${DEPS}/x64*.lib")

set(SOURCES src/antidebug.cpp src/Simulator.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".dp64")

target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN64)

target_include_directories(${PROJECT_NAME} PRIVATE ${DEPS})

find_path(UNICORN_INCLUDE_DIR unicorn/unicorn.h)
find_library(UNICORN_LIB NAMES unicorn)
if(NOT UNICORN_INCLUDE_DIR OR NOT UNICORN_LIB)
    message(FATAL_ERROR "Unicorn not found. Pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake")
endif()
target_include_directories(${PROJECT_NAME} PRIVATE "${UNICORN_INCLUDE_DIR}")
target_link_libraries(${PROJECT_NAME} PRIVATE "${UNICORN_LIB}")

target_link_directories(${PROJECT_NAME} PRIVATE ${DEPS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${X64DBG_LIBS})

if(MSVC)
    target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)
endif()
