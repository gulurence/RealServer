"""restyle_atlas.py — 图集「逐帧 img2img」改风格（重打包）。

从客户端资源里读 TexturePacker 图集（plist + png），逐帧 img2img 改风格后贴回原位置。
plist 元数据（帧矩形 / rotated / offset / sourceColorRect / sourceSize）一律不动，只替换像素，
所以贴回后 sprite 的裁剪与定位和原来完全一致。

关键正确性（2026-10-21 从客户端 CCSprite::setTextureCoords 反推 + 实测钉死）：
- rotated 帧：TexturePacker 把逻辑图「顺时针旋转 90°」存储，且 frame 矩形里 w/h 是**逻辑**宽高，
  实际像素占的是**转置**矩形 (x, y, x+h, y+w)。所以拆帧要：
    裁剪转置矩形 → 逆时针转正(PIL rotate(90)) → img2img → 顺时针转回(PIL rotate(-90)) → 贴回。
- alpha 通道按「存储朝向」原样保留（在贴回前套回），保证 sprite 轮廓/透明区不变。
- 429 限流：provider 内部退避重试；帧间再加 sleep 平滑（默认 ~12s，约 5 张/分钟）。

用法：
  python restyle_atlas.py --kind pic                       # 处理 res zip 里的 pic 图集
  python restyle_atlas.py --kind anim                      # 处理 Debug.win32/anim 松散图集
  python restyle_atlas.py --kind anim --atlas police --limit 5 --prompt "..."   # 逐帧试跑
  python restyle_atlas.py --kind anim --mode sheet --atlas police              # 整张试跑
  python restyle_atlas.py --kind anim --mode sheet --workers 6                # 整张全量（6 路并发）

两种模式：
  frame（逐帧）：每帧单独 img2img。小图（<32px 占比 46%）会被放大 100 倍→糊掉，且 2803 帧成本 ~¥841 / ~10 小时。
  sheet（整张，推荐）：把整张图集当一张大图 img2img。小图保持原尺寸不再糊，ref_type=ip
  锁住整张构图（每帧在 plist 矩形内结构相关 r≈0.97 实测），成本/时间降 ~100 倍（~35 张图集 / ¥35~50）。
  两阶段并发：整张 + 低 r 帧补绘（--fix-r）全部吃满 --workers 路（实测方舟 6 路并发无 429）。
"""
import argparse
import io
import json
import logging
import re
import threading
import time
import zipfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from PIL import Image
from plistlib import loads as plist_loads

from providers import get_provider

log = logging.getLogger("restyle")

RES_ZIP = Path(r"E:/RealServer/client/soulofgun/Debug.win32/res")
ANIM_DIR = Path(r"E:/RealServer/client/soulofgun/Debug.win32/anim")
EXT_DIR = Path(r"E:/RealServer/client/soulofgun/Debug.win32/extension")
OUT_DIR = Path(r"E:/RealServer/tools/spritegen/restyled")

# SiliconFlow Kolors 官方推荐尺寸（只有竖版/方形，横版靠「按宽高比就近 + 拉伸」）
PRESETS = [(1024, 1024), (960, 1280), (768, 1024), (720, 1440), (720, 1280)]

DEFAULT_PROMPT = (
    "魔幻奇幻风格重绘，保持原有构图、物体位置与轮廓完全不变，仅改变画风："
    "蓝紫色调、魔法光效、发光粒子、手绘精细质感、史诗氛围"
)

# 多 worker 并行处理多个图集时，state json 的读写要串行
STATE_LOCK = threading.Lock()


