#include "pcview/win32_window.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "pcview/gl_api.hpp"
#include "pcview/screenshot.hpp"

namespace pcview {

namespace {

const char* kClassName = "PcGlViewerWindow";
Win32Window* g_active_window = nullptr;

typedef HGLRC(WINAPI* PFN_wglCreateContextAttribsARB)(HDC, HGLRC, const int*);
typedef BOOL(WINAPI* PFN_wglSwapIntervalEXT)(int);

PFN_wglCreateContextAttribsARB load_create_context_attribs() {
  return reinterpret_cast<PFN_wglCreateContextAttribsARB>(wglGetProcAddress("wglCreateContextAttribsARB"));
}

std::string describe_pixel_format(HDC dc) {
  PIXELFORMATDESCRIPTOR pfd{};
  const int pf = GetPixelFormat(dc);
  if (pf == 0 || DescribePixelFormat(dc, pf, sizeof(pfd), &pfd) == 0) return "unknown";
  char buf[128];
  std::snprintf(buf, sizeof(buf), "pf=%d color=%d-%d-%d depth=%d", pf, pfd.cRedBits, pfd.cGreenBits,
                pfd.cBlueBits, pfd.cDepthBits);
  return buf;
}

}  // namespace

bool Win32Window::create(const Win32WindowConfig& cfg, std::string* err) {
  auto fail = [err](const std::string& msg) {
    if (err) *err = msg;
    return false;
  };

  SetProcessDPIAware();

  WNDCLASSA wc{};
  wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = &Win32Window::wnd_proc;
  wc.hInstance = GetModuleHandleA(nullptr);
  wc.hCursor = LoadCursorA(nullptr, reinterpret_cast<LPCSTR>(IDC_ARROW));
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kClassName;
  if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return fail("RegisterClass 失败");
  }

  DWORD style = WS_OVERLAPPEDWINDOW;
  RECT rect{0, 0, cfg.width, cfg.height};
  AdjustWindowRect(&rect, style, FALSE);

  hwnd_ = CreateWindowExA(0, kClassName, cfg.title.c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT,
                          rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr,
                          GetModuleHandleA(nullptr), nullptr);
  if (!hwnd_) return fail("CreateWindow 失败");
  g_active_window = this;

  width_ = cfg.width;
  height_ = cfg.height;
  hdc_ = GetDC(hwnd_);

  PIXELFORMATDESCRIPTOR pfd{};
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cDepthBits = 24;
  pfd.cStencilBits = 8;
  pfd.iLayerType = PFD_MAIN_PLANE;
  const int pf = ChoosePixelFormat(hdc_, &pfd);
  if (pf == 0) return fail("ChoosePixelFormat 失败");
  if (!SetPixelFormat(hdc_, pf, &pfd)) return fail("SetPixelFormat 失败");

  // 先用一个旧式上下文把 wglCreateContextAttribsARB 取出来（Windows 上取 GL 扩展入口的常规做法）
  HGLRC legacy = wglCreateContext(hdc_);
  if (!legacy) return fail("wglCreateContext 失败");
  if (!wglMakeCurrent(hdc_, legacy)) return fail("wglMakeCurrent 失败");

  const auto create_attribs = load_create_context_attribs();
  if (create_attribs) {
    const int attribs[] = {0x2091 /*CONTEXT_MAJOR_VERSION*/, 3, 0x2092 /*MINOR*/, 3,
                           0x9126 /*PROFILE_MASK*/, 0x00000001 /*CORE*/, 0};
    glrc_ = create_attribs(hdc_, nullptr, attribs);
  }
  if (!glrc_) {
    // 装不了 3.3 Core（老驱动/软件渲染）就退回兼容上下文：着色器仍可编译（330 core 需要 3.3，
    // 所以这里如实报错，而不是悄悄降级成黑屏）。
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(legacy);
    return fail("无法创建 OpenGL 3.3 Core 上下文（请更新显卡驱动）。" + describe_pixel_format(hdc_));
  }
  wglMakeCurrent(nullptr, nullptr);
  wglDeleteContext(legacy);
  if (!wglMakeCurrent(hdc_, glrc_)) return fail("wglMakeCurrent(3.3) 失败");

  if (auto swap_interval = reinterpret_cast<PFN_wglSwapIntervalEXT>(wglGetProcAddress("wglSwapIntervalEXT"))) {
    swap_interval(cfg.vsync ? 1 : 0);
  }

