#!/usr/bin/env python3
"""BMP -> PNG 转换（把 --capture 的截图换成能放进 README 的格式）。

用法:
    python tools/bmp2png.py docs
    python tools/bmp2png.py docs/hero_intensity.bmp
"""
import glob
import os
import sys

from PIL import Image


def convert(path):
    out = os.path.splitext(path)[0] + ".png"
    with Image.open(path) as im:
        im.save(out)
    return out


def main():
    targets = []
    for arg in sys.argv[1:] or ["."]:
        if os.path.isdir(arg):
            targets += sorted(glob.glob(os.path.join(arg, "*.bmp")))
        elif os.path.isfile(arg):
            targets.append(arg)
    if not targets:
        print("没有找到 .bmp 文件")
        return 1
    for p in targets:
        out = convert(p)
        print(f"{p} -> {out} ({os.path.getsize(out) // 1024} KB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
