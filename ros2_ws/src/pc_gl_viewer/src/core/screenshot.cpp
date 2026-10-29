#include "pcview/screenshot.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "pcview/gl_api.hpp"

namespace pcview {

bool save_framebuffer_bmp(const std::string& path, int width, int height, std::string* err) {
  if (width <= 0 || height <= 0) {
    if (err) *err = "抓图尺寸非法";
    return false;
  }

  std::vector<unsigned char> rgb(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());

  const int row_raw = width * 3;
  const int row_padded = (row_raw + 3) & ~3;  // BMP 每行按 4 字节对齐
  const std::uint32_t image_size = static_cast<std::uint32_t>(row_padded) * static_cast<std::uint32_t>(height);
  const std::uint32_t file_size = 54 + image_size;

  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) {
    if (err) *err = "无法写入 " + path;
    return false;
  }

  unsigned char header[54] = {0};
  header[0] = 'B';
  header[1] = 'M';
  std::memcpy(&header[2], &file_size, 4);
  const std::uint32_t offset = 54;
  std::memcpy(&header[10], &offset, 4);
  const std::uint32_t dib = 40;
  std::memcpy(&header[14], &dib, 4);
  const std::int32_t iw = width, ih = height;
  std::memcpy(&header[18], &iw, 4);
  std::memcpy(&header[22], &ih, 4);
  const std::uint16_t planes = 1, bpp = 24;
  std::memcpy(&header[26], &planes, 2);
  std::memcpy(&header[28], &bpp, 2);
  std::memcpy(&header[34], &image_size, 4);
  std::fwrite(header, 1, 54, f);

  std::vector<unsigned char> row(static_cast<std::size_t>(row_padded), 0);
  // glReadPixels 的行序是「下到上」，正好和 BMP 一致，不用翻转。
  for (int y = 0; y < height; ++y) {
    const unsigned char* src = rgb.data() + static_cast<std::size_t>(y) * row_raw;
    for (int x = 0; x < width; ++x) {
      row[x * 3 + 0] = src[x * 3 + 2];  // B
      row[x * 3 + 1] = src[x * 3 + 1];  // G
      row[x * 3 + 2] = src[x * 3 + 0];  // R
    }
    std::fwrite(row.data(), 1, static_cast<std::size_t>(row_padded), f);
  }
  std::fclose(f);
  if (err) err->clear();
  return true;
}

}  // namespace pcview
