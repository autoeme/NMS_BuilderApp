# -*- coding: utf-8 -*-
# Ремонт мешей (двери-«плиты», составные) БЕЗ numpy — чистый Python.
# Конвертирует сцены игры -> OBJ (ВСЕ узлы LOD0), заменяет изменившиеся,
# переимпортирует их на месте (replace, без удаления).
# Запуск в редакторе: py "...\fix_doors.py"

import unreal, os, re, base64, math, struct

CONTENT = unreal.SystemLibrary.get_project_content_directory()
PROJ = os.path.normpath(os.path.join(CONTENT, ".."))
MESHSRC = os.path.join(PROJ, "MeshSrc")
GEO2 = r"C:\Users\User\Desktop\MBINCompiler\GEO2"
GEO1 = r"C:\Users\User\Desktop\MBINCompiler\GEO"
GEO3 = r"C:\Users\User\Desktop\MBINCompiler\GEO3"
PKG = "/Game/NMSBaseBuilder/GameMeshes"

def euler_to_mat(rx, ry, rz):
    rx, ry, rz = (math.radians(a) for a in (rx, ry, rz))
    cx,sx,cy,sy,cz,sz = math.cos(rx),math.sin(rx),math.cos(ry),math.sin(ry),math.cos(rz),math.sin(rz)
    # Rz @ Ry @ Rx
    return (
        (cz*cy, cz*sy*sx - sz*cx, cz*sy*cx + sz*sx),
        (sz*cy, sz*sy*sx + cz*cx, sz*sy*cx - cz*sx),
        (-sy,   cy*sx,            cy*cx),
    )

def b64d(s):
    s += "=" * (-len(s) % 4)
    return base64.b64decode(s)

def u10(b):
    return (b - 1024 if b >= 512 else b) / 511.0

def parse_nodes(sc):
    nodes = []
    for nd in re.split(r'value="TkSceneNodeData"', sc):
        nm = re.search(r'name="Name" value="([^"]+)"', nd)
        tp = re.search(r'name="Type" value="(\w+)"', nd)
        if not nm or not tp or tp.group(1) != "MESH": continue
        low = nm.group(1).lower()
        if "shadow" in low or "collision" in low: continue
        pairs = dict(re.findall(r'name="Name" value="(\w+)" />\s*<Property name="Value" value="([^"]*)"', nd))
        if "BATCHSTARTGRAPH" not in pairs: continue
        tr = re.search(r'TransX" value="([-\d.eE]+)" />\s*<Property name="TransY" value="([-\d.eE]+)" />\s*<Property name="TransZ" value="([-\d.eE]+)" />\s*<Property name="RotX" value="([-\d.eE]+)" />\s*<Property name="RotY" value="([-\d.eE]+)" />\s*<Property name="RotZ" value="([-\d.eE]+)" />\s*<Property name="ScaleX" value="([-\d.eE]+)" />\s*<Property name="ScaleY" value="([-\d.eE]+)" />\s*<Property name="ScaleZ" value="([-\d.eE]+)"', nd)
        tv = [float(x) for x in tr.groups()] if tr else [0,0,0,0,0,0,1,1,1]
        nodes.append(dict(name=nm.group(1), low=low,
                          b0=int(pairs["BATCHSTARTGRAPH"]), bc=int(pairs["BATCHCOUNT"]),
                          v0=int(pairs["VERTRSTARTGRAPH"]), v1=int(pairs["VERTRENDGRAPHIC"]), tv=tv))
    return nodes

def select_lod0(nodes):
    has0 = [i for i, n in enumerate(nodes) if "lod0" in n["low"]]
    if has0:
        sel = list(has0)                      # ВСЕ узлы LOD0 (стена + дверь!)
        start = has0[0]
        for i in range(start + 1, len(nodes)):
            if re.search(r'lod[1-9]', nodes[i]["low"]): break
            if i not in sel: sel.append(i)
        seen = set(); out = []
        for i in sorted(sel):
            if i not in seen: seen.add(i); out.append(nodes[i])
        return out
    rest = [n for n in nodes if not re.search(r'lod[1-9]|_lod', n["low"])]
    return rest if rest else nodes[:1]

def parse_entries(d):
    entries = {}
    for b in re.split(r'value="TkMeshData"', d)[1:]:
        idm = re.search(r'IdString" value="([^"]*)"', b)
        mdm = re.search(r'"MeshDataStream" value="([A-Za-z0-9+/=]*)"', b)
        ppm = re.search(r'"MeshPositionDataStream" value="([A-Za-z0-9+/=]*)"', b)
        if idm and mdm and ppm:
            entries[idm.group(1).upper()] = (mdm.group(1), ppm.group(1))
    return entries

