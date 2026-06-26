import unreal, os, json

# Этап 2 (план «детали в идеал»): импорт пересобранных мешей по stage2_import_list.json.
# Те же опции, что reimport_fixed_meshes: LOD0/секции, nanite off, collision off, replace.

CONTENT = unreal.SystemLibrary.get_project_content_directory()
PROJ = os.path.normpath(os.path.join(CONTENT, ".."))
MESHSRC = os.path.join(PROJ, "MeshSrc")
PKG = "/Game/NMSBaseBuilder/GameMeshes"

names = json.load(open(os.path.join(CONTENT, "Python", "stage2_import_list.json")))

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

unreal.log("NMS STAGE2: import %d meshey" % len(names))
imp = 0; miss = 0
for name in names:
    path = os.path.join(MESHSRC, name + ".obj")
    if not os.path.exists(path):
        unreal.log_warning("NMS STAGE2: net fayla %s" % path); miss += 1; continue
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = PKG
    task.destination_name = name
    task.automated = True; task.save = True; task.replace_existing = True
    task.options = make_options()
    tools.import_asset_tasks([task])
    imp += 1

unreal.log("NMS STAGE2: GOTOVO import=%d miss=%d" % (imp, miss))
