# -*- coding: utf-8 -*-
# Точечный переимпорт ОДНОГО починенного меша GARAGE_M (vehiclegaragecustomiser).
# Остальные 543 уже импортированы корректно. Запуск: py "...\reimport_garage.py"
import unreal, os

CONTENT = unreal.SystemLibrary.get_project_content_directory()
SRC = os.path.normpath(os.path.join(CONTENT, "..", "MeshSrc"))
PKG = "/Game/NMSBaseBuilder/GameMeshes"
NAME = "vehiclegaragecustomiser"

unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.OBJ 1")
tools = unreal.AssetToolsHelpers.get_asset_tools()

pl = unreal.InterchangeGenericAssetsPipeline()
mp = pl.mesh_pipeline
for prop, val in (("build_nanite", False), ("build_reversed_index_buffer", False),
                  ("collision", False), ("generate_lightmap_u_vs", False),
                  ("combine_static_meshes", True)):
    try: mp.set_editor_property(prop, val)
    except Exception: pass
cm = pl.common_meshes_properties
for prop, val in (("auto_detect_meshes", True), ("bake_meshes", True)):
    try: cm.set_editor_property(prop, val)
    except Exception: pass
ov = unreal.InterchangePipelineStackOverride()
ov.add_pipeline(pl)

task = unreal.AssetImportTask()
task.filename = os.path.join(SRC, NAME + ".obj")
task.destination_path = PKG
task.destination_name = NAME
task.automated = True
task.save = True
task.replace_existing = True
task.options = ov
tools.import_asset_tasks([task])
unreal.log("NMS: GARAGE_M переимпортирован")