class TokenBucket:
    """全局令牌桶，限制每分钟请求数（0=不限）。

    多 worker 并发时靠它把整体吞吐压到账户 IPM 上限内，避免打爆 429。
    """

    def __init__(self, rpm):
        self.rpm = rpm
        self.capacity = max(1.0, float(rpm))
        self.tokens = self.capacity
        self.last = time.time()
        self.lock = threading.Lock()

    def acquire(self):
        if self.rpm <= 0:
            return
        while True:
            with self.lock:
                now = time.time()
                self.tokens = min(self.capacity,
                                  self.tokens + (now - self.last) * self.rpm / 60.0)
                self.last = now
                if self.tokens >= 1.0:
                    self.tokens -= 1.0
                    return
                need = (1.0 - self.tokens) * 60.0 / self.rpm
            time.sleep(need)


def parse_rect(s):
    """'{{x,y},{w,h}}' 或 {x,y} 或 {w,h} → [x,y,w,h]（不足 4 位补 0）。"""
    nums = [int(n) for n in re.findall(r"-?\d+", str(s))]
    while len(nums) < 4:
        nums.append(0)
    return nums[:4]


def best_preset(w, h):
    """按宽高比就近选一个官方预设尺寸（返回 (pw, ph)）。"""
    ar = w / h if h else 1.0
    return min(PRESETS, key=lambda p: abs(p[0] / p[1] - ar))


def frame_corr(a, b, n=16):
    """两张同尺寸图的灰度结构相关（-1..1，越高结构越像）。

    用作整张 img2img 后每帧的「保真度」打分：低分帧说明模型把那帧重画漂了，
    需要单独逐帧重绘补上。用 tobBytes()（灰度 L 模式每像素 1 字节）避免 getdata 弃用告警。
    """
    ga = a.convert("L").resize((n, n), Image.LANCZOS)
    gb = b.convert("L").resize((n, n), Image.LANCZOS)
    pa = list(ga.tobytes())
    pb = list(gb.tobytes())
    ma = sum(pa) / len(pa)
    mb = sum(pb) / len(pb)
    va = sum((q - ma) ** 2 for q in pa)
    vb = sum((q - mb) ** 2 for q in pb)
    if va == 0 or vb == 0:
        return 1.0
    return sum((q - ma) * (s - mb) for q, s in zip(pa, pb)) / (va ** 0.5 * vb ** 0.5)


def crop_stored(atlas, meta):
    """从图集里裁出某帧的「存储朝向」像素与 alpha。

    返回 (region_rgba, is_rotated)。region 的尺寸=实际存储矩形：
      - 非 rotated：frame {x,y,w,h} → (w, h)
      - rotated：   frame {x,y,w,h} → (h, w)（转置矩形）
    """
    x, y, w, h = parse_rect(meta["frame"])
    rotated = bool(meta.get("rotated", False))
    if rotated:
        region = atlas.crop((x, y, x + h, y + w))   # 转置：宽 h 高 w
    else:
        region = atlas.crop((x, y, x + w, y + h))
    return region, rotated


def unrotate(region):
    """存储朝向 → 逻辑正立朝向（逆时针 90°）。"""
    return region.rotate(90, expand=True)


def rerotate(region):
    """逻辑正立朝向 → 存储朝向（顺时针 90°）。"""
    return region.rotate(-90, expand=True)


def restyle_frame(provider, region, rotated, prompt):
    """对单帧 img2img 改风格，返回「存储朝向」的 RGBA 结果（alpha 保持原轮廓）。"""
    stored_alpha = region.getchannel("A")

    # 转正（rotated 才需要）后再喂模型，避免角色横着被重绘
    upright = unrotate(region) if rotated else region

    w, h = upright.size
    if w <= 0 or h <= 0:
        return region  # 空帧不动
    if provider.name == "volcark":
        # Seedream 只认方形/16:9 预设，统一方形 1024（拉伸；alpha 轮廓已单独保留，失真可控）
        pw, ph = 1024, 1024
    elif "image-edit" in provider.model.lower():
        # edit 模型内部定尺寸且保宽高比，这里按长边放大到 ~1024 避免拉伸失真
        scale = 1024 / max(w, h) if max(w, h) < 1024 else 1.0
        pw, ph = max(1, round(w * scale)), max(1, round(h * scale))
    else:
        pw, ph = best_preset(w, h)

    rgb = upright.convert("RGB").resize((pw, ph), Image.LANCZOS)
    r = provider.img2img(rgb, prompt, size=(pw, ph))
    restyled = r.image.convert("RGB").resize((w, h), Image.LANCZOS).convert("RGBA")

    # 转回存储朝向，再套回原 alpha（alpha 是存储朝向的，直接对上）
    if rotated:
        restyled = rerotate(restyled)
    restyled.putalpha(stored_alpha)
    return restyled


