// Windows 本地验收程序：不依赖 ROS，用同一套渲染核心跑「合成激光雷达 -> OpenGL 点云」，
// 支持交互浏览、离屏截图、以及 --selftest 纯逻辑自检（点云编解码 / 体素降采样 / 射线求交）。
//
// 交互：左键拖动=旋转，右键拖动=平移，滚轮=缩放，1/2/3=预设视角，C=切换着色，
//       +/-=点大小，G=网格，H=HUD，R=复位，P=截图，ESC=退出。
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "pcview/app.hpp"
#include "pcview/cloud_wire.hpp"
#include "pcview/synth_scene.hpp"
#include "pcview/win32_window.hpp"

namespace {

struct Options {
  int width = 1600;
  int height = 900;
  float voxel = 0.0f;
  int preset = 0;
  pcview::ColorMode color = pcview::ColorMode::kIntensity;
  float point_size = 2.4f;
  bool selftest = false;
  bool hide = false;
  bool vsync = true;
  std::string capture;      // 单帧截图输出（.bmp）
  std::string capture_seq;  // 序列前缀（frame_000.bmp ...）
  int frames = 1;
  double t0 = 0.0;
  bool hud = true;
  // 手动取景（三个都给才算数，便于复现同一张构图）
  float yaw = 0.f, pitch = 0.f, dist = 0.f;
  bool has_view = false;
};

void print_usage() {
  std::printf(
      "pc_gl_viewer_preview - 合成激光雷达点云 + OpenGL 3.3 显示（Windows 验收版）\n"
      "用法: pc_gl_viewer_preview [选项]\n"
      "  --width N --height N     窗口/离屏尺寸 (默认 1600x900)\n"
      "  --voxel F                体素降采样边长(米)，0 表示关闭\n"
      "  --color 0|1|2            着色: 0=强度 1=高度 2=距离\n"
      "  --point-size F           点像素基准大小\n"
      "  --preset 1|2|3           预设视角: 1=俯视 2=侧视 3=贴地\n"
      "  --yaw F --pitch F --dist F  手动指定轨道相机（三者同时给才生效）\n"
      "  --capture out.bmp        渲染一帧后截图退出\n"
      "  --capture-seq prefix     连续截图 prefix_000.bmp ...\n"
      "  --frames N               序列帧数 (默认 1)\n"
      "  --t0 S                   动画起始时间\n"
      "  --no-hud                 不画 HUD\n"
      "  --hide                   隐藏窗口（仅截图模式）\n"
      "  --selftest               只跑逻辑自检\n");
}

bool parse_args(int argc, char** argv, Options* opt) {
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&](double* out) {
      if (i + 1 >= argc) return false;
      *out = std::atof(argv[++i]);
      return true;
    };
    double v = 0.0;
    if (a == "--width" && next(&v)) {
      opt->width = static_cast<int>(v);
    } else if (a == "--height" && next(&v)) {
      opt->height = static_cast<int>(v);
    } else if (a == "--voxel" && next(&v)) {
      opt->voxel = static_cast<float>(v);
    } else if (a == "--preset" && next(&v)) {
      opt->preset = static_cast<int>(v);
    } else if (a == "--color" && next(&v)) {
      opt->color = static_cast<pcview::ColorMode>(static_cast<int>(v));
    } else if (a == "--point-size" && next(&v)) {
      opt->point_size = static_cast<float>(v);
    } else if (a == "--t0" && next(&v)) {
      opt->t0 = v;
    } else if (a == "--yaw" && next(&v)) {
      opt->yaw = static_cast<float>(v);
      opt->has_view = true;
    } else if (a == "--pitch" && next(&v)) {
      opt->pitch = static_cast<float>(v);
      opt->has_view = true;
    } else if (a == "--dist" && next(&v)) {
      opt->dist = static_cast<float>(v);
      opt->has_view = true;
    } else if (a == "--frames" && next(&v)) {
      opt->frames = std::max(1, static_cast<int>(v));
    } else if (a == "--capture" && i + 1 < argc) {
      opt->capture = argv[++i];
    } else if (a == "--capture-seq" && i + 1 < argc) {
      opt->capture_seq = argv[++i];
    } else if (a == "--no-hud") {
      opt->hud = false;
    } else if (a == "--hide") {
      opt->hide = true;
    } else if (a == "--no-vsync") {
      opt->vsync = false;
    } else if (a == "--selftest") {
      opt->selftest = true;
    } else if (a == "--help" || a == "-h") {
      print_usage();
      return false;
    } else {
      std::fprintf(stderr, "未知参数: %s\n", a.c_str());
      print_usage();
      return false;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// 逻辑自检：不需要 GPU / ROS，验证最容易被面试官追问的几块正确性
// ---------------------------------------------------------------------------
int run_selftest() {
  using namespace pcview;
  int fails = 0;
  auto check = [&fails](bool ok, const std::string& what) {
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++fails;
  };

  // 1) PointCloud2 编解码往返
  PointCloud src;
  for (int i = 0; i < 5; ++i) {
    src.push(PointXYZI{static_cast<float>(i), static_cast<float>(i) * 0.5f, 1.f + static_cast<float>(i) * 0.1f,
                       0.1f * static_cast<float>(i)});
  }
  const EncodedCloud2 enc = encode_point_cloud2(src);
  CloudWireView view;
  view.data = enc.data.data();
  view.width = enc.width;
  view.height = enc.height;
  view.point_step = enc.point_step;
  view.row_step = enc.row_step;
  view.is_bigendian = false;
  view.fields = enc.fields.data();
  view.field_count = static_cast<std::uint32_t>(enc.fields.size());

  PointCloud dec;
  std::string err;
  const bool ok_round = decode_point_cloud2(view, dec, &err);
  check(ok_round && dec.size() == src.size(), "PointCloud2 encode->decode 往返点数一致");
  bool same = ok_round && dec.size() == src.size();
  if (same) {
    for (std::size_t i = 0; i < dec.size(); ++i) {
      if (std::fabs(dec.points[i].x - src.points[i].x) > 1e-6f ||
          std::fabs(dec.points[i].z - src.points[i].z) > 1e-6f ||
          std::fabs(dec.points[i].intensity - src.points[i].intensity) > 1e-6f) {
        same = false;
        break;
      }
    }
  }
  check(same, "PointCloud2 编解码数值一致");

  // 2) 缺少 z 字段必须报错而不是读出随机值
  PointFieldView partial[2] = {{"x", 0, kFloat32, 1}, {"y", 4, kFloat32, 1}};
  CloudWireView bad = view;
  bad.fields = partial;
  bad.field_count = 2;
  PointCloud tmp;
  check(!decode_point_cloud2(bad, tmp, &err) && !err.empty(), "缺少 z 字段时返回错误");
  check(!decode_point_cloud2(CloudWireView{}, tmp, &err), "空消息返回错误");

  // 3) NaN 点（is_dense=false 的常见情况）应被跳过
  PointCloud with_nan = src;
  std::vector<std::uint8_t> raw = enc.data;
  const float nan = std::nanf("");
  std::memcpy(raw.data() + 8, &nan, 4);  // 第 0 个点的 z = NaN
  CloudWireView nan_view = view;
  nan_view.data = raw.data();
  PointCloud dec_nan;
  check(decode_point_cloud2(nan_view, dec_nan, &err) && dec_nan.size() == 4, "NaN 点被过滤（5 -> 4）");

  // 4) 带行填充（row_step > width*point_step）的消息也要能解。
  //    构造：1 点/行 x 2 行，point_step=16（正好 4 个 float），row_step=24（每行尾 8 字节垃圾填充）。
  //    若解码时错用 width*point_step 当行 stride，第 2 行会读到填充区，用例就会失败。
  {
    const std::uint32_t step = 16, row = 24, height = 2;
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(row) * height, 0xAB);
    const float pts[2][4] = {{1.f, 2.f, 3.f, 0.5f}, {4.f, 5.f, 6.f, 0.25f}};
    for (std::uint32_t r = 0; r < height; ++r) {
      std::memcpy(buf.data() + static_cast<std::size_t>(r) * row, pts[r], step);
    }
    CloudWireView v2 = view;
    v2.data = buf.data();
    v2.width = 1;
    v2.height = height;
    v2.point_step = step;
    v2.row_step = row;
    PointCloud out;
    const bool ok = decode_point_cloud2(v2, out, &err) && out.size() == 2 && out.points[1].x == 4.f &&
                    std::fabs(out.points[1].intensity - 0.25f) < 1e-6f;
    check(ok, "带 row_step 行填充的布局解码正确");
    if (!ok) std::printf("       解码结果: size=%zu err=%s\n", out.size(), err.c_str());
  }

  // 5) 体素降采样：1cm 抖动的地面点应被压到每格 1 个，且质心正确
  {
    PointCloud pc;
    for (int ix = 0; ix < 40; ++ix) {
      for (int iy = 0; iy < 40; ++iy) {
        for (int k = 0; k < 4; ++k) {
          const float jx = 0.02f * static_cast<float>(k);
          pc.push(PointXYZI{static_cast<float>(ix) * 0.2f + jx, static_cast<float>(iy) * 0.2f, 0.f, 0.4f});
        }
      }
    }
    VoxelGridResult res;
    const PointCloud out = voxel_grid_downsample(pc, 0.2f, &res);
    check(out.size() == 1600 && res.input_points == 6400, "体素降采样 6400 -> 1600 点");
    check(out.bbox.valid() && std::fabs(out.bbox.min.z) < 1e-6f, "降采样后 bbox 合理");
    bool centroid_ok = true;
    for (const auto& p : out.points) {
      if (std::fabs(p.intensity - 0.4f) > 1e-4f) centroid_ok = false;  // 强度取均值，应保持 0.4
    }
    check(centroid_ok, "体素内强度均值正确");
    std::printf("       体素滤波耗时 %.2f ms (6400 点, 20cm 边长)\n", res.elapsed_ms);
  }

  // 6) 射线求交：正下方应打到地面，水平方向应打到墙
  {
    SyntheticLidarScene scene;
    const float down = scene.raycast(Vec3{0.f, 0.f, 1.15f}, Vec3{0.f, 0.f, -1.f}, 30.f);
    check(std::fabs(down - 1.15f) < 1e-3f, "垂直向下射线命中地面 z=0");
    const float west = scene.raycast(Vec3{0.f, 0.f, 1.15f}, Vec3{-1.f, 0.f, 0.f}, 30.f);
    check(std::fabs(west - 13.f) < 1e-2f, "水平射线命中西墙 x=-13");
    const float too_far = scene.raycast(Vec3{0.f, 0.f, 1.15f}, Vec3{0.f, 0.f, 1.f}, 30.f);
    check(too_far < 0.f, "朝天射线无命中（未命中返回 -1）");
  }

  // 7) 一帧扫描的规模与数值合理性
  {
    SyntheticLidarScene scene;
    LidarConfig cfg;
    const PointCloud cloud = scene.sweep(scene.platform_pose(0.0f), cfg, 0.0f);
    std::printf("       单帧点数 %zu，含 32 线 x %.2f deg\n", cloud.size(),
                static_cast<double>(cfg.az_resolution_deg));
    check(cloud.size() > 20000, "单帧点数 > 20000");
    bool finite = true, in_range = true;
    for (const auto& p : cloud.points) {
      if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) finite = false;
      if (std::fabs(p.x) > 30.f || p.z < -0.5f || p.z > 8.f) in_range = false;
    }
    check(finite, "点云无 NaN/Inf");
    check(in_range, "点云落在场景包络内");
  }

