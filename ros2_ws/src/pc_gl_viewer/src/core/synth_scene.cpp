#include "pcview/synth_scene.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace pcview {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.f;
}  // namespace

namespace {

// 单条射线与一个图元求交，返回最近的正向 t（未命中返回 -1）。
float intersect(const SyntheticLidarScene::Primitive& p, const Vec3& origin, const Vec3& dir) {
  switch (p.kind) {
    case SyntheticLidarScene::Primitive::Kind::kPlaneZ: {
      if (std::fabs(dir.z) < 1e-6f) return -1.f;
      const float t = (p.a.z - origin.z) / dir.z;
      return t > 0.f ? t : -1.f;
    }
    case SyntheticLidarScene::Primitive::Kind::kBox: {
      float tmin = -std::numeric_limits<float>::infinity();
      float tmax = std::numeric_limits<float>::infinity();
      const float o[3] = {origin.x, origin.y, origin.z};
      const float d[3] = {dir.x, dir.y, dir.z};
      const float mn[3] = {p.a.x, p.a.y, p.a.z};
      const float mx[3] = {p.b.x, p.b.y, p.b.z};
      for (int k = 0; k < 3; ++k) {
        if (std::fabs(d[k]) < 1e-8f) {
          if (o[k] < mn[k] || o[k] > mx[k]) return -1.f;
        } else {
          const float inv = 1.f / d[k];
          float t0 = (mn[k] - o[k]) * inv;
          float t1 = (mx[k] - o[k]) * inv;
          if (t0 > t1) std::swap(t0, t1);
          tmin = std::max(tmin, t0);
          tmax = std::min(tmax, t1);
          if (tmin > tmax) return -1.f;
        }
      }
      if (tmax <= 0.f) return -1.f;
      return tmin > 0.f ? tmin : tmax;
    }
    case SyntheticLidarScene::Primitive::Kind::kVerticalCylinder: {
      const float cx = p.a.x, cy = p.a.y, r = p.b.x, z0 = p.a.z, z1 = p.b.y;
      const float ox = origin.x - cx, oy = origin.y - cy;
      const float A = dir.x * dir.x + dir.y * dir.y;
      if (A < 1e-9f) return -1.f;
      const float B = 2.f * (ox * dir.x + oy * dir.y);
      const float C = ox * ox + oy * oy - r * r;
      const float disc = B * B - 4.f * A * C;
      if (disc < 0.f) return -1.f;
      const float sq = std::sqrt(disc);
      float t0 = (-B - sq) / (2.f * A);
      float t1 = (-B + sq) / (2.f * A);
      if (t0 > t1) std::swap(t0, t1);
      for (float cand : {t0, t1}) {
        if (cand <= 0.f) continue;
        const float zz = origin.z + dir.z * cand;
        if (zz >= z0 && zz <= z1) return cand;
      }
      return -1.f;
    }
  }
  return -1.f;
}

}  // namespace

SyntheticLidarScene::SyntheticLidarScene() {
  auto box = [this](float x0, float y0, float z0, float x1, float y1, float z1, float refl) {
    Primitive p;
    p.kind = Primitive::Kind::kBox;
    p.a = Vec3{x0, y0, z0};
    p.b = Vec3{x1, y1, z1};
    p.reflectivity = refl;
    primitives_.push_back(p);
  };
  auto cylinder = [this](float cx, float cy, float r, float z0, float z1, float refl) {
    Primitive p;
    p.kind = Primitive::Kind::kVerticalCylinder;
    p.a = Vec3{cx, cy, z0};
    p.b = Vec3{r, z1, 0.f};
    p.reflectivity = refl;
    primitives_.push_back(p);
  };

  // 地面：低反射率，配强度着色能看出与墙面/箱体的差别
  Primitive ground;
  ground.kind = Primitive::Kind::kPlaneZ;
  ground.a = Vec3{0.f, 0.f, 0.f};
  ground.reflectivity = 0.22f;
  primitives_.push_back(ground);

  // 四面墙（厚度 0.4 m），比雷达高度高
  const float hx = room_half_x_, hy = room_half_y_, h = room_height_, t = 0.4f;
  box(-hx - t, -hy - t, 0.f, -hx, hy + t, h, 0.60f);
  box(hx, -hy - t, 0.f, hx + t, hy + t, h, 0.62f);
  box(-hx - t, -hy - t, 0.f, hx + t, -hy, h, 0.50f);
  box(-hx - t, hy, 0.f, hx + t, hy + t, h, 0.56f);

  // 室内目标：货箱、设备柜、车辆、两根立柱、台阶
  box(2.0f, 2.6f, 0.0f, 4.2f, 4.8f, 1.25f, 0.72f);
  box(-5.6f, -4.6f, 0.0f, -2.6f, -2.2f, 1.85f, 0.66f);
  box(5.2f, -6.4f, 0.35f, 9.6f, -4.2f, 2.30f, 0.82f);  // 车厢
  box(6.2f, -5.6f, 0.0f, 8.6f, -4.9f, 0.55f, 0.45f);   // 底盘
  cylinder(-8.4f, 5.2f, 0.55f, 0.0f, room_height_, 0.88f);
  cylinder(8.6f, 6.8f, 0.55f, 0.0f, room_height_, 0.88f);
  box(-1.2f, -8.4f, 0.00f, 3.4f, -6.6f, 0.30f, 0.40f);  // 台阶（两级）
  box(-1.2f, -8.4f, 0.30f, 3.4f, -7.4f, 0.60f, 0.46f);
}

