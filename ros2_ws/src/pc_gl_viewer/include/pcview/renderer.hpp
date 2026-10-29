// 点云 / 网格 / HUD 的 OpenGL 3.3 Core 渲染器。
// 设计目标是「一次性上传、多次绘制」：点云只在订阅到新帧时更新 VBO，颜色映射放在 GLSL 里，
// 因此切换强度着色/高度着色/距离着色不需要重传顶点数据（这点在讲性能时很好用）。
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "pcview/camera.hpp"
#include "pcview/colormap.hpp"
#include "pcview/gl_api.hpp"
#include "pcview/hud.hpp"
#include "pcview/point_cloud.hpp"

namespace pcview {

struct RenderOptions {
  ColorMode color_mode = ColorMode::kIntensity;
  float point_size = 2.4f;   // 像素基准大小（近距离会被放大）
  bool show_grid = true;
  bool show_axes = true;
  bool show_hud = true;
  float hud_scale = 2.f;
};

struct RenderStats {
  std::size_t points = 0;     // 当前 VBO 中的点数
  double upload_ms = 0.0;     // 最近一次 CPU->GPU 上传耗时
  double draw_ms = 0.0;       // 最近一帧的绘制耗时
};

class Renderer {
 public:
  bool init(std::string* err);
  void shutdown();

  // 上传一整帧点云（GL_STATIC_DRAW），并记录耗时。
  void upload(const PointCloud& cloud);

  // 传感器位置仅用于「距离」着色模式，以及 HUD 上的方位信息。
  void set_sensor_position(const Vec3& p) { sensor_pos_ = p; }

  void render(const OrbitCamera& cam, int fb_w, int fb_h, const RenderOptions& opt,
              const std::vector<std::string>& hud_lines);

  const RenderStats& stats() const { return stats_; }
  const BBox& bbox() const { return bbox_; }

 private:
  struct Program {
    GLuint id = 0;
    GLint view_proj = -1;
  };

  bool build_point_program(std::string* err);
  bool build_line_program(std::string* err);
  void build_grid();

  GLuint point_prog_ = 0;
  GLint point_u_view_proj_ = -1, point_u_zmin_ = -1, point_u_zmax_ = -1, point_u_imin_ = -1,
        point_u_imax_ = -1, point_u_mode_ = -1, point_u_size_ = -1, point_u_sensor_ = -1,
        point_u_far_ = -1;
  GLuint point_vao_ = 0, point_vbo_ = 0;
  std::size_t point_capacity_ = 0;

  GLuint line_prog_ = 0;
  GLint line_u_view_proj_ = -1;
  GLuint line_vao_ = 0, line_vbo_ = 0;
  GLsizei line_vertex_count_ = 0;

  RenderStats stats_;
  BBox bbox_;
  Vec3 sensor_pos_{0.f, 0.f, 1.f};
  HudRenderer hud_;
  bool ok_ = false;
};

}  // namespace pcview
