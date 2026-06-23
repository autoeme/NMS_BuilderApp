# -*- coding: utf-8 -*-
# Переимпорт трёх эталонных деталей (камень: арка + 2 стены) из MeshSrc.
import unreal
import os

CONTENT = unreal.SystemLibrary.get_project_content_directory()
SRC = os.path.normpath(os.path.join(CONTENT, "..", "MeshSrc"))
PKG = "/Game/NMSBaseBuilder/GameMeshes"
NAMES = ["stone_basic_wall_arch", "stone_basic_wall_halft",
         "stone_basic_wall_small_half", "stone_basic_wall_small"]

unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.OBJ 1")
tools = unreal.AssetToolsHelpers.get_asset_tools()
for name in NAMES:
    full = PKG + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        unreal.EditorAssetLibrary.delete_asset(full)
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SRC, name + ".obj")
    task.destination_path = PKG
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    tools.import_asset_tasks([task])
    unreal.log("NMS: переимпортирован " + name)
unreal.log("NMS: эталонная тройка готова")
