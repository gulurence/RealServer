#
# Copyright (c) 2026 GuMingYao,  All rights reserved
#
# File name: comfyui.py
# Purpose:   ComfyUI 后端 provider
#
# 对接本地或远端 ComfyUI HTTP 接口，POST /prompt 提交工作流后轮询 /history/{id} 再 GET /view 取 PNG
# _patch 自动改写工作流模板中的 CLIPTextEncode 正负提示词与 EmptyLatentImage 宽高
# 需项目自备 workflow 模板 JSON，模板其余节点保持不动
#

"""ComfyUI provider：本地/远端 ComfyUI 的 HTTP 接口。

流程：POST /prompt 提交工作流 → 轮询 /history/{id} → 取第一张图 → GET /view 拿 PNG。
需要一个项目自备的 workflow 模板 JSON，模板里的正/负向 CLIPTextEncode 与
EmptyLatentImage 节点会被本 provider 自动改写（其余节点保持不动）。
"""
import json
import time
import uuid
from pathlib import Path

from .base import GenerateResult, Provider, ProviderError


class ComfyUIProvider(Provider):
    name = "comfyui"

    def __init__(self, **kwargs):
        self.base_url = kwargs.get("base_url", "http://127.0.0.1:8188").rstrip("/")
        self.template = kwargs.get("workflow_template", "")

    def generate(self, prompt, size, mode="background", **kwargs):
        if not self.template:
            raise ProviderError("ComfyUI 需要 provider_config.workflow_template 指向一个工作流 JSON 模板")
        workflow = json.loads(Path(self.template).read_text(encoding="utf-8"))
        self._patch(workflow, prompt, size)

        import requests

        client_id = str(uuid.uuid4())
        r = requests.post(f"{self.base_url}/prompt", json={"prompt": workflow, "client_id": client_id}, timeout=60)
        r.raise_for_status()
        prompt_id = r.json()["prompt_id"]

        img = self._poll(requests, prompt_id, timeout=kwargs.get("timeout", 300))
        return GenerateResult(image=img, meta={"provider": self.name, "prompt_id": prompt_id})

    def _patch(self, workflow, prompt, size):
        w, h = size
        pos_set = neg_set = False
        for node in workflow.values():
            ct = node.get("class_type")
            if ct == "CLIPTextEncode":
                if not pos_set:
                    node.setdefault("inputs", {})["text"] = prompt
                    pos_set = True
                elif not neg_set:
                    node.setdefault("inputs", {})["text"] = "lowres, blurry, text, watermark"
                    neg_set = True
            elif ct == "EmptyLatentImage":
                node.setdefault("inputs", {}).update({"width": w, "height": h, "batch_size": 1})

    def _poll(self, requests, prompt_id, timeout):
        import io

        from PIL import Image

        deadline = time.time() + timeout
        while time.time() < deadline:
            r = requests.get(f"{self.base_url}/history/{prompt_id}", timeout=60)
            r.raise_for_status()
            entry = r.json().get(prompt_id)
            if entry:
                for out in entry.get("outputs", {}).values():
                    for im in out.get("images", []):
                        if im.get("type") == "output":
                            img_r = requests.get(
                                f"{self.base_url}/view",
                                params={"filename": im["filename"], "subfolder": im.get("subfolder", ""), "type": "output"},
                                timeout=60,
                            )
                            img_r.raise_for_status()
                            return Image.open(io.BytesIO(img_r.content)).convert("RGBA")
                raise ProviderError("工作流完成但未产出图片")
            time.sleep(2)
        raise ProviderError(f"ComfyUI 生成超时（{timeout}s）")