SensorPose SyntheticLidarScene::platform_pose(float t_s) const {
  const float omega = 0.22f;  // rad/s
  const float radius = 5.6f;
  const float a = omega * t_s;
  SensorPose pose;
  pose.position = Vec3{radius * std::cos(a), radius * std::sin(a) * 0.75f, 1.15f};
  // 朝向场景中心，yaw 按 REP-103（绕 z 轴，从 x 转向 y）
  pose.yaw_deg = std::atan2(-pose.position.y, -pose.position.x) / kDeg2Rad;
  return pose;
}

float SyntheticLidarScene::raycast(const Vec3& origin, const Vec3& dir, float max_range) const {
  float best = -1.f;
  for (const Primitive& p : primitives_) {
    const float t = intersect(p, origin, dir);
    if (t > 0.f && (best < 0.f || t < best)) best = t;
  }
  if (best > max_range) return -1.f;
  return best;
}

PointCloud SyntheticLidarScene::sweep(const SensorPose& pose, const LidarConfig& cfg, float t_s) const {
  PointCloud cloud;
  const int rings = std::max(1, cfg.rings);
  const float az_step = std::max(0.02f, cfg.az_resolution_deg);
  const int az_count = static_cast<int>(360.f / az_step);

  // 动态目标：一个横向来回移动的箱体，用来体现「每帧点云都在变」。
  // 拷贝一份场景再追加，比在 const 方法里改成员更干净（图元只有十几个，拷贝开销可忽略）。
  //
  // 注意：这个箱体必须始终待在平台轨迹（半径 5.6 / 4.2 的椭圆）**内侧**。
  // 早期版本摆幅 ±6.5 m，会周期性把传感器包进箱体内部，射线全部打在近处内壁上，
  // 整帧点云塌成一小块、地面和墙全被遮挡（表现为"点云不见了"）。
  SyntheticLidarScene scene = *this;
  {
    const float c = std::sin(t_s * 0.9f) * 2.6f;  // |c|+0.9 < 4.2，永远在轨迹内侧
    Primitive mover;
    mover.kind = Primitive::Kind::kBox;
    mover.a = Vec3{c - 0.9f, -0.5f, 0.f};
    mover.b = Vec3{c + 0.9f, 1.1f, 1.5f};
    mover.reflectivity = 0.95f;
    scene.primitives_.push_back(mover);
  }

  cloud.points.reserve(static_cast<std::size_t>(rings) * static_cast<std::size_t>(az_count));
  std::mt19937 rng(cfg.seed);
  std::normal_distribution<float> noise(0.f, cfg.range_sigma);
  std::uniform_real_distribution<float> uni(0.f, 1.f);

  const float yaw = pose.yaw_deg * kDeg2Rad;
  const float cy = std::cos(yaw), sy = std::sin(yaw);

  for (int r = 0; r < rings; ++r) {
    const float v = rings > 1 ? static_cast<float>(r) / static_cast<float>(rings - 1) : 0.f;
    const float elev = (cfg.elev_min_deg + v * (cfg.elev_max_deg - cfg.elev_min_deg)) * kDeg2Rad;
    const float ce = std::cos(elev), se = std::sin(elev);
    for (int a = 0; a < az_count; ++a) {
      const float az = static_cast<float>(a) * az_step * kDeg2Rad;
      // 传感器坐标系 -> 世界坐标系（仅绕 z 轴的 yaw）
      const float lx = ce * std::cos(az);
      const float ly = ce * std::sin(az);
      const Vec3 dir{lx * cy - ly * sy, lx * sy + ly * cy, se};

      if (uni(rng) < cfg.dropout) continue;  // 随机丢包

      // 一次遍历同时拿到最近距离与命中材质
      float best_t = -1.f;
      float refl = 0.3f;
      for (const Primitive& p : scene.primitives_) {
        const float t = intersect(p, pose.position, dir);
        if (t > 0.f && (best_t < 0.f || t < best_t)) {
          best_t = t;
          refl = p.reflectivity;
        }
      }
      if (best_t <= 0.f || best_t > cfg.max_range) continue;

      const float dist = best_t + noise(rng);
      if (dist <= 0.15f) continue;
      const Vec3 pt = pose.position + dir * dist;

      // 强度模型：材质反射率 × 距离衰减 + 少量噪声（接近真实雷达输出的灰度观感）
      const float falloff = 1.f - 0.45f * std::min(1.f, dist / cfg.max_range);
      float intensity = refl * falloff + noise(rng) * 1.5f;
      intensity = std::min(1.f, std::max(0.f, intensity));
      cloud.push(PointXYZI{pt.x, pt.y, pt.z, cfg.include_intensity ? intensity : 0.f});
    }
  }
  return cloud;
}

}  // namespace pcview