def sheet_target(w, h):
    """整张图集（拼接大图）的目标尺寸：Seedream 只认方形 / 16:9。

    按宽高比就近映射（2026-10-21 实测：ref_type=ip 锁构图，即使竖版压扁 2x
    再拉回，每帧在 plist 矩形内结构相关仍 r≈0.97，所以「压扁→方形→拉回」是安全的）：
      - 宽屏（ar>=1.4）→ 16:9 1920x1080
      - 大图（max>=1536，含 2048x2048 / 1024x2048 竖版）→ 2048x2048（不压扁，宽度放大 2x）
      - 常规 → 1024x1024
    """
    ar = w / h if h else 1.0
    if ar >= 1.4:
        return (1920, 1080)
    if max(w, h) >= 1536:
        return (2048, 2048)
    return (1024, 1024)


def restyle_sheet(provider, atlas, prompt):
    """整张图集 img2img（拼接大图方案），返回同尺寸 RGBA，alpha 原样保留。

    对比逐帧：小图不再被单独放大 100 倍→糊掉；整张构图由 ref_type=ip 锁住，
    每帧结构相关 r≈0.97（实测）；成本/时间降 ~100 倍（2803 帧 → ~35 张图集）。
    """
    w, h = atlas.size
    alpha = atlas.getchannel("A")
    pw, ph = sheet_target(w, h)
    rgb = atlas.convert("RGB").resize((pw, ph), Image.LANCZOS)
    r = provider.img2img(rgb, prompt, size=(pw, ph))
    restyled = r.image.convert("RGB").resize((w, h), Image.LANCZOS).convert("RGBA")
    restyled.putalpha(alpha)
    return restyled


def load_atlas(zip_path, plist_name, png_name, out_png):
    """读原图集 plist+png；若已有部分完成的 staging png 则续用它。"""
    if zip_path:
        z = zipfile.ZipFile(zip_path)
        data = plist_loads(z.read(plist_name))
        if out_png.exists():
            atlas = Image.open(out_png).convert("RGBA")
        else:
            atlas = Image.open(io.BytesIO(z.read(png_name))).convert("RGBA")
    else:
        data = plist_loads(open(plist_name, "rb").read())
        if out_png.exists():
            atlas = Image.open(out_png).convert("RGBA")
        else:
            atlas = Image.open(png_name).convert("RGBA")
    return data, atlas


