"""mock provider：无需 API key 的占位生成器。

用途：在接真实 AI 后端前，用确定性程序图跑通「配置 → 生成 → 后处理 → 落盘」全流程。
同名 prompt 输出一致（可复现），不同 prompt 配色/构图不同。仅供验证流水线，不代表真实美术效果。
"""
import colorsys
import hashlib
import random

from PIL import Image, ImageDraw

from .base import GenerateResult, Provider


def _rgb(h, s, v):
    r, g, b = colorsys.hsv_to_rgb(h % 1.0, s, v)
    return (int(r * 255), int(g * 255), int(b * 255))


def _lerp(c1, c2, t):
    return tuple(int(a + (b - a) * t) for a, b in zip(c1, c2))


class MockProvider(Provider):
    name = "mock"

    def generate(self, prompt, size, mode="background", seed=None, **kwargs):
        w, h = size
        digest = hashlib.sha256(prompt.encode("utf-8")).digest()
        rng = random.Random(seed if seed is not None else int.from_bytes(digest[:4], "big"))
        hue = rng.random()

        if mode == "ui":
            img = self._ui(w, h, hue, rng)
        else:
            img = self._background(w, h, hue, rng)

        return GenerateResult(
            image=img,
            meta={"provider": self.name, "placeholder": True, "prompt_hash": digest[:4].hex()},
        )

    def _background(self, w, h, hue, rng):
        img = Image.new("RGB", (w, h))
        d = ImageDraw.Draw(img)
        top = _rgb(hue, rng.uniform(0.15, 0.5), rng.uniform(0.75, 1.0))
        bottom = _rgb((hue + rng.uniform(-0.12, 0.12)) % 1.0, rng.uniform(0.3, 0.65), rng.uniform(0.25, 0.55))
        for y in range(h):  # 垂直渐变
            d.line([(0, y), (w, y)], fill=_lerp(top, bottom, y / max(1, h - 1)))
        for _ in range(rng.randint(3, 7)):  # 半透明光斑
            x0 = rng.randint(-w // 4, w)
            y0 = rng.randint(-h // 4, h)
            r = rng.randint(w // 10, w // 3)
            d.ellipse([x0, y0, x0 + r, y0 + r], fill=_rgb(hue, rng.uniform(0.2, 0.5), rng.uniform(0.6, 1.0)))
        d.rectangle([0, 0, w - 1, h - 1], outline=(255, 255, 255))  # 描边提示
        return img

    def _ui(self, w, h, hue, rng):
        # 纯品红底，方便后处理用「色键去除」摘掉背景
        img = Image.new("RGB", (w, h), (255, 0, 255))
        d = ImageDraw.Draw(img)
        m = max(4, min(w, h) // 8)
        r = max(4, min(w, h) // 4)
        c1 = _rgb(hue, 0.5, 0.9)
        c2 = _rgb(hue, 0.6, 0.55)
        d.rounded_rectangle([m, m, w - m - 1, h - m - 1], radius=r, fill=c1,
                            outline=(255, 255, 255), width=max(2, m // 3))
        d.rounded_rectangle([m * 2, m * 2, w - m * 2 - 1, h - m * 2 - 1], radius=r // 2, fill=c2)
        return img
