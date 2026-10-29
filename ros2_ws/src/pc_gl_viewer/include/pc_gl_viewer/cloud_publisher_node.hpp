// /points 发布节点的类声明（与 main 分离，方便无 ROS 环境下的回环测试直接实例化）。
#pragma once

#include <cstdint>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"

#include "pcview/synth_scene.hpp"

namespace pc_gl_viewer {

class CloudPublisherNode : public rclcpp::Node {
 public:
  CloudPublisherNode();

  std::uint64_t published_frames() const { return published_frames_; }

 private:
  void publish_once();

  std::string frame_id_;
  pcview::LidarConfig lidar_;
  pcview::SyntheticLidarScene scene_;
  double sim_time_ = 0.0;
  rclcpp::Time last_call_{0, 0, RCL_ROS_TIME};
  std::uint64_t published_frames_ = 0;
  // 实测发布频率（不是参数值）：每统计窗口结束后打印一次
  double window_start_s_ = 0.0;
  std::uint64_t window_frames_ = 0;
  double window_bytes_ = 0.0;
  rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace pc_gl_viewer
