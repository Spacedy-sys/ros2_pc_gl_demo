// 极简 HUD：位图字体图集 + 屏幕空间 quad，用来显示点数/FPS/着色模式/视角与颜色图例。
// 不引入 freetype/imgui，代价是字体固定（由 tools/gen_font_atlas.py 预生成）。
#pragma once

#include <string>
#include <vector>

#include "pcview/gl_api.hpp"

namespace pcview {

class HudRenderer {
 public:
  bool init(std::string* err);
  void draw(int fb_w, int fb_h, const std::vector<std::string>& lines, float scale);

  // 颜色图例（turbo 渐变条）
  void draw_color_legend(int fb_w, int fb_h, float x_px, float y_px, float w_px, float h_px, float scale);

 private:
  void begin(int fb_w, int fb_h);
  void flush(int mode);
  void push_quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1);

  GLuint prog_ = 0;
  GLint u_viewport_ = -1, u_tex_ = -1, u_color_ = -1, u_mode_ = -1;
  GLuint tex_ = 0;
  GLuint vao_ = 0, vbo_ = 0;
  std::size_t vbo_capacity_ = 0;
  std::vector<float> verts_;
  int fb_w_ = 0, fb_h_ = 0;
};

}  // namespace pcview
