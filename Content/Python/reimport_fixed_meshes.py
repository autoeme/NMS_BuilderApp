import unreal, os, glob, json

# ПЕРЕИМПОРТ исправленных мешей (LOD0 + правильные трансформы + секции).
# Чинит «жалюзи» (раньше LOD-уровни накладывались). Сохраняет usemtl-секции под per-slot.

CONTENT = unreal.SystemLibrary.get_project_content_directory()
PROJ = os.path.normpath(os.path.join(CONTENT, ".."))
MESHSRC = os.path.join(PROJ, "MeshSrc")
PKG = "/Game/NMSBaseBuilder/GameMeshes"

listf = os.path.join(CONTENT, "Python", "fixed_meshes_list.json")
names = json.load(open(listf)) if os.path.exists(listf) else \
    [os.path.splitext(os.path.basename(x))[0] for x in glob.glob(os.path.join(MESHSRC, "*.obj"))]

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
    ov = unreal.InterchangePipelineStackOverride(); ov.add_pipeline(pl)
    return ov

unreal.log("NMS: pereimport ispravlennyh meshey: %d" % len(names))
imp = 0; miss = 0
for name in names:
    path = os.path.join(MESHSRC, name + ".obj")
    if not os.path.exists(path):
        miss += 1; continue
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = PKG
    task.destination_name = name
    task.automated = True; task.save = True; task.replace_existing = True
    task.options = make_options()
    tools.import_asset_tasks([task])
    imp += 1
    if imp % 100 == 0: unreal.log("NMS: pereimport %d / %d" % (imp, len(names)))

unreal.log("NMS: GOTOVO. Pereimportirovano: %d (propusheno net fayla: %d). Perezapusti vkladku." % (imp, miss))
