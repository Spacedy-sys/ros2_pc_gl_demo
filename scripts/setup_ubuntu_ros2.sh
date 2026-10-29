#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Ubuntu 侧准备脚本：检查 ROS2 -> 装依赖 -> colcon build -> 打印运行命令
#
# 用法（在仓库根目录执行）：
#     bash scripts/setup_ubuntu_ros2.sh
#
# 特点：只做「装依赖 + 编译」，不 sudo 改系统设置、不删文件、不自动跑 GUI。
# ---------------------------------------------------------------------------
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PKG_DIR="$HERE/ros2_ws/src/pc_gl_viewer"

echo "=== 环境检查 ==="
if [ ! -f /etc/os-release ]; then
  echo "[x] 不是 Linux？/etc/os-release 不存在"
  exit 1
fi
. /etc/os-release
echo "    发行版: $PRETTY_NAME"

case "${VERSION_CODENAME:-}" in
  jammy) SUGGEST=humble ;;
  noble) SUGGEST=jazzy ;;
  focal) SUGGEST=foxy ;;
  *)     SUGGEST="" ;;
esac

ROS_DIR=""
for d in /opt/ros/*/; do
  [ -d "$d" ] || continue
  ROS_DIR="${d%/}"
done

if [ -z "$ROS_DIR" ]; then
  echo "[!] 没找到 /opt/ros/ 下的安装，需要先装 ROS2（建议 $SUGGEST）。"
  echo "    参见 docs/PROMPT_ubuntu_ros2.md 里的安装命令。"
  exit 2
fi
echo "    ROS2: $ROS_DIR"

echo "=== 安装依赖（需要 sudo） ==="
sudo apt update
sudo apt install -y \
  libglfw3-dev libgl1-mesa-dev mesa-utils \
  python3-colcon-common-extensions python3-pip
# 截图转换用（可选）
python3 -m pip install --quiet --user pillow || echo "[!] Pillow 安装失败，不影响 demo"

echo "=== 编译 ==="
# shellcheck disable=SC1091
source "$ROS_DIR/setup.bash"
cd "$HERE/ros2_ws"
colcon build --packages-select pc_gl_viewer --cmake-args -DCMAKE_BUILD_TYPE=Release

echo
echo "=== 完成 ==="
echo "  新开一个终端执行："
echo "    source $HERE/ros2_ws/install/setup.bash"
echo "    ros2 launch pc_gl_viewer pc_gl_viewer.launch.py"
echo
echo "  验收（再开一个终端）："
echo "    ros2 topic hz /points                                   # 预期 ~10 Hz"
echo "    ros2 topic echo /points --once --field width            # 预期 ~4 万"
echo
echo "  渲染信息（确认不是软件渲染）："
echo "    glxinfo | grep -E 'OpenGL renderer|OpenGL version'"
