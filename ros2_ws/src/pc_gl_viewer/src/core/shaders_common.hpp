// 渲染相关 GLSL 片段的单一来源（renderer.cpp / hud.cpp 共用，避免两处调色板不一致）。
#pragma once

namespace pcview {
namespace shaders {

// Google Turbo 调色板的 6 段线性近似：色域宽、过渡均匀，适合强度/高度这类连续标量。
inline const char* kTurboGLSL = R"GLSL(
vec3 turbo(float t) {
  t = clamp(t, 0.0, 1.0);
  const vec3 c0 = vec3(0.145, 0.075, 0.290);
  const vec3 c1 = vec3(0.235, 0.300, 0.720);
  const vec3 c2 = vec3(0.100, 0.760, 0.580);
  const vec3 c3 = vec3(0.620, 0.900, 0.180);
  const vec3 c4 = vec3(0.985, 0.760, 0.130);
  const vec3 c5 = vec3(0.900, 0.180, 0.055);
  float s = t * 5.0;
  if (s < 1.0) return mix(c0, c1, smoothstep(0.0, 1.0, s));
  if (s < 2.0) return mix(c1, c2, smoothstep(0.0, 1.0, s - 1.0));
  if (s < 3.0) return mix(c2, c3, smoothstep(0.0, 1.0, s - 2.0));
  if (s < 4.0) return mix(c3, c4, smoothstep(0.0, 1.0, s - 3.0));
  return mix(c4, c5, smoothstep(0.0, 1.0, s - 4.0));
}
)GLSL";

}  // namespace shaders
}  // namespace pcview
