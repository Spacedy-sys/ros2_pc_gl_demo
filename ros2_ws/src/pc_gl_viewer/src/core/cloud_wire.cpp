#include "pcview/cloud_wire.hpp"

#include <cmath>
#include <cstring>

namespace pcview {

namespace {

inline bool host_is_little_endian() {
  const std::uint32_t probe = 1u;
  return *reinterpret_cast<const std::uint8_t*>(&probe) == 1u;
}

// PointField.datatype -> 字节数（用于边界检查）
std::uint32_t field_size(std::uint8_t datatype) {
  switch (datatype) {
    case kFloat64: return 8;
    case kFloat32:
    case kInt32:
    case kUint32: return 4;
    case kInt16:
    case kUint16: return 2;
    default: return 1;
  }
}

// 把任意字段读成 double（支持 FLOAT32/FLOAT64，整数类型也顺手支持）。
bool read_field_as_double(const std::uint8_t* base, const PointFieldView& f, bool swap_bytes, double* out) {
  std::uint8_t buf[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  std::size_t n = 0;
  switch (f.datatype) {
    case kFloat32: n = 4; break;
    case kFloat64: n = 8; break;
    case kInt32: n = 4; break;
    case kUint32: n = 4; break;
    case kInt16: n = 2; break;
    case kUint16: n = 2; break;
    case kInt8: n = 1; break;
    case kUint8: n = 1; break;
    default: return false;
  }
  std::memcpy(buf, base + f.offset, n);
  if (swap_bytes && n > 1) {
    for (std::size_t i = 0; i < n / 2; ++i) std::swap(buf[i], buf[n - 1 - i]);
  }
  switch (f.datatype) {
    case kFloat32: {
      float v;
      std::memcpy(&v, buf, 4);
      *out = static_cast<double>(v);
      return true;
    }
    case kFloat64: {
      double v;
      std::memcpy(&v, buf, 8);
      *out = v;
      return true;
    }
    case kInt32: {
      std::int32_t v;
      std::memcpy(&v, buf, 4);
      *out = static_cast<double>(v);
      return true;
    }
    case kUint32: {
      std::uint32_t v;
      std::memcpy(&v, buf, 4);
      *out = static_cast<double>(v);
      return true;
    }
    case kInt16: {
      std::int16_t v;
      std::memcpy(&v, buf, 2);
      *out = static_cast<double>(v);
      return true;
    }
    case kUint16: {
      std::uint16_t v;
      std::memcpy(&v, buf, 2);
      *out = static_cast<double>(v);
      return true;
    }
    case kInt8: {
      std::int8_t v;
      std::memcpy(&v, buf, 1);
      *out = static_cast<double>(v);
      return true;
    }
    case kUint8: {
      std::uint8_t v;
      std::memcpy(&v, buf, 1);
      *out = static_cast<double>(v);
      return true;
    }
    default:
      return false;
  }
}

const PointFieldView* find_field(const CloudWireView& v, const char* name) {
  for (std::uint32_t i = 0; i < v.field_count; ++i) {
    const PointFieldView& f = v.fields[i];
    if (f.name && std::strcmp(f.name, name) == 0) return &f;
  }
  return nullptr;
}

}  // namespace

bool decode_point_cloud2(const CloudWireView& view, PointCloud& out, std::string* err) {
  auto fail = [err](const std::string& msg) {
    if (err) *err = msg;
    return false;
  };
  out.clear();

  if (view.data == nullptr) return fail("PointCloud2.data 为空");
  if (view.point_step == 0) return fail("point_step == 0");
  if (view.width == 0 || view.height == 0) return fail("width/height 为 0");
  if (view.fields == nullptr || view.field_count == 0) return fail("没有 field 描述");

  const PointFieldView* fx = find_field(view, "x");
  const PointFieldView* fy = find_field(view, "y");
  const PointFieldView* fz = find_field(view, "z");
  if (!fx || !fy || !fz) return fail("缺少 x/y/z 字段");
  for (const PointFieldView* f : {fx, fy, fz}) {
    if (f->datatype != kFloat32 && f->datatype != kFloat64) {
      return fail("x/y/z 必须是 FLOAT32 或 FLOAT64");
    }
    if (f->offset + (f->datatype == kFloat64 ? 8u : 4u) > view.point_step) {
      return fail("字段偏移超出 point_step");
    }
  }
  const PointFieldView* fi = find_field(view, "intensity");
  if (fi) {
    // intensity 也要做边界检查，否则畸形消息会在解码时越界读
    if (fi->offset + field_size(fi->datatype) > view.point_step) {
      return fail("intensity 字段偏移超出 point_step");
    }
  }

  const std::uint32_t row_step = view.row_step ? view.row_step : view.point_step * view.width;
  if (row_step < view.point_step * view.width) return fail("row_step 小于 point_step * width");

  // 是否需要逐字段翻转字节：比较「数据字节序」与「主机字节序」。
  // 注意别写成 view.is_bigendian != host_is_little_endian()（那是在比"是否小端"，
  // 小端主机 + 小端数据会被误判成需要翻转，所有 float 直接变成噪声）。
  const bool swap_bytes = (view.is_bigendian != !host_is_little_endian());

  const std::size_t total = static_cast<std::size_t>(view.width) * view.height;
  out.points.reserve(total);
  std::size_t bad = 0;
  for (std::uint32_t r = 0; r < view.height; ++r) {
    const std::uint8_t* row = view.data + static_cast<std::size_t>(r) * row_step;
    for (std::uint32_t c = 0; c < view.width; ++c) {
      const std::uint8_t* p = row + static_cast<std::size_t>(c) * view.point_step;
      double x = 0, y = 0, z = 0, i = 0;
      if (!read_field_as_double(p, *fx, swap_bytes, &x) || !read_field_as_double(p, *fy, swap_bytes, &y) ||
          !read_field_as_double(p, *fz, swap_bytes, &z)) {
        return fail("字段读取失败");
      }
      if (fi && !read_field_as_double(p, *fi, swap_bytes, &i)) return fail("intensity 读取失败");
      // NaN 是雷达驱动常见的无效点表示（is_dense=false 时）。
      if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        ++bad;
        continue;
      }
      PointXYZI pt;
      pt.x = static_cast<float>(x);
      pt.y = static_cast<float>(y);
      pt.z = static_cast<float>(z);
      pt.intensity = static_cast<float>(i);
      out.push(pt);
    }
  }
  if (out.empty()) return fail("解码后没有有效点（全部为 NaN？）");
  if (bad > total / 2) return fail("超过一半的点是非有限值");
  if (err) err->clear();
  return true;
}

EncodedCloud2 encode_point_cloud2(const PointCloud& cloud) {
  EncodedCloud2 out;
  out.width = static_cast<std::uint32_t>(cloud.points.size());
  out.height = 1;
  out.point_step = 16;
  out.row_step = out.width * out.point_step;
  out.data.resize(static_cast<std::size_t>(out.row_step));
  for (std::size_t k = 0; k < cloud.points.size(); ++k) {
    std::uint8_t* p = out.data.data() + k * 16;
    std::memcpy(p + 0, &cloud.points[k].x, 4);
    std::memcpy(p + 4, &cloud.points[k].y, 4);
    std::memcpy(p + 8, &cloud.points[k].z, 4);
    std::memcpy(p + 12, &cloud.points[k].intensity, 4);
  }
  static const char* kNames[4] = {"x", "y", "z", "intensity"};
  out.fields.resize(4);
  for (int i = 0; i < 4; ++i) {
    out.fields[i].name = kNames[i];
    out.fields[i].offset = static_cast<std::uint32_t>(i) * 4u;
    out.fields[i].datatype = kFloat32;
    out.fields[i].count = 1;
  }
  return out;
}

}  // namespace pcview
