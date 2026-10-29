#include "pcview/app.hpp"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace pcview {

bool ViewerApp::init(const Config& cfg, std::string* err) {
  cfg_ = cfg;
  options_.color_mode = ColorMode::kIntensity;
  options_.point_size = 2.4f;
  if (!renderer_.init(err)) return false;
  gpu_info_ = gl_renderer();
  return true;
}

void ViewerApp::submit_cloud(PointCloud cloud, bool has_sensor, const Vec3& sensor_pos) {
  std::lock_guard<std::mutex> lock(mutex_);
  pending_cloud_ = std::move(cloud);
  pending_ready_ = true;
  pending_has_sensor_ = has_sensor;
  pending_sensor_ = sensor_pos;
}

void ViewerApp::update(double dt_s, const Input& in) {
  if (in.orbit_dx != 0.f || in.orbit_dy != 0.f) camera_.orbit(in.orbit_dx, in.orbit_dy);
  if (in.pan_dx != 0.f || in.pan_dy != 0.f) camera_.pan(in.pan_dx, in.pan_dy);
  if (in.wheel != 0.f) camera_.dolly(in.wheel);
  if (in.preset > 0) camera_.apply_preset(in.preset);
  if (in.reset_view) camera_.reset();
  if (in.next_color_mode) options_.color_mode = next_color_mode(options_.color_mode);
  if (in.bigger) options_.point_size = std::min(9.f, options_.point_size * 1.15f);
  if (in.smaller) options_.point_size = std::max(1.f, options_.point_size / 1.15f);
  if (in.toggle_grid) options_.show_grid = !options_.show_grid;
  if (in.toggle_hud) options_.show_hud = !options_.show_hud;

  // FPS：0.35 s 窗口的滑动平均，比逐帧瞬时值稳
  fps_accum_ += static_cast<double>(dt_s);
  ++frames_since_fps_;
  // 帧间隔统计：平均值用 EMA，最大值取窗口内峰值（判断"周期性地顿一下"用）
  frame_ms_ = frame_ms_ * 0.9 + static_cast<double>(dt_s) * 1000.0 * 0.1;
  if (static_cast<double>(dt_s) * 1000.0 > frame_ms_max_) frame_ms_max_ = static_cast<double>(dt_s) * 1000.0;
  if (fps_accum_ >= 0.35) {
    fps_ = static_cast<double>(frames_since_fps_) / fps_accum_;
    fps_accum_ = 0.0;
    frames_since_fps_ = 0;
    frame_ms_max_ = frame_ms_;
    swap_ms_max_ = swap_ms_;
  }
}

void ViewerApp::note_swap_time(double ms) {
  swap_ms_ = swap_ms_ * 0.9 + ms * 0.1;
  if (ms > swap_ms_max_) swap_ms_max_ = ms;
}

void ViewerApp::render(int fb_w, int fb_h) {
  PointCloud cloud;
  bool has_new = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pending_ready_) {
      cloud = std::move(pending_cloud_);
      pending_cloud_ = PointCloud{};
      pending_ready_ = false;
      has_new = true;
      has_sensor_ = pending_has_sensor_;
      sensor_pos_ = pending_sensor_;
      raw_points_ = cloud.points.size();
    }
  }
  (void)paused_;

  if (has_new) {
    if (cfg_.voxel_leaf > 0.f) {
      cloud = voxel_grid_downsample(cloud, cfg_.voxel_leaf, &voxel_result_);
    } else {
      voxel_result_ = VoxelGridResult{};
      voxel_result_.input_points = cloud.points.size();
      voxel_result_.output_points = cloud.points.size();
    }
    renderer_.upload(cloud);
    if (first_frame_ && cfg_.auto_frame) {
      camera_.frame(cloud.bbox);
      first_frame_ = false;
    }
    if (!has_sensor_) {
      // 没有传感器位姿时（例如离线回放），用点云中心代替，距离着色依然可用
      sensor_pos_ = cloud.bbox.valid() ? cloud.bbox.center() : Vec3{0.f, 0.f, 0.f};
    }
  }

  camera_.set_aspect(fb_w > 0 && fb_h > 0 ? static_cast<float>(fb_w) / static_cast<float>(fb_h) : 1.6f);
  // HUD 按分辨率自适应：900p 用 2 倍，600p 用 1.3 倍，避免小窗口里字占掉半个屏幕
  options_.hud_scale = std::max(1.f, std::min(3.f, static_cast<float>(fb_h) / 450.f));
  renderer_.set_sensor_position(sensor_pos_);
  renderer_.render(camera_, fb_w, fb_h, options_, hud_lines(fb_w, fb_h));
}

std::vector<std::string> ViewerApp::hud_lines(int fb_w, int fb_h) const {
  char buf[192];
  std::vector<std::string> lines;
  lines.emplace_back("PC_GL_VIEWER - ROS2 POINTCLOUD DEMO");

  if (voxel_result_.input_points > 0) {
    if (cfg_.voxel_leaf > 0.f) {
      std::snprintf(buf, sizeof(buf), "PTS %zu -> %zu  VOXEL %.2fm (%.0f%%)  %.1fms", raw_points_,
                    renderer_.stats().points, static_cast<double>(cfg_.voxel_leaf),
                    voxel_result_.reduction_ratio(), voxel_result_.elapsed_ms);
    } else {
      std::snprintf(buf, sizeof(buf), "PTS %zu  GPU %.1fMB", raw_points_,
                    static_cast<double>(renderer_.stats().points * sizeof(PointXYZI)) / (1024.0 * 1024.0));
    }
    lines.emplace_back(buf);
  }

  // CPU_UP / CPU_DRAW 是 CPU 端提交耗时（GL 调用异步，函数返回时 GPU 往往还没画完），
  // 端到端性能以 FPS 为准。这里如实标注，避免"0.00ms"被误读成造假。
  std::snprintf(buf, sizeof(buf), "FPS %5.1f  CPU_UP %.2fms  CPU_DRAW %.2fms", fps_,
                renderer_.stats().upload_ms, renderer_.stats().draw_ms);
  lines.emplace_back(buf);

  // 诊断卡顿用的三件事：
  //   RES  —— 实际帧缓冲分辨率（HiDPI 缩放会让它成倍变大）
  //   FRAME—— 平均/峰值帧间隔；峰值远大于均值说明"周期性顿一下"
  //   SWAP —— 交换缓冲耗时；接近 16.7 ms 且抖动大 → 呈现端（合成器/vsync）问题
  std::snprintf(buf, sizeof(buf), "RES %dx%d  FRAME %.1f/%.1fms  SWAP %.2f/%.1fms (avg/max)", fb_w, fb_h,
                frame_ms_, frame_ms_max_, swap_ms_, swap_ms_max_);
  lines.emplace_back(buf);

  std::snprintf(buf, sizeof(buf), "COLOR %s  PSIZE %.1f  GRID %s  YAW %.0f PITCH %.0f DIST %.1f",
                color_mode_name(options_.color_mode), static_cast<double>(options_.point_size),
                options_.show_grid ? "ON" : "OFF", static_cast<double>(camera_.yaw_deg()),
                static_cast<double>(camera_.pitch_deg()), static_cast<double>(camera_.distance()));
  lines.emplace_back(buf);

  if (!gpu_info_.empty()) {
    std::string gpu = gpu_info_;
    if (gpu.size() > 58) gpu = gpu.substr(0, 58);
    lines.emplace_back("GPU " + gpu);
  }
  return lines;
}

}  // namespace pcview
