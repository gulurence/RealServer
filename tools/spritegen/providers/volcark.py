"""Volcengine Ark（火山方舟）provider：字节豆包 Seedream 图像生成 / 图生图。

接口：POST {base_url}/images/generations（OpenAI 兼容）
  - 文生图：prompt + size
  - 图生图：image（Data URI base64）+ ref_type_list（参考类型）

关键点（2026-10-21 实测钉死）：
  - model 用「推理接入点」ep-xxxxx ID（控制台开通模型后创建），而非模型名直调
    （模型名大多返回 InvalidEndpointOrModel.NotFound / ModelNotOpen）。
  - 图生图参考类型 ref_type_list：["ip"]（主体特征）构图保真度最高（灰度结构相关 r≈0.84），
    其次是 ["style"]（风格特征 r≈0.80）；不传则 r≈0.69。
  - 认证：Authorization: Bearer ARK_API_KEY。
  - 429 自动退避重试，重试次数/间隔用 retries / retry_backoff 控制。
"""
import base64
import io
import logging
import os
import time

from .base import GenerateResult, Provider, ProviderError, image_from_b64, image_from_url

log = logging.getLogger(__name__)


class VolcArkProvider(Provider):
    name = "volcark"

    def __init__(self, **kwargs):
        self.api_key = kwargs.get("api_key") or os.environ.get(
            kwargs.get("api_key_env", "ARK_API_KEY"), ""
        )
        self.base_url = kwargs.get("base_url", "https://ark.cn-beijing.volces.com/api/v3").rstrip("/")
        self.model = kwargs.get("model", "")  # ep-xxxxx 接入点 ID
        self.ref_type_list = kwargs.get("ref_type_list", ["ip"])

    def generate(self, prompt, size, mode="background", **kwargs):
        """文生图：仅 prompt + size。"""
        if not self.api_key:
            raise ProviderError("未配置 ARK_API_KEY：设置环境变量 ARK_API_KEY，或传 api_key")
        w, h = size
        payload = {
            "model": self.model,
            "prompt": prompt,
            "size": f"{w}x{h}",
            "response_format": "url",
            "n": kwargs.get("n", 1),
        }
        if kwargs.get("seed") is not None:
            payload["seed"] = kwargs["seed"]
        return self._request(payload, **kwargs)

    def img2img(self, image, prompt, size=None, **kwargs):
        """图生图：image（Data URI）+ ref_type_list 控制参考强度。"""
        if not self.api_key:
            raise ProviderError("未配置 ARK_API_KEY")

        if size is None:
            size = image.size
        w, h = size

        buf = io.BytesIO()
        image.convert("RGB").save(buf, format="PNG")
        b64 = base64.b64encode(buf.getvalue()).decode("ascii")

        payload = {
            "model": self.model,
            "prompt": prompt,
            "image": f"data:image/png;base64,{b64}",
            "size": f"{w}x{h}",
            "response_format": "url",
            "n": kwargs.get("n", 1),
            "ref_type_list": kwargs.get("ref_type_list", self.ref_type_list),
        }
        if kwargs.get("seed") is not None:
            payload["seed"] = kwargs["seed"]
        return self._request(payload, **kwargs)

    def _request(self, payload, **kwargs):
        import requests

        headers = {"Authorization": f"Bearer {self.api_key}"}
        max_retries = int(kwargs.get("retries", 3))
        backoff = float(kwargs.get("retry_backoff", 20))

        for attempt in range(max_retries + 1):
            r = requests.post(
                f"{self.base_url}/images/generations",
                json=payload,
                headers=headers,
                timeout=kwargs.get("timeout", 300),
            )
            if r.status_code == 429:
                if attempt < max_retries:
                    log.warning("429 限流，%ds 后重试（%d/%d）", backoff, attempt + 1, max_retries)
                    time.sleep(backoff)
                    continue
                raise ProviderError(f"重试 {max_retries} 次后仍被限流（429）")
            r.raise_for_status()
            return GenerateResult(
                image=self._extract(r.json()),
                meta={"provider": self.name, "model": self.model},
            )
        raise ProviderError(f"请求失败（{max_retries} 次重试耗尽）")

    @staticmethod
    def _extract(data):
        items = data.get("data") or []
        if not items:
            raise ProviderError(f"Ark 响应无图片: {data}")
        first = items[0]
        if first.get("b64_json"):
            return image_from_b64(first["b64_json"])
        if first.get("url"):
            return image_from_url(first["url"])
        raise ProviderError(f"无法解析 Ark 图片字段: {first}")