  std::string gl_err;
  if (!gl_load(reinterpret_cast<GetProcAddrFn>(wglGetProcAddress), &gl_err)) {
    return fail("OpenGL 入口加载失败: " + gl_err);
  }

  ShowWindow(hwnd_, cfg.visible ? SW_SHOW : SW_HIDE);
  UpdateWindow(hwnd_);
  return true;
}

void Win32Window::destroy() {
  if (glrc_) {
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(glrc_);
    glrc_ = nullptr;
  }
  if (hdc_ && hwnd_) {
    ReleaseDC(hwnd_, hdc_);
    hdc_ = nullptr;
  }
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  if (g_active_window == this) g_active_window = nullptr;
}

void Win32Window::get_size(int* w, int* h) const {
  RECT r{};
  if (hwnd_ && GetClientRect(hwnd_, &r)) {
    *w = r.right - r.left;
    *h = r.bottom - r.top;
  } else {
    *w = width_;
    *h = height_;
  }
}

void Win32Window::set_title(const std::string& t) {
  if (hwnd_) SetWindowTextA(hwnd_, t.c_str());
}

void Win32Window::collect_input(Input& in) {
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

bool Win32Window::pump(Input& in) {
  MSG msg;
  while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) quit_ = true;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }
  get_size(&width_, &height_);
  collect_input(in);
  return !quit_;
}

void Win32Window::swap_buffers() {
  if (hdc_) SwapBuffers(hdc_);
}

LRESULT CALLBACK Win32Window::wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  Win32Window* self = g_active_window;
  switch (msg) {
    case WM_CLOSE:
      if (self) self->quit_ = true;
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    case WM_LBUTTONDOWN:
      if (self) {
        self->lbutton_down_ = true;
        GetCursorPos(&self->last_mouse_);
        SetCapture(hwnd);
      }
      return 0;
    case WM_LBUTTONUP:
      if (self) {
        self->lbutton_down_ = false;
        ReleaseCapture();
      }
      return 0;
    case WM_RBUTTONDOWN:
      if (self) {
        self->rbutton_down_ = true;
        GetCursorPos(&self->last_mouse_);
        SetCapture(hwnd);
      }
      return 0;
    case WM_RBUTTONUP:
      if (self) {
        self->rbutton_down_ = false;
        ReleaseCapture();
      }
      return 0;
    case WM_MOUSEMOVE:
      if (self && (self->lbutton_down_ || self->rbutton_down_)) {
        POINT p;
        GetCursorPos(&p);
        const float dx = static_cast<float>(p.x - self->last_mouse_.x);
        const float dy = static_cast<float>(p.y - self->last_mouse_.y);
        self->last_mouse_ = p;
        if (self->lbutton_down_) {
          self->orbit_dx_ += dx;
          self->orbit_dy_ += -dy;
        } else {
          self->pan_dx_ += dx;
          self->pan_dy_ += dy;
        }
      }
      return 0;
    case WM_MOUSEWHEEL:
      if (self) self->wheel_ += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) / 120.f;
      return 0;
    case WM_KEYDOWN:
      if (self) {
        switch (wp) {
          case VK_ESCAPE: self->quit_ = true; break;
          case '1': self->preset_ = 1; break;
          case '2': self->preset_ = 2; break;
          case '3': self->preset_ = 3; break;
          case 'C': self->next_color_mode_ = true; break;
          case 'R': self->reset_view_ = true; break;
          case 'G': self->toggle_grid_ = true; break;
          case 'H': self->toggle_hud_ = true; break;
          case 'P': self->screenshot_requested_ = true; break;
          case VK_OEM_PLUS:
          case VK_ADD: self->bigger_ = true; break;
          case VK_OEM_MINUS:
          case VK_SUBTRACT: self->smaller_ = true; break;
          default: break;
        }
      }
      return 0;
    default:
      return DefWindowProcA(hwnd, msg, wp, lp);
  }
}

bool Win32Window::save_screenshot_bmp(const std::string& path, std::string* err) {
  int w = 0, h = 0;
  get_size(&w, &h);
  // 具体写 BMP 的实现在 core/screenshot.cpp，和 GLFW 平台层共用同一份
  return save_framebuffer_bmp(path, w, h, err);
}

}  // namespace pcview
