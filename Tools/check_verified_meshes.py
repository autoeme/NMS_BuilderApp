#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
pre-commit проверка: не меняет ли коммит уже ПРОВЕРЕННЫЕ меши.
Сверяет staged .uasset с эталонными хэшами из Content/NMSData/verified_meshes.json.
Если проверенный меш изменился (возможная порча от нового импорта) — предупреждает и
блокирует коммит (exit 1). Намеренная правка: обнови verified_meshes.json или --no-verify.
"""
import json, subprocess, sys, os, re, hashlib

try:
    sys.stderr.reconfigure(encoding="utf-8")
except Exception:
    pass

def run(args):
    return subprocess.check_output(args)

try:
    root = run(["git", "rev-parse", "--show-toplevel"]).decode().strip()
except Exception:
    sys.exit(0)

vf = os.path.join(root, "Content", "NMSData", "verified_meshes.json")
if not os.path.exists(vf):
    sys.exit(0)
verified = json.load(open(vf, encoding="utf-8")).get("meshes", {})
if not verified:
    sys.exit(0)

staged = run(["git", "diff", "--cached", "--name-only", "--diff-filter=ACM"]).decode("utf-8", "replace").splitlines()

def staged_hash(path):
    try:
        blob = run(["git", "show", ":" + path])
    except Exception:
        return None
    if blob[:22] == b"version https://git-l":
        m = re.search(rb"oid sha256:([0-9a-f]{64})", blob)
        return m.group(1).decode() if m else None
    return hashlib.sha256(blob).hexdigest()

hits = []
for p in staged:
    p2 = p.replace("\\", "/")
    if p2 in verified:
        h = staged_hash(p)
        if h and h != verified[p2]:
            hits.append(p2)

if hits:
    sys.stderr.write("\n⚠️  ВНИМАНИЕ: коммит меняет УЖЕ ПРОВЕРЕННЫЕ меши (возможна порча от импорта):\n")
    for h in hits:
        sys.stderr.write("   - " + h + "\n")
    sys.stderr.write("Проверь их (превью/вершины). Откат: git checkout -- <файл>.\n")
    sys.stderr.write("Если правка НАМЕРЕННАЯ: обнови Content/NMSData/verified_meshes.json (Tools/rebuild_verified.py) или git commit --no-verify.\n\n")
    sys.exit(1)
sys.exit(0)
