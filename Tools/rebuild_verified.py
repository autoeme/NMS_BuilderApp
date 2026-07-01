#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Обновить Content/NMSData/verified_meshes.json: занести/пересчитать хэши мешей
для указанных категорий (проверенных групп). Запуск после закрытия группы:
    python Tools/rebuild_verified.py "Furnishings" "Legacy Structures"
Без аргументов — пересчитывает хэши УЖЕ имеющихся в списке мешей (после намеренной правки).
"""
import json, hashlib, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CAT = sys.argv[1:]

def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(65536), b""):
            h.update(b)
    return h.hexdigest()

vf = os.path.join(ROOT, "Content", "NMSData", "verified_meshes.json")
data = {"_note": "Проверенные меши (целость+ориентация). Хук pre-commit сверяет хэши. Пополнять Tools/rebuild_verified.py.", "meshes": {}}
if os.path.exists(vf):
    data = json.load(open(vf, encoding="utf-8"))
    data.setdefault("meshes", {})

meshes = data["meshes"]

if CAT:
    db = json.load(open(os.path.join(ROOT, "Content", "nms_parts_db.json"), encoding="utf-8"))
    rows = db if isinstance(db, list) else db.get("Rows", db)
    it = rows.values() if isinstance(rows, dict) else rows
    want = set(CAT)
    added = 0
    for r in it:
        if r.get("Category") not in want:
            continue
        mp = r.get("ModelPath") or ""
        if not mp:
            continue
        rel = "Content/NMSBaseBuilder/GameMeshes/%s.uasset" % mp.split("/")[-1].split(".")[0]
        ap = os.path.join(ROOT, rel.replace("/", os.sep))
        if os.path.exists(ap):
            meshes[rel] = sha(ap); added += 1
    print("Категории %s: занесено/обновлено %d мешей" % (CAT, added))
else:
    # пересчитать хэши имеющихся (после намеренных правок)
    n = 0
    for rel in list(meshes):
        ap = os.path.join(ROOT, rel.replace("/", os.sep))
        if os.path.exists(ap):
            meshes[rel] = sha(ap); n += 1
    print("Пересчитано хэшей: %d" % n)

json.dump(data, open(vf, "w", encoding="utf-8"), indent=1)
print("Всего в verified_meshes.json:", len(meshes))
