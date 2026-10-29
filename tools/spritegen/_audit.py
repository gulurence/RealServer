import re, zipfile, io, glob, os
from PIL import Image
from plistlib import loads as plist_loads
import restyle_atlas as R

CLIENT = r"E:/RealServer/client/soulofgun/Debug.win32"
RES = CLIENT + "/res"
rows = []
all_r = []


def per_frame(rsrc, rout, data):
    rs = []
    for k, m in data["frames"].items():
        x, y, w, h = [int(n) for n in re.findall(r"-?\d+", str(m["frame"]))][:4]
        rot = bool(m.get("rotated"))
        box = (x, y, x + h, y + w) if rot else (x, y, x + w, y + h)
        rs.append(R.frame_corr(rsrc.crop(box), rout.crop(box)))
    return rs


def add_row(name, src, out, data):
    rs = per_frame(src, out, data)
    rs.sort()
    n = len(rs)
    all_r.extend(rs)
    rows.append((name, n, rs[n // 2], sum(rs) / n, sum(1 for r in rs if r < 0.5)))


# anim + extension 松散图集
for out in sorted(glob.glob("restyled/anim/*.png") + glob.glob("restyled/extension/*.png")):
    stem = os.path.splitext(os.path.basename(out))[0]
    for d in ("anim", "extension"):
        p = f"{CLIENT}/{d}/{stem}.png"
        pl = f"{CLIENT}/{d}/{stem}.plist"
        if os.path.exists(p) and os.path.exists(pl):
            add_row(f"{d}/{stem}", Image.open(p).convert("RGBA"),
                    Image.open(out).convert("RGBA"),
                    plist_loads(open(pl, "rb").read()))
            break

# pic + map zip 图集
z = zipfile.ZipFile(RES)
for out in sorted(glob.glob("restyled/pic/*.png") + glob.glob("restyled/map/*.png")):
    stem = os.path.splitext(os.path.basename(out))[0]
    for prefix in ("pic/", "map/"):
        png = prefix + stem + ".png"
        pl = prefix + stem + ".plist"
        if png in z.namelist():
            add_row(prefix + stem, Image.open(io.BytesIO(z.read(png))).convert("RGBA"),
                    Image.open(out).convert("RGBA"),
                    plist_loads(z.read(pl)))
            break

print(f"{'图集':26} {'帧数':>5} {'中位r':>7} {'均值r':>7} {'<0.5':>5}")
print("-" * 54)
for name, n, med, avg, low in rows:
    flag = "  <-- 需关注" if low > 0 else ""
    print(f"{name:26} {n:5} {med:7.3f} {avg:7.3f} {low:5}{flag}")

all_r.sort()
N = len(all_r)
print("-" * 54)
print(f"总计 {len(rows)} 张图集 / {N} 帧")
print(f"整体中位 r={all_r[N//2]:.3f}  均值 r={sum(all_r)/N:.3f}")
print(f">=0.7: {sum(1 for r in all_r if r>=0.7)} ({sum(1 for r in all_r if r>=0.7)/N:.1%})")
print(f"<0.5:  {sum(1 for r in all_r if r<0.5)} ({sum(1 for r in all_r if r<0.5)/N:.1%})")
print(f"<0.3:  {sum(1 for r in all_r if r<0.3)}")
