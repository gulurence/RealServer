#!/usr/bin/env python3
"""spritegen：AI 资产生成流水线入口。

用法：
    python spritegen.py                          # 读默认 config.yaml
    python spritegen.py --asset btn_confirm       # 只生成某个资产
    python spritegen.py --provider mock
    python spritegen.py --dry-run                 # 只打印将生成什么，不实际生成
    # 生成后部署进客户端运行时（zip 类进 res 存档，松散类进 Debug.win32）
    python spritegen.py --res-zip E:/.../Debug.win32/res --loose-dir E:/.../Debug.win32
"""
import argparse
import logging
import shutil
import sys
import time
from pathlib import Path

try:
    import yaml
except ImportError:
    sys.exit("缺少 PyYAML，请先执行: pip install -r requirements.txt")

from postprocess import process
from providers import get_provider

log = logging.getLogger("spritegen")


def load_config(path: str) -> dict:
    path = Path(path)
    with path.open("r", encoding="utf-8") as f:
        cfg = yaml.safe_load(f)
    cfg["_config_dir"] = path.parent
    return cfg


def _resolve(cfg: dict, p: str) -> Path:
    p = Path(p)
    if not p.is_absolute():
        p = (cfg["_config_dir"] / p).resolve()
    return p


def _prep(asset: dict) -> dict:
    size = asset.get("size", [1024, 1024])
    return {
        "name": asset["name"],
        "mode": asset.get("mode", "background"),
        "prompt": asset.get("prompt", ""),
        "size": tuple(size),
        "gen_size": tuple(asset.get("gen_size", size)),
        "out_subdir": asset.get("out_subdir", ""),
        "fit": asset.get("fit", "cover"),
        "remove_bg": asset.get("remove_bg", None),
        "tolerance": asset.get("tolerance", 40),
        "seed": asset.get("seed", None),
        "frames": int(asset.get("frames", 1)),
        "palette": asset.get("palette", None),
        "pack": bool(asset.get("pack", False)),
        "max_width": int(asset.get("max_width", 1024)),
    }


def generate_asset(provider, asset: dict, out_root: Path) -> list[Path]:
    a = _prep(asset)
    if a["frames"] > 1 or a["pack"]:
        return _generate_frames(provider, a, out_root)

    log.info("生成 %s [%s] %dx%d", a["name"], a["mode"], a["size"][0], a["size"][1])
    result = provider.generate(a["prompt"], a["gen_size"], mode=a["mode"], seed=a["seed"])
    img = process(result.image, a["mode"], a["size"],
                  remove_bg=a["remove_bg"], fit=a["fit"], tolerance=a["tolerance"], palette=a["palette"])

    dest_dir = out_root / a["out_subdir"]
    dest_dir.mkdir(parents=True, exist_ok=True)
    dest = dest_dir / f"{a['name']}.png"
    img.save(dest)
    log.info("  -> %s (meta=%s)", dest, result.meta)
    return [dest]


def _generate_frames(provider, a: dict, out_root: Path) -> list[Path]:
    from pack import pack

    dest_dir = out_root / a["out_subdir"]
    dest_dir.mkdir(parents=True, exist_ok=True)

    frames = []
    for i in range(a["frames"]):
        s = None if a["seed"] is None else a["seed"] + i
        log.info("生成 %s 第 %d/%d 帧", a["name"], i + 1, a["frames"])
        result = provider.generate(a["prompt"], a["gen_size"], mode=a["mode"], seed=s)
        img = process(result.image, a["mode"], a["size"],
                      remove_bg=a["remove_bg"], fit=a["fit"], tolerance=a["tolerance"], palette=a["palette"])
        frames.append((f"{a['name']}_{i}.png", img))

    png_path, plist_path = pack(frames, a["name"], dest_dir, max_width=a["max_width"])
    log.info("  -> 图集 %s / %s（%d 帧）", png_path.name, plist_path.name, len(frames))
    return [png_path, plist_path]


def sync_outputs(cfg: dict, generated: list[Path], out_root: Path) -> None:
    sync_to = cfg["output"].get("sync_to", "")
    if not sync_to:
        return
    sync_root = _resolve(cfg, sync_to)
    for src in generated:
        rel = src.relative_to(out_root)
        dst = sync_root / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        log.info("  同步 -> %s", dst)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="AI 资产生成流水线")
    ap.add_argument("--config", default="config.yaml")
    ap.add_argument("--provider", default=None, help="覆盖 config 里的 provider")
    ap.add_argument("--asset", default=None, help="只生成指定 name")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--res-zip", default=None, help="客户端 res zip 路径，生成后注入 zip 类资产")
    ap.add_argument("--loose-dir", default=None, help="客户端运行时目录，生成后拷入松散资产(anim/raw)")
    ap.add_argument("--delay", type=float, default=0.0, help="每张图之间的间隔秒数（应对 API 每分钟限流）")
    args = ap.parse_args(argv)

    # Windows 控制台默认 GBK，强制 UTF-8 避免中文日志乱码
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except Exception:
            pass

    logging.basicConfig(level=logging.INFO, format="[%(name)s] %(levelname)s %(message)s")

    cfg = load_config(args.config)
    provider_name = args.provider or cfg.get("provider", "mock")
    provider = get_provider(provider_name, **cfg.get("provider_config", {}))
    out_root = _resolve(cfg, cfg["output"].get("root", "./out"))

    assets = cfg.get("assets", [])
    if args.asset:
        assets = [a for a in assets if a.get("name") == args.asset]
        if not assets:
            log.error("未找到资产: %s", args.asset)
            return 1

    if args.dry_run:
        for a in assets:
            print(f"[dry-run] {a['name']} ({a.get('mode')}) {a.get('size')} -> {out_root / a.get('out_subdir', '')}")
        return 0

    generated = []
    ok = 0
    for idx, a in enumerate(assets):
        try:
            generated.extend(generate_asset(provider, a, out_root))
            ok += 1
        except Exception:
            log.exception("生成 %s 失败", a.get("name"))
        if args.delay and idx < len(assets) - 1:
            time.sleep(args.delay)

    sync_outputs(cfg, generated, out_root)

    if generated and (args.res_zip or args.loose_dir):
        from respack import deploy
        deploy(generated, out_root, res_zip=args.res_zip, loose_dir=args.loose_dir)

    log.info("完成：%d/%d 个资产生成成功", ok, len(assets))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
