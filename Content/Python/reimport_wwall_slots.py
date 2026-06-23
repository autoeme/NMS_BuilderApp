import unreal, os

# Переимпорт wood_basic_wall.obj с РАЗДЕЛЬНЫМИ слотами (combine=False),
# чтобы у меша было 6 материал-слотов (по узлам игры). Только эта деталь.

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
                  ("combine_static_meshes", True)):   # ВАЖНО: True -> сохранить слоты по usemtl (правило 15)
    try: mp.set_editor_property(prop, val)
    except Exception: pass
try: pl.common_meshes_properties.set_editor_property("auto_detect_meshes", True)
except Exception: pass
ov = unreal.InterchangePipelineStackOverride()
ov.add_pipeline(pl)

task = unreal.AssetImportTask()
task.filename = os.path.join(MESHSRC, ASSET + ".obj")
task.destination_path = PKG
task.destination_name = ASSET
task.automated = True
task.save = True
task.replace_existing = True
task.options = ov
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

obj = unreal.EditorAssetSubsystem().load_asset("%s/%s.%s" % (PKG, ASSET, ASSET))
nmat = obj.get_num_sections(0) if obj else -1
unreal.log("NMS: wood_basic_wall переимпортирован. Слотов в меше: %d (надо 6). Перезапусти вкладку." % (obj.get_static_material_count() if obj else -1))
