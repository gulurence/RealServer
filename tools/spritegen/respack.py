"""respack.py: 把生成产物部署进客户端运行时。

客户端有两类资源路径：
1. zip 类（map/pic/plist/particle/fonts/out + 根级文件）—— 运行时经 zip:: 前缀从
   Debug.win32/res（一个 zip 存档）读取；
2. 松散类（anim/raw 等）—— 直接从 Debug.win32/ 目录读取。

deploy() 按 out_subdir 把生成文件分发到对应位置，zip 注入前自动备份为 res.bak。
"""
import logging
import shutil
import zipfile
from pathlib import Path

log = logging.getLogger("respack")

ZIP_DIRS = {"map", "pic", "plist", "particle", "fonts", "out"}


def _is_zip_managed(rel: str) -> bool:
    top = rel.split("/", 1)[0]
    return "/" not in rel or top in ZIP_DIRS


def _inject_zip(res_zip: Path, files: list[tuple[str, bytes]]) -> int:
    if not res_zip.exists():
        raise FileNotFoundError(f"找不到 res 存档: {res_zip}")
    bak = res_zip.with_name(res_zip.name + ".bak")
    if not bak.exists():
        shutil.copy2(res_zip, bak)
        log.info("已备份原存档 -> %s", bak)

    entries = {}
    with zipfile.ZipFile(res_zip, "r") as z:
        for info in z.infolist():
            entries[info.filename] = z.read(info.filename)
    for rel, data in files:
        entries[rel] = data

    with zipfile.ZipFile(res_zip, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in entries.items():
            z.writestr(name, data)
    return len(files)


def deploy(generated, out_root: Path, res_zip=None, loose_dir=None) -> int:
    """generated: 本次生成的文件路径列表；按类型分发到 zip 或松散目录。"""
    zip_files = []
    loose_files = []
    for p in generated:
        p = Path(p)
        rel = p.relative_to(out_root).as_posix()
        if _is_zip_managed(rel):
            zip_files.append((rel, p.read_bytes()))
        else:
            loose_files.append(p)

    n = 0
    if zip_files and res_zip:
        n += _inject_zip(Path(res_zip), zip_files)
        log.info("注入 %d 个文件到 %s", len(zip_files), res_zip)
    elif zip_files and not res_zip:
        log.warning("跳过 %d 个 zip 类文件（未指定 --res-zip）", len(zip_files))

    if loose_files and loose_dir:
        loose = Path(loose_dir)
        for p in loose_files:
            rel = p.relative_to(out_root)
            dst = loose / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(p, dst)
        n += len(loose_files)
        log.info("拷贝 %d 个松散文件到 %s", len(loose_files), loose_dir)
    elif loose_files and not loose_dir:
        log.warning("跳过 %d 个松散文件（未指定 --loose-dir）", len(loose_files))

    return n