def process_atlas(provider, zip_path, plist_name, png_name, out_png, prompt,
                  limit, delay, done, state_path, state, min_size=0, limiter=None):
    """处理一个图集，逐帧 img2img。done 为已完成的帧名集合（断点续跑）。

    每帧完成后同步落盘 png + state json，中途中断可精确续跑。
    """
    data, atlas = load_atlas(zip_path, plist_name, png_name, out_png)
    frames = list(data.get("frames", {}).items())

    todo = [(k, m) for k, m in frames if k not in done]
    if limit > 0:
        todo = todo[:limit]

    if not todo:
        log.info("图集 %s 已全部完成（%d 帧），跳过", plist_name, len(frames))
        return

    log.info("图集 %s：总 %d 帧，待处理 %d 帧", plist_name, len(frames), len(todo))
    for i, (key, meta) in enumerate(todo):
        x, y, w, h = parse_rect(meta["frame"])
        region, rotated = crop_stored(atlas, meta)
        if w <= 0 or h <= 0:
            log.warning("跳过空帧 %s (%dx%d)", key, w, h)
            done.add(key)
            continue
        if min_size and min(w, h) < min_size:
            log.info("跳过过小帧 %s (%dx%d < %d)", key, w, h, min_size)
            done.add(key)
            continue

        if limiter:
            limiter.acquire()                 # 全局限流（多 worker 共用一个令牌桶）
        t0 = time.time()
        new = restyle_frame(provider, region, rotated, prompt)
        if rotated:
            atlas.paste(new, (x, y))          # 存储矩形已由 restyle 恢复成 (h,w)
        else:
            atlas.paste(new, (x, y))

        done.add(key)
        out_png.parent.mkdir(parents=True, exist_ok=True)
        atlas.save(out_png, format="PNG")     # 每帧落盘，中断可续
        with STATE_LOCK:
            state[str(out_png)] = sorted(done)
            save_state(state_path, state)     # 进度同步落盘

        cost = time.time() - t0
        log.info("[%d/%d] %s (%dx%d%s) %.1fs", i + 1, len(todo), key, w, h,
                 "R" if rotated else "", cost)
        if delay and i < len(todo) - 1:
            time.sleep(delay)


def is_font_atlas(name):
    """字体/字模图集：重绘会破坏字形，且密集文本易触发内容审核误判（400）。"""
    n = Path(name).stem.lower()
    return n.startswith("text_") or any(k in n for k in ("shuzi", "wenzi", "zi_", "font"))


def sheet_load(zip_path, plist_name, png_name, out_png, skip_text=True):
    """读图集。已生成 / 字体字模 / 无帧 → 返回 None；否则 (data, atlas)。"""
    if out_png.exists():
        log.info("图集 %s 整张已生成，跳过", png_name)
        return None
    if zip_path:
        z = zipfile.ZipFile(zip_path)
        data = plist_loads(z.read(plist_name))
        atlas = Image.open(io.BytesIO(z.read(png_name))).convert("RGBA")
    else:
        data = plist_loads(open(plist_name, "rb").read())
        atlas = Image.open(png_name).convert("RGBA")

    w, h = atlas.size
    frames = data.get("frames", {})
    if skip_text and (is_font_atlas(png_name) or (w / h if h else 1.0) > 3.0):
        log.info("跳过字体/字模图集 %s（%dx%d）", png_name, w, h)
        return None
    if max(w, h) <= 64:
        log.info("跳过过小图集 %s（%dx%d，拉伸到 1024 会糊成渣）", png_name, w, h)
        return None
    if not frames:
        log.warning("图集 %s 无帧，跳过", png_name)
        return None
    return data, atlas


def sheet_fixups(data, atlas, restyled, fix_r):
    """整张重绘后找出需补绘的低 r 帧，返回 [(key, meta, box)]，box=存储朝向矩形。"""
    if fix_r <= 0:
        return []
    low = []
    for key, meta in data.get("frames", {}).items():
        x, y, fw, fh = parse_rect(meta["frame"])
        if fw <= 0 or fh <= 0:
            continue
        rotated = bool(meta.get("rotated", False))
        box = (x, y, x + fh, y + fw) if rotated else (x, y, x + fw, y + fh)
        if frame_corr(atlas.crop(box), restyled.crop(box)) < fix_r:
            low.append((key, meta, box))
    return low


