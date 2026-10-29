"""Gemini provider：Google 文生图 REST 接口（需 GEMINI_API_KEY，国内需代理）。"""
import base64
import io
import os

from PIL import Image

from .base import GenerateResult, Provider, ProviderError


class GeminiProvider(Provider):
    name = "gemini"

    def __init__(self, **kwargs):
        self.api_key = kwargs.get("api_key") or os.environ.get(kwargs.get("api_key_env", "GEMINI_API_KEY"), "")
        self.model = kwargs.get("model", "gemini-2.0-flash-exp")
        self.base = "https://generativelanguage.googleapis.com/v1beta"

    def generate(self, prompt, size, mode="background", **kwargs):
        if not self.api_key:
            raise ProviderError("未配置 GEMINI_API_KEY")
        import requests

        body = {
            "contents": [{"parts": [{"text": prompt}]}],
            "generationConfig": {"responseModalities": ["IMAGE"]},
        }
        r = requests.post(
            f"{self.base}/models/{self.model}:generateContent",
            json=body,
            headers={"x-goog-api-key": self.api_key},
            timeout=180,
        )
        r.raise_for_status()
        data = r.json()
        for part in data.get("candidates", [{}])[0].get("content", {}).get("parts", []):
            inline = part.get("inlineData")
            if inline and inline.get("data"):
                img = Image.open(io.BytesIO(base64.b64decode(inline["data"]))).convert("RGBA")
                return GenerateResult(image=img, meta={"provider": self.name, "model": self.model})
        raise ProviderError("Gemini 响应中未找到图片")
