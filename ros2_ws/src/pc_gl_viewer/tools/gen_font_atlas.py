#!/usr/bin/env python3
"""生成 HUD 用的等宽点阵字体图集（C++ 头文件）。

demo 不希望为了显示几行状态文字而引入 freetype/imgui，所以用 Python(Pillow) 在开发机上
把系统等宽字体烘成一张灰度图集，运行时只做一次 glTexImage2D，HUD 渲染就是一个 quad 列表。

用法：
    python tools/gen_font_atlas.py \
        --out ../ros2_ws/src/pc_gl_viewer/include/pcview/hud_font_atlas.hpp \
        --preview ../docs/font_atlas_preview.png
"""
import argparse
import os

from PIL import Image, ImageDraw, ImageFont

FIRST_CHAR = 32
LAST_CHAR = 126  # 空格 ~ '~'：ASCII 全表（含小写），HUD 才能显示 GPU 型号这类混合大小写的字符串
COLS = 8
FONT_CANDIDATES = [
    r"C:\Windows\Fonts\consola.ttf",
    r"C:\Windows\Fonts\cour.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
]


def load_font(size):
    for path in FONT_CANDIDATES:
        if os.path.exists(path):
            return ImageFont.truetype(path, size), path
    return ImageFont.load_default(), "default"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--preview", default="")
    ap.add_argument("--size", type=int, default=15)
    args = ap.parse_args()

    font, path = load_font(args.size)
    ascent, descent = font.getmetrics()
    cell_w = int(round(font.getlength("M"))) + 1
    cell_h = ascent + descent
    count = LAST_CHAR - FIRST_CHAR + 1
    rows = (count + COLS - 1) // COLS
    atlas_w, atlas_h = COLS * cell_w, rows * cell_h

    img = Image.new("L", (atlas_w, atlas_h), 0)
    draw = ImageDraw.Draw(img)
    for i in range(count):
        ch = chr(FIRST_CHAR + i)
        if ch == " ":
            continue
        cx = (i % COLS) * cell_w
        cy = (i // COLS) * cell_h
        draw.text((cx, cy), ch, fill=255, font=font)

    if args.preview:
        os.makedirs(os.path.dirname(os.path.abspath(args.preview)), exist_ok=True)
        big = img.resize((atlas_w * 3, atlas_h * 3), Image.NEAREST)
        big.save(args.preview)

    data = img.tobytes()
    assert len(data) == atlas_w * atlas_h

    with open(args.out, "w", encoding="utf-8", newline="\n") as f:
        f.write("// 由 tools/gen_font_atlas.py 自动生成，请勿手改。\n")
        f.write(f"// 源字体: {path}  size={args.size}px\n")
        f.write("// 图集为 8bit 灰度（GL_R8），按 COLS 列等宽排列，字符范围 [FIRST_CHAR, LAST_CHAR]。\n")
        f.write("#pragma once\n\n")
        f.write("namespace pcview {\nnamespace hud_font {\n\n")
        f.write(f"inline constexpr int kFirstChar = {FIRST_CHAR};\n")
        f.write(f"inline constexpr int kLastChar = {LAST_CHAR};\n")
        f.write(f"inline constexpr int kCols = {COLS};\n")
        f.write(f"inline constexpr int kCellW = {cell_w};\n")
        f.write(f"inline constexpr int kCellH = {cell_h};\n")
        f.write(f"inline constexpr int kAtlasW = {atlas_w};\n")
        f.write(f"inline constexpr int kAtlasH = {atlas_h};\n\n")
        f.write(f"inline constexpr unsigned char kAtlas[{atlas_w * atlas_h}] = {{\n")
        for y in range(atlas_h):
            chunk = data[y * atlas_w:(y + 1) * atlas_w]
            f.write("  " + ",".join(f"{b}" for b in chunk) + ",\n")
        f.write("};\n\n}  // namespace hud_font\n}  // namespace pcview\n")

    print(f"font={path} cell={cell_w}x{cell_h} atlas={atlas_w}x{atlas_h} -> {args.out}")


if __name__ == "__main__":
    main()
