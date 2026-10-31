#!/usr/bin/env python

#
# Copyright (c) 2026 GuMingYao,  All rights reserved
#
# File name: deploy_restyle.py
# Purpose:   把 restyled 重绘图集部署进客户端资源目录
#
# backup 先备份原松散图与 res 存档，deploy_loose 覆盖 anim、extension 松散 png
# 按 ZIP_MAP 重写 res zip 中指定 pic、map png，写临时 zip 校验后再原子替换
# 默认目标为 client/soulofgun/Debug.win32/Resources
#

# -*- coding: utf-8 -*-
"""把 restyled/ 的重绘图集部署进客户端资源目录。
- anim/*.png、extension/mf_p.png 是松散文件，直接覆盖。
- pic/*.png、map/mapPic1.png 在 res(zip) 里，重写 zip 替换 png（plist 不动）。
用法：python deploy_restyle.py [目标目录]
  默认目标 = client/soulofgun/Debug.win32/Resources
"""
import os
import shutil
import sys
import zipfile

CLIENT = r"E:/RealServer/client/soulofgun/Debug.win32"
DEFAULT_TARGET = os.path.join(CLIENT, "Resources")
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RESTYLED = os.path.join(SCRIPT_DIR, "restyled")

# zip 内要替换的 png（zip 内部路径 -> restyled 相对路径）
ZIP_MAP = {
    "pic/commonPic.png": "pic/commonPic.png",
    "pic/picInGameJinBi.png": "pic/picInGameJinBi.png",
    "pic/picInGame_fullScreen.png": "pic/picInGame_fullScreen.png",
    "pic/pic_In_gameUI.png": "pic/pic_In_gameUI.png",
    "pic/ui_store1.png": "pic/ui_store1.png",
    "map/mapPic1.png": "map/mapPic1.png",
}
LOOSE_SUBS = ("anim", "extension")  # 松散文件子目录


def backup(target):
    bak = os.path.join(target, "_restyle_backup")
    os.makedirs(bak, exist_ok=True)
    n = 0
    for sub in LOOSE_SUBS:
        src_dir = os.path.join(RESTYLED, sub)
        for f in os.listdir(src_dir):
            if not f.endswith(".png"):
                continue
            orig = os.path.join(target, sub, f)
            if os.path.exists(orig):
                dst = os.path.join(bak, sub, f)
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.copy2(orig, dst)
                n += 1
    res = os.path.join(target, "res")
    if os.path.exists(res):
        shutil.copy2(res, os.path.join(bak, "res"))
    print(f"[备份] {n} 张松散原图 + res -> {bak}")


def deploy_loose(target):
    n = 0
    for sub in LOOSE_SUBS:
        src_dir = os.path.join(RESTYLED, sub)
        for f in os.listdir(src_dir):
            if not f.endswith(".png"):
                continue
            dst = os.path.join(target, sub, f)
            shutil.copy2(os.path.join(src_dir, f), dst)
            n += 1
    print(f"[部署] 松散文件覆盖 {n} 张")


def deploy_zip(target):
    res = os.path.join(target, "res")
    tmp = res + ".tmp"
    with zipfile.ZipFile(res, "r") as zin:
        names_in = zin.namelist()
        with zipfile.ZipFile(tmp, "w", compression=zipfile.ZIP_DEFLATED) as zout:
            for info in zin.infolist():
                if info.filename in ZIP_MAP:
                    src = os.path.join(RESTYLED, ZIP_MAP[info.filename])
                    with open(src, "rb") as fp:
                        zout.writestr(info.filename, fp.read(),
                                      compress_type=zipfile.ZIP_DEFLATED)
                else:
                    zout.writestr(info.filename, zin.read(info.filename),
                                  compress_type=info.compress_type)
    # 校验后再替换
    with zipfile.ZipFile(tmp, "r") as z:
        assert z.testzip() is None, "重写后 zip 损坏"
        assert len(z.namelist()) == len(names_in), \
            f"条目数变化 {len(names_in)} -> {len(z.namelist())}"
    os.replace(tmp, res)
    print(f"[部署] res zip 替换 {len(ZIP_MAP)} 张 png（共 {len(names_in)} 条，其余原样）")


if __name__ == "__main__":
    target = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_TARGET
    print(f"目标目录：{target}")
    backup(target)
    deploy_loose(target)
    deploy_zip(target)
    print("完成。")
