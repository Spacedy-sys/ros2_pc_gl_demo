#include "pc_gl_viewer/cloud_publisher_node.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/msg/point_field.hpp"

#include "pcview/cloud_wire.hpp"
#include "pcview/point_cloud.hpp"

namespace pc_gl_viewer {

CloudPublisherNode::CloudPublisherNode() : rclcpp::Node("pc_cloud_publisher") {
  frame_id_ = declare_parameter<std::string>("frame_id", "lidar_link");
  const double rate_hz = declare_parameter<double>("rate_hz", 10.0);

  lidar_.rings = declare_parameter<int>("rings", 32);
  lidar_.az_resolution_deg = static_cast<float>(declare_parameter<double>("az_resolution_deg", 0.25));
  lidar_.elev_min_deg = static_cast<float>(declare_parameter<double>("elev_min_deg", -25.0));
  lidar_.elev_max_deg = static_cast<float>(declare_parameter<double>("elev_max_deg", 16.0));
  lidar_.max_range = static_cast<float>(declare_parameter<double>("max_range", 30.0));
  lidar_.dropout = static_cast<float>(declare_parameter<double>("dropout", 0.02));
  lidar_.range_sigma = static_cast<float>(declare_parameter<double>("range_sigma", 0.012));

  // SensorDataQoS = best_effort + 小队列：点云是"最新值优先"的数据，丢旧帧比堆积旧帧更好
  publisher_ = create_publisher<sensor_msgs::msg::PointCloud2>("points", rclcpp::SensorDataQoS());

  const auto period = std::chrono::duration<double>(1.0 / std::max(0.5, rate_hz));
  timer_ = create_wall_timer(period, [this]() { publish_once(); });

  RCLCPP_INFO(get_logger(), "发布 %s 帧率 %.1f Hz, %d 线 x %.2f deg, 量程 %.1f m", "points", rate_hz,
              lidar_.rings, static_cast<double>(lidar_.az_resolution_deg),
              static_cast<double>(lidar_.max_range));
}

void CloudPublisherNode::publish_once() {
  // 用真实时间推进场景（而不是"每次回调 +0.1s"），这样即使定时器抖动，
  // 平台绕场运动的角速度也和墙钟一致。
  const rclcpp::Time now_time = now();
  if (last_call_.nanoseconds() == 0) last_call_ = now_time;
  sim_time_ += (now_time - last_call_).seconds();
  last_call_ = now_time;

  const pcview::SensorPose pose = scene_.platform_pose(static_cast<float>(sim_time_));
  const pcview::PointCloud cloud = scene_.sweep(pose, lidar_, static_cast<float>(sim_time_));

  // 复用 core 里的编码器：x,y,z,intensity 全 FLOAT32、point_step=16、无行填充
  const pcview::EncodedCloud2 enc = pcview::encode_point_cloud2(cloud);

  sensor_msgs::msg::PointCloud2 msg;
  msg.header.stamp = now_time;
  msg.header.frame_id = frame_id_;
  msg.height = enc.height;
  msg.width = enc.width;
  msg.fields.resize(enc.fields.size());
  for (std::size_t i = 0; i < enc.fields.size(); ++i) {
    msg.fields[i].name = enc.fields[i].name;
    msg.fields[i].offset = enc.fields[i].offset;
    msg.fields[i].datatype = sensor_msgs::msg::PointField::FLOAT32;
    msg.fields[i].count = enc.fields[i].count;
  }
  msg.is_bigendian = false;
  msg.point_step = enc.point_step;
  msg.row_step = enc.row_step;
  msg.data = enc.data;
  msg.is_dense = true;  // 无效点已经在仿真阶段剔除

  publisher_->publish(msg);
  ++published_frames_;

  // ---- 实测频率/带宽（注意：构造函数里那行日志打印的是「配置值」，这里是「实测值」）----
  // 用墙钟的 sim_time_ 做统计窗口，避免上报"参数写的是 10 Hz"这种无效证据。
  ++window_frames_;
  window_bytes_ += static_cast<double>(msg.data.size());
  const double window_dt = sim_time_ - window_start_s_;
  if (window_dt >= 2.0) {
    const double hz = static_cast<double>(window_frames_) / window_dt;
    const double mbps = window_bytes_ / window_dt / (1024.0 * 1024.0);
    RCLCPP_INFO(get_logger(), "[实测] 发布 %.2f Hz | %.2f MB/s | 最近一帧 %u 点", hz, mbps, msg.width);
    window_start_s_ = sim_time_;
    window_frames_ = 0;
    window_bytes_ = 0.0;
  }
}

}  // namespace pc_gl_viewer
