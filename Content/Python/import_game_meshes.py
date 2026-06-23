# -*- coding: utf-8 -*-
# Импорт игровых мешей (OBJ из MeshSrc, ВНЕ Content) в /Game/NMSBaseBuilder/GameMeshes.
# Interchange-импортёр с отключённым Nanite/LOD — быстро и без диалогов.
# Запуск: py "...\import_game_meshes.py"

import unreal
import os

CONTENT = unreal.SystemLibrary.get_project_content_directory()
SRC = os.path.normpath(os.path.join(CONTENT, "..", "MeshSrc"))
PKG = "/Game/NMSBaseBuilder/GameMeshes"

# новый импортёр включён обратно
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.OBJ 1")

tools = unreal.AssetToolsHelpers.get_asset_tools()

def make_options():
    pl = unreal.InterchangeGenericAssetsPipeline()
    # без Nanite и без генерации лишнего — обычные лёгкие меши
    try: pl.mesh_pipeline.set_editor_property("build_nanite", False)
    except Exception: pass
    try: pl.mesh_pipeline.set_editor_property("build_reversed_index_buffer", False)
    except Exception: pass
    try: pl.common_meshes_properties.set_editor_property("auto_detect_meshes", True)
    except Exception: pass
    try: pl.mesh_pipeline.set_editor_property("collision", False)
    except Exception: pass
    ov = unreal.InterchangePipelineStackOverride()
    ov.add_pipeline(pl)
    return ov

done = 0
skip = 0
files = sorted(f for f in os.listdir(SRC) if f.endswith(".obj"))
msg0 = "NMS: OBJ в MeshSrc: {}".format(len(files))
unreal.log(msg0)
# СРАЗУ показываем что скрипт стартовал (чтобы было видно, что py отработал)
try: unreal.EditorDialog.show_message("NMS импорт", msg0 + "\nИмпорт начался. Жди второе окно 'Готово'.", unreal.AppMsgType.OK)
except Exception: pass
for fn in files:
    name = fn[:-4]
    full = PKG + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        skip += 1
        continue
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
    if done % 25 == 0:
        unreal.log("NMS: импортировано {}...".format(done))
result = "NMS ГОТОВО: импортировано новых {}, уже было {}, всего OBJ {}".format(done, skip, len(files))
unreal.log(result)
# лог-файл на диск (Claude проверит, что скрипт отработал)
try:
    with open(os.path.join(SRC, "_import_result.txt"), "w", encoding="utf-8") as f:
        f.write(result + "\n")
except Exception as e:
    unreal.log_warning("NMS: не записал лог: {}".format(e))
# видимое окно В КОНЦЕ
try: unreal.EditorDialog.show_message("NMS импорт - ГОТОВО", result + "\n\nПерезапусти вкладку строителя.", unreal.AppMsgType.OK)
except Exception: pass
