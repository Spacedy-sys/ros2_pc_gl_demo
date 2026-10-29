#include "pcview/glfw_window.hpp"

#include <GLFW/glfw3.h>

#include <cstdio>

#include "pcview/gl_api.hpp"
#include "pcview/screenshot.hpp"

namespace pcview {

namespace {
GlfwWindow* from(GLFWwindow* w) {
  return static_cast<GlfwWindow*>(glfwGetWindowUserPointer(w));
}
}  // namespace

bool GlfwWindow::create(const GlfwWindowConfig& cfg, std::string* err) {
  auto fail = [err](const std::string& msg) {
    if (err) *err = msg;
    return false;
  };

  if (!glfwInit()) return fail("glfwInit 失败（没有可用的显示？WSL 里需要 WSLg 或 X 转发）");

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);  // macOS 需要，Linux 无害

  window_ = glfwCreateWindow(cfg.width, cfg.height, cfg.title.c_str(), nullptr, nullptr);
  if (!window_) {
    glfwTerminate();
    return fail("glfwCreateWindow 失败：无法创建 OpenGL 3.3 Core 上下文（请检查显卡驱动/远程桌面）");
  }

  glfwMakeContextCurrent(window_);
  glfwSwapInterval(cfg.vsync ? 1 : 0);
  glfwSetWindowUserPointer(window_, this);
  glfwSetMouseButtonCallback(window_, &GlfwWindow::on_mouse_button);
  glfwSetCursorPosCallback(window_, &GlfwWindow::on_cursor_pos);
  glfwSetScrollCallback(window_, &GlfwWindow::on_scroll);
  glfwSetKeyCallback(window_, &GlfwWindow::on_key);

  std::string gl_err;
  // GLFW 的 glfwGetProcAddress 就是 Linux 上的 glXGetProcAddress 包装，签名正好对上
  if (!gl_load(reinterpret_cast<GetProcAddrFn>(glfwGetProcAddress), &gl_err)) {
    glfwDestroyWindow(window_);
    glfwTerminate();
    window_ = nullptr;
    return fail("OpenGL 入口加载失败: " + gl_err);
  }
  return true;
}

void GlfwWindow::destroy() {
  if (window_) {
    glfwDestroyWindow(window_);
    window_ = nullptr;
  }
  glfwTerminate();
}

bool GlfwWindow::pump(Input& in) {
  glfwPollEvents();
  collect_input(in);
  if (window_ && glfwWindowShouldClose(window_)) quit_ = true;
  return !quit_;
}

void GlfwWindow::swap_buffers() {
  if (window_) glfwSwapBuffers(window_);
}

void GlfwWindow::get_size(int* w, int* h) const {
  int fb_w = 0, fb_h = 0;
  if (window_) glfwGetFramebufferSize(window_, &fb_w, &fb_h);
  *w = fb_w;
  *h = fb_h;
}

void GlfwWindow::collect_input(Input& in) {
  in.orbit_dx = orbit_dx_;
  in.orbit_dy = orbit_dy_;
  in.pan_dx = pan_dx_;
  in.pan_dy = pan_dy_;
  in.wheel = wheel_;
  in.preset = preset_;
  in.next_color_mode = next_color_mode_;
  in.bigger = bigger_;
  in.smaller = smaller_;
  in.reset_view = reset_view_;
  in.toggle_grid = toggle_grid_;
  in.toggle_hud = toggle_hud_;
  orbit_dx_ = orbit_dy_ = pan_dx_ = pan_dy_ = wheel_ = 0.f;
  preset_ = -1;
  next_color_mode_ = bigger_ = smaller_ = false;
  reset_view_ = toggle_grid_ = toggle_hud_ = false;
}

void GlfwWindow::on_mouse_button(GLFWwindow* w, int button, int action, int /*mods*/) {
  GlfwWindow* self = from(w);
  if (!self) return;
  const bool down = (action == GLFW_PRESS);
  if (button == GLFW_MOUSE_BUTTON_LEFT) self->lbutton_down_ = down;
  if (button == GLFW_MOUSE_BUTTON_RIGHT) self->rbutton_down_ = down;
  if (down) glfwGetCursorPos(w, &self->last_x_, &self->last_y_);
}

void GlfwWindow::on_cursor_pos(GLFWwindow* w, double xpos, double ypos) {
  GlfwWindow* self = from(w);
  if (!self || (!self->lbutton_down_ && !self->rbutton_down_)) return;
  const float dx = static_cast<float>(xpos - self->last_x_);
  const float dy = static_cast<float>(ypos - self->last_y_);
  self->last_x_ = xpos;
  self->last_y_ = ypos;
  if (self->lbutton_down_) {
    self->orbit_dx_ += dx;
    self->orbit_dy_ += -dy;  // 屏幕 y 向下，轨道相机 pitch 向上 → 取反
  } else {
    self->pan_dx_ += dx;
    self->pan_dy_ += dy;
  }
}

void GlfwWindow::on_scroll(GLFWwindow* w, double dx, double dy) {
  GlfwWindow* self = from(w);
  if (!self) return;
  (void)dx;
  self->wheel_ += static_cast<float>(dy);
}

void GlfwWindow::on_key(GLFWwindow* w, int key, int /*scancode*/, int action, int /*mods*/) {
  GlfwWindow* self = from(w);
  if (!self || action != GLFW_PRESS) return;
  switch (key) {
    case GLFW_KEY_ESCAPE: self->quit_ = true; break;
    case GLFW_KEY_1: self->preset_ = 1; break;
    case GLFW_KEY_2: self->preset_ = 2; break;
    case GLFW_KEY_3: self->preset_ = 3; break;
    case GLFW_KEY_C: self->next_color_mode_ = true; break;
    case GLFW_KEY_R: self->reset_view_ = true; break;
    case GLFW_KEY_G: self->toggle_grid_ = true; break;
    case GLFW_KEY_H: self->toggle_hud_ = true; break;
    case GLFW_KEY_P: self->screenshot_requested_ = true; break;
    case GLFW_KEY_EQUAL:
    case GLFW_KEY_KP_ADD: self->bigger_ = true; break;
    case GLFW_KEY_MINUS:
    case GLFW_KEY_KP_SUBTRACT: self->smaller_ = true; break;
    default: break;
  }
}

bool GlfwWindow::save_screenshot_bmp(const std::string& path, std::string* err) {
  int w = 0, h = 0;
  get_size(&w, &h);
  return save_framebuffer_bmp(path, w, h, err);
}

}  // namespace pcview
