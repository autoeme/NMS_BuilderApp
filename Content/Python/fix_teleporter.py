import unreal, os

# Rebuild refiner2 correctly: refiner2model.scene had THREE refiners at origin
# (Refiner2 + CORE2 + a stray Refiner3 duplicate); previous build superimposed all
# three into a shapeless blob. Correct = Refiner2 + CORE2 nodes only (drop refiner3_dup).

SRC = os.path.join(os.path.normpath(os.path.join(
    unreal.SystemLibrary.get_project_content_directory(), "..")), "MeshSrc")
PKG = "/Game/NMSBaseBuilder/GameMeshes"
NAMES = ["teleporter"]

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

for name in NAMES:
    path = os.path.join(SRC, name + ".obj")
    if not os.path.exists(path):
        unreal.log_warning("NMS FIX: net OBJ %s" % name); continue
    task = unreal.AssetImportTask()
    task.filename = path; task.destination_path = PKG; task.destination_name = name
    task.automated = True; task.save = True; task.replace_existing = True
    task.options = make_options()
    tools.import_asset_tasks([task])
    unreal.log("NMS FIX: imported %s" % name)

unreal.log("NMS FIX TELEPORTER: GOTOVO")
try:
    unreal.EditorAssetLibrary.save_directory(PKG, only_if_is_dirty=False, recursive=False)
except Exception as e:
    unreal.log_warning("save_directory: %s" % e)
unreal.SystemLibrary.quit_editor()
