# -*- coding: utf-8 -*-
# Импорт игровых мешей v2: без лайтмап-UV (они вешали импорт на больших мешах),
# один ассет на OBJ (объединение групп), пропуск уже готовых (можно перезапускать).
# Запуск: py "...\reimport_all_meshes.py"

import unreal
import os

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
                      ("generate_lightmap_u_vs", False),   # ГЛАВНОЕ: без лайтмапов
                      ("combine_static_meshes", True)):    # один ассет на файл
        try: mp.set_editor_property(prop, val)
        except Exception: pass
    cm = pl.common_meshes_properties
    for prop, val in (("auto_detect_meshes", True),
                      ("bake_meshes", True)):
        try: cm.set_editor_property(prop, val)
        except Exception: pass
    ov = unreal.InterchangePipelineStackOverride()
    ov.add_pipeline(pl)
    return ov

files = sorted(f for f in os.listdir(SRC) if f.endswith(".obj"))
names = {f[:-4] for f in files}
unreal.log("NMS: всего OBJ: {}".format(len(files)))

# ВНИМАНИЕ: осколки и устаревшие ассеты удаляются С ДИСКА при закрытом редакторе
# (удаление через редактор зависало из-за ссылок открытых вкладок).
done = skipped = 0
for fn in files:
    name = fn[:-4]
    full = PKG + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        skipped += 1
        continue
    unreal.log("NMS: импорт {}".format(name))  # видно, на ком застряло
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SRC, fn)
    task.destination_path = PKG
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = make_options()
    tools.import_asset_tasks([task])
    done += 1
    if done % 50 == 0:
        unreal.log("NMS: === {} новых, {} пропущено ===".format(done, skipped))
unreal.log("NMS: ГОТОВО: {} новых, {} уже были".format(done, skipped))
