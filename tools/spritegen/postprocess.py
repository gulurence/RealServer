#
# Copyright (c) 2026 GuMingYao,  All rights reserved
#
# File name: postprocess.py
# Purpose:   AI 直出图后处理为游戏可用资产
#
# process 按 mode 分 background、ui、sprite 三类，分别做 cover 裁切、去背景、像素风下采样
# remove_background 支持 rembg 与色键去除 chroma 两种策略，缺 rembg 时自动退回 chroma
# pixelate 用最近邻缩放并可 median-cut 量化锁色板，保留 alpha 通道
#

"""后处理：AI 直出图 → 游戏可用资产。

- background：等比裁切（cover）/缩放到目标尺寸，输出不透明 PNG（全出血背景）。
- ui：去背景 → 输出带透明通道 PNG。
- sprite：去背景 → 像素风最近邻下采样（可选锁色板），输出 RGBA PNG。

去背景策略：
- auto（默认）：装了 rembg 就用 rembg，否则退回「色键去除」（适合纯色/品红背景）。
- 也可在资产上显式 remove_bg: false 跳过。
"""
from PIL import Image, ImageChops, ImageOps

_DEFAULT_BG_TOLERANCE = 40


def _rembg_available() -> bool:
    try:
        import rembg  # noqa: F401
        return True
    except Exception:
        return False


def _chroma_remove(img: Image.Image, tolerance: int) -> Image.Image:
    """色键去除：把接近「四角平均色」的像素设为透明。适合纯色背景，快且无依赖。"""
    rgba = img.convert("RGBA")
    w, h = rgba.size
    corners = [rgba.getpixel(p) for p in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1))]
    r = sum(p[0] for p in corners) // 4
    g = sum(p[1] for p in corners) // 4
    b = sum(p[2] for p in corners) // 4

    bg = Image.new("RGB", rgba.size, (r, g, b))
    diff = ImageChops.difference(rgba.convert("RGB"), bg)
    mask = ImageOps.grayscale(diff).point(lambda v: 0 if v <= tolerance else 255)
    rgba.putalpha(mask)
    return rgba


def remove_background(img: Image.Image, method: str = "auto", tolerance: int = _DEFAULT_BG_TOLERANCE) -> Image.Image:
    """去背景，返回 RGBA 图。method: auto | rembg | chroma。"""
    if method == "auto":
        method = "rembg" if _rembg_available() else "chroma"
    if method == "rembg":
        from rembg import remove
        return remove(img.convert("RGB")).convert("RGBA")
    if method == "chroma":
        return _chroma_remove(img, tolerance)
    raise ValueError(f"未知去背景方法: {method!r}")


def resize_to(img: Image.Image, size: tuple[int, int], fit: str = "cover") -> Image.Image:
    """把图缩放到目标尺寸。fit: cover | contain | stretch | none。"""
    size = tuple(size)
    if img.size == size:
        return img
    if fit == "cover":
        return ImageOps.fit(img, size, Image.LANCZOS)
    if fit == "contain":
        return ImageOps.contain(img, size, Image.LANCZOS)
    if fit == "stretch":
        return img.resize(size, Image.LANCZOS)
    if fit == "none":
        return img
    raise ValueError(f"未知 fit: {fit!r}")


def pixelate(img: Image.Image, size: tuple[int, int], palette=None) -> Image.Image:
    """像素风下采样：最近邻缩放到 size，可选锁色板（median-cut 量化，保留 alpha）。"""
    img = img.convert("RGBA")
    img = img.resize(tuple(size), Image.NEAREST)
    if palette:
        alpha = img.getchannel("A")
        rgb = img.convert("RGB").quantize(
            colors=palette, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE
        ).convert("RGB")
        img = rgb.convert("RGBA")
        img.putalpha(alpha)
    return img


def process(img: Image.Image, mode: str, size: tuple[int, int],
            remove_bg=None, fit: str = "cover", tolerance: int = _DEFAULT_BG_TOLERANCE,
            palette=None) -> Image.Image:
    """对一个资产做完整后处理。

    remove_bg: None 则按 mode 推断（ui/sprite 去背景，background 不去）。
    """
    if remove_bg is None:
        remove_bg = mode in ("ui", "sprite")

    if remove_bg:
        img = remove_background(img, tolerance=tolerance)

    if mode == "sprite":
        img = pixelate(img, size, palette=palette)
    else:
        img = resize_to(img, size, fit)
        if mode == "background":
            img = img.convert("RGB")  # 全出血背景，丢弃 alpha
    return img
