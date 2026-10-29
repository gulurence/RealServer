"""pack.py: 把多帧 PNG 打包成 cocos2d-x（TexturePacker）格式的 plist + png 图集。

与客户端 anim/ 下现有图集（如 boy.plist）同格式（format=2），可直接被
CCSpriteFrameCache::addSpriteFramesWithFile 加载。不做旋转，rotated 恒为 false。
"""
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image


def _rect(x, y, w, h):
    return "{{%d,%d},{%d,%d}}" % (x, y, w, h)


def _size(w, h):
    return "{%d,%d}" % (w, h)


def _add(parent, key, value):
    ET.SubElement(parent, "key").text = key
    if isinstance(value, bool):
        ET.SubElement(parent, "true" if value else "false")
    elif isinstance(value, int):
        e = ET.SubElement(parent, "integer")
        e.text = str(value)
    else:
        e = ET.SubElement(parent, "string")
        e.text = str(value)


def _shelf_pack(sizes, max_width=1024, padding=1):
    """简单 shelf 打包：按高度降序，从左到右排，超宽换行。返回 {idx:(x,y,w,h)} 与总尺寸。"""
    order = sorted(range(len(sizes)), key=lambda i: sizes[i][1], reverse=True)
    x = y = padding
    shelf_h = 0
    total_w = 0
    total_h = 0
    placed = {}
    for i in order:
        w, h = sizes[i]
        if x + w + padding > max_width:
            x = padding
            y += shelf_h + padding
            shelf_h = 0
        placed[i] = (x, y, w, h)
        total_w = max(total_w, x + w)
        x += w + padding
        shelf_h = max(shelf_h, h)
        total_h = max(total_h, y + h + padding)
    return placed, (total_w + padding, total_h)


def pack(frames, atlas_name, dest_dir, max_width=1024, padding=1):
    """frames: list[(key, PIL.Image)]；输出 dest_dir/atlas_name.png 与 .plist，返回 (png_path, plist_path)。"""
    dest_dir = Path(dest_dir)
    dest_dir.mkdir(parents=True, exist_ok=True)

    keys = [k for k, _ in frames]
    imgs = [im.convert("RGBA") for _, im in frames]
    placed, (atlas_w, atlas_h) = _shelf_pack([im.size for im in imgs], max_width, padding)

    atlas = Image.new("RGBA", (atlas_w, atlas_h), (0, 0, 0, 0))
    for i, im in enumerate(imgs):
        x, y, w, h = placed[i]
        atlas.paste(im, (x, y), im)

    png_path = dest_dir / f"{atlas_name}.png"
    plist_path = dest_dir / f"{atlas_name}.plist"
    atlas.save(png_path)

    root = ET.Element("plist", {"version": "1.0"})
    d = ET.SubElement(root, "dict")
    ET.SubElement(d, "key").text = "frames"
    frames_dict = ET.SubElement(d, "dict")
    for i, key in enumerate(keys):
        x, y, w, h = placed[i]
        ET.SubElement(frames_dict, "key").text = key
        fd = ET.SubElement(frames_dict, "dict")
        _add(fd, "frame", _rect(x, y, w, h))
        _add(fd, "offset", "{0,0}")
        _add(fd, "rotated", False)
        _add(fd, "sourceColorRect", _rect(0, 0, w, h))
        _add(fd, "sourceSize", _size(w, h))

    ET.SubElement(d, "key").text = "metadata"
    md = ET.SubElement(d, "dict")
    _add(md, "format", 2)
    _add(md, "realTextureFileName", f"{atlas_name}.png")
    _add(md, "size", _size(atlas_w, atlas_h))
    _add(md, "textureFileName", f"{atlas_name}.png")

    _write_plist(root, plist_path)
    return png_path, plist_path


def _write_plist(root, path):
    body = ET.tostring(root, encoding="utf-8").decode("utf-8")
    header = ('<?xml version="1.0" encoding="UTF-8"?>\n'
              '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" '
              '"http://www.apple.com/DTDs/PropertyList-1.0.dtd">\n')
    path.write_text(header + body, encoding="utf-8")
