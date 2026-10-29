// 查看器入口：GLFW 主循环在这里，ROS 订阅在 GlViewerNode 里。
//
// 线程模型（这类"ROS + GUI"程序最容易写错的地方）：
//   - ROS executor 在独立线程跑 spin()，回调里只做解码 + 交接；
//   - OpenGL 上下文属于主线程，所有 GL 调用（含 VBO 上传）只在主线程发生；
//   - ViewerApp 内部用 mutex 保护"待渲染点云"，两边交接因此是安全的。
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp/executors/single_threaded_executor.hpp"

#include "pcview/app.hpp"
#include "pcview/glfw_window.hpp"
#include "pc_gl_viewer/gl_viewer_node.hpp"

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<pc_gl_viewer::GlViewerNode>();

  std::string err;
  pcview::GlfwWindow window;
  pcview::GlfwWindowConfig wcfg;
  wcfg.width = node->width();
  wcfg.height = node->height();
  wcfg.title = "pc_gl_viewer - ROS2 " + node->topic() + " (OpenGL 3.3 core)";
  wcfg.vsync = node->vsync();
  if (!window.create(wcfg, &err)) {
    RCLCPP_FATAL(node->get_logger(), "%s", err.c_str());
    rclcpp::shutdown();
    return 1;
  }

  pcview::ViewerApp app;
  pcview::ViewerApp::Config cfg;
  cfg.voxel_leaf = static_cast<float>(node->voxel_leaf());
  cfg.auto_frame = true;
  if (!app.init(cfg, &err)) {
    RCLCPP_FATAL(node->get_logger(), "渲染器初始化失败: %s", err.c_str());
    window.destroy();
    rclcpp::shutdown();
    return 2;
  }
  app.options().color_mode = static_cast<pcview::ColorMode>(node->color_mode());
  app.options().point_size = static_cast<float>(node->point_size());
  node->attach(&app);

  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node);
  std::thread spinner([&exec]() { exec.spin(); });

  using clock = std::chrono::steady_clock;
  auto last = clock::now();
  while (rclcpp::ok()) {
    pcview::Input in;
    if (!window.pump(in)) break;

    const auto now_t = clock::now();
    double dt = std::chrono::duration<double>(now_t - last).count();
    last = now_t;
    if (dt > 0.1) dt = 0.1;  // 窗口拖动/断点后不要出现巨大步长

    app.update(dt, in);
    int w = 0, h = 0;
    window.get_size(&w, &h);
    if (w <= 0 || h <= 0) continue;
    app.render(w, h);

    if (window.screenshot_requested()) {
      window.clear_screenshot_request();
      if (window.save_screenshot_bmp("pc_gl_viewer_shot.bmp", &err)) {
        RCLCPP_INFO(node->get_logger(), "已保存截图: pc_gl_viewer_shot.bmp");
      } else {
        RCLCPP_WARN(node->get_logger(), "截图失败: %s", err.c_str());
      }
    }
    const auto t_swap0 = clock::now();
    window.swap_buffers();
    // 交换耗时单独统计：它能区分"渲染慢"和"呈现/合成器慢"
    app.note_swap_time(std::chrono::duration<double, std::milli>(clock::now() - t_swap0).count());
  }

  exec.cancel();
  spinner.join();
  window.destroy();
  RCLCPP_INFO(node->get_logger(), "共收到 %llu 帧点云，退出",
              static_cast<unsigned long long>(node->received_frames()));
  rclcpp::shutdown();
  return 0;
}
