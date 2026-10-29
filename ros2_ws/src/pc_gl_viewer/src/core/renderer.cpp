#include "pcview/renderer.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>

#include "shaders_common.hpp"

namespace pcview {

namespace {

const char* kPointVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in float aIntensity;

uniform mat4  uViewProj;
uniform float uZmin;
uniform float uZmax;
uniform float uImin;
uniform float uImax;
uniform float uPointSize;
uniform vec3  uSensorPos;
uniform int   uMode;        // 0=强度 1=高度 2=距离

out float vT;
out float vFog;

void main() {
  vec4 clip = uViewProj * vec4(aPos, 1.0);
  gl_Position = clip;

  float t;
  if (uMode == 0) {
    t = (aIntensity - uImin) / max(uImax - uImin, 1e-4);
  } else if (uMode == 1) {
    t = (aPos.z - uZmin) / max(uZmax - uZmin, 1e-4);
  } else {
    t = length(aPos - uSensorPos) / 30.0;
  }
  vT = clamp(t, 0.0, 1.0);

  // 透视衰减：远处的点变小，避免密集区糊成一片
  float w = max(clip.w, 0.05);
  gl_PointSize = clamp(uPointSize * (34.0 / w), 1.0, 26.0);
  vFog = clamp((w - 25.0) / 90.0, 0.0, 0.55);
}
)GLSL";

// 注意：片元着色器的 #version 必须是最前面一行，所以这里只放 varying/输出声明，
// #version 与调色板函数在 build_point_program() 里按顺序拼上去。
const char* kPointFS = R"GLSL(
in float vT;
in float vFog;
out vec4 FragColor;
)GLSL";

const char* kPointFSBody = R"GLSL(
void main() {
  // 圆形点精灵：方形点位在密集的点云里显得很"糊"
  vec2 d = gl_PointCoord * 2.0 - 1.0;
  if (dot(d, d) > 1.0) discard;

  vec3 c = turbo(vT);
  // 轻微的高光，让点云有体积感（居中位置更亮）
  c *= mix(0.82, 1.12, 1.0 - clamp(length(d), 0.0, 1.0));
  c = mix(c, vec3(0.035, 0.045, 0.065), vFog);
  FragColor = vec4(c, 1.0);
}
)GLSL";

const char* kLineVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uViewProj;
out vec3 vColor;
out float vFogW;
void main() {
  vec4 clip = uViewProj * vec4(aPos, 1.0);
  gl_Position = clip;
  vColor = aColor;
  vFogW = clip.w;
}
)GLSL";

const char* kLineFS = R"GLSL(
#version 330 core
in vec3 vColor;
in float vFogW;
out vec4 FragColor;
void main() {
  float fog = clamp((vFogW - 30.0) / 110.0, 0.0, 0.7);
  FragColor = vec4(mix(vColor, vec3(0.035, 0.045, 0.065), fog), 1.0);
}
)GLSL";

}  // namespace

bool Renderer::init(std::string* err) {
  if (!build_point_program(err)) return false;
  if (!build_line_program(err)) return false;
  build_grid();
  if (!hud_.init(err)) return false;

  glGenVertexArrays(1, &point_vao_);
  glGenBuffers(1, &point_vbo_);
  glBindVertexArray(point_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, point_vbo_);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PointXYZI),
                        reinterpret_cast<const void*>(offsetof(PointXYZI, x)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(PointXYZI),
                        reinterpret_cast<const void*>(offsetof(PointXYZI, intensity)));
  glBindVertexArray(0);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_PROGRAM_POINT_SIZE);
  ok_ = true;
  return true;
}

bool Renderer::build_point_program(std::string* err) {
  // 顺序：#version -> 调色板函数 -> varying 声明 -> main
  const std::string fs = std::string("#version 330 core\n") + shaders::kTurboGLSL + kPointFS + kPointFSBody;
  point_prog_ = compile_program(kPointVS, fs.c_str(), err);
  if (!point_prog_) return false;
  point_u_view_proj_ = glGetUniformLocation(point_prog_, "uViewProj");
  point_u_zmin_ = glGetUniformLocation(point_prog_, "uZmin");
  point_u_zmax_ = glGetUniformLocation(point_prog_, "uZmax");
  point_u_imin_ = glGetUniformLocation(point_prog_, "uImin");
  point_u_imax_ = glGetUniformLocation(point_prog_, "uImax");
  point_u_mode_ = glGetUniformLocation(point_prog_, "uMode");
  point_u_size_ = glGetUniformLocation(point_prog_, "uPointSize");
  point_u_sensor_ = glGetUniformLocation(point_prog_, "uSensorPos");
  return true;
}

bool Renderer::build_line_program(std::string* err) {
  line_prog_ = compile_program(kLineVS, kLineFS, err);
  if (!line_prog_) return false;
  line_u_view_proj_ = glGetUniformLocation(line_prog_, "uViewProj");
  return true;
}

