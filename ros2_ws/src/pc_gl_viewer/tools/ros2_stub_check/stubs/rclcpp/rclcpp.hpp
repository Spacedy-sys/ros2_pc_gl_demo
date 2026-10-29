// 仅用于「没有 ROS2 环境时做运行时校验」的最小桩头文件。
//
// 和纯语法桩的区别：这里的 Publisher / Timer / Subscription 是**有行为**的——
//   - Publisher::publish 会把消息收集到静态队列里；
//   - Node::create_wall_timer 把回调存下来，测试可以手动 trigger()；
//   - Node::create_subscription 把回调登记到按消息类型区分的注册表，测试可以据此调用；
//   - Clock::now 每次调用前进 0.1 s，让"按时间推进的场景"在测试里真的会动。
// 于是 tools/ros2_stub_check/loopback_test.cpp 能在 Windows 上把
// 「发布节点填消息 -> 适配器 -> 解码 -> 订阅节点回调」这条链路真跑一遍。
//
// 真机编译（Ubuntu + ROS2）用的当然是 /opt/ros/<distro>/include 下的真头文件。
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// 真实 ROS 里 rcl_clock_type_t 定义在 rcl/time.h 的全局作用域，
// 所以 RCL_ROS_TIME 这类值在全局作用域可见（节点代码直接写 RCL_ROS_TIME）。
enum rcl_clock_type_t { RCL_ROS_TIME = 1, RCL_SYSTEM_TIME = 2, RCL_STEADY_TIME = 3 };

namespace rclcpp {

class Time {
 public:
  Time() = default;
  Time(std::int32_t sec, std::uint32_t nsec, rcl_clock_type_t /*type*/ = RCL_SYSTEM_TIME)
      : ns_(static_cast<std::int64_t>(sec) * 1000000000LL + static_cast<std::int64_t>(nsec)) {}
  static Time from_seconds(double s) { return from_ns(static_cast<std::int64_t>(s * 1e9)); }
  static Time from_ns(std::int64_t ns) {
    Time t;
    t.ns_ = ns;
    return t;
  }
  std::int64_t nanoseconds() const { return ns_; }
  double seconds() const { return static_cast<double>(ns_) / 1e9; }
  Time operator-(const Time& o) const { return from_ns(ns_ - o.ns_); }
  Time operator+(const Time& o) const { return from_ns(ns_ + o.ns_); }

 private:
  std::int64_t ns_ = 0;
};

// 每次 now() 前进 step_ns，模拟真实的墙钟推进
class Clock {
 public:
  using SharedPtr = std::shared_ptr<Clock>;
  Time now() const {
    calls_ += 1;
    return Time::from_ns(static_cast<std::int64_t>(calls_) * step_ns_);
  }
  void set_step_ns(std::int64_t step) { step_ns_ = step; }

 private:
  mutable std::int64_t calls_ = 0;
  std::int64_t step_ns_ = 100000000LL;  // 0.1 s
};

class Logger {};

// 具备"可手动触发"能力的定时器桩
class TimerBase {
 public:
  using SharedPtr = std::shared_ptr<TimerBase>;
  virtual ~TimerBase() = default;
  void trigger() {
    if (callback_) callback_();
  }
  void set_callback(std::function<void()> cb) { callback_ = std::move(cb); }

 private:
  std::function<void()> callback_;
};

class QoS {
 public:
  explicit QoS(std::size_t depth) : depth_(depth) {}
  std::size_t depth() const { return depth_; }

 private:
  std::size_t depth_ = 10;
};

inline QoS SensorDataQoS() { return QoS(5); }
inline QoS SystemDefaultsQoS() { return QoS(10); }

// 发布端桩：把消息收进静态队列，测试可以取出来
template <typename MsgT>
class Publisher {
 public:
  using SharedPtr = std::shared_ptr<Publisher<MsgT>>;

  void publish(const MsgT& msg) { captured().push_back(msg); }

  static std::vector<MsgT>& captured() {
    static std::vector<MsgT> queue;
    return queue;
  }
  static void clear() { captured().clear(); }
};

// 订阅端桩：按消息类型登记回调，测试可以据此"投递"消息
template <typename MsgT>
class Subscription {
 public:
  using SharedPtr = std::shared_ptr<Subscription<MsgT>>;
  using CallbackT = std::function<void(std::shared_ptr<MsgT>)>;

  static std::vector<CallbackT>& registry() {
    static std::vector<CallbackT> callbacks;
    return callbacks;
  }
  static void clear() { registry().clear(); }
  // 模拟一次消息投递（把所有订阅者都调一遍）
  static std::size_t deliver(std::shared_ptr<MsgT> msg) {
    std::size_t n = 0;
    for (auto& cb : registry()) {
      if (cb) {
        cb(msg);
        ++n;
      }
    }
    return n;
  }
};

class Node {
 public:
  explicit Node(const std::string& name) : name_(name) {}
  virtual ~Node() = default;

  const std::string& get_name() const { return name_; }
  Logger& get_logger() { return logger_; }
  Clock* get_clock() { return &clock_; }
  Time now() const { return clock_.now(); }

  template <typename T>
  T declare_parameter(const std::string&, const T& default_value) {
    return default_value;
  }

  template <typename MsgT>
  typename Publisher<MsgT>::SharedPtr create_publisher(const std::string&, const QoS&) {
    return std::make_shared<Publisher<MsgT>>();
  }

  template <typename MsgT, typename CallbackT>
  typename Subscription<MsgT>::SharedPtr create_subscription(const std::string&, const QoS&, CallbackT&& cb) {
    Subscription<MsgT>::registry().push_back(
        typename Subscription<MsgT>::CallbackT(std::forward<CallbackT>(cb)));
    return std::make_shared<Subscription<MsgT>>();
  }

  template <typename DurationT, typename CallbackT>
  TimerBase::SharedPtr create_wall_timer(DurationT /*period*/, CallbackT&& cb) {
    auto timer = std::make_shared<TimerBase>();
    timer->set_callback(std::forward<CallbackT>(cb));
    timers_.push_back(timer);
    return timer;
  }

  // 测试用：取出本节点创建的所有定时器
  const std::vector<TimerBase::SharedPtr>& timers() const { return timers_; }

 private:
  std::string name_;
  Logger logger_;
  Clock clock_;
  std::vector<TimerBase::SharedPtr> timers_;
};

inline void init(int, char**) {}
inline void shutdown() {}
inline bool ok() { return true; }

template <typename NodeT>
void spin(std::shared_ptr<NodeT> /*node*/) {}

}  // namespace rclcpp

// 日志宏：参数参与编译（能抓出拼写/类型错误），默认不产生输出
#ifndef PCVIEW_STUB_VERBOSE_LOG
#define RCLCPP_INFO(logger, ...) \
  do {                           \
    (void)(logger);              \
  } while (0)
#define RCLCPP_WARN(logger, ...) \
  do {                           \
    (void)(logger);              \
  } while (0)
#define RCLCPP_ERROR(logger, ...) \
  do {                            \
    (void)(logger);               \
  } while (0)
#define RCLCPP_FATAL(logger, ...) \
  do {                            \
    (void)(logger);               \
  } while (0)
#define RCLCPP_WARN_THROTTLE(logger, clock, ms, ...) \
  do {                                               \
    (void)(logger);                                  \
    (void)(clock);                                   \
    (void)(ms);                                      \
  } while (0)
#endif
