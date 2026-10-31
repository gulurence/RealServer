#
# Copyright (c) 2026 GuMingYao,  All rights reserved
#
# File name: __init__.py
# Purpose:   provider 注册表与工厂入口
#
# _REGISTRY 维护 mock、siliconflow、comfyui、gemini、volcark 五类生成后端
# get_provider 按名字查找并实例化，未知名字抛 ProviderError 并列出可选后端
#

"""provider 注册表：按名字返回生成后端实例。"""
from .base import GenerateResult, Provider, ProviderError
from .comfyui import ComfyUIProvider
from .gemini import GeminiProvider
from .mock import MockProvider
from .siliconflow import SiliconFlowProvider
from .volcark import VolcArkProvider

_REGISTRY = {
    MockProvider.name: MockProvider,
    SiliconFlowProvider.name: SiliconFlowProvider,
    ComfyUIProvider.name: ComfyUIProvider,
    GeminiProvider.name: GeminiProvider,
    VolcArkProvider.name: VolcArkProvider,
}


def get_provider(name: str, **kwargs) -> Provider:
    cls = _REGISTRY.get(name)
    if cls is None:
        raise ProviderError(f"未知 provider: {name!r}，可选: {sorted(_REGISTRY)}")
    return cls(**kwargs)
