// 合成一个「32 线激光雷达 + 室内场景」的数据源，用于在没有真实雷达/ROS 环境时也能产出
// 结构真实（环状扫描线、遮挡、强度按材质、丢包、测距噪声）的点云。
// 场景 = 地面 + 四面墙 + 若干箱体/立柱/卡车；传感器沿圆周运动并指向场景中心，
// 因此每帧的遮挡关系与点数都会变化，能直观演示「流式点云」的刷新。
#pragma once

#include <cstdint>
#include <vector>

#include "pcview/point_cloud.hpp"

namespace pcview {

struct LidarConfig {
  int rings = 32;                    // 线数
  float elev_min_deg = -25.f;        // 垂直视场下界
  float elev_max_deg = 16.f;         // 垂直视场上界
  float az_resolution_deg = 0.25f;   // 水平角分辨率
  float max_range = 30.f;            // 最大测距
  float range_sigma = 0.012f;        // 测距高斯噪声
  float dropout = 0.02f;             // 随机丢包率
  std::uint32_t seed = 20241024u;    // 每帧随机种子（丢包/噪声）
  bool include_intensity = true;
};

struct SensorPose {
  Vec3 position{0.f, 0.f, 1.1f};
  float yaw_deg = 0.f;
};

class SyntheticLidarScene {
 public:
  // 场景图元。放在 public 是为了让 .cpp 里的自由函数（射线求交）能直接用它，
  // 也避免 std::vector<不完整类型> 在头文件里被析构时踩到未定义行为。
  // 外部代码不需要关心这个类型。
  struct Primitive {
    enum class Kind { kPlaneZ, kBox, kVerticalCylinder };
    Kind kind = Kind::kBox;
    float reflectivity = 0.5f;
    // kPlaneZ: 平面 z = a.z
    // kBox: a = min, b = max
    // kVerticalCylinder: a = (cx, cy, zmin), b = (radius, zmax, _)
    Vec3 a{0.f, 0.f, 0.f};
    Vec3 b{0.f, 0.f, 0.f};
  };

  SyntheticLidarScene();

  // 平台位姿：绕场景中心做匀速圆周运动（t 单位秒）。
  SensorPose platform_pose(float t_s) const;

  // 用一个完整的扫描周期生成点云（世界坐标系，z 轴向上，与 ROS REP-103 一致）。
  // t_s 用于驱动场景里的动态目标（移动的人员/车辆），便于演示「流式点云」。
  PointCloud sweep(const SensorPose& pose, const LidarConfig& cfg, float t_s = 0.f) const;

  // 自检用：单条射线的求交结果（命中距离），未命中返回 -1。
  float raycast(const Vec3& origin, const Vec3& dir, float max_range) const;

 private:
  std::vector<Primitive> primitives_;
  float room_half_x_ = 13.f;
  float room_half_y_ = 9.5f;
  float room_height_ = 3.6f;
};

}  // namespace pcview