def process_sheets(provider, atlases, prompt, skip_text, fix_r, workers,
                   state_path, state, limiter=None):
    """整张图集 img2img（拼接大图），两阶段并发：

    阶段1 并发整张重绘全部图集 → 阶段2 并发补绘所有低 r 帧 → 阶段3 统一落盘。
    整张 + 补绘共 ~165 次调用都吃满 workers 路并发，而非按图集串行（旧实现
    会让「补 5 帧的图集」卡一个 worker ~6 分钟、其余 worker 干等）。
    """
    # 阶段1：并发整张重绘（单张失败只跳过，不拖垮全批）
    def whole_sheet(a):
        zip_path, plist_name, png_name, out_png = a
        try:
            loaded = sheet_load(zip_path, plist_name, png_name, out_png, skip_text)
            if loaded is None:
                return None
            data, atlas = loaded
            if limiter:
                limiter.acquire()
            t0 = time.time()
            restyled = restyle_sheet(provider, atlas, prompt)
            log.info("整张图集 %s（%dx%d，%d 帧）%.1fs", png_name, atlas.size[0],
                     atlas.size[1], len(data.get("frames", {})), time.time() - t0)
            return {"out_png": out_png, "png_name": png_name, "data": data,
                    "atlas": atlas, "restyled": restyled}
        except Exception as e:
            log.error("整张图集 %s 失败，跳过：%s", png_name, e)
            return None

    with ThreadPoolExecutor(max_workers=max(1, workers)) as ex:
        sheets = [s for s in ex.map(whole_sheet, atlases) if s]

    # 阶段2：并发补绘所有低 r 帧（贴回在主线串行，避免并发改同一张图）
    tasks = []
    for s in sheets:
        for key, meta, box in sheet_fixups(s["data"], s["atlas"], s["restyled"], fix_r):
            tasks.append((s, key, meta, box))
    if tasks:
        def fix_one(t):
            s, key, meta, box = t
            try:
                if limiter:
                    limiter.acquire()
                region, rotated = crop_stored(s["atlas"], meta)
                new = restyle_frame(provider, region, rotated, prompt)
                return s, key, box, new
            except Exception as e:
                log.error("补绘帧 %s 失败，跳过：%s", key, e)
                return None
        with ThreadPoolExecutor(max_workers=max(1, workers)) as ex:
            for r in ex.map(fix_one, tasks):
                if r is None:
                    continue
                s, key, box, new = r
                s["restyled"].paste(new, (box[0], box[1]))
        log.info("并发补绘完成：%d 帧", len(tasks))

    # 阶段3：统一落盘 + state
    for s in sheets:
        s["out_png"].parent.mkdir(parents=True, exist_ok=True)
        s["restyled"].save(s["out_png"], format="PNG")
        state[str(s["out_png"])] = ["*sheet*"]
    with STATE_LOCK:
        save_state(state_path, state)
    log.info("整张模式完成：%d 张图集落盘", len(sheets))


def load_state(state_path):
    if state_path.exists():
        try:
            return json.loads(state_path.read_text(encoding="utf-8"))
        except (ValueError, OSError):
            return {}
    return {}


def save_state(state_path, state):
    state_path.write_text(json.dumps(state, ensure_ascii=False, indent=1),
                          encoding="utf-8")


def iter_atlases(kind, atlas_filter):
    """产出 (zip_path, plist_name, png_name, out_png)。

    pic → res zip 内有帧的图集（pic/ map/ plist/）；anim → 松散图集（anim/ extension/）。
    只收含非空 frames 的，跳过粒子/配置等 0 帧 plist。
    """
    if kind == "pic":
        z = zipfile.ZipFile(RES_ZIP)
        names = set(z.namelist())
        for pl in sorted(n for n in names if n.endswith(".plist")):
            if not pl.startswith(("pic/", "map/", "plist/")):
                continue
            try:
                if not plist_loads(z.read(pl)).get("frames"):
                    continue
            except Exception:
                continue
            png = pl[:-len(".plist")] + ".png"
            if png not in names:
                log.warning("跳过无配对的 plist（缺 png）：%s", pl)
                continue
            name = Path(pl).stem
            if atlas_filter and atlas_filter not in name:
                continue
            yield RES_ZIP, pl, png, OUT_DIR / Path(pl).parent / f"{name}.png"
    elif kind == "anim":
        for pl in sorted([*ANIM_DIR.glob("*.plist"), *EXT_DIR.glob("*.plist")]):
            try:
                if not plist_loads(pl.read_bytes()).get("frames"):
                    continue
            except Exception:
                continue
            if atlas_filter and atlas_filter not in pl.stem:
                continue
            yield None, str(pl), str(pl.with_suffix(".png")), OUT_DIR / pl.parent.name / f"{pl.stem}.png"
    else:
        raise SystemExit(f"未知 kind: {kind}")


