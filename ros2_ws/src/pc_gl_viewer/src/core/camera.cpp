#include "pcview/camera.hpp"

#include <algorithm>
#include <cmath>

namespace pcview {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.f;
}  // namespace

OrbitCamera::OrbitCamera() { reset(); }

void OrbitCamera::reset() {
  target_ = Vec3{0.f, 0.f, 1.2f};
  yaw_deg_ = 35.f;
  pitch_deg_ = 26.f;
  distance_ = 34.f;
}

void OrbitCamera::clamp() {
  pitch_deg_ = std::max(-89.f, std::min(89.f, pitch_deg_));
  distance_ = std::max(0.8f, std::min(260.f, distance_));
}

void OrbitCamera::orbit(float dx, float dy) {
  yaw_deg_ += dx * 0.28f;
  pitch_deg_ += dy * 0.28f;
  if (yaw_deg_ > 360.f) yaw_deg_ -= 360.f;
  if (yaw_deg_ < -360.f) yaw_deg_ += 360.f;
  clamp();
}

void OrbitCamera::pan(float dx, float dy) {
  // 沿相机的右/上方向平移目标点，速度随距离缩放，手感一致。
  const float yaw = yaw_deg_ * kDeg2Rad;
  const float pitch = pitch_deg_ * kDeg2Rad;
  const Vec3 fwd{std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};
  const Vec3 right = cross(fwd, Vec3{0.f, 0.f, 1.f}).normalized();
  const Vec3 up = cross(right, fwd);
  const float s = distance_ * 0.0016f;
  target_ += right * (-dx * s) + up * (dy * s);
}

void OrbitCamera::dolly(float wheel_ticks) {
  distance_ *= std::pow(0.88f, wheel_ticks);
  clamp();
}

void OrbitCamera::apply_preset(int preset) {
  switch (preset) {
    case 1:  // 俯视
      pitch_deg_ = 78.f;
      yaw_deg_ = 90.f;
      distance_ = 40.f;
      break;
    case 2:  // 侧视
      pitch_deg_ = 8.f;
      yaw_deg_ = 0.f;
      distance_ = 30.f;
      break;
    case 3:  // 贴地/第一人称
      pitch_deg_ = 4.f;
      yaw_deg_ = 210.f;
      distance_ = 11.f;
      target_ = Vec3{0.f, 0.f, 1.1f};
      break;
    default:
      reset();
      break;
  }
  clamp();
}

void OrbitCamera::set_orbit(float yaw_deg, float pitch_deg, float distance) {
  yaw_deg_ = yaw_deg;
  pitch_deg_ = pitch_deg;
  distance_ = distance;
  clamp();
}

void OrbitCamera::frame(const BBox& box) {
  if (!box.valid()) return;
  const Vec3 c = box.center();
  const Vec3 e = box.extent();
  target_ = Vec3{c.x, c.y, c.z};
  const float radius = std::max(2.f, 0.5f * std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z));
  distance_ = std::max(6.f, radius * 2.1f);
  zfar_ = std::max(400.f, distance_ * 12.f);
  clamp();
}

Vec3 OrbitCamera::eye() const {
  const float yaw = yaw_deg_ * kDeg2Rad;
  const float pitch = pitch_deg_ * kDeg2Rad;
  const Vec3 dir{std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};
  return target_ + dir * distance_;
}

Mat4 OrbitCamera::view_matrix() const { return make_look_at(eye(), target_, Vec3{0.f, 0.f, 1.f}); }

Mat4 OrbitCamera::proj_matrix() const {
  return make_perspective(fov_deg_, aspect_, znear_, std::max(zfar_, distance_ * 8.f));
}

}  // namespace pcview
