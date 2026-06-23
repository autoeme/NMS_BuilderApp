import unreal, os
# ОТКАТ: вернуть wood_basic_wall в рабочий вид (combine=True, 1 слот).
CONTENT = unreal.SystemLibrary.get_project_content_directory()
PROJ = os.path.normpath(os.path.join(CONTENT, ".."))
MESHSRC = os.path.join(PROJ, "MeshSrc")
PKG = "/Game/NMSBaseBuilder/GameMeshes"
ASSET = "wood_basic_wall"
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.OBJ 1")
pl = unreal.InterchangeGenericAssetsPipeline()
mp = pl.mesh_pipeline
for prop, val in (("build_nanite", False), ("build_reversed_index_buffer", False),
                  ("collision", False), ("generate_lightmap_u_vs", False),
                  ("combine_static_meshes", True)):
    try: mp.set_editor_property(prop, val)
    except Exception: pass
ov = unreal.InterchangePipelineStackOverride(); ov.add_pipeline(pl)
task = unreal.AssetImportTask()
task.filename = os.path.join(MESHSRC, ASSET + ".obj")
task.destination_path = PKG; task.destination_name = ASSET
task.automated = True; task.save = True; task.replace_existing = True; task.options = ov
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
unreal.log("NMS: wood_basic_wall ВОЗВРАЩЁН в рабочий вид. Perezapusti vkladku.")
