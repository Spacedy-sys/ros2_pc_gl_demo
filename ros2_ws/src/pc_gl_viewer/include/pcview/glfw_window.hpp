// Linux / ROS2 平台层（GLFW 3.3 Core + WGL 等价物）。
// 与 win32_window 保持同一套语义：create / pump(Input&) / swap_buffers / get_size /
// save_screenshot_bmp，因此 ViewerApp 之上的主循环两边几乎一字不差。
#pragma once

#include <string>

#include "pcview/app.hpp"

struct GLFWwindow;  // 前向声明，头文件里不引入 glfw3.h

namespace pcview {

struct GlfwWindowConfig {
  int width = 1600;
  int height = 900;
  std::string title = "pc_gl_viewer";
  bool vsync = true;
};

class GlfwWindow {
 public:
  bool create(const GlfwWindowConfig& cfg, std::string* err);
  void destroy();

  // 泵事件；返回 false 表示用户要求退出（关窗 / ESC）。
  bool pump(Input& in);
  void swap_buffers();
  void get_size(int* w, int* h) const;

  bool save_screenshot_bmp(const std::string& path, std::string* err = nullptr);
  bool screenshot_requested() const { return screenshot_requested_; }
  void clear_screenshot_request() { screenshot_requested_ = false; }

  GLFWwindow* handle() const { return window_; }

 private:
  static void on_mouse_button(GLFWwindow* w, int button, int action, int mods);
  static void on_cursor_pos(GLFWwindow* w, double xpos, double ypos);
  static void on_scroll(GLFWwindow* w, double dx, double dy);
  static void on_key(GLFWwindow* w, int key, int scancode, int action, int mods);
  void collect_input(Input& in);

  GLFWwindow* window_ = nullptr;
  bool quit_ = false;
  bool screenshot_requested_ = false;

  float orbit_dx_ = 0.f, orbit_dy_ = 0.f;
  float pan_dx_ = 0.f, pan_dy_ = 0.f;
  float wheel_ = 0.f;
  int preset_ = -1;
  bool next_color_mode_ = false;
  bool bigger_ = false, smaller_ = false;
  bool reset_view_ = false, toggle_grid_ = false, toggle_hud_ = false;
  bool lbutton_down_ = false, rbutton_down_ = false;
  double last_x_ = 0.0, last_y_ = 0.0;
};

}  // namespace pcview
