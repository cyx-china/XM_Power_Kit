# -*- coding: utf-8 -*-
"""批量文件操作的"计划"构建：把用户选中的本地/远端路径展开成具体动作。

不依赖 Qt，自检脚本可以直接测试；worker.py 只负责按计划执行并上报进度。
"""
from __future__ import annotations

import os

from protocol import LinkError

MAX_REMOTE_PATH = 100        # 设备端路径上限（字节，UTF-8）
FS_ATTR_DIR = 0x10


def join_remote(dir_path: str, name: str) -> str:
    if dir_path in ("", "/"):
        return "/" + name
    return dir_path.rstrip("/") + "/" + name


def human_size(n: int) -> str:
    if n < 1024:
        return "%d B" % n
    if n < 1024 * 1024:
        return "%.1f KB" % (n / 1024.0)
    return "%.2f MB" % (n / 1024.0 / 1024.0)


def build_upload_plan(local_paths, remote_dir: str):
    """展开本地文件/文件夹：返回 (远端目录列表[父在前], [(本地, 远端, 大小)], 总字节, 跳过的路径)"""
    dirs, files, skipped = [], [], []
    total = 0
    for lp in local_paths:
        lp = os.path.normpath(lp)
        if os.path.isdir(lp):
            base = os.path.basename(lp.rstrip(os.sep)) or "folder"
            rroot = join_remote(remote_dir, base)
            dirs.append(rroot)
            for dirpath, dirnames, filenames in os.walk(lp):
                rel = os.path.relpath(dirpath, lp)
                rdir = rroot if rel in (".", "") else rroot + "/" + rel.replace(os.sep, "/")
                for d in sorted(dirnames):
                    dirs.append(rdir + "/" + d)
                for f in sorted(filenames):
                    p = os.path.join(dirpath, f)
                    rp = rdir + "/" + f
                    if len(rp.encode("utf-8")) > MAX_REMOTE_PATH:
                        skipped.append(rp)
                        continue
                    size = os.path.getsize(p)
                    files.append((p, rp, size))
                    total += size
        elif os.path.isfile(lp):
            rp = join_remote(remote_dir, os.path.basename(lp))
            if len(rp.encode("utf-8")) > MAX_REMOTE_PATH:
                skipped.append(rp)
                continue
            size = os.path.getsize(lp)
            files.append((lp, rp, size))
            total += size
    if not files and not dirs:
        raise LinkError("没有可上传的文件")
    return dirs, files, total, skipped


def build_download_plan(dev, remotes):
    """返回 (本地相对目录列表, [(远端路径, 大小, 本地相对路径)])"""
    dirs, files = [], []
    for rp in remotes:
        rp = rp.rstrip("/")
        name = rp.rsplit("/", 1)[-1]
        attr, size = dev.fs_stat(rp)
        if attr & FS_ATTR_DIR:
            dirs.append(name)
            for sub, is_dir, sz in dev.walk(rp):
                rel = name + sub[len(rp):]
                if is_dir:
                    dirs.append(rel)
                else:
                    files.append((sub, sz, rel))
        else:
            files.append((rp, size, name))
    if not files:
        raise LinkError("没有可下载的文件")
    return dirs, files


def build_delete_plan(dev, remotes):
    """返回 (要删的目录[深的在前], 要删的文件)"""
    files, dirs = [], []
    for rp in remotes:
        rp = rp.rstrip("/")
        attr, _size = dev.fs_stat(rp)
        if attr & FS_ATTR_DIR:
            for sub, is_dir, _sz in dev.walk(rp):
                (dirs if is_dir else files).append(sub)
            dirs.append(rp)
        else:
            files.append(rp)
    dirs.sort(key=lambda p: p.count("/"), reverse=True)
    if not files and not dirs:
        raise LinkError("没有可删除的条目")
    return dirs, files
