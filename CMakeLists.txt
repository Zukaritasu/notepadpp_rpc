cmake_minimum_required(VERSION 3.16)
project(DiscordRPC
  VERSION 1.0
  LANGUAGES CXX RC
  DESCRIPTION "Notepad++ Discord RPC Plugin"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Plugin sources (relative to vstudio/src from project root)
set(PLUGIN_SOURCES
  vstudio/src/DiscordRichPresence.cpp
  vstudio/src/FileFilter.cpp
  vstudio/src/PluginDefinition.cpp
  vstudio/src/PluginConfig.cpp
  vstudio/src/PluginDlgOption.cpp
  vstudio/src/PluginError.cpp
  vstudio/src/PluginDiscord.cpp
  vstudio/src/PluginUtil.cpp
  vstudio/src/TextEditorInfo.cpp
  vstudio/src/LanguageInfo.cpp
)

set(PLUGIN_RESOURCES
  vstudio/src/PluginResources.rc
)

# Optional: build 32-bit on 64-bit host (MinGW: -DCMAKE_CXX_FLAGS="-m32" or use toolchain)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(OUTPUT_ARCH_DIR "bin64")
else()
  set(OUTPUT_ARCH_DIR "bin")
endif()

add_library(DiscordRPC SHARED ${PLUGIN_SOURCES} ${PLUGIN_RESOURCES})
set_target_properties(DiscordRPC PROPERTIES PREFIX "")

target_include_directories(DiscordRPC PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/vstudio/src
)

# Preprocessor definitions (Unicode = _UNICODE/UNICODE; project uses wide chars)
target_compile_definitions(DiscordRPC PRIVATE
  WIN32
  NPP_ENABLE_CONSOLE=0
  _WINDOWS
  _USRDLL
  NPPPLUGINTEMPLATE_EXPORTS
  _CRT_NONSTDC_NO_DEPRECATE
  _CRT_SECURE_NO_WARNINGS
  _CRT_NON_CONFORMING_SWPRINTFS=1
  UNICODE
  _UNICODE
)

if(MSVC)
  target_compile_options(DiscordRPC PRIVATE /W4 /utf-8)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(DiscordRPC PRIVATE /wd4251 /wd4275)
  endif()
else()
  target_compile_options(DiscordRPC PRIVATE -Wall -Wextra)
  if(MINGW OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(DiscordRPC PRIVATE -Wno-maybe-uninitialized)
  endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(DiscordRPC PRIVATE _DEBUG)
  set(OUTPUT_SUBDIR "${OUTPUT_ARCH_DIR}/debug")
else()
  target_compile_definitions(DiscordRPC PRIVATE NDEBUG)
  set(OUTPUT_SUBDIR "${OUTPUT_ARCH_DIR}")
endif()

# Windows system libraries (same as vcxproj)
target_link_libraries(DiscordRPC PRIVATE
  shlwapi
  kernel32
  user32
  gdi32
  winspool
  comdlg32
  comctl32
  advapi32
  shell32
  ole32
  oleaut32
  uuid
  odbc32
  odbccp32
)

# MinGW: link runtime and deps statically so the plugin has no external DLL dependency
# Use vcpkg triplet x64-mingw-static (or x86-mingw-static) so yaml-cpp is also static
if(MINGW OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32))
  target_link_options(DiscordRPC PRIVATE
    -static-libgcc
    -static-libstdc++
  )
  message(STATUS "MinGW: linking libgcc and libstdc++ statically (no runtime DLLs required)")
endif()

# yaml-cpp via VCPKG (no submodule; install with: vcpkg install yaml-cpp)
# Configure: cmake -B build -DCMAKE_TOOLCHAIN_FILE=[path]/vcpkg/scripts/buildsystems/vcpkg.cmake
find_package(yaml-cpp CONFIG REQUIRED)
target_link_libraries(DiscordRPC PRIVATE yaml-cpp::yaml-cpp)

# MinGW: link winpthread statically *after* yaml-cpp so the linker uses .a instead of the DLL
if(MINGW OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32))
  # Must appear after yaml-cpp so linker picks static libwinpthread.a
  target_link_libraries(DiscordRPC PRIVATE -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic)
  message(STATUS "MinGW: linking winpthread statically (no libwinpthread-1.dll required)")
endif()

# Output: build/bin or build/bin64 (and build/bin/debug, build/bin64/debug for Debug)
set_target_properties(DiscordRPC PROPERTIES
  OUTPUT_NAME "DiscordRPC"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${OUTPUT_SUBDIR}"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${OUTPUT_SUBDIR}"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${OUTPUT_SUBDIR}"
)

# Single-config generators (Makefiles, Ninja): respect CMAKE_BUILD_TYPE
# Multi-config (VS, Xcode): all configs get their own output dir
foreach(CONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  string(TOLOWER ${CONFIG} CONFIG_LOWER)
  if(CONFIG_LOWER STREQUAL "debug")
    set(CONFIG_OUT "${OUTPUT_ARCH_DIR}/debug")
  else()
    set(CONFIG_OUT "${OUTPUT_ARCH_DIR}")
  endif()
  set_target_properties(DiscordRPC PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}/${CONFIG_OUT}"
    LIBRARY_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}/${CONFIG_OUT}"
    ARCHIVE_OUTPUT_DIRECTORY_${CONFIG} "${CMAKE_BINARY_DIR}/${CONFIG_OUT}"
  )
endforeach()

message(STATUS "DiscordRPC plugin: output directory ${OUTPUT_SUBDIR} (${CMAKE_SIZEOF_VOID_P}-bit)")
