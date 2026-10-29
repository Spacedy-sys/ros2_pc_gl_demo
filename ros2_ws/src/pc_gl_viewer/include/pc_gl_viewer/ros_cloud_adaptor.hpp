// 把「长得像 sensor_msgs::msg::PointCloud2 的消息」映射成 core 的只读视图。
//
// 写成模板的好处：
//   1. 节点里和测试里用的是同一份映射代码，不存在"测试测的是另一份实现"的问题；
//   2. 测试时可以用 tools/ros2_stub_check 里的桩消息类型，不需要装 ROS；
//   3. core 保持零 ROS 依赖（CloudWireView 只认裸指针+字段描述）。
#pragma once

#include <cstdint>
#include <vector>

#include "pcview/cloud_wire.hpp"

namespace pc_gl_viewer {

// fields 由调用方持有，生命周期必须覆盖返回的 view（它只存指针）。
template <typename PointCloud2T>
pcview::CloudWireView wrap_cloud2(const PointCloud2T& msg, std::vector<pcview::PointFieldView>& fields) {
  fields.clear();
  fields.reserve(msg.fields.size());
  for (const auto& f : msg.fields) {
    fields.push_back(pcview::PointFieldView{f.name.c_str(), f.offset, f.datatype, f.count});
  }

  pcview::CloudWireView view;
  view.data = msg.data.data();
  view.width = msg.width;
  view.height = msg.height;
  view.point_step = msg.point_step;
  view.row_step = msg.row_step;
  view.is_bigendian = msg.is_bigendian != 0;
  view.fields = fields.data();
  view.field_count = static_cast<std::uint32_t>(fields.size());
  return view;
}

}  // namespace pc_gl_viewer