  std::printf("\n%s (%d 项失败)\n", fails == 0 ? "全部自检通过" : "存在失败项", fails);
  return fails == 0 ? 0 : 1;
}

std::string frame_path(const std::string& prefix, int index) {
  char buf[512];
  std::snprintf(buf, sizeof(buf), "%s_%03d.bmp", prefix.c_str(), index);
  return buf;
}

}  // namespace

int main(int argc, char** argv) {
  // 源码是 UTF-8（/utf-8 编译），把控制台也切到 UTF-8，中文日志才不会变乱码
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

  Options opt;
  if (!parse_args(argc, argv, &opt)) return 1;
  if (opt.selftest) return run_selftest();

  pcview::Win32Window window;
  pcview::Win32WindowConfig wcfg;
  wcfg.width = opt.width;
  wcfg.height = opt.height;
  wcfg.title = "pc_gl_viewer - synthetic lidar pointcloud (OpenGL 3.3 core)";
  wcfg.visible = !opt.hide;
  wcfg.vsync = opt.vsync;
  std::string err;
  if (!window.create(wcfg, &err)) {
    std::fprintf(stderr, "创建窗口/GL 上下文失败: %s\n", err.c_str());
    return 2;
  }

  pcview::ViewerApp app;
  pcview::ViewerApp::Config cfg;
  cfg.voxel_leaf = opt.voxel;
  // 命令行指定了预设视角 / 手动相机时不要再自动取景，否则会覆盖构图设置
  cfg.auto_frame = (opt.preset <= 0 && !opt.has_view);
  if (!app.init(cfg, &err)) {
    std::fprintf(stderr, "渲染器初始化失败: %s\n", err.c_str());
    window.destroy();
    return 3;
  }
  app.options().color_mode = opt.color;
  app.options().point_size = opt.point_size;
  app.options().show_hud = opt.hud;
  if (opt.preset > 0) app.camera().apply_preset(opt.preset);
  if (opt.has_view && opt.dist > 0.f) app.camera().set_orbit(opt.yaw, opt.pitch, opt.dist);

  // 本地数据源：模拟 10 Hz 的雷达驱动（对应 ROS2 里的 pc_publisher 节点）
  pcview::SyntheticLidarScene scene;
  pcview::LidarConfig lidar;
  double sim_time = opt.t0;
  double next_publish = 0.0;
  const bool capture_mode = !opt.capture.empty() || !opt.capture_seq.empty();

  auto publish = [&]() {
    const auto pose = scene.platform_pose(static_cast<float>(sim_time));
    app.submit_cloud(scene.sweep(pose, lidar, static_cast<float>(sim_time)), true, pose.position);
  };

  using clock = std::chrono::steady_clock;
  auto last = clock::now();
  int captured = 0;
  // 截图前先空转 60 帧：FPS 统计窗口是 0.35 s，vsync 下 60 帧 ≈ 0.8 s，
  // 这样抓到的 HUD 里 FPS 是一个真实测量值，而不是初始的 0.0。
  int warmup_left = capture_mode ? 60 : 0;

  while (true) {
    pcview::Input in;
    if (!window.pump(in)) break;

    const auto now = clock::now();
    double dt = std::chrono::duration<double>(now - last).count();
    last = now;
    dt = std::min(dt, 0.1);  // 拖动窗口/断点后不要出现巨大步长

    // 数据流：交互模式模拟 10 Hz 雷达驱动；截图模式每帧一帧数据，按固定 0.1 s 节奏推进动画
    if (capture_mode) {
      publish();
    } else {
      if (sim_time >= next_publish) {
        publish();
        next_publish = sim_time + 0.1;
      }
      sim_time += dt;
    }

    app.update(dt, in);
    int w = 0, h = 0;
    window.get_size(&w, &h);
    if (w <= 0 || h <= 0) continue;
    app.render(w, h);

    if (window.screenshot_requested()) {
      window.clear_screenshot_request();
      const std::string path = "screenshot_" + std::to_string(static_cast<long long>(sim_time * 100)) + ".bmp";
      if (window.save_screenshot_bmp(path, &err)) {
        std::printf("已保存截图: %s\n", path.c_str());
      } else {
        std::fprintf(stderr, "截图失败: %s\n", err.c_str());
      }
    }

    if (capture_mode && warmup_left == 0) {
      if (!opt.capture.empty()) {
        // 先 glFinish 再抓帧，避免读到未完成的绘制
        glFinish();
        if (window.save_screenshot_bmp(opt.capture, &err)) {
          std::printf("已保存: %s\n", opt.capture.c_str());
        } else {
          std::fprintf(stderr, "截图失败: %s\n", err.c_str());
        }
        window.swap_buffers();
        break;
      }
      glFinish();
      const std::string path = frame_path(opt.capture_seq, captured);
      if (window.save_screenshot_bmp(path, &err)) {
        std::printf("已保存: %s\n", path.c_str());
      } else {
        std::fprintf(stderr, "截图失败: %s\n", err.c_str());
      }
      window.swap_buffers();
      if (++captured >= opt.frames) break;
      sim_time += 0.1;  // 每帧推进 0.1 s，让平台/动态目标明显移动
      continue;
    }
    if (capture_mode && warmup_left > 0) {
      --warmup_left;  // 预热阶段冻结时间，保证首帧截图可复现
    }

    const auto t_swap = clock::now();
    window.swap_buffers();
    // 与 ROS 侧一致：单独统计交换耗时，便于和 Ubuntu 的表现对比
    app.note_swap_time(std::chrono::duration<double, std::milli>(clock::now() - t_swap).count());
  }

  window.destroy();
  return 0;
}
