# -*- coding: utf-8 -*-
# Импорт новых игровых атласов (внешние детали баз + корветы BIGGS).
# Источник: <проект>/TexSrcNew/*.png (вне Content — без авто-импорта).
# Правила NMS->UE: нормали Flip Green; маски/покраска SRGB OFF.
import unreal, os

SRC = os.path.join(unreal.SystemLibrary.get_project_directory(), "TexSrcNew")
DST = "/Game/NMSBaseBuilder/PartTex2"

# имя файла-png -> (имя ассета, тип: d=диффуз, n=нормаль, m=маска)
PLAN = {
    "textures__multitextures__basebuilding__basebuildingexterior.png":              ("T_BBEXT",        "d"),
    "textures__multitextures__basebuilding__basebuildingexterior.normal.png":       ("T_BBEXT_N",      "n"),
    "textures__multitextures__basebuilding__basebuildingexterior.masks.png":        ("T_BBEXT_M",      "m"),
    "textures__multitextures__basebuilding__basebuildingexterior.colourisemask.png":("T_BBEXT_CM",     "m"),
    "textures__multitextures__biggs__biggsexterior.png":                            ("T_BIGGS",        "d"),
    "textures__multitextures__biggs__biggsexterior.normal.png":                     ("T_BIGGS_N",      "n"),
    "textures__multitextures__biggs__biggsexterior.masks.png":                      ("T_BIGGS_M",      "m"),
    "textures__multitextures__biggs__biggsexterior.colourisemask.png":              ("T_BIGGS_CM",     "m"),
    "textures__multitextures__biggs__biggstrim.png":                                ("T_BIGGSTRIM",    "d"),
    "textures__multitextures__biggs__biggstrim.normal.png":                         ("T_BIGGSTRIM_N",  "n"),
    "textures__multitextures__biggs__biggstrim.masks.png":                          ("T_BIGGSTRIM_M",  "m"),
    "textures__multitextures__biggs__biggstrim.colourisemask.png":                  ("T_BIGGSTRIM_CM", "m"),
}

tools = unreal.AssetToolsHelpers.get_asset_tools()
done = 0
for fname, (asset, kind) in PLAN.items():
    path = os.path.join(SRC, fname)
    if not os.path.isfile(path):
        print("НЕТ ФАЙЛА:", path); continue
    if unreal.EditorAssetLibrary.does_asset_exist(DST + "/" + asset):
        print("уже есть:", asset); continue
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DST
    task.destination_name = asset
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    obj = unreal.EditorAssetLibrary.load_asset(DST + "/" + asset)
    if not obj:
        print("ОШИБКА импорта:", asset); continue
    obj.set_editor_property("never_stream", True)
    if kind == "n":
        obj.set_editor_property("srgb", False)
        obj.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        obj.set_editor_property("flip_green_channel", True)   # NMS -Y -> UE +Y
    elif kind == "m":
        obj.set_editor_property("srgb", False)                 # линейные маски
    obj.post_edit_change()
    unreal.EditorAssetLibrary.save_loaded_asset(obj)
    done += 1
    print("импортирован:", asset)

print("ГОТОВО: %d новых атласов в %s" % (done, DST))
