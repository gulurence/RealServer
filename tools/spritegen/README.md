# spritegen — AI 资产生成流水线

给 `soulofgun` 客户端生成**手绘风背景/UI + 像素风 sprite** 的命令行工具。
生成层做成**可插拔 provider**，后处理层把 AI 直出图变成游戏能直接用的资产
（去背景 + 缩放/像素下采样 + 图集打包）。

## 快速开始

```bash
cd tools/spritegen
pip install -r requirements.txt     # Pillow + PyYAML（真实 provider 再加 requests）

python spritegen.py                  # 用 mock 占位跑通全流程，产物在 out/
python spritegen.py --dry-run        # 只看会生成什么，不实际生成
python spritegen.py --asset icon_coin
```

`provider: mock` 时无需任何 key，生成的是**确定性程序图**（同名 prompt 每次一致），
用于验证「配置 → 生成 → 后处理 → 落盘」链路；不同 prompt 会得到不同配色的占位图。

## 接真实后端

编辑 `config.yaml` 顶部的 `provider` 与 `provider_config`。

**SiliconFlow（国内直连，已实测可用）**
- API Key 放**环境变量** `SILICONFLOW_API_KEY`（不入 git）。本机已设到 User 级环境变量：
  ```powershell
  [Environment]::SetEnvironmentVariable("SILICONFLOW_API_KEY","sk-xxx","User")
  ```
  （新开的终端才生效；旧终端需重开。）
- 模型填 `provider_config.model`。**FLUX 系列已禁用**，可用文生图模型：
  `Kwai-Kolors/Kolors`（当前默认）、`Qwen/Qwen-Image`、`Tongyi-MAI/Z-Image(-Turbo)`。
- 有每分钟生成数限流（IPM），批量生成建议分次跑或只跑单个 `--asset`。

**本地 ComfyUI**：`provider: comfyui`，填 `base_url` 与 `workflow_template`（一个 JSON 工作流，
provider 会自动改写其中的正向/负向 CLIPTextEncode 和 EmptyLatentImage 节点）。

**Gemini**：`provider: gemini`，设 `GEMINI_API_KEY`（国内需代理）。

未配 key 时不会静默失败，而是抛出明确的 `ProviderError` 提示缺哪个 key。

> 透明背景说明：AI 模型不能真出透明，UI/sprite 资产提示词请写**纯白背景**，
> 由后处理的色键去除摘掉。要更干净的抠图效果可 `pip install rembg`（自动优先使用）。

## 配置说明

```yaml
provider: mock
provider_config: {}          # 只填当前 provider 的字段，其余会被忽略
output:
  root: ./out                # 相对本配置文件
  sync_to: ""                # 客户端资产源目录的绝对路径，生成后自动同步
assets:
  - name: bg_warm_village    # 输出文件名 name.png
    mode: background         # background(不透明全出血) | ui(去背景透明) | sprite(像素风)
    prompt: 手绘风...        # 生成提示词
    size: [1280, 720]        # 最终尺寸
    gen_size: [512, 512]     # 可选，AI 生成尺寸（sprite 建议大图生成再下采样）
    out_subdir: map          # 输出到 out/map/
    fit: cover               # cover|contain|stretch|none（sprite 忽略）
    remove_bg: true          # 可选，覆盖 mode 默认行为
    tolerance: 40            # 色键去背景容差
    frames: 4                # 可选，>1 时生成多帧并打包成 plist+png 图集
    palette: 16              # 可选，sprite 锁色板（median-cut 量化）
    max_width: 1024          # 可选，图集最大宽度
```

- `mode: sprite`：`gen_size` 高分辨率生成 → 最近邻下采样到 `size` →（`frames>1` 时）打包成
  **TexturePacker format=2** 的 `plist + png` 图集，帧命名 `name_0.png`/`name_1.png`…，与客户端
  `anim/` 现有图集同格式，可直接 `addSpriteFramesWithFile` 加载。
- 注意：多帧动画的「角色一致性」要靠 provider 层保证（如 ComfyUI 用同一 base + 姿态控制）。
  mock 只是用 seed 递增造出不同占位帧，真实动画请用动画工作流。

## 部署进客户端

客户端有两种资源路径（`MyHead.h` 的 `DATADECODE_HEAD="zip::"`）：

| out_subdir | 运行时来源 |
|---|---|
| `map` `pic` `plist` `particle` `fonts` `out` | `Debug.win32/res`（zip 存档，经 `zip::` 读） |
| `anim` `raw` | `Debug.win32/` 下的松散文件 |

1. **同步源目录**：`output.sync_to` 指向 `UIEditor/CocoStudio/assets`（编辑器松散源，与存档同构），
   每次生成自动拷贝到位。
2. **部署运行时**（让改动在游戏里生效）：
   ```bash
   python spritegen.py --res-zip E:/RealServer/client/soulofgun/Debug.win32/res \
                       --loose-dir E:/RealServer/client/soulofgun/Debug.win32
   ```
   `res` 存档注入前会自动备份为 `res.bak`。注意：改 `res` 前先停掉正在跑的客户端
   （文件锁，见项目「deploy」约定）。

## 扩展

新增 provider：在 `providers/` 建 `xxx.py` 继承 `Provider` 实现 `generate()`（见 `mock.py`），
在 `providers/__init__.py` 的 `_REGISTRY` 注册，再在 `config.yaml` 补配置示例即可。
