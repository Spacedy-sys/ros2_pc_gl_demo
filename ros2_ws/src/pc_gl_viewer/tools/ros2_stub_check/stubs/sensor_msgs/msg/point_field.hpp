#pragma once
// 桩：sensor_msgs/msg/point_field.hpp
#include <cstdint>
#include <string>

namespace sensor_msgs {
namespace msg {

struct PointField {
  // 与真实消息一致的常量表（真实实现里是 static constexpr uint8_t）
  static constexpr std::uint8_t INT8 = 1;
  static constexpr std::uint8_t UINT8 = 2;
  static constexpr std::uint8_t INT16 = 3;
  static constexpr std::uint8_t UINT16 = 4;
  static constexpr std::uint8_t INT32 = 5;
  static constexpr std::uint8_t UINT32 = 6;
  static constexpr std::uint8_t FLOAT32 = 7;
  static constexpr std::uint8_t FLOAT64 = 8;

  std::string name;
  std::uint32_t offset = 0;
  std::uint8_t datatype = 0;
  std::uint32_t count = 0;
};

}  // namespace msg
}  // namespace sensor_msgs
