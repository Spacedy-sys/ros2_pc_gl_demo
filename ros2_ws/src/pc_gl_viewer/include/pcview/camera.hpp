// 轨道相机（Orbit）：目标点 + 球面角 + 距离，适合点云巡视。
#pragma once

#include "pcview/math3d.hpp"

namespace pcview {

class OrbitCamera {
 public:
  OrbitCamera();

  void set_aspect(float aspect) { aspect_ = aspect; }
  float aspect() const { return aspect_; }

  // 交互：dx/dy 为像素增量，wheel 为滚轮刻度。
  void orbit(float dx, float dy);
  void pan(float dx, float dy);
  void dolly(float wheel_ticks);
  void reset();

  // 预设视角：0=斜视, 1=俯视, 2=侧视, 3=第一人称/贴地。
  void apply_preset(int preset);
  // 直接指定球面角与距离（命令行 --yaw/--pitch/--dist 用，便于复现同一张构图）
  void set_orbit(float yaw_deg, float pitch_deg, float distance);
  // 根据点云包围盒自动取景（订阅到第一帧时调用）。
  void frame(const BBox& box);

  Vec3 eye() const;
  Vec3 target() const { return target_; }
  float distance() const { return distance_; }
  float yaw_deg() const { return yaw_deg_; }
  float pitch_deg() const { return pitch_deg_; }
  Mat4 view_matrix() const;
  Mat4 proj_matrix() const;
  Mat4 view_proj() const { return proj_matrix() * view_matrix(); }

 private:
  void clamp();

  Vec3 target_{0.f, 0.f, 1.2f};
  float yaw_deg_ = 35.f;
  float pitch_deg_ = 28.f;
  float distance_ = 34.f;
  float fov_deg_ = 45.f;
  float znear_ = 0.05f;
  float zfar_ = 400.f;
  float aspect_ = 16.f / 9.f;
};

}  // namespace pcview
