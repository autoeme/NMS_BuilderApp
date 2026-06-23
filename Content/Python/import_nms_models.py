# -*- coding: utf-8 -*-
# Импорт всех FBX-моделей деталей NMS из плагина в проект.
# Запуск: Tools -> Execute Python Script -> выбрать этот файл.
# (нужен включённый плагин "Python Editor Script Plugin")

import unreal
import os

# Папка с FBX внутри плагина (Downloads)
SRC = r"C:\Users\User\Downloads\no_mans_sky_base_builder-6.3.1 (1)\models"
# Путь назначения в проекте — совпадает с ModelPath в nms_parts_db.json
DEST = "/Game/NMSBaseBuilder/Features/Models_FBX"

# Собираем все .fbx
fbx_files = []
for root, _dirs, files in os.walk(SRC):
    for f in files:
        if f.lower().endswith(".fbx"):
            fbx_files.append(os.path.join(root, f))

unreal.log("NMS import: найдено FBX = {}".format(len(fbx_files)))

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
done = 0
batch = []

def make_task(path):
    name = os.path.splitext(os.path.basename(path))[0]
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True          # без диалогов
    task.replace_existing = True
    task.save = True
    opts = unreal.FbxImportUI()
    opts.import_mesh = True
    opts.import_as_skeletal = False
    opts.import_materials = False
    opts.import_textures = False
    opts.static_mesh_import_data.combine_meshes = True
    opts.static_mesh_import_data.generate_lightmap_u_vs = False
    task.options = opts
    return task

# Импортируем партиями по 50, чтобы не забивать память и видеть прогресс
for i, path in enumerate(fbx_files):
    batch.append(make_task(path))
    if len(batch) >= 50 or i == len(fbx_files) - 1:
        asset_tools.import_asset_tasks(batch)
        done += len(batch)
        unreal.log("NMS import: {}/{}".format(done, len(fbx_files)))
        batch = []

unreal.log("NMS import: ГОТОВО, импортировано {} мешей в {}".format(done, DEST))