def fin(x):
    return x if math.isfinite(x) else 0.0

def conv_text(srcdir, scenefile, asset):
    base = os.path.join(srcdir, scenefile.replace(".scene.MXML", ""))
    if not (os.path.isfile(base + ".geometry.MXML") and os.path.isfile(base + ".geometry.data.MXML")):
        return None
    sc = open(base + ".scene.MXML", encoding="utf-8", errors="replace").read()
    d  = open(base + ".geometry.data.MXML", encoding="utf-8", errors="replace").read()
    nodes = parse_nodes(sc)
    if not nodes: return None  # сцены-ссылки не трогаем
    sel = select_lod0(nodes)
    entries = parse_entries(d)
    single = None
    if not entries:
        md = re.search(r'"MeshDataStream" value="([A-Za-z0-9+/=]+)"', d)
        pp = re.search(r'"MeshPositionDataStream" value="([A-Za-z0-9+/=]+)"', d)
        if not md or not pp: return None
        single = (md.group(1), pp.group(1))
    AV = []; AT = []; AN = []; AF = []
    voff = 0
    for nd in sel:
        if entries:
            e = entries.get(nd["name"].upper())
            if e is None: continue
            mds, pps = e
        else:
            mds, pps = single
        md = b64d(mds); pos = b64d(pps)
        n = len(pos) // 16
        vals = struct.unpack("<%de" % (n * 8), pos[:n*16])
        if len(md) < n * 8: continue
        nrs = struct.unpack_from("<%dI" % (2*n), md, 0)[0::2]
        icnt = (len(md) - n*8) // 2
        idx = struct.unpack_from("<%dH" % icnt, md, n*8)
        if entries:
            v0c, v1c, b0c, bcc = 0, n - 1, 0, icnt
        else:
            if nd["b0"] + nd["bc"] > icnt or nd["v1"] >= n: return None
            v0c, v1c, b0c, bcc = nd["v0"], nd["v1"], nd["b0"], nd["bc"]
        cnt = v1c - v0c + 1
        tv = nd["tv"]
        R = euler_to_mat(tv[3], tv[4], tv[5])
        sx, sy, sz = tv[6], tv[7], tv[8]
        tx, ty, tz = tv[0], tv[1], tv[2]
        ident = (abs(tx)+abs(ty)+abs(tz) < 1e-6 and abs(sx-1)+abs(sy-1)+abs(sz-1) < 1e-6
                 and abs(tv[3])+abs(tv[4])+abs(tv[5]) < 1e-6)
        P = []
        for i in range(v0c, v1c + 1):
            o = i * 8
            x, y, z = fin(vals[o]), fin(vals[o+1]), fin(vals[o+2])
            if not ident:
                x, y, z = x*sx, y*sy, z*sz
                x, y, z = (R[0][0]*x + R[0][1]*y + R[0][2]*z + tx,
                           R[1][0]*x + R[1][1]*y + R[1][2]*z + ty,
                           R[2][0]*x + R[2][1]*y + R[2][2]*z + tz)
            P.append((x, y, z))
            AT.append("vt %.5f %.5f" % (fin(vals[o+4]), 1.0 - fin(vals[o+5])))
            u = nrs[i]
            nx0, ny0, nz0 = u10(u & 0x3FF), u10((u >> 10) & 0x3FF), u10((u >> 20) & 0x3FF)
            if not ident:
                nx0, ny0, nz0 = (R[0][0]*nx0 + R[0][1]*ny0 + R[0][2]*nz0,
                                 R[1][0]*nx0 + R[1][1]*ny0 + R[1][2]*nz0,
                                 R[2][0]*nx0 + R[2][1]*ny0 + R[2][2]*nz0)
            if nx0*nx0 + ny0*ny0 + nz0*nz0 < 1e-6:
                nx0, ny0, nz0 = 0.0, 0.0, 1.0
            AN.append("vn %.4f %.4f %.4f" % (nx0, -nz0, ny0))
        for (x, y, z) in P:
            AV.append("v %.4f %.4f %.4f" % (x*100, -z*100, y*100))
        ntri = bcc // 3
        for t in range(ntri):
            a = idx[b0c + t*3] - v0c; b = idx[b0c + t*3 + 1] - v0c; c = idx[b0c + t*3 + 2] - v0c
            if a < 0 or b < 0 or c < 0 or a >= cnt or b >= cnt or c >= cnt: continue
            pa, pb, pc = P[a], P[b], P[c]
            ux, uy, uz = pb[0]-pa[0], pb[1]-pa[1], pb[2]-pa[2]
            wx, wy, wz = pc[0]-pa[0], pc[1]-pa[1], pc[2]-pa[2]
            cxv = uy*wz - uz*wy; cyv = uz*wx - ux*wz; czv = ux*wy - uy*wx
            if cxv*cxv + cyv*cyv + czv*czv < 1e-10: continue  # вырожденный
            AF.append("f %d/%d/%d %d/%d/%d %d/%d/%d" % (
                a+voff+1, a+voff+1, a+voff+1, b+voff+1, b+voff+1, b+voff+1, c+voff+1, c+voff+1, c+voff+1))
        voff += cnt
    if not AF: return None
    return "o %s\ng %s\n" % (asset, asset) + "\n".join(AV + AT + AN + AF) + "\n"

