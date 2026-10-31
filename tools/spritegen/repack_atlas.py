#
# Copyright (c) 2026 GuMingYao,  All rights reserved
#
# File name: repack_atlas.py
# Purpose:   把 AI 生成的魔幻 UI 图贴回客户端图集对应帧
#
# 按 JOBS 表从 res zip 读取 plist 帧矩形，用 assets_magic 源图替换图集 PNG 目标帧像素
# 不改 plist 帧位置与尺寸，仅套回原帧 alpha 掩码保留透明边角
# 要求所有目标帧 rotated 为 false，回写整个 res zip 存档
#

"""repack_atlas.py: 把 AI 生成的魔幻 UI 图贴回客户端 TexturePacker 图集的对应帧。

不改 plist（帧位置/尺寸不变），只替换图集 PNG 里目标帧的像素，并套用原帧的
alpha 掩码以保留透明边角。所有目标帧均 rotated=False，无需旋转处理。
"""
import io
import re
import zipfile
import plistlib
from pathlib import Path
from PIL import Image

RES = Path(r'E:/RealServer/client/soulofgun/Debug.win32/res')
MAGIC = Path(r'E:/RealServer/client/soulofgun/UIEditor/CocoStudio/assets_magic')

# (atlas plist, atlas png, 目标帧名, 魔幻源图[相对 assets_magic])
JOBS = [
    ('pic/commonPic.plist', 'pic/commonPic.png', 'uistore_bg.png', 'pic/uistore_bg.png'),
    ('pic/commonPic.plist', 'pic/commonPic.png', 'bc_jifei.png', 'pic/bc_jifei.png'),
    ('pic/ui_store1.plist', 'pic/ui_store1.png', 'zhuanpan_bg.png', 'pic/zhuanpan_bg.png'),
    ('pic/picInGame_fullScreen.plist', 'pic/picInGame_fullScreen.png', 'fullScreen01.png', 'pic/fullScreen01.png'),
    ('pic/picInGame_fullScreen.plist', 'pic/picInGame_fullScreen.png', 'fullScreen02.png', 'pic/fullScreen02.png'),
    # 源名错位：生成的 fengmianzi5 对应运行时帧 fengmianzi1
    ('pic/text_ui2.plist', 'pic/text_ui2.png', 'fengmianzi1.png', 'pic/fengmianzi5.png'),
]


def parse_frame_rect(s) -> tuple[int, int, int, int]:
    """解析 cocos 的 '{{x,y},{w,h}}' 字符串（也兼容 list/dict 形式）。返回 x,y,w,h。"""
    nums = [int(n) for n in re.findall(r'-?\d+', str(s))]
    assert len(nums) == 4, f'无法解析 frame: {s!r}'
    return nums[0], nums[1], nums[2], nums[3]


def main() -> int:
    with zipfile.ZipFile(RES, 'r') as z:
        entries = {info.filename: z.read(info.filename) for info in z.infolist()}

    atlases: dict[str, Image.Image] = {}
    for plist_path, png_path, frame_key, src_rel in JOBS:
        pl = plistlib.loads(entries[plist_path])
        meta = pl['frames'][frame_key]
        if meta.get('rotated'):
            raise RuntimeError(f'{frame_key} 是 rotated，脚本未处理旋转')

        x, y, w, h = parse_frame_rect(meta['frame'])

        if png_path not in atlases:
            atlases[png_path] = Image.open(io.BytesIO(entries[png_path])).convert('RGBA')
        atlas = atlases[png_path]
        assert x + w <= atlas.width and y + h <= atlas.height, f'{frame_key} 越界'

        orig_alpha = atlas.crop((x, y, x + w, y + h)).getchannel('A')

        src = MAGIC / src_rel
        img = Image.open(src).convert('RGBA').resize((w, h), Image.LANCZOS)
        img.putalpha(orig_alpha)
        atlas.paste(img, (x, y))
        print(f'  替换 {png_path} [{frame_key}] @({x},{y}) {w}x{h}  <- {src_rel}')

    for png_path, atlas in atlases.items():
        buf = io.BytesIO()
        atlas.save(buf, format='PNG')
        entries[png_path] = buf.getvalue()

    with zipfile.ZipFile(RES, 'w', zipfile.ZIP_DEFLATED) as z:
        for name, data in entries.items():
            z.writestr(name, data)

    print(f'repack 完成：{len(atlases)} 个图集已回写')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
