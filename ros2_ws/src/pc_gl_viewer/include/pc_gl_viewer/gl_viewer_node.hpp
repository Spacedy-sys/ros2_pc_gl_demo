// 订阅 PointCloud2 并用 OpenGL 显示的节点（类声明与 main 分离，便于测试）。
#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"

namespace pcview {
class ViewerApp;  // 只持有指针，头文件里不引入整个渲染栈
}

namespace pc_gl_viewer {

class GlViewerNode : public rclcpp::Node {
 public:
  GlViewerNode();

  // 渲染器准备好之后再把指针交给回调（避免第一帧竞态）
  void attach(pcview::ViewerApp* app) { app_ = app; }

  const std::string& topic() const { return topic_; }
  double voxel_leaf() const { return voxel_leaf_; }
  int color_mode() const { return color_mode_; }
  double point_size() const { return point_size_; }
  int width() const { return width_; }
  int height() const { return height_; }
  bool vsync() const { return vsync_; }
  std::uint64_t received_frames() const { return received_frames_.load(); }

 private:
  void on_cloud(sensor_msgs::msg::PointCloud2::SharedPtr msg);

  std::string topic_;
  double voxel_leaf_ = 0.0;
  int color_mode_ = 0;
  double point_size_ = 2.4;
  int width_ = 1600;
  int height_ = 900;
  bool vsync_ = true;
  pcview::ViewerApp* app_ = nullptr;
  std::atomic<std::uint64_t> received_frames_{0};
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_;
};

}  // namespace pc_gl_viewer
