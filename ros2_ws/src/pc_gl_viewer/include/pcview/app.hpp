// 与平台无关的「查看器」逻辑：帧率统计、体素降采样、HUD 文案、相机交互、离屏上传。
// Windows 预览程序和 ROS2 节点共用这一份，保证两边行为一致（也是我能在 Windows 上
// 真机验证渲染结果的原因）。
#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "pcview/camera.hpp"
#include "pcview/point_cloud.hpp"
#include "pcview/renderer.hpp"

namespace pcview {

// 一帧的交互输入（由平台层填充：鼠标拖拽 / 滚轮 / 按键）。
struct Input {
  float orbit_dx = 0.f, orbit_dy = 0.f;
  float pan_dx = 0.f, pan_dy = 0.f;
  float wheel = 0.f;
  int preset = -1;  // 1..3 预设视角，-1 表示不切换
  bool next_color_mode = false;
  bool bigger = false;
  bool smaller = false;
  bool reset_view = false;
  bool toggle_grid = false;
  bool toggle_hud = false;
};

class ViewerApp {
 public:
  struct Config {
    float voxel_leaf = 0.f;  // >0 时对每帧点云做体素降采样
    bool auto_frame = true;  // 首帧自动取景
  };

  bool init(const Config& cfg, std::string* err);

  // 可在任意线程调用；渲染线程在下一帧取走。
  void submit_cloud(PointCloud cloud, bool has_sensor, const Vec3& sensor_pos);

  void update(double dt_s, const Input& in);
  void render(int fb_w, int fb_h);

  // 由平台主循环在 swap_buffers 两侧计时后回填（用于区分"渲染慢"和"呈现慢"）
  void note_swap_time(double ms);

  const Renderer& renderer() const { return renderer_; }
  OrbitCamera& camera() { return camera_; }
  RenderOptions& options() { return options_; }
  const RenderOptions& options() const { return options_; }

 private:
  std::vector<std::string> hud_lines(int fb_w, int fb_h) const;

  Config cfg_;
  OrbitCamera camera_;
  Renderer renderer_;
  RenderOptions options_;

  std::mutex mutex_;
  PointCloud pending_cloud_;
  bool pending_ready_ = false;
  bool pending_has_sensor_ = false;
  Vec3 pending_sensor_{0.f, 0.f, 0.f};

  // 统计
  std::size_t raw_points_ = 0;
  VoxelGridResult voxel_result_;
  bool paused_ = false;
  double fps_ = 0.0;
  double last_fps_update_ = 0.0;
  int frames_since_fps_ = 0;
  double fps_accum_ = 0.0;
  bool first_frame_ = true;
  bool has_sensor_ = false;
  Vec3 sensor_pos_{0.f, 0.f, 0.f};
  std::string gpu_info_;
  // 帧间隔 / 交换缓冲耗时（区分"渲染慢"与"呈现慢"，排查卡顿用）
  double frame_ms_ = 16.7;
  double frame_ms_max_ = 0.0;
  double swap_ms_ = 0.0;
  double swap_ms_max_ = 0.0;
};

}  // namespace pcview
