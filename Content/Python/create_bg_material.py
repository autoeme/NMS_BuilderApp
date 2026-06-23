# -*- coding: utf-8 -*-
# Фон как в меню игры: импортирует Content/NMSData/UI/optionsbg.png (настоящий фон
# меню NMS, вытащен из файлов игры) и создаёт небесный материал M_SkyBG.
# Запуск: Tools -> Execute Python Script (или py "...\create_bg_material.py" в Cmd).
# После запуска перезапусти вкладку NMS Base Builder.

import unreal
import os

PKG_MAT = "/Game/NMSBaseBuilder/Materials"
PKG_TEX = "/Game/NMSBaseBuilder/Textures"
TEX_NAME = "T_OptionsBG"
MAT_NAME = "M_SkyBG"

tools = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary

# --- 1) импорт PNG как Texture2D ---
png = os.path.join(unreal.SystemLibrary.get_project_content_directory(),
                   "NMSData", "UI", "optionsbg.png")
tex_full = PKG_TEX + "/" + TEX_NAME
if unreal.EditorAssetLibrary.does_asset_exist(tex_full):
    unreal.EditorAssetLibrary.delete_asset(tex_full)

task = unreal.AssetImportTask()
task.filename = png
task.destination_path = PKG_TEX
task.destination_name = TEX_NAME
task.automated = True
task.save = True
tools.import_asset_tasks([task])

tex = unreal.EditorAssetLibrary.load_asset(tex_full)
if not tex:
    raise RuntimeError("не удалось импортировать " + png)

# --- 2) материал неба: Unlit, текстура на весь купол ---
mat_full = PKG_MAT + "/" + MAT_NAME
if unreal.EditorAssetLibrary.does_asset_exist(mat_full):
    unreal.EditorAssetLibrary.delete_asset(mat_full)

mat = tools.create_asset(MAT_NAME, PKG_MAT, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)  # купол видим изнутри

ts = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -420, 0)
ts.set_editor_property("parameter_name", "BGTex")
ts.set_editor_property("texture", tex)

# UV = позиция на ЭКРАНЕ: картинка ложится ровным задником на весь экран,
# без растяжения по куполу (как фон меню игры)
sp = MEL.create_material_expression(mat, unreal.MaterialExpressionScreenPosition, -640, 0)
MEL.connect_material_expressions(sp, "", ts, "UVs")

br = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -420, 250)
br.set_editor_property("parameter_name", "Brightness")
br.set_editor_property("default_value", 3.0)
bm = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, 60)
MEL.connect_material_expressions(ts, "RGB", bm, "A")
MEL.connect_material_expressions(br, "", bm, "B")
MEL.connect_material_property(bm, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

MEL.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(mat_full)
unreal.log("NMS: фон меню игры готов -> " + mat_full + " (текстура " + tex_full + ")")

# --- 3) ПАНОРАМНЫЙ материал (M_SkyPano): HDRI оборачивается вокруг сцены ---
# UV считается из направления взгляда (equirect-проекция) — фон вращается с камерой.
PANO_NAME = "M_SkyPano"
pano_full = PKG_MAT + "/" + PANO_NAME
if unreal.EditorAssetLibrary.does_asset_exist(pano_full):
    unreal.EditorAssetLibrary.delete_asset(pano_full)

pano = tools.create_asset(PANO_NAME, PKG_MAT, unreal.Material, unreal.MaterialFactoryNew())
pano.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
pano.set_editor_property("two_sided", True)

cu = MEL.create_material_expression(pano, unreal.MaterialExpressionCustom, -700, 0)
cu.set_editor_property("code", """
float3 d = normalize(GetTranslatedWorldPosition(Parameters) - ResolvedView.TranslatedWorldCameraOrigin);
float u = atan2(d.y, d.x) / 6.2831853 + 0.5;
float v = acos(clamp(d.z, -1.0, 1.0)) / 3.14159265;
return float2(u, v);
""")
cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
cu.set_editor_property("description", "EquirectUV")

ts2 = MEL.create_material_expression(pano, unreal.MaterialExpressionTextureSampleParameter2D, -420, 0)
ts2.set_editor_property("parameter_name", "BGTex")
ts2.set_editor_property("texture", tex)
MEL.connect_material_expressions(cu, "", ts2, "UVs")
br2 = MEL.create_material_expression(pano, unreal.MaterialExpressionScalarParameter, -420, 250)
br2.set_editor_property("parameter_name", "Brightness")
br2.set_editor_property("default_value", 3.0)
bm2 = MEL.create_material_expression(pano, unreal.MaterialExpressionMultiply, -200, 60)
MEL.connect_material_expressions(ts2, "RGB", bm2, "A")
MEL.connect_material_expressions(br2, "", bm2, "B")
MEL.connect_material_property(bm2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

MEL.recompile_material(pano)
unreal.EditorAssetLibrary.save_asset(pano_full)
unreal.log("NMS: панорамный материал готов -> " + pano_full)
