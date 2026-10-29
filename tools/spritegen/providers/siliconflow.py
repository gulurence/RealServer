"""SiliconFlow provider：国内可直连的文生图 / 图生图 API（需 SILICONFLOW_API_KEY）。

接口：POST /v1/images/generations
  - 文生图：images[].url / b64_json
  - 图生图：请求带 image_base64（或 image_url），同 endpoint

可用模型（FLUX 系列已禁用）：Kwai-Kolors/Kolors、Qwen/Qwen-Image、Tongyi-MAI/Z-Image(-Turbo)。

对 429（每分钟限流）自动退避重试，重试次数/间隔可用 retries / retry_backoff 控制。
"""
import base64
import io
import logging
import os
import time

from .base import GenerateResult, Provider, ProviderError, image_from_b64, image_from_url

log = logging.getLogger(__name__)


class SiliconFlowProvider(Provider):
    name = "siliconflow"

    def __init__(self, **kwargs):
        self.api_key = kwargs.get("api_key") or os.environ.get(kwargs.get("api_key_env", "SILICONFLOW_API_KEY"), "")
        self.base_url = kwargs.get("base_url", "https://api.siliconflow.cn/v1").rstrip("/")
        self.model = kwargs.get("model", "Kwai-Kolors/Kolors")

    def generate(self, prompt, size, mode="background", **kwargs):
        """文生图：仅 prompt + image_size。"""
        if not self.api_key:
            raise ProviderError(
                "未配置 SiliconFlow API Key：在 config.yaml 的 provider_config 里填 api_key，"
                "或设置环境变量 SILICONFLOW_API_KEY"
            )
        w, h = size
        payload = {
            "model": self.model,
            "prompt": prompt,
            "image_size": f"{w}x{h}",
            "batch_size": 1,
            "num_inference_steps": kwargs.get("steps", 20),
        }
        if kwargs.get("seed") is not None:
            payload["seed"] = kwargs["seed"]
        return self._request(payload, **kwargs)

    def img2img(self, image, prompt, size=None, **kwargs):
        """图生图：对输入图做风格迁移。image 为 PIL.Image（调用方应已缩放到模型支持尺寸）。

        注意字段按模型区分（2026-10-21 实测钉死）：
          - Qwen-Image-Edit 系列：参考图走 ``image`` 字段，值必须是 Data URI
            （``data:image/png;base64,...``，带前缀），且**不支持** image_size/steps（内部定尺寸）。
          - Kwai-Kolors/Kolors 等 SD 系：参考图走 ``image_base64``（裸 base64），需 image_size。
          - Z-Image(-Turbo)/Qwen-Image 是纯文生图，**不认任何参考图字段**，不能用于图生图。
        """
        if not self.api_key:
            raise ProviderError("未配置 SiliconFlow API Key")

        if size is None:
            size = image.size
        w, h = size

        buf = io.BytesIO()
        image.convert("RGB").save(buf, format="PNG")
        b64 = base64.b64encode(buf.getvalue()).decode("ascii")

        payload = {"model": self.model, "prompt": prompt}

        if "image-edit" in self.model.lower():
            # Qwen-Image-Edit：image 字段 + Data URI 前缀，无 image_size/steps
            payload["image"] = f"data:image/png;base64,{b64}"
        else:
            # Kolors / SD 系：image_base64 裸 base64 + 明确 image_size
            payload["image_base64"] = b64
            payload["image_size"] = f"{w}x{h}"
            payload["batch_size"] = 1
            payload["num_inference_steps"] = kwargs.get("steps", 30)

        if kwargs.get("guidance_scale") is not None:
            payload["guidance_scale"] = kwargs["guidance_scale"]
        if kwargs.get("seed") is not None:
            payload["seed"] = kwargs["seed"]
        return self._request(payload, **kwargs)

    def _request(self, payload, **kwargs):
        import requests

        headers = {"Authorization": f"Bearer {self.api_key}"}
        max_retries = int(kwargs.get("retries", 3))
        backoff = float(kwargs.get("retry_backoff", 25))

        for attempt in range(max_retries + 1):
            r = requests.post(f"{self.base_url}/images/generations", json=payload, headers=headers, timeout=180)
            if r.status_code == 429:
                if attempt < max_retries:
                    log.warning("429 限流，%ds 后重试（%d/%d）", backoff, attempt + 1, max_retries)
                    time.sleep(backoff)
                    continue
                raise ProviderError(f"重试 {max_retries} 次后仍被限流（429）")
            r.raise_for_status()
            return GenerateResult(image=self._extract(r.json()), meta={"provider": self.name, "model": self.model})
        raise ProviderError(f"请求失败（{max_retries} 次重试耗尽）")

    @staticmethod
    def _extract(data):
        # 兼容 {images:[{url|b64_json}]} 与 {data:[{url|b64_json}]}
        items = data.get("images") or data.get("data") or []
        if not items:
            raise ProviderError(f"SiliconFlow 响应无图片: {data}")
        first = items[0]
        if first.get("b64_json"):
            return image_from_b64(first["b64_json"])
        if first.get("url"):
            return image_from_url(first["url"])
        raise ProviderError(f"无法解析 SiliconFlow 图片字段: {first}")
