# -*- coding: utf-8 -*-
# ПУНКТ 4: переимпорт пересобранных мешей СО СЛОТАМИ (usemtl на материал-узел).
# Принудительно (replace_existing), НЕ пропускает существующие — иначе старые
# одно-слотовые версии останутся. Список: Content/NMSData/reimport_slots_list.json
# Запуск в редакторе: py "...\reimport_slots.py"
import unreal, os, json

CONTENT = unreal.SystemLibrary.get_project_content_directory()
SRC = os.path.normpath(os.path.join(CONTENT, "..", "MeshSrc"))
PKG = "/Game/NMSBaseBuilder/GameMeshes"

unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.OBJ 1")
tools = unreal.AssetToolsHelpers.get_asset_tools()

def make_options():
    pl = unreal.InterchangeGenericAssetsPipeline()
    mp = pl.mesh_pipeline
    for prop, val in (("build_nanite", False),
                      ("build_reversed_index_buffer", False),
                      ("collision", False),
                      ("generate_lightmap_u_vs", False),
                      ("combine_static_meshes", True)):   # слоты по usemtl сохраняются
        try: mp.set_editor_property(prop, val)
        except Exception: pass
    cm = pl.common_meshes_properties
    for prop, val in (("auto_detect_meshes", True), ("bake_meshes", True)):
        try: cm.set_editor_property(prop, val)
        except Exception: pass
    ov = unreal.InterchangePipelineStackOverride()
    ov.add_pipeline(pl)
    return ov

with open(os.path.join(CONTENT, "NMSData", "reimport_slots_list.json"), "r") as f:
    names = json.load(f)
unreal.log("NMS: переимпорт со слотами: {}".format(len(names)))

done = miss = 0
for name in names:
    fn = os.path.join(SRC, name + ".obj")
    if not os.path.exists(fn):
        miss += 1
        continue
    task = unreal.AssetImportTask()
    task.filename = fn
    task.destination_path = PKG
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = make_options()
    tools.import_asset_tasks([task])
    done += 1
    if done % 50 == 0:
        unreal.log("NMS: === {}/{} ===".format(done, len(names)))
unreal.log("NMS: ГОТОВО слоты: {} переимпортировано, {} нет файла".format(done, miss))
