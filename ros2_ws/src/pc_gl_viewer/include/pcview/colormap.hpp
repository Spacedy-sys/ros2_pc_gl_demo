// 调色板：把归一化标量映射成颜色。CPU 侧只维护名字/枚举，真正的映射在 GLSL 里做，
// 这样每帧不需要重算颜色缓冲（省一次 CPU->GPU 的上传），也方便加新模式。
#pragma once

namespace pcview {

enum class ColorMode : int {
  kIntensity = 0,  // 反射强度 -> turbo
  kHeight = 1,     // 高度 -> turbo
  kRange = 2,      // 到传感器距离 -> 灰度/冷色
};

inline const char* color_mode_name(ColorMode m) {
  switch (m) {
    case ColorMode::kIntensity: return "INTENSITY";
    case ColorMode::kHeight: return "HEIGHT";
    case ColorMode::kRange: return "RANGE";
  }
  return "UNKNOWN";
}

inline ColorMode next_color_mode(ColorMode m) {
  return static_cast<ColorMode>((static_cast<int>(m) + 1) % 3);
}

}  // namespace pcview
