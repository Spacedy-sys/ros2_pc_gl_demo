// 帧缓冲抓图 -> 24bit BMP。两个平台层（Win32 / GLFW）共用，避免各写一份读像素的代码。
// 选 BMP 而不是 PNG 是为了不引入 libpng/stb：demo 的定位是「零第三方依赖」，
// 需要 PNG 时在构建脚本里用一行 Python/Pillow 转即可（tools/bmp2png.py）。
#pragma once

#include <string>

namespace pcview {

// 从当前 GL 读缓冲（双缓冲窗口默认 GL_BACK）抓 w*h 像素写成 BMP。
bool save_framebuffer_bmp(const std::string& path, int width, int height, std::string* err = nullptr);

}  // namespace pcview
