# -*- coding: utf-8 -*-
# Импорт ФОССИЛ-мешей напрямую из FBX плагина NMS Base Builder (каждый = свой ID).
# Список: MeshSrc\_fbx_fossils.txt  (строки "PartName|полный_путь_к_fbx").
# Имя ассета = ID детали -> каталог уже на него ссылается. Никакого угадывания.
# Запуск: Tools -> Execute Python Script, ИЛИ консоль: py "путь".
import unreal, os

CONTENT = unreal.SystemLibrary.get_project_content_directory()
SRC = os.path.normpath(os.path.join(CONTENT, "..", "MeshSrc"))
PKG = "/Game/NMSBaseBuilder/GameMeshes"
listfile = os.path.join(SRC, "_fbx_fossils.txt")

tools = unreal.AssetToolsHelpers.get_asset_tools()

def fbx_options():
    opt = unreal.FbxImportUI()
    opt.import_mesh = True
    opt.import_as_skeletal = False
    opt.import_materials = False
    opt.import_textures = False
    opt.import_animations = False
    opt.static_mesh_import_data.set_editor_property("combine_meshes", True)
    opt.static_mesh_import_data.set_editor_property("generate_lightmap_u_vs", False)
    opt.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
    try: opt.static_mesh_import_data.set_editor_property("build_nanite", False)
    except Exception: pass
    return opt

rows = []
if os.path.isfile(listfile):
    for ln in open(listfile, encoding="utf-8"):
        ln = ln.strip()
        if "|" in ln:
            name, path = ln.split("|", 1)
            rows.append((name, path))

try: unreal.EditorDialog.show_message("NMS FBX импорт", "Фоссил-мешей из FBX: {}\nНачинаю. Жди окно 'Готово'.".format(len(rows)), unreal.AppMsgType.OK)
except Exception: pass

done = 0; miss = 0
for name, path in rows:
    if not os.path.isfile(path):
        miss += 1
        unreal.log_warning("NMS FBX: нет файла {}".format(path)); continue
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = PKG
    task.destination_name = name           # имя ассета = ID детали
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = fbx_options()
    tools.import_asset_tasks([task])
    done += 1
    if done % 25 == 0:
        unreal.log("NMS FBX: {} / {}".format(done, len(rows)))

result = "NMS FBX ГОТОВО: импортировано {} (нет файла: {})".format(done, miss)
unreal.log(result)
try:
    with open(os.path.join(SRC, "_fbx_result.txt"), "w", encoding="utf-8") as f:
        f.write(result + "\n")
except Exception: pass
try: unreal.EditorDialog.show_message("NMS FBX - ГОТОВО", result + "\n\nПерезапусти вкладку строителя.", unreal.AppMsgType.OK)
except Exception: pass
