# -*- coding: utf-8 -*-
# Сравнивает СТАРЫЙ меш (наш конвейер) и ПЛАГИН-меш (FBX по ID) по геометрии/UV/слотам.
# Цель: понять, ляжет ли текстура на плагин-меш как на старый (совпадают ли UV/раскладка).
# Результат -> MeshSrc/_uv_compare.txt (Claude прочтёт).
# Запуск: Tools -> Execute Python, ИЛИ py "...\compare_uv.py"
import unreal, os, json

CONTENT = unreal.SystemLibrary.get_project_content_directory()
SRC = os.path.normpath(os.path.join(CONTENT, "..", "MeshSrc"))
PKG = "/Game/NMSBaseBuilder/GameMeshes"

# тестовые детали: (имя детали = плагин-меш, старый меш)
TESTS = [
    ("S_WALL",        "stone_basic_wall"),
    ("T_WALL",        "timber_basic_wall"),
    ("W_WALL",        "wood_basic_wall"),
    ("S_DOORB0",      "builders_basic_wallb_door0"),
    ("ABAND_BARREL",  "barrel"),
    ("S_FLOOR",       "stone_basic_floor"),
]

def mesh_info(asset_name):
    path = PKG + "/" + asset_name + "." + asset_name
    sm = unreal.load_object(None, path)
    if not sm:
        return None
    info = {}
    try: info["verts"] = sm.get_num_vertices(0)
    except Exception: info["verts"] = -1
    try: info["tris"] = sm.get_num_triangles(0)
    except Exception: info["tris"] = -1
    try: info["uv_channels"] = sm.get_num_uv_channels(0)
    except Exception: info["uv_channels"] = -1
    try: info["materials"] = sm.get_num_sections(0)
    except Exception:
        try: info["materials"] = len(sm.static_materials)
        except Exception: info["materials"] = -1
    try:
        b = sm.get_bounding_box()
        ext = b.max - b.min
        info["size"] = (round(ext.x), round(ext.y), round(ext.z))
    except Exception:
        info["size"] = (0,0,0)
    return info

lines = ["UV/GEOMETRY СРАВНЕНИЕ: старый меш vs плагин-меш", "="*55]
for part, oldmesh in TESTS:
    pi = mesh_info(part)       # плагин-меш (по ID детали)
    oi = mesh_info(oldmesh)    # старый меш
    lines.append("")
    lines.append("ДЕТАЛЬ %s:" % part)
    lines.append("  СТАРЫЙ  (%s): %s" % (oldmesh, oi))
    lines.append("  ПЛАГИН  (%s): %s" % (part, pi))
    if pi and oi:
        same_v = abs(pi["verts"]-oi["verts"]) <= max(2, oi["verts"]*0.02)
        same_uv = pi["uv_channels"]==oi["uv_channels"]
        same_mat = pi["materials"]==oi["materials"]
        same_size = pi["size"]==oi["size"] or all(abs(a-b)<5 for a,b in zip(pi["size"],oi["size"]))
        verdict = "СОВПАДАЮТ (текстура ляжет так же)" if (same_v and same_uv and same_mat) else "РАЗНЫЕ (текстура поедет)"
        lines.append("  -> верш=%s uv=%s слоты=%s размер=%s : %s" % (same_v,same_uv,same_mat,same_size,verdict))

txt = "\n".join(lines)
unreal.log(txt)
with open(os.path.join(SRC, "_uv_compare.txt"), "w", encoding="utf-8") as f:
    f.write(txt + "\n")
try: unreal.EditorDialog.show_message("UV сравнение", "Готово. Результат записан, скажи Claude.", unreal.AppMsgType.OK)
except Exception: pass
