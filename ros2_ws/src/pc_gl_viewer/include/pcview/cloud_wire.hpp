// PointCloud2 线格式的编解码，故意做成「不依赖 ROS」的纯函数：
//   - ROS 节点里负责把 sensor_msgs::msg::PointCloud2 的字段拼成 CloudWireView 传进来；
//   - 单元自检里可以用 encode -> decode 往返验证；
//   - 好处是这部分解析逻辑可以在没有 ROS 环境的机器上编译、跑测试（本 demo 的 Windows 预览器
//     就是用同一份代码做 --selftest 的）。
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "pcview/point_cloud.hpp"

namespace pcview {

// 与 sensor_msgs/PointField 的 datatype 常量一致。
enum PointFieldType : std::uint8_t {
  kInt8 = 1,
  kUint8 = 2,
  kInt16 = 3,
  kUint16 = 4,
  kInt32 = 5,
  kUint32 = 6,
  kFloat32 = 7,
  kFloat64 = 8,
};

struct PointFieldView {
  const char* name = nullptr;
  std::uint32_t offset = 0;
  std::uint8_t datatype = kFloat32;
  std::uint32_t count = 1;
};

// PointCloud2 的「只读视图」，字段布局由 ROS 消息填充，这里不持有内存。
struct CloudWireView {
  const std::uint8_t* data = nullptr;
  std::uint32_t width = 0;
  std::uint32_t height = 1;
  std::uint32_t point_step = 0;
  std::uint32_t row_step = 0;
  bool is_bigendian = false;
  const PointFieldView* fields = nullptr;
  std::uint32_t field_count = 0;
};

// 从任意 x/y/z(+intensity) 字段布局解码为统一的 PointXYZI。
// 支持 FLOAT32/FLOAT64/FLOAT16?(不支持) 的 x/y/z，以及可选 intensity。
// 返回 false 时 err 给出可读原因（消息格式不合法、字段缺失、非有限值过多等）。
bool decode_point_cloud2(const CloudWireView& view, PointCloud& out, std::string* err = nullptr);

// 生成符合 PointCloud2（x,y,z,intensity 全 FLOAT32，point_step=16）的裸 buffer，
// 供发布端使用，也供自检做往返验证。
struct EncodedCloud2 {
  std::vector<std::uint8_t> data;
  std::uint32_t width = 0;
  std::uint32_t height = 1;
  std::uint32_t point_step = 16;
  std::uint32_t row_step = 0;
  std::vector<PointFieldView> fields;  // name 指向静态字符串，可直接映射成 PointField
};

EncodedCloud2 encode_point_cloud2(const PointCloud& cloud);

}  // namespace pcview
