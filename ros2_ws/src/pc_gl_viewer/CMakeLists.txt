cmake_minimum_required(VERSION 3.8)
project(pc_gl_viewer)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

# GLVND 优先：Ubuntu 20.04 的 CMake 会提示 "OpenGL_GL_PREFERENCE has not been set"，
# 显式指定后构建的 stderr 就干净了（不影响功能，只是消除歧义）。
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

# GLFW 走 pkg-config（Ubuntu: sudo apt install libglfw3-dev），比硬找 cmake config 稳
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED IMPORTED_TARGET glfw3)

# ---------------------------------------------------------------------------
# 核心渲染库：不含任何 ROS 依赖，所以它既能被 ROS 节点用，也能被 windows_preview 用
# ---------------------------------------------------------------------------
add_library(pcview_core STATIC
  src/core/gl_api.cpp
  src/core/screenshot.cpp
  src/core/point_cloud.cpp
  src/core/cloud_wire.cpp
  src/core/camera.cpp
  src/core/synth_scene.cpp
  src/core/renderer.cpp
  src/core/hud.cpp
  src/core/app.cpp
)
target_include_directories(pcview_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(pcview_core PUBLIC OpenGL::GL)
# ---------------------------------------------------------------------------
# 发布节点：合成雷达 -> /points
# 节点类与 main 分开，这样 tools/ros2_stub_check 的回环测试可以直接实例化节点类
# ---------------------------------------------------------------------------
add_executable(pc_cloud_publisher
  src/ros/pc_cloud_publisher_main.cpp
  src/ros/cloud_publisher_node.cpp
)
target_link_libraries(pc_cloud_publisher pcview_core)
ament_target_dependencies(pc_cloud_publisher rclcpp sensor_msgs)

# ---------------------------------------------------------------------------
# 查看器节点：订阅 PointCloud2 -> OpenGL 显示
# ---------------------------------------------------------------------------
add_executable(pc_gl_viewer
  src/ros/pc_gl_viewer_main.cpp
  src/ros/gl_viewer_node.cpp
  src/platform/glfw_window.cpp
)
target_link_libraries(pc_gl_viewer pcview_core PkgConfig::GLFW3)
ament_target_dependencies(pc_gl_viewer rclcpp sensor_msgs)

# ---------------------------------------------------------------------------
# 频率/带宽探针：C++ 订阅端计数器。用来给 `ros2 topic hz` 的低读数做对照证据
# （Python 版 CLI 在 0.65 MB/帧的点云上会被反序列化拖慢）。
# ---------------------------------------------------------------------------
add_executable(pc_rate_probe src/ros/rate_probe_main.cpp)
target_link_libraries(pc_rate_probe)
ament_target_dependencies(pc_rate_probe rclcpp sensor_msgs)

install(TARGETS pc_cloud_publisher pc_gl_viewer pc_rate_probe DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

ament_package()
