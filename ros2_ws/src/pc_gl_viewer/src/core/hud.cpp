#include "pcview/hud.hpp"

#include <algorithm>
#include <cstddef>

#include "pcview/hud_font_atlas.hpp"
#include "shaders_common.hpp"

namespace pcview {

namespace {

const char* kHudVS = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPx;
layout(location = 1) in vec2 aUV;
uniform vec2 uViewport;
out vec2 vUV;
void main() {
  vec2 ndc = vec2(aPx.x / uViewport.x * 2.0 - 1.0, 1.0 - aPx.y / uViewport.y * 2.0);
  gl_Position = vec4(ndc, 0.0, 1.0);
  vUV = aUV;
}
)GLSL";

std::string hud_fragment_shader() {
  return std::string(R"GLSL(
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec4 uColor;
uniform int uMode;   // 0=文字 1=纯色块 2=turbo 渐变条
out vec4 FragColor;
)GLSL") + shaders::kTurboGLSL + R"GLSL(
void main() {
  if (uMode == 0) {
    float a = texture(uTex, vUV).r;
    FragColor = vec4(uColor.rgb, uColor.a * a);
  } else if (uMode == 1) {
    FragColor = uColor;
  } else {
    FragColor = vec4(turbo(vUV.x), uColor.a);
  }
}
)GLSL";
}

}  // namespace

bool HudRenderer::init(std::string* err) {
  prog_ = compile_program(kHudVS, hud_fragment_shader().c_str(), err);
  if (!prog_) return false;
  u_viewport_ = glGetUniformLocation(prog_, "uViewport");
  u_tex_ = glGetUniformLocation(prog_, "uTex");
  u_color_ = glGetUniformLocation(prog_, "uColor");
  u_mode_ = glGetUniformLocation(prog_, "uMode");

  glGenTextures(1, &tex_);
  glBindTexture(GL_TEXTURE_2D, tex_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, hud_font::kAtlasW, hud_font::kAtlasH, 0, GL_RED, GL_UNSIGNED_BYTE,
               hud_font::kAtlas);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<const void*>(0));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
                        reinterpret_cast<const void*>(sizeof(float) * 2));
  glBindVertexArray(0);
  return true;
}

void HudRenderer::begin(int fb_w, int fb_h) {
  fb_w_ = fb_w;
  fb_h_ = fb_h;
  verts_.clear();
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(prog_);
  glUniform2f(u_viewport_, static_cast<float>(fb_w), static_cast<float>(fb_h));
  glUniform1i(u_tex_, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
}

void HudRenderer::push_quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1) {
  // 两个三角形（不用索引缓冲，HUD 顶点量很小）
  const float v[24] = {x0, y0, u0, v0, x1, y0, u1, v0, x1, y1, u1, v1,
                       x0, y0, u0, v0, x1, y1, u1, v1, x0, y1, u0, v1};
  verts_.insert(verts_.end(), v, v + 24);
}

void HudRenderer::flush(int mode) {
  if (verts_.empty()) return;
  glUniform1i(u_mode_, mode);
  const GLsizeiptr bytes = static_cast<GLsizeiptr>(verts_.size() * sizeof(float));
  // 容量不够就重新分配，够就原地覆盖（避免每帧 realloc）
  if (verts_.size() > vbo_capacity_) {
    glBufferData(GL_ARRAY_BUFFER, bytes, verts_.data(), GL_DYNAMIC_DRAW);
    vbo_capacity_ = verts_.size();
  } else {
    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, verts_.data());
  }
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts_.size() / 4));
  verts_.clear();
}

void HudRenderer::draw(int fb_w, int fb_h, const std::vector<std::string>& lines, float scale) {
  if (!prog_ || lines.empty()) return;
  begin(fb_w, fb_h);

  const float cell_w = hud_font::kCellW * scale;
  const float cell_h = hud_font::kCellH * scale;
  const float margin = 10.f * scale;
  std::size_t max_len = 0;
  for (const auto& l : lines) max_len = std::max(max_len, l.size());

  // 半透明底板，保证任何背景上文字都可读
  const float panel_w = static_cast<float>(max_len) * cell_w + margin * 1.6f;
  const float panel_h = static_cast<float>(lines.size()) * cell_h + margin * 1.1f;
  glUniform4f(u_color_, 0.02f, 0.03f, 0.045f, 0.62f);
  push_quad(margin * 0.6f, margin * 0.6f, margin * 0.6f + panel_w, margin * 0.6f + panel_h, 0, 0, 0, 0);
  flush(1);

  glUniform4f(u_color_, 0.80f, 0.89f, 1.0f, 1.0f);
  float y = margin;
  for (const auto& line : lines) {
    float x = margin;
    for (char ch : line) {
      int idx = static_cast<int>(static_cast<unsigned char>(ch)) - hud_font::kFirstChar;
      if (idx >= 0 && idx <= hud_font::kLastChar - hud_font::kFirstChar) {
        const int cx = idx % hud_font::kCols;
        const int cy = idx / hud_font::kCols;
        const float u0 = static_cast<float>(cx * hud_font::kCellW) / hud_font::kAtlasW;
        const float v0 = static_cast<float>(cy * hud_font::kCellH) / hud_font::kAtlasH;
        const float u1 = static_cast<float>((cx + 1) * hud_font::kCellW) / hud_font::kAtlasW;
        const float v1 = static_cast<float>((cy + 1) * hud_font::kCellH) / hud_font::kAtlasH;
        push_quad(x, y, x + cell_w, y + cell_h, u0, v0, u1, v1);
      }
      x += cell_w;
    }
    y += cell_h;
  }
  flush(0);

  glDisable(GL_BLEND);
  glBindVertexArray(0);
}

void HudRenderer::draw_color_legend(int fb_w, int fb_h, float x_px, float y_px, float w_px, float h_px,
                                    float scale) {
  if (!prog_) return;
  begin(fb_w, fb_h);
  glUniform4f(u_color_, 0.85f, 0.9f, 1.0f, 1.0f);
  push_quad(x_px, y_px, x_px + w_px, y_px + h_px, 0.f, 0.f, 1.f, 1.f);
  flush(2);
  // 图例两端加个 1px 的暗边，看起来更像工具
  glUniform4f(u_color_, 0.02f, 0.03f, 0.045f, 0.9f);
  push_quad(x_px - 2.f, y_px - 2.f, x_px + w_px + 2.f, y_px, 0, 0, 0, 0);
  push_quad(x_px - 2.f, y_px + h_px, x_px + w_px + 2.f, y_px + h_px + 2.f, 0, 0, 0, 0);
  flush(1);
  glDisable(GL_BLEND);
  glBindVertexArray(0);
  (void)scale;
}

}  // namespace pcview
