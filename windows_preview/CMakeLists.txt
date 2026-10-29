cmake_minimum_required(VERSION 3.16)
project(pc_gl_demo_preview LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Windows 本地验收程序：不依赖 ROS，直接复用 ros2_ws 里 pc_gl_viewer 包的核心源码
# （src/core/*）。这样「离屏截图能跑通」就等于「ROS2 节点里的渲染代码能跑通」。
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
  message(FATAL_ERROR "这个目标只用于 Windows 本地验收；ROS2 请走 ros2_ws（colcon build）。")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

get_filename_component(PC_GL_PKG "${CMAKE_CURRENT_SOURCE_DIR}/../ros2_ws/src/pc_gl_viewer" ABSOLUTE)

file(GLOB PC_GL_CORE_SOURCES CONFIGURE_DEPENDS "${PC_GL_PKG}/src/core/*.cpp")
if(NOT PC_GL_CORE_SOURCES)
  message(FATAL_ERROR "没找到核心源码: ${PC_GL_PKG}/src/core/*.cpp")
endif()

add_executable(pc_gl_viewer_preview
  src/main_win32.cpp
  src/win32_window.cpp
  ${PC_GL_CORE_SOURCES}
)

target_include_directories(pc_gl_viewer_preview PRIVATE
  "${PC_GL_PKG}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(pc_gl_viewer_preview PRIVATE opengl32 gdi32 user32)

if(MSVC)
  # /utf-8: 源码与执行字符集都是 UTF-8（源码里有中文注释和中文日志）
  # /wd4819: MSVC 在部分非交互环境下仍会对 UTF-8 中文报 C4819（源文件编码告警），
  #          已确认源码是 UTF-8 + BOM，这里只关掉这条噪声告警。
  target_compile_options(pc_gl_viewer_preview PRIVATE /W3 /utf-8 /permissive- /wd4819)
  target_compile_definitions(pc_gl_viewer_preview PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

message(STATUS "core sources: ${PC_GL_CORE_SOURCES}")
