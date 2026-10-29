// pc_rate_probe：C++ 侧的「点云频率/带宽探针」。
//
// 为什么需要它：`ros2 topic hz` / `ros2 topic bw` 是 Python 实现，每收到一帧都要把
// 0.65 MB 的 PointCloud2 反序列化成 Python 对象，40k 点的消息下会被订阅端拖慢，
// 读数低于真实频率（Foxy 上实测 4~6 Hz，而实际是 10 Hz）。这个探针用 C++ 订阅，
// 只累加消息计数与字节数，用墙钟算频率，因此可以当作对照证据。
//
// 用法：
//   ros2 run pc_gl_viewer pc_rate_probe                      # 默认订阅 /points，2 s 一个窗口
//   ros2 run pc_gl_viewer pc_rate_probe --ros-args -p topic:=points -p window_s:=3.0
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"

namespace pc_gl_viewer {

class RateProbeNode : public rclcpp::Node {
 public:
  RateProbeNode() : rclcpp::Node("pc_rate_probe") {
    topic_ = declare_parameter<std::string>("topic", "points");
    window_s_ = declare_parameter<double>("window_s", 2.0);

    sub_ = create_subscription<sensor_msgs::msg::PointCloud2>(
        topic_, rclcpp::SensorDataQoS(),
        [this](sensor_msgs::msg::PointCloud2::SharedPtr msg) { on_msg(*msg); });

    RCLCPP_INFO(get_logger(), "探针已启动：订阅 %s，每 %.1f s 输出一次实测频率", topic_.c_str(), window_s_);
  }

 private:
  void on_msg(const sensor_msgs::msg::PointCloud2& msg) {
    const auto now = std::chrono::steady_clock::now();
    if (!running_) {
      running_ = true;
      window_start_ = now;
      frames_ = 0;
      bytes_ = 0;
      points_ = 0;
      min_points_ = msg.width;
      max_points_ = msg.width;
    }

    ++frames_;
    bytes_ += msg.data.size();
    points_ += msg.width;
    if (msg.width < min_points_) min_points_ = msg.width;
    if (msg.width > max_points_) max_points_ = msg.width;

    const double dt = std::chrono::duration<double>(now - window_start_).count();
    if (dt >= window_s_ && frames_ > 1) {
      // 用 (frames-1)/dt：第一帧只作为时间起点，避免系统性高估
      const double hz = static_cast<double>(frames_ - 1) / dt;
      const double mbps = static_cast<double>(bytes_) / dt / (1024.0 * 1024.0);
      const auto avg_points = static_cast<std::uint64_t>(points_ / frames_);
      RCLCPP_INFO(get_logger(), "[实测] 接收 %.2f Hz | %u 点/帧(平均) | %.2f MB/s | 点数范围 %u~%u | 累计 %llu 帧",
                  hz, static_cast<unsigned>(avg_points), mbps, min_points_, max_points_,
                  static_cast<unsigned long long>(total_frames_ + frames_));
      total_frames_ += frames_;
      running_ = false;
    }
  }

  std::string topic_;
  double window_s_ = 2.0;
  bool running_ = false;
  std::chrono::steady_clock::time_point window_start_{};
  std::uint64_t frames_ = 0;
  std::uint64_t total_frames_ = 0;
  std::uint64_t points_ = 0;
  std::uint64_t bytes_ = 0;
  std::uint32_t min_points_ = 0;
  std::uint32_t max_points_ = 0;
  rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr sub_;
};

}  // namespace pc_gl_viewer

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<pc_gl_viewer::RateProbeNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
