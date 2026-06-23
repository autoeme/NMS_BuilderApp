# -*- coding: utf-8 -*-
# Принудительный переимпорт ТОЛЬКО изменённых за сессию мешей (список _reimport_list.txt).
# В отличие от import_game_meshes.py — НЕ пропускает существующие, а заменяет их.
# Запуск: Tools -> Execute Python Script -> этот файл.
import unreal, os

CONTENT = unreal.SystemLibrary.get_project_content_directory()
SRC = os.path.normpath(os.path.join(CONTENT, "..", "MeshSrc"))
PKG = "/Game/NMSBaseBuilder/GameMeshes"

unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.OBJ 1")
tools = unreal.AssetToolsHelpers.get_asset_tools()

def make_options():
    pl = unreal.InterchangeGenericAssetsPipeline()
    for prop, val in (("build_nanite", False), ("build_reversed_index_buffer", False), ("collision", False)):
        try: pl.mesh_pipeline.set_editor_property(prop, val)
        except Exception: pass
    try: pl.common_meshes_properties.set_editor_property("auto_detect_meshes", True)
    except Exception: pass
    ov = unreal.InterchangePipelineStackOverride(); ov.add_pipeline(pl)
    return ov

listfile = os.path.join(SRC, "_reimport_list.txt")
names = [l.strip() for l in open(listfile, encoding="utf-8")] if os.path.isfile(listfile) else []
names = [n for n in names if n and os.path.isfile(os.path.join(SRC, n + ".obj"))]

try: unreal.EditorDialog.show_message("NMS переимпорт", "Переимпорт мешей: {}\nНачинаю. Жди окно 'Готово' (~2-5 мин).".format(len(names)), unreal.AppMsgType.OK)
except Exception: pass

done = 0
for name in names:
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SRC, name + ".obj")
    task.destination_path = PKG
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True          # ЗАМЕНА существующего
    task.options = make_options()
    tools.import_asset_tasks([task])
    done += 1
    if done % 25 == 0:
        unreal.log("NMS reimport: {} / {}".format(done, len(names)))

result = "NMS ПЕРЕИМПОРТ ГОТОВО: обновлено мешей {} из {}".format(done, len(names))
unreal.log(result)
try:
    with open(os.path.join(SRC, "_reimport_result.txt"), "w", encoding="utf-8") as f:
        f.write(result + "\n")
except Exception: pass
try: unreal.EditorDialog.show_message("NMS переимпорт - ГОТОВО", result + "\n\nПерезапусти вкладку строителя.", unreal.AppMsgType.OK)
except Exception: pass
