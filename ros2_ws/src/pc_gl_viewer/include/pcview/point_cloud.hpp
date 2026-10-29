// 点云数据结构：本 demo 统一使用 {x, y, z, intensity} 单精度四元组（与 PointCloud2 的
// FLOAT32 x/y/z/intensity 布局一致），便于零拷贝式的存取与直接上传 GPU。
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "pcview/math3d.hpp"

namespace pcview {

struct PointXYZI {
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  float intensity = 0.f;
};

struct PointCloud {
  std::vector<PointXYZI> points;
  BBox bbox;
  float intensity_min = 0.f;
  float intensity_max = 1.f;

  void clear();
  void push(const PointXYZI& p);
  // 数据可能来自外部（已是 float4 连续数组），这里直接拷入。
  void assign(const float* xyzi, std::size_t count);
  void update_stats();  // 重算 bbox 与强度范围
  std::size_t size() const { return points.size(); }
  bool empty() const { return points.empty(); }
  std::size_t memory_bytes() const { return points.size() * sizeof(PointXYZI); }
};

// 体素网格降采样（VoxelGrid），参考 PCL 的做法：按体素为单位取质心/强度均值。
// 加速结构用 open addressing 哈希（线性探测），避免引入 unordered_map 的开销。
struct VoxelGridResult {
  std::size_t input_points = 0;
  std::size_t output_points = 0;
  std::size_t occupied_voxels = 0;
  double elapsed_ms = 0.0;
  double reduction_ratio() const {
    return input_points ? 100.0 * static_cast<double>(output_points) / static_cast<double>(input_points)
                        : 0.0;
  }
};

// leaf_size <= 0 时返回输入副本，result 里 elapsed 照常计时。
PointCloud voxel_grid_downsample(const PointCloud& in, float leaf_size, VoxelGridResult* result = nullptr);

// 仅保留 bbox 内的点（用来演示 clip / ROI 过滤）。
PointCloud crop_box(const PointCloud& in, const Vec3& mn, const Vec3& mx);

}  // namespace pcview
