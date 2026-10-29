"""Provider 抽象基类与公共工具。

所有 AI 生成后端都实现 Provider.generate()，返回 GenerateResult。
新增 provider 只需：建一个子类 → 实现 generate() → 在 providers/__init__.py 注册。
"""
import base64
import io
from abc import ABC, abstractmethod
from dataclasses import dataclass, field

from PIL import Image


class ProviderError(RuntimeError):
    """provider 配置错误或调用失败。"""


@dataclass
class GenerateResult:
    """provider 返回的生成结果。"""
    image: Image.Image
    meta: dict = field(default_factory=dict)


def image_from_b64(b64: str) -> Image.Image:
    """base64 图片串 → RGBA PIL.Image。"""
    return Image.open(io.BytesIO(base64.b64decode(b64))).convert("RGBA")


def image_from_url(url: str, timeout: int = 180) -> Image.Image:
    """URL 下载图片 → RGBA PIL.Image。"""
    import requests

    r = requests.get(url, timeout=timeout)
    r.raise_for_status()
    return Image.open(io.BytesIO(r.content)).convert("RGBA")


class Provider(ABC):
    """AI 图片生成后端基类。"""

    name = "base"

    @abstractmethod
    def generate(self, prompt: str, size: tuple[int, int],
                 mode: str = "background", **kwargs) -> GenerateResult:
        """根据 prompt 生成一张图。

        Args:
            prompt: 生成提示词。
            size: (宽, 高) 目标尺寸。
            mode: 资产模式（background / ui / sprite），provider 可据此调整策略。
            **kwargs: 透传参数（seed、steps、timeout 等）。

        Returns:
            GenerateResult，含 PIL.Image（RGBA 或 RGB 均可，后处理会统一）。
        """
        raise NotImplementedError

    def img2img(self, image, prompt, size=None, **kwargs) -> GenerateResult:
        """图生图：对输入图做风格迁移（image 为 PIL.Image）。

        Args:
            image: 输入参考图（PIL.Image，调用方应已缩放到模型支持的尺寸）。
            prompt: 风格迁移提示词。
            size: 输出尺寸 (宽, 高)，None 则用输入图尺寸。
            **kwargs: 透传（steps、guidance_scale、seed、retries 等）。

        Returns:
            GenerateResult，含风格迁移后的 PIL.Image。
        """
        raise NotImplementedError