void Renderer::build_grid() {
  // 1 m 次网格 + 5 m 主网格 + 三根彩色坐标轴，全部烘成一个静态 VBO。
  struct V { float x, y, z, r, g, b; };
  std::vector<V> v;
  const float z = -0.012f;  // 略微下沉，避免和地面点 z-fighting
  const float ext = 16.f;
  auto line = [&v, z](float x0, float y0, float x1, float y1, float r, float g, float b) {
    v.push_back(V{x0, y0, z, r, g, b});
    v.push_back(V{x1, y1, z, r, g, b});
  };
  for (int i = -static_cast<int>(ext); i <= static_cast<int>(ext); ++i) {
    const bool major = (i % 5 == 0);
    const float c = major ? 0.42f : 0.21f;
    const float cc = major ? 0.46f : 0.23f;
    line(static_cast<float>(i), -ext, static_cast<float>(i), ext, c, cc, c * 1.25f);
    line(-ext, static_cast<float>(i), ext, static_cast<float>(i), c, cc, c * 1.25f);
  }
  // 坐标轴：X 红、Y 绿、Z 蓝
  v.push_back(V{0.f, 0.f, z, 0.88f, 0.32f, 0.32f});
  v.push_back(V{6.f, 0.f, z, 0.88f, 0.32f, 0.32f});
  v.push_back(V{0.f, 0.f, z, 0.36f, 0.82f, 0.42f});
  v.push_back(V{0.f, 6.f, z, 0.36f, 0.82f, 0.42f});
  v.push_back(V{0.f, 0.f, 0.f, 0.36f, 0.55f, 0.95f});
  v.push_back(V{0.f, 0.f, 3.f, 0.36f, 0.55f, 0.95f});

  glGenVertexArrays(1, &line_vao_);
  glGenBuffers(1, &line_vbo_);
  glBindVertexArray(line_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, line_vbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(V)), v.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), reinterpret_cast<const void*>(0));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), reinterpret_cast<const void*>(sizeof(float) * 3));
  glBindVertexArray(0);
  line_vertex_count_ = static_cast<GLsizei>(v.size());
}

void Renderer::upload(const PointCloud& cloud) {
  const auto t0 = std::chrono::steady_clock::now();
  glBindBuffer(GL_ARRAY_BUFFER, point_vbo_);
  const GLsizeiptr bytes = static_cast<GLsizeiptr>(cloud.points.size() * sizeof(PointXYZI));

  if (static_cast<std::size_t>(bytes) > point_capacity_) {
    // 增长：一次性申请更大存储（也顺手起到"孤儿化"作用，旧存储由驱动延迟回收）
    const GLsizeiptr cap = bytes + bytes / 2;
    glBufferData(GL_ARRAY_BUFFER, cap, nullptr, GL_DYNAMIC_DRAW);
    point_capacity_ = cap;
  } else {
    // 关键：先 orphan（传 nullptr）再 subdata。若直接 glBufferData 传数据，驱动可能因为
    // 上一帧还在用同一块存储而插入隐式同步，表现为"周期性顿一下"。
    glBufferData(GL_ARRAY_BUFFER, point_capacity_, nullptr, GL_DYNAMIC_DRAW);
  }
  glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, cloud.points.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  const auto t1 = std::chrono::steady_clock::now();
  stats_.upload_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  stats_.points = cloud.points.size();
  bbox_ = cloud.bbox;
}

void Renderer::render(const OrbitCamera& cam, int fb_w, int fb_h, const RenderOptions& opt,
                      const std::vector<std::string>& hud_lines) {
  if (!ok_) return;
  const auto t0 = std::chrono::steady_clock::now();

  glViewport(0, 0, fb_w, fb_h);
  glClearColor(0.035f, 0.045f, 0.065f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);

  const Mat4 vp = cam.view_proj();

  if (opt.show_grid && line_vertex_count_ > 0) {
    glUseProgram(line_prog_);
    glUniformMatrix4fv(line_u_view_proj_, 1, GL_FALSE, vp.m);
    glBindVertexArray(line_vao_);
    glDrawArrays(GL_LINES, 0, line_vertex_count_);
    glBindVertexArray(0);
  }

  if (stats_.points > 0) {
    glUseProgram(point_prog_);
    glUniformMatrix4fv(point_u_view_proj_, 1, GL_FALSE, vp.m);
    glUniform1f(point_u_zmin_, bbox_.valid() ? bbox_.min.z : 0.f);
    glUniform1f(point_u_zmax_, bbox_.valid() ? bbox_.max.z : 1.f);
    glUniform1f(point_u_imin_, 0.f);
    glUniform1f(point_u_imax_, 1.f);
    glUniform1i(point_u_mode_, static_cast<int>(opt.color_mode));
    glUniform1f(point_u_size_, opt.point_size);
    glUniform3f(point_u_sensor_, sensor_pos_.x, sensor_pos_.y, sensor_pos_.z);
    glBindVertexArray(point_vao_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(stats_.points));
    glBindVertexArray(0);
  }

  if (opt.show_hud && !hud_lines.empty()) {
    glDisable(GL_DEPTH_TEST);
    hud_.draw(fb_w, fb_h, hud_lines, opt.hud_scale);
    if (opt.color_mode != ColorMode::kRange) {
      hud_.draw_color_legend(fb_w, fb_h, 24.f, static_cast<float>(fb_h) - 30.f * opt.hud_scale,
                             220.f * opt.hud_scale * 0.5f, 8.f * opt.hud_scale * 0.5f, opt.hud_scale);
    }
    glEnable(GL_DEPTH_TEST);
  }

  const auto t1 = std::chrono::steady_clock::now();
  stats_.draw_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
}

void Renderer::shutdown() {
  if (point_vbo_) glDeleteBuffers(1, &point_vbo_);
  if (point_vao_) glDeleteVertexArrays(1, &point_vao_);
  if (line_vbo_) glDeleteBuffers(1, &line_vbo_);
  if (line_vao_) glDeleteVertexArrays(1, &line_vao_);
  if (point_prog_) glDeleteProgram(point_prog_);
  if (line_prog_) glDeleteProgram(line_prog_);
  point_vbo_ = point_vao_ = line_vbo_ = line_vao_ = point_prog_ = line_prog_ = 0;
  ok_ = false;
}

}  // namespace pcview
