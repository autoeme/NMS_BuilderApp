# -*- coding: utf-8 -*-
# Импорт всех игровых атласов (PNG) в PartTex2 — БАТЧЕМ (одна команда), не виснет.
# Источник: <проект>/TexSrcAll/*.png. Типы: Content/tex_kind.json (d/n/m/c).
# Запуск в редакторе: py "...\import_all_atlases.py"
import unreal, os, json

CONTENT = unreal.SystemLibrary.get_project_content_directory()
PROJ = os.path.normpath(os.path.join(CONTENT, ".."))
SRC = os.path.join(PROJ, "TexSrcAll")
DST = "/Game/NMSBaseBuilder/PartTex2"

kind = {}
kp = os.path.join(CONTENT, "tex_kind.json")
if os.path.isfile(kp):
    kind = json.load(open(kp))

tools = unreal.AssetToolsHelpers.get_asset_tools()
files = sorted(f for f in os.listdir(SRC) if f.endswith(".png"))
unreal.log("NMS TEX: атласов: %d. Импорт батчем..." % len(files))

# --- 1) собрать ВСЕ задачи и импортировать ОДНОЙ командой (нативный батч) ---
tasks = []
for fn in files:
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, fn)
    t.destination_path = DST
    t.destination_name = fn[:-4]
    t.automated = True
    t.save = True
    t.replace_existing = True
    tasks.append(t)
tools.import_asset_tasks(tasks)
unreal.log("NMS TEX: импорт завершён, выставляю флаги...")

# --- 2) флаги по типу карты (отдельным проходом) ---
done = 0
for fn in files:
    asset = fn[:-4]
    full = DST + "/" + asset
    obj = unreal.EditorAssetLibrary.load_asset(full)
    if not obj:
        continue
    k = kind.get(asset, "d")
    obj.set_editor_property("never_stream", False)  # стриминг ВКЛ — иначе пул переполняется и всё мутнеет (1044 текстуры)
    if k == "n":
        obj.set_editor_property("srgb", False)
        obj.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        obj.set_editor_property("flip_green_channel", True)
    elif k in ("m", "c"):
        obj.set_editor_property("srgb", False)
    obj.post_edit_change()
    done += 1
    if done % 100 == 0:
        unreal.log("NMS TEX: флаги %d / %d" % (done, len(files)))

# --- 3) сохранить всё разом ---
unreal.EditorAssetLibrary.save_directory(DST, only_if_is_dirty=True, recursive=True)
unreal.log("NMS TEX: ГОТОВО. текстур: %d. Перезапусти вкладку строителя." % done)