def objname_geo2(f):
    p = f.replace(".scene.MXML", "").split("__")
    if "meshes" in p:
        i = p.index("meshes")
        return (p[i+1] + "_" + p[-1]).lower()
    return p[-1].lower()

def objname_geo1(f):
    p = f.replace(".scene.MXML", "").split("__")
    return ((p[-2] + "_" + p[-1]) if len(p) >= 2 else p[-1]).lower()

jobs = []
if os.path.isdir(GEO2):
    for f in sorted(os.listdir(GEO2)):
        if f.endswith(".scene.MXML"): jobs.append((GEO2, f, objname_geo2(f)))
have = {j[2] for j in jobs}
if os.path.isdir(GEO1):
    for f in sorted(os.listdir(GEO1)):
        if f.endswith(".scene.MXML"):
            a = objname_geo1(f)
            if a not in have: jobs.append((GEO1, f, a)); have.add(a)
if os.path.isdir(GEO3):
    for f in sorted(os.listdir(GEO3)):
        if f.endswith(".scene.MXML"):
            a = f.replace(".scene.MXML", "").split("__")[-1].lower()
            if a not in have: jobs.append((GEO3, f, a)); have.add(a)

unreal.log("NMS FIX: сцен к проверке: %d" % len(jobs))
changed = []
done = 0
for srcdir, scf, asset in jobs:
    try:
        txt = conv_text(srcdir, scf, asset)
    except Exception as e:
        unreal.log_warning("NMS FIX: ошибка %s: %s" % (asset, str(e)[:80])); txt = None
    done += 1
    if done % 100 == 0: unreal.log("NMS FIX: проверено %d / %d (изменилось %d)" % (done, len(jobs), len(changed)))
    if txt is None: continue
    p = os.path.join(MESHSRC, asset + ".obj")
    old = None
    if os.path.isfile(p):
        old = open(p, encoding="utf-8", errors="replace").read()
    if old != txt:
        open(p, "w", encoding="utf-8").write(txt)
        changed.append(asset)
unreal.log("NMS FIX: изменилось OBJ: %d" % len(changed))

unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.OBJ 1")
tools = unreal.AssetToolsHelpers.get_asset_tools()
def make_options():
    pl = unreal.InterchangeGenericAssetsPipeline()
    mp = pl.mesh_pipeline
    for prop, val in (("build_nanite", False), ("build_reversed_index_buffer", False),
                      ("collision", False), ("generate_lightmap_u_vs", False),
                      ("combine_static_meshes", True)):
        try: mp.set_editor_property(prop, val)
        except Exception: pass
    try: pl.common_meshes_properties.set_editor_property("auto_detect_meshes", True)
    except Exception: pass
    ov = unreal.InterchangePipelineStackOverride()
    ov.add_pipeline(pl)
    return ov

imp = 0
for asset in changed:
    task = unreal.AssetImportTask()
    task.filename = os.path.join(MESHSRC, asset + ".obj")
    task.destination_path = PKG
    task.destination_name = asset
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = make_options()
    unreal.log("NMS FIX: переимпорт %s" % asset)
    tools.import_asset_tasks([task])
    imp += 1
    if imp % 25 == 0: unreal.log("NMS FIX: === %d / %d ===" % (imp, len(changed)))

unreal.log("NMS FIX: ГОТОВО. Обновлено мешей: %d. Перезапусти вкладку строителя." % imp)
