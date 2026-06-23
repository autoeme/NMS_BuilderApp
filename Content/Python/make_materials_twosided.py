# -*- coding: utf-8 -*-
# Делает материалы деталей ДВУСТОРОННИМИ — чтобы деталь не просвечивала снизу/изнутри
# (как Blender Solid: видно и тыльные грани). Запуск: py "...\make_materials_twosided.py"
import unreal

MATS = [
    "/Game/NMSBaseBuilder/Materials/M_PartLit.M_PartLit",
    "/Game/NMSBaseBuilder/Materials/M_PartUnlit.M_PartUnlit",
    "/Game/NMSBaseBuilder/Materials/M_BasePart.M_BasePart",
]
done = 0
for path in MATS:
    m = unreal.EditorAssetLibrary.load_asset(path)
    if not m:
        unreal.log_warning("NMS: не найден материал " + path)
        continue
    try:
        m.set_editor_property("two_sided", True)
        m.post_edit_change()
        unreal.EditorAssetLibrary.save_asset(path)
        done += 1
        unreal.log("NMS: двусторонний -> " + path)
    except Exception as e:
        unreal.log_warning("NMS: {} -> {}".format(path, e))
unreal.log("NMS: материалов сделано двусторонними: {}".format(done))
