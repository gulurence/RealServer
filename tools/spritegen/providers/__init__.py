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
