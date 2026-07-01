import unreal, os

# Починка битых мешей группы Furnishings (переимпорт из целых OBJ).
# Те же опции, что reimport_stage2: LOD0/секции, nanite off, collision off, replace.

SRC = r"C:\Users\User\Desktop\NMS_BACKUP_20260608_233516\NMS_BuilderApp\MeshSrc"
SRC2 = os.path.join(os.path.normpath(os.path.join(unreal.SystemLibrary.get_project_content_directory(), "..")), "MeshSrc")
PKG = "/Game/NMSBaseBuilder/GameMeshes"

# меш-имена для починки (битые в Furnishings; robothand добавится после конвертации)
NAMES = ["sofa2", "sofa2l", "largecrate", "lighttable", "robothand"]

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

imp = 0; miss = []
for name in NAMES:
    path = os.path.join(SRC, name + ".obj")
    if not os.path.exists(path):
        path = os.path.join(SRC2, name + ".obj")
    if not os.path.exists(path):
        unreal.log_warning("NMS FIX: net OBJ %s" % name); miss.append(name); continue
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = PKG
    task.destination_name = name
    task.automated = True; task.save = True; task.replace_existing = True
    task.options = make_options()
    tools.import_asset_tasks([task])
    imp += 1
    unreal.log("NMS FIX: imported %s" % name)

unreal.log("NMS FIX FURNISHINGS: GOTOVO import=%d miss=%s" % (imp, miss))
