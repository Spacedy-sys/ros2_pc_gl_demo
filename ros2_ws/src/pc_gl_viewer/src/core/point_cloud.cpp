#include "pcview/point_cloud.hpp"

#include <chrono>
#include <cmath>
#include <vector>

namespace pcview {

void PointCloud::clear() {
  points.clear();
  bbox = BBox{};
  intensity_min = 0.f;
  intensity_max = 1.f;
}

void PointCloud::push(const PointXYZI& p) {
  points.push_back(p);
  bbox.add(Vec3{p.x, p.y, p.z});
  // 增量维护强度范围，避免每帧再扫一遍。
  if (points.size() == 1) {
    intensity_min = intensity_max = p.intensity;
  } else {
    if (p.intensity < intensity_min) intensity_min = p.intensity;
    if (p.intensity > intensity_max) intensity_max = p.intensity;
  }
}

void PointCloud::assign(const float* xyzi, std::size_t count) {
  clear();
  points.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    PointXYZI p{xyzi[i * 4 + 0], xyzi[i * 4 + 1], xyzi[i * 4 + 2], xyzi[i * 4 + 3]};
    points[i] = p;
    bbox.add(Vec3{p.x, p.y, p.z});
    if (i == 0) {
      intensity_min = intensity_max = p.intensity;
    } else {
      if (p.intensity < intensity_min) intensity_min = p.intensity;
      if (p.intensity > intensity_max) intensity_max = p.intensity;
    }
  }
}

void PointCloud::update_stats() {
  bbox = BBox{};
  intensity_min = intensity_max = 0.f;
  bool first = true;
  for (const auto& p : points) {
    bbox.add(Vec3{p.x, p.y, p.z});
    if (first) {
      intensity_min = intensity_max = p.intensity;
      first = false;
    } else {
      if (p.intensity < intensity_min) intensity_min = p.intensity;
      if (p.intensity > intensity_max) intensity_max = p.intensity;
    }
  }
}

// ---------------------------------------------------------------------------
// 体素降采样
// ---------------------------------------------------------------------------
namespace {

struct VoxelAccum {
  float sx = 0.f, sy = 0.f, sz = 0.f, si = 0.f;
  std::uint32_t count = 0;
};

// 21bit/轴 打包成 63bit 体素键：±1e6 个体素范围内精确无碰撞，同时可直接做哈希比较。
inline std::uint64_t voxel_key(std::int32_t x, std::int32_t y, std::int32_t z) {
  const std::uint64_t ux = static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) & 0x1FFFFFull;
  const std::uint64_t uy = static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) & 0x1FFFFFull;
  const std::uint64_t uz = static_cast<std::uint64_t>(static_cast<std::uint32_t>(z)) & 0x1FFFFFull;
  return (ux << 42) | (uy << 21) | uz;
}

// FNV-1a 混合，让体素键在哈希表里分散开（注意：只用于定位，不用于相等判断）。
inline std::uint64_t hash_key(std::uint64_t k) {
  k ^= k >> 33;
  k *= 0xff51afd7ed558ccdull;
  k ^= k >> 33;
  k *= 0xc4ceb9fe1a85ec53ull;
  k ^= k >> 33;
  return k;
}

}  // namespace

PointCloud voxel_grid_downsample(const PointCloud& in, float leaf_size, VoxelGridResult* result) {
  const auto t0 = std::chrono::steady_clock::now();
  PointCloud out;
  if (result) result->input_points = in.points.size();

  if (leaf_size <= 0.f || in.points.empty()) {
    out = in;
    if (result) {
      result->output_points = out.points.size();
      result->occupied_voxels = out.points.size();
      result->elapsed_ms = 0.0;
    }
    return out;
  }

  const float inv_leaf = 1.f / leaf_size;
  // 容量取 2 的幂，负载因子控制在 ~0.5。
  std::size_t capacity = 16;
  while (capacity < in.points.size() * 2) capacity <<= 1;

  struct Slot {
    std::uint64_t key = 0;
    bool used = false;
    VoxelAccum acc;
  };
  std::vector<Slot> table(capacity);

  std::vector<std::int32_t> keys;  // 用于恢复顺序无关的输出（按首次出现顺序）
  keys.reserve(in.points.size());

  for (const auto& p : in.points) {
    const std::int32_t ix = static_cast<std::int32_t>(std::floor(p.x * inv_leaf));
    const std::int32_t iy = static_cast<std::int32_t>(std::floor(p.y * inv_leaf));
    const std::int32_t iz = static_cast<std::int32_t>(std::floor(p.z * inv_leaf));
    const std::uint64_t key = voxel_key(ix, iy, iz);
    std::size_t idx = static_cast<std::size_t>(hash_key(key)) & (capacity - 1);
    bool inserted = false;
    for (std::size_t probe = 0; probe < capacity; ++probe) {
      Slot& s = table[idx];
      if (!s.used) {
        s.used = true;
        s.key = key;
        s.acc.count = 1;
        s.acc.sx = p.x;
        s.acc.sy = p.y;
        s.acc.sz = p.z;
        s.acc.si = p.intensity;
        keys.push_back(static_cast<std::int32_t>(idx));
        inserted = true;
        break;
      }
      if (s.key == key) {
        s.acc.count++;
        s.acc.sx += p.x;
        s.acc.sy += p.y;
        s.acc.sz += p.z;
        s.acc.si += p.intensity;
        inserted = true;
        break;
      }
      idx = (idx + 1) & (capacity - 1);
    }
    if (!inserted) {  // 理论上不会发生，保险起见退化为单点输出
      out.push(PointXYZI{p.x, p.y, p.z, p.intensity});
    }
  }

  out.points.reserve(keys.size());
  for (std::int32_t k : keys) {
    const Slot& s = table[static_cast<std::size_t>(k)];
    const float inv_n = 1.f / static_cast<float>(s.acc.count);
    out.push(PointXYZI{s.acc.sx * inv_n, s.acc.sy * inv_n, s.acc.sz * inv_n, s.acc.si * inv_n});
  }

  if (result) {
    result->output_points = out.points.size();
    result->occupied_voxels = out.points.size();
    const auto t1 = std::chrono::steady_clock::now();
    result->elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  }
  return out;
}

PointCloud crop_box(const PointCloud& in, const Vec3& mn, const Vec3& mx) {
  PointCloud out;
  out.points.reserve(in.points.size());
  for (const auto& p : in.points) {
    if (p.x >= mn.x && p.x <= mx.x && p.y >= mn.y && p.y <= mx.y && p.z >= mn.z && p.z <= mx.z) {
      out.push(p);
    }
  }
  return out;
}

}  // namespace pcview
