// 发布节点入口：只有 main，节点逻辑在 cloud_publisher_node.cpp 里（便于测试复用）。
#include <memory>

#include "rclcpp/rclcpp.hpp"

#include "pc_gl_viewer/cloud_publisher_node.hpp"

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<pc_gl_viewer::CloudPublisherNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
