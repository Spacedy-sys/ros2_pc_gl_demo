// ROS2 数据链路「回环测试」（不需要装 ROS）。
//
// 用 tools/ros2_stub_check/stubs 里带行为的桩，把真实节点代码跑起来：
//   1) 实例化 CloudPublisherNode，手动触发它的定时器 -> 节点自己填好的 PointCloud2 进桩队列；
//   2) 校验消息元数据（fields / point_step / row_step / data 长度 / is_dense）；
//   3) 用与节点相同的适配器 wrap_cloud2 + core 解码器还原点云，校验点数与包围盒；
//   4) 实例化 GlViewerNode，把消息投递给它，验证回调真的走完了整条链路。
//
// 它不能替代 ROS 真机验证（QoS 行为、参数类型、spin 调度都是桩模拟的），
// 但能证明「消息拼装 + 解码」这段最容易写错的代码是通的。
#include <chrono>
#include <cstdio>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"

#include "pc_gl_viewer/cloud_publisher_node.hpp"
#include "pc_gl_viewer/gl_viewer_node.hpp"
#include "pc_gl_viewer/ros_cloud_adaptor.hpp"
#include "pcview/app.hpp"
#include "pcview/cloud_wire.hpp"
#include "pcview/point_cloud.hpp"

namespace {

int g_fails = 0;
int g_checks = 0;

void check(bool ok, const std::string& what) {
  ++g_checks;
  std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
  if (!ok) ++g_fails;
}

}  // namespace

