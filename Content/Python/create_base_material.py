# -*- coding: utf-8 -*-
# Создаёт материал /Game/NMSBaseBuilder/Materials/M_BasePart с параметром цвета "Color".
# Материал БЕЗ ОСВЕЩЕНИЯ (Unlit): деталь показывает ровный плоский цвет со всех сторон,
# без теней (нет чёрных сторон). Этим материалом красятся детали.
# Запуск: Tools -> Execute Python Script.

import unreal

PKG = "/Game/NMSBaseBuilder/Materials"
NAME = "M_BasePart"
full = PKG + "/" + NAME

tools = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary

if unreal.EditorAssetLibrary.does_asset_exist(full):
    unreal.EditorAssetLibrary.delete_asset(full)

mat = tools.create_asset(NAME, PKG, unreal.Material, unreal.MaterialFactoryNew())

# Без освещения — ровный цвет, никаких теней/чёрных сторон
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

# Параметр цвета -> Emissive (для Unlit это и есть итоговый цвет)
col = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -380, 0)
col.set_editor_property("parameter_name", "Color")
col.set_editor_property("default_value", unreal.LinearColor(0.78, 0.78, 0.74, 1.0))
MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

MEL.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full)
unreal.log("NMS: M_BasePart (Unlit) создан -> " + full)

# --- Зелёная «голограмма» размещения (как в игре) ---
GNAME = "M_Ghost"
gfull = PKG + "/" + GNAME
if unreal.EditorAssetLibrary.does_asset_exist(gfull):
    unreal.EditorAssetLibrary.delete_asset(gfull)
ghost = tools.create_asset(GNAME, PKG, unreal.Material, unreal.MaterialFactoryNew())
ghost.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
ghost.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
gcol = MEL.create_material_expression(ghost, unreal.MaterialExpressionVectorParameter, -380, 0)
gcol.set_editor_property("parameter_name", "Color")
gcol.set_editor_property("default_value", unreal.LinearColor(0.15, 1.0, 0.4, 1.0))
MEL.connect_material_property(gcol, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
gop = MEL.create_material_expression(ghost, unreal.MaterialExpressionScalarParameter, -380, 220)
gop.set_editor_property("parameter_name", "Opacity")
gop.set_editor_property("default_value", 0.45)
MEL.connect_material_property(gop, "", unreal.MaterialProperty.MP_OPACITY)
MEL.recompile_material(ghost)
unreal.EditorAssetLibrary.save_asset(gfull)
unreal.log("NMS: M_Ghost (зелёная голограмма) создан -> " + gfull)
