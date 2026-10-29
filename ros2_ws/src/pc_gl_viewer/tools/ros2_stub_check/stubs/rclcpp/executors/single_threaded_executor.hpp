#pragma once
// 桩：rclcpp/executors/single_threaded_executor.hpp
#include <memory>

#include "rclcpp/rclcpp.hpp"

namespace rclcpp {
namespace executors {

class SingleThreadedExecutor {
 public:
  void add_node(std::shared_ptr<Node> /*node*/) {}
  void spin() {}
  void cancel() {}
};

}  // namespace executors
}  // namespace rclcpp
