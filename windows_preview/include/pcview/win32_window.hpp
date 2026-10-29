// Windows 平台层（Win32 + WGL）：创建 3.3 Core 上下文、泵消息、把鼠标键盘翻译成 Input、
// 以及 glReadPixels 落盘成 BMP（用于离屏验收截图）。
//
// 说明：ROS2 包里对应的是 glfw_window.cpp（同一套 Input/窗口语义），
// 这样核心渲染代码在「无 ROS 的 Windows 预览」和「Ubuntu + ROS2」下完全一致。
#pragma once

#include <string>
#include <vector>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

#include "pcview/app.hpp"

namespace pcview {

struct Win32WindowConfig {
  int width = 1600;
  int height = 900;
  std::string title = "pc_gl_viewer";
  bool visible = true;
  bool vsync = true;
};

class Win32Window {
 public:
  bool create(const Win32WindowConfig& cfg, std::string* err);
  void destroy();

  // 泵消息；返回 false 表示用户要求退出（关闭窗口 / ESC）。
  bool pump(Input& in);
  void swap_buffers();
  void get_size(int* w, int* h) const;
  void set_title(const std::string& t);

  // 把当前帧缓冲（后缓冲）存成 24bit BMP；返回是否成功。
  bool save_screenshot_bmp(const std::string& path, std::string* err = nullptr);

  bool screenshot_requested() const { return screenshot_requested_; }
  void clear_screenshot_request() { screenshot_requested_ = false; }

  int width() const { return width_; }
  int height() const { return height_; }
  HWND hwnd() const { return hwnd_; }

 private:
  static LRESULT CALLBACK wnd_proc(HWND, UINT, WPARAM, LPARAM);
  void collect_input(Input& in);

  HWND hwnd_ = nullptr;
  HDC hdc_ = nullptr;
  HGLRC glrc_ = nullptr;
  int width_ = 0;
  int height_ = 0;
  bool quit_ = false;
  bool screenshot_requested_ = false;

  // 累积的输入（消息线程写入，pump 里取走）
  float orbit_dx_ = 0.f, orbit_dy_ = 0.f;
  float pan_dx_ = 0.f, pan_dy_ = 0.f;
  float wheel_ = 0.f;
  int preset_ = -1;
  bool next_color_mode_ = false;
  bool bigger_ = false, smaller_ = false;
  bool reset_view_ = false, toggle_grid_ = false, toggle_hud_ = false;
  bool lbutton_down_ = false, rbutton_down_ = false;
  POINT last_mouse_{0, 0};
};

}  // namespace pcview
