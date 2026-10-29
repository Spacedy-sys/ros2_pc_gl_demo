#pragma once
// 桩：std_msgs/msg/header.hpp
#include <string>

#include "rclcpp/rclcpp.hpp"

namespace std_msgs {
namespace msg {

struct Header {
  rclcpp::Time stamp;
  std::string frame_id;
};

}  // namespace msg
}  // namespace std_msgs