int main() {
  using sensor_msgs::msg::PointCloud2;

  rclcpp::Publisher<PointCloud2>::clear();
  rclcpp::Subscription<PointCloud2>::clear();

  // ---- 1) 发布节点：触发定时器 3 次 ----
  auto publisher_node = std::make_shared<pc_gl_viewer::CloudPublisherNode>();
  check(publisher_node->timers().size() == 1, "发布节点注册了 1 个定时器");

  const auto t_enc0 = std::chrono::steady_clock::now();
  for (int i = 0; i < 3; ++i) {
    for (const auto& timer : publisher_node->timers()) timer->trigger();
  }
  const auto t_enc1 = std::chrono::steady_clock::now();
  const double enc_ms = std::chrono::duration<double, std::milli>(t_enc1 - t_enc0).count() / 3.0;

  auto& queue = rclcpp::Publisher<PointCloud2>::captured();
  check(queue.size() == 3, "触发 3 次定时器 -> 桩队列收到 3 帧");
  check(publisher_node->published_frames() == 3, "节点内部发布计数为 3");
  if (queue.size() != 3) {
    std::printf("\n存在失败项 (%d/%d)\n", g_fails, g_checks);
    return 1;
  }

  // ---- 2) 消息元数据（这部分最容易踩坑：步长/行宽/字段偏移） ----
  const PointCloud2& msg = queue.front();
  check(msg.header.frame_id == "lidar_link", "header.frame_id 默认 lidar_link");
  check(msg.height == 1, "height == 1（无序点云）");
  check(msg.width > 20000 && msg.width < 80000, "width 在 2万~8万之间");
  check(msg.fields.size() == 4, "fields 数量为 4");
  bool field_ok = msg.fields.size() == 4;
  if (field_ok) {
    const char* names[4] = {"x", "y", "z", "intensity"};
    for (int i = 0; i < 4; ++i) {
      field_ok = field_ok && msg.fields[i].name == names[i] &&
                 msg.fields[i].offset == static_cast<std::uint32_t>(i * 4) &&
                 msg.fields[i].datatype == sensor_msgs::msg::PointField::FLOAT32 && msg.fields[i].count == 1;
    }
  }
  check(field_ok, "x/y/z/intensity 字段名、偏移、类型、count 全对");
  check(msg.point_step == 16, "point_step == 16 (4 x float32)");
  check(msg.row_step == msg.width * msg.point_step, "row_step == width * point_step（无行填充）");
  check(msg.data.size() == static_cast<std::size_t>(msg.row_step) * msg.height, "data 字节数与步长一致");
  check(!msg.is_bigendian, "is_bigendian == false");
  check(msg.is_dense, "is_dense == true");

  // ---- 3) 适配器 + 解码：与节点用的是同一份映射代码 ----
  std::vector<pcview::PointFieldView> fields;
  pcview::CloudWireView view = pc_gl_viewer::wrap_cloud2(msg, fields);

  pcview::PointCloud cloud;
  std::string err;
  const auto t_dec0 = std::chrono::steady_clock::now();
  const bool decoded = pcview::decode_point_cloud2(view, cloud, &err);
  const auto t_dec1 = std::chrono::steady_clock::now();
  const double dec_ms = std::chrono::duration<double, std::milli>(t_dec1 - t_dec0).count();

  check(decoded, "解码成功: " + err);
  check(cloud.size() == msg.width, "解码点数 == width");
  if (decoded) {
    const pcview::BBox& bb = cloud.bbox;
    const bool bbox_ok = bb.valid() && std::fabs(bb.min.x) < 15.f && std::fabs(bb.max.x) < 15.f &&
                         std::fabs(bb.min.y) < 12.f && std::fabs(bb.max.y) < 12.f && bb.min.z > -1.f &&
                         bb.max.z < 5.f;
    check(bbox_ok, "解码后的包围盒落在房间尺寸内");
    std::printf("       点数 %zu, x[%.2f, %.2f] y[%.2f, %.2f] z[%.2f, %.2f]\n", cloud.size(),
                static_cast<double>(bb.min.x), static_cast<double>(bb.max.x),
                static_cast<double>(bb.min.y), static_cast<double>(bb.max.y),
                static_cast<double>(bb.min.z), static_cast<double>(bb.max.z));
  }

  check(queue[0].width != queue[1].width || queue[1].width != queue[2].width, "连续 3 帧的点数不同（场景在动）");

  // ---- 4) 订阅节点：把消息投递给它，验证回调走完整条链路 ----
  auto viewer_node = std::make_shared<pc_gl_viewer::GlViewerNode>();
  pcview::ViewerApp app;  // 故意不 init：submit_cloud 不碰 GL，可以安全构造
  viewer_node->attach(&app);

  std::size_t delivered = 0;
  for (auto& m : queue) {
    delivered += rclcpp::Subscription<PointCloud2>::deliver(std::make_shared<PointCloud2>(m));
  }
  check(delivered == 3, "投递 3 帧给订阅节点");
  check(viewer_node->received_frames() == 3, "订阅节点成功处理 3 帧（解码 + submit_cloud 走通）");

  // ---- 5) 异常消息：空消息不应崩溃，也不应计数 ----
  {
    auto bad = std::make_shared<PointCloud2>();
    rclcpp::Subscription<PointCloud2>::deliver(bad);  // width/height = 0
    check(viewer_node->received_frames() == 3, "空消息被拒绝且不影响计数（异常路径不崩溃）");

    auto bad2 = std::make_shared<PointCloud2>(queue.front());
    bad2->fields.erase(bad2->fields.begin() + 2);  // 删掉 z 字段
    rclcpp::Subscription<PointCloud2>::deliver(bad2);
    check(viewer_node->received_frames() == 3, "缺 z 字段的消息被拒绝");
  }

  // 单帧耗时：trigger() 里包含「场景射线求交 + 编码 + 填消息」，是发布端的真实单帧成本
  std::printf("\n       发布端 仿真+编码+填消息 %.2f ms/帧, 订阅端解码 %.2f ms/帧\n", enc_ms, dec_ms);
  std::printf("%s (%d/%d 项失败)\n", g_fails == 0 ? "回环测试全部通过" : "存在失败项", g_fails, g_checks);
  return g_fails == 0 ? 0 : 1;
}
