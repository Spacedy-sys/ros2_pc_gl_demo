// 轻量线性代数：仅覆盖本 demo 需要的部分（列主序 4x4，直接对应 GLSL mat4）。
// 不依赖 glm，保证 demo 在无第三方数学库的环境也能一键构建。
#pragma once

#include <cmath>
#include <algorithm>

namespace pcview {

struct Vec3 {
  float x = 0.f, y = 0.f, z = 0.f;
  Vec3() = default;
  Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
  Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
  float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
  float length() const { return std::sqrt(dot(*this)); }
  Vec3 normalized() const {
    const float l = length();
    return l > 1e-8f ? Vec3{x / l, y / l, z / l} : Vec3{0.f, 0.f, 0.f};
  }
};

inline Vec3 cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// 列主序矩阵：m[col * 4 + row]，与 glUniformMatrix4fv(..., GL_FALSE, m) 一致。
struct Mat4 {
  float m[16] = {1.f, 0.f, 0.f, 0.f,
                 0.f, 1.f, 0.f, 0.f,
                 0.f, 0.f, 1.f, 0.f,
                 0.f, 0.f, 0.f, 1.f};

  static Mat4 identity() { return Mat4{}; }

  float& at(int row, int col) { return m[col * 4 + row]; }
  const float& at(int row, int col) const { return m[col * 4 + row]; }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
  Mat4 r;
  for (int c = 0; c < 4; ++c) {
    for (int row = 0; row < 4; ++row) {
      float s = 0.f;
      for (int k = 0; k < 4; ++k) s += a.at(row, k) * b.at(k, c);
      r.at(row, c) = s;
    }
  }
  return r;
}

// 右手系透视投影，深度范围 [-1, 1]（与 GL 默认一致）。
inline Mat4 make_perspective(float fov_y_deg, float aspect, float znear, float zfar) {
  const float f = 1.f / std::tan(fov_y_deg * 0.5f * 3.14159265358979323846f / 180.f);
  Mat4 r;
  for (float& v : r.m) v = 0.f;
  r.at(0, 0) = f / std::max(aspect, 1e-6f);
  r.at(1, 1) = f;
  r.at(2, 2) = (zfar + znear) / (znear - zfar);
  r.at(2, 3) = (2.f * zfar * znear) / (znear - zfar);
  r.at(3, 2) = -1.f;
  return r;
}

inline Mat4 make_look_at(const Vec3& eye, const Vec3& center, const Vec3& up) {
  const Vec3 f = (center - eye).normalized();
  const Vec3 s = cross(f, up).normalized();
  const Vec3 u = cross(s, f);
  Mat4 r;
  r.at(0, 0) = s.x;  r.at(0, 1) = s.y;  r.at(0, 2) = s.z;  r.at(0, 3) = -s.dot(eye);
  r.at(1, 0) = u.x;  r.at(1, 1) = u.y;  r.at(1, 2) = u.z;  r.at(1, 3) = -u.dot(eye);
  r.at(2, 0) = -f.x; r.at(2, 1) = -f.y; r.at(2, 2) = -f.z; r.at(2, 3) = f.dot(eye);
  r.at(3, 0) = 0.f;  r.at(3, 1) = 0.f;  r.at(3, 2) = 0.f;  r.at(3, 3) = 1.f;
  return r;
}

// 把世界坐标投到裁剪空间（用于 HUD 定位 / 调试）。
inline Vec3 transform_point(const Mat4& mat, const Vec3& p) {
  return {mat.at(0, 0) * p.x + mat.at(0, 1) * p.y + mat.at(0, 2) * p.z + mat.at(0, 3),
          mat.at(1, 0) * p.x + mat.at(1, 1) * p.y + mat.at(1, 2) * p.z + mat.at(1, 3),
          mat.at(2, 0) * p.x + mat.at(2, 1) * p.y + mat.at(2, 2) * p.z + mat.at(2, 3)};
}

struct BBox {
  Vec3 min{ 1e30f,  1e30f,  1e30f};
  Vec3 max{-1e30f, -1e30f, -1e30f};
  bool valid() const { return min.x <= max.x; }
  void add(const Vec3& p) {
    min.x = std::min(min.x, p.x); min.y = std::min(min.y, p.y); min.z = std::min(min.z, p.z);
    max.x = std::max(max.x, p.x); max.y = std::max(max.y, p.y); max.z = std::max(max.z, p.z);
  }
  void merge(const BBox& o) {
    if (!o.valid()) return;
    add(o.min);
    add(o.max);
  }
  Vec3 center() const { return (min + max) * 0.5f; }
  Vec3 extent() const { return max - min; }
};

}  // namespace pcview
