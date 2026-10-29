#include "pc_gl_viewer/gl_viewer_node.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "pcview/app.hpp"
#include "pcview/cloud_wire.hpp"
#include "pcview/point_cloud.hpp"
#include "pc_gl_viewer/ros_cloud_adaptor.hpp"

namespace pc_gl_viewer {

GlViewerNode::GlViewerNode() : rclcpp::Node("pc_gl_viewer") {
  topic_ = declare_parameter<std::string>("topic", "points");
  voxel_leaf_ = declare_parameter<double>("voxel_leaf", 0.0);
  color_mode_ = declare_parameter<int>("color_mode", 0);
  point_size_ = declare_parameter<double>("point_size", 2.4);
  width_ = declare_parameter<int>("width", 1600);
  height_ = declare_parameter<int>("height", 900);
  vsync_ = declare_parameter<bool>("vsync", true);

  sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
      topic_, rclcpp::SensorDataQoS(),
      [this](sensor_msgs::msg::PointCloud2::SharedPtr msg) { on_cloud(std::move(msg)); });

  RCLCPP_INFO(get_logger(), "订阅 %s, voxel_leaf=%.2f, color_mode=%d, vsync=%s", topic_.c_str(), voxel_leaf_,
              color_mode_, vsync_ ? "on" : "off");
}

void GlViewerNode::on_cloud(sensor_msgs::msg::PointCloud2::SharedPtr msg) {
  if (!app_) return;

  // 消息 -> core 只读视图（映射代码与回环测试共用，见 ros_cloud_adaptor.hpp）
  std::vector<pcview::PointFieldView> fields;
  const pcview::CloudWireView view = wrap_cloud2(*msg, fields);

  pcview::PointCloud cloud;
  std::string err;
  if (!pcview::decode_point_cloud2(view, cloud, &err)) {
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "点云解码失败: %s", err.c_str());
    return;
  }

  // 传感器位姿这里没有（真实系统里查 TF），交给 app 用 bbox 中心兜底
  app_->submit_cloud(std::move(cloud), false, pcview::Vec3{});
  received_frames_.fetch_add(1);
}

}  // namespace pc_gl_viewer
