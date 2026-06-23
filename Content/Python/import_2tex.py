# -*- coding: utf-8 -*-
# Точечный импорт 2 текстур, не вставших в общем батче.
import unreal, os

CONTENT = unreal.SystemLibrary.get_project_content_directory()
PROJ = os.path.normpath(os.path.join(CONTENT, ".."))
SRC = os.path.join(PROJ, "TexSrcAll")
DST = "/Game/NMSBaseBuilder/PartTex2"

NAMES = [
    ("T_basicparts_stonetrim_normal_2", "n"),
    ("T_plants_scrubgrassbuildable_base", "d"),
]
tools = unreal.AssetToolsHelpers.get_asset_tools()
tasks = []
for name, k in NAMES:
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, name + ".png")
    t.destination_path = DST
    t.destination_name = name
    t.automated = True
    t.save = True
    t.replace_existing = True
    tasks.append(t)
tools.import_asset_tasks(tasks)

for name, k in NAMES:
    obj = unreal.EditorAssetLibrary.load_asset(DST + "/" + name)
    if not obj:
        unreal.log_warning("NMS TEX: не загрузился " + name)
        continue
    obj.set_editor_property("never_stream", True)
    if k == "n":
        obj.set_editor_property("srgb", False)
        obj.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        obj.set_editor_property("flip_green_channel", True)
    elif k in ("m", "c"):
        obj.set_editor_property("srgb", False)
    obj.post_edit_change()
unreal.EditorAssetLibrary.save_directory(DST, only_if_is_dirty=True, recursive=True)
unreal.log("NMS TEX: 2 текстуры добраны.")
