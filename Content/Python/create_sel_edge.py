# -*- coding: utf-8 -*-
# Создаёт /Game/NMSBaseBuilder/Materials/M_SelEdge — зелёный КОНТУР выделения (Fresnel).
# Накладывается как OverlayMaterial: текстура/цвет детали видны, зелёным светятся только края.
import unreal
PKG="/Game/NMSBaseBuilder/Materials"; NAME="M_SelEdge"; full=PKG+"/"+NAME
tools=unreal.AssetToolsHelpers.get_asset_tools(); MEL=unreal.MaterialEditingLibrary
if unreal.EditorAssetLibrary.does_asset_exist(full):
    unreal.EditorAssetLibrary.delete_asset(full)
mat=tools.create_asset(NAME,PKG,unreal.Material,unreal.MaterialFactoryNew())
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("two_sided", True)
# зелёный цвет края
col=MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter,-450,0)
col.set_editor_property("parameter_name","EdgeColor")
col.set_editor_property("default_value", unreal.LinearColor(0.15,1.0,0.45,1.0))
MEL.connect_material_property(col,"",unreal.MaterialProperty.MP_EMISSIVE_COLOR)
# Fresnel -> прозрачность только по краям
fr=MEL.create_material_expression(mat, unreal.MaterialExpressionFresnel,-450,260)
fr.set_editor_property("exponent", 4.5)
fr.set_editor_property("base_reflect_fraction", 0.0)
strg=MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter,-450,430)
strg.set_editor_property("parameter_name","EdgeStrength"); strg.set_editor_property("default_value",1.0)
mul=MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply,-180,300)
MEL.connect_material_expressions(fr,"",mul,"A")
MEL.connect_material_expressions(strg,"",mul,"B")
MEL.connect_material_property(mul,"",unreal.MaterialProperty.MP_OPACITY)
MEL.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full)
unreal.log("NMS: M_SelEdge (зелёный контур) создан -> "+full)