def main():
    ap = argparse.ArgumentParser(description="图集 img2img 改风格（逐帧 / 整张）")
    ap.add_argument("--kind", required=True, choices=["pic", "anim"])
    ap.add_argument("--mode", default="frame", choices=["frame", "sheet"],
                    help="frame=逐帧 img2img；sheet=整张图集拼接大图 img2img（快 ~100 倍，推荐）")
    ap.add_argument("--atlas", help="只处理名称含此串的图集（试跑用）")
    ap.add_argument("--limit", type=int, default=0, help="[frame] 每图集最多处理 N 帧（0=全部）")
    ap.add_argument("--prompt", default=DEFAULT_PROMPT)
    ap.add_argument("--delay", type=float, default=12.0, help="[frame] 帧间 sleep 秒数（限流）")
    ap.add_argument("--provider", default="volcark", help="生成后端：volcark / siliconflow / comfyui / gemini")
    ap.add_argument("--model", default="ep-20261021140943-r8mvc",
                    help="volcark 推理接入点 ep-xxxxx（默认 Flash；Pro=ep-20261021122148-hr2r2）")
    ap.add_argument("--ref-type", default="ip", help="volcark 图生图参考类型：ip(主体,构图最保真)/style(风格)")
    ap.add_argument("--min-size", type=int, default=0, help="[frame] 跳过 min(w,h) 小于此值的帧（0=全量）")
    ap.add_argument("--skip-text", action="store_true", default=True,
                    help="[sheet] 跳过字模条（数字/文字，ar>3），避免重绘破坏字形")
    ap.add_argument("--no-skip-text", action="store_false", dest="skip_text",
                    help="[sheet] 连字模条也一起重绘")
    ap.add_argument("--fix-r", type=float, default=0.5,
                    help="[sheet] 整张重绘后，结构相关低于此值的帧单独逐帧补绘（0=关闭）")
    ap.add_argument("--workers", type=int, default=1,
                    help="并发路数（sheet：整张+补绘全并发；frame：并行处理多个图集）")
    ap.add_argument("--rpm", type=float, default=0, help="全局每分钟请求上限（0=不限，靠 429 退避）")
    args = ap.parse_args()

    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")

    provider = get_provider(args.provider, model=args.model,
                           ref_type_list=[args.ref_type])
    state_path = OUT_DIR / "restyle_state.json"
    state = load_state(state_path)
    limiter = TokenBucket(args.rpm)

    atlases = list(iter_atlases(args.kind, args.atlas))

    if args.mode == "sheet":
        process_sheets(provider, atlases, args.prompt, args.skip_text, args.fix_r,
                       args.workers, state_path, state, limiter)
        return

    def run_one(a):
        zip_path, plist_name, png_name, out_png = a
        key = str(out_png)
        with STATE_LOCK:
            done = set(state.get(key, []))
        process_atlas(provider, zip_path, plist_name, png_name, out_png,
                      args.prompt, args.limit, args.delay, done, state_path, state,
                      args.min_size, limiter)
        with STATE_LOCK:
            state[key] = sorted(done)
            save_state(state_path, state)

    if args.workers > 1:
        with ThreadPoolExecutor(max_workers=args.workers) as ex:
            list(ex.map(run_one, atlases))
    else:
        for a in atlases:
            run_one(a)


if __name__ == "__main__":
    main()
