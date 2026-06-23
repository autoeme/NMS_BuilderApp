# -*- coding: utf-8 -*-
# Текстуры деталей из игры + освещённый материал M_PartLit.
# BaseColor = атлас * (цвет игрока по маске покраски) — как в игре.
# Запуск: py "...\create_part_material.py" в Cmd редактора, потом перезапустить вкладку.

import unreal
import os

CONTENT = unreal.SystemLibrary.get_project_content_directory()
PKG_MAT = "/Game/NMSBaseBuilder/Materials"

tools = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary

# --- 1) импорт всех PNG (PartTex2 — точные текстуры из материалов игры) ---
imported = 0
for sub, pkg in (("PartTex2", "/Game/NMSBaseBuilder/PartTex2"),):
    src = os.path.join(CONTENT, "NMSData", sub)
    if not os.path.isdir(src):
        continue
    for fn in sorted(os.listdir(src)):
        if not fn.endswith(".png"):
            continue
        name = "T_" + fn[:-4].replace(".", "_")
        full = pkg + "/" + name
        if unreal.EditorAssetLibrary.does_asset_exist(full):
            continue
        task = unreal.AssetImportTask()
        task.filename = os.path.join(src, fn)
        task.destination_path = pkg
        task.destination_name = name
        task.automated = True
        task.save = True
        tools.import_asset_tasks([task])
        imported += 1
unreal.log("NMS: текстур импортировано: {}".format(imported))

# превью-сцена не стримит текстуры — выключаем стриминг; PTN* — карты рельефа
fixed = 0
for ad in unreal.EditorAssetLibrary.list_assets("/Game/NMSBaseBuilder/PartTex2"):
    tex = unreal.EditorAssetLibrary.load_asset(ad)
    if not isinstance(tex, unreal.Texture2D):
        continue
    changed = False
    if not tex.get_editor_property("never_stream"):
        tex.set_editor_property("never_stream", True); changed = True
    if "T_PTM" in ad or "T_PTO" in ad:  # маски/затенение: линейное пространство
        if tex.get_editor_property("srgb"):
            tex.set_editor_property("srgb", False); changed = True
    if "T_PTN" in ad:  # нормал-карта: линейное пространство + компрессия нормалей
        if tex.get_editor_property("srgb"):
            tex.set_editor_property("srgb", False); changed = True
        if tex.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_NORMALMAP:
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            changed = True
    if changed:
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
        fixed += 1
unreal.log("NMS: настроено текстур: {}".format(fixed))

# --- 2) материал M_PartLit (освещённый, с атласом и маской покраски) ---
MAT = "M_PartLit"
full = PKG_MAT + "/" + MAT
if unreal.EditorAssetLibrary.does_asset_exist(full):
    unreal.EditorAssetLibrary.delete_asset(full)

m = tools.create_asset(MAT, PKG_MAT, unreal.Material, unreal.MaterialFactoryNew())
# обычная освещённая модель — детали получают свет и объём

base = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, -100)
base.set_editor_property("parameter_name", "BaseTex")

mask = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 200)
mask.set_editor_property("parameter_name", "PaintMask")

# ПОКРАСКА ПО 4 ЗОНАМ (точно как в игре): zone = 4-int(colourisemask.R*4+0.5);
# albedo = diffuse * paletteColor[zone]. colourisemask = параметр PaintMask (.R).
# 4 цвета зон — параметры, ставятся из палитры детали при спавне.
z0 = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -560, 360)
z0.set_editor_property("parameter_name", "ZoneCol0"); z0.set_editor_property("default_value", unreal.LinearColor(1,1,1,1))
z1 = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -560, 440)
z1.set_editor_property("parameter_name", "ZoneCol1"); z1.set_editor_property("default_value", unreal.LinearColor(1,1,1,1))
z2 = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -560, 520)
z2.set_editor_property("parameter_name", "ZoneCol2"); z2.set_editor_property("default_value", unreal.LinearColor(1,1,1,1))
z3 = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -560, 600)
z3.set_editor_property("parameter_name", "ZoneCol3"); z3.set_editor_property("default_value", unreal.LinearColor(1,1,1,1))
white = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -560, 680)
white.set_editor_property("constant", unreal.LinearColor(1,1,1,1))
# выбор цвета зоны по порогам colourisemask.R (R>0.875->z0 ... R<=0.125->white)
def IF(a_in, port_a, thr, gt_node, gt_port, lt_node, lt_port, x, y):
    n = MEL.create_material_expression(m, unreal.MaterialExpressionIf, x, y)
    n.set_editor_property("equals_threshold", 0.0001)
    MEL.connect_material_expressions(a_in, port_a, n, "A")
    cb = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, x-120, y+60)
    cb.set_editor_property("r", thr)
    MEL.connect_material_expressions(cb, "", n, "B")
    MEL.connect_material_expressions(gt_node, gt_port, n, "A > B")
    MEL.connect_material_expressions(gt_node, gt_port, n, "A == B")
    MEL.connect_material_expressions(lt_node, lt_port, n, "A < B")
    return n
c3 = IF(mask, "R", 0.125, z3, "", white, "", -360, 300)
c2 = IF(mask, "R", 0.375, z2, "", c3, "", -300, 300)
c1 = IF(mask, "R", 0.625, z1, "", c2, "", -240, 300)
c0 = IF(mask, "R", 0.875, z0, "", c1, "", -180, 300)
# albedo = diffuse * выбранный цвет зоны
lerp = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -120, 0)
MEL.connect_material_expressions(base, "RGB", lerp, "A")
MEL.connect_material_expressions(c0, "", lerp, "B")
MEL.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

# рельеф (фактура камня/дерева, как в игре)
norm = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 700)
norm.set_editor_property("parameter_name", "NormalTex")
norm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
flat = unreal.load_asset("/Engine/EngineMaterials/FlatNormal")
if flat: norm.set_editor_property("texture", flat)  # дефолт: плоская нормаль
MEL.connect_material_property(norm, "", unreal.MaterialProperty.MP_NORMAL)

# --- карта масок игры: R=затенение швов(AO), G=шероховатость, B=металл ---
mtex = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 950)
mtex.set_editor_property("parameter_name", "MasksTex")
mtex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
wt = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
if wt: mtex.set_editor_property("texture", wt)

use = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, 1200)
use.set_editor_property("parameter_name", "UseMasks")
use.set_editor_property("default_value", 0.0)

# Roughness = lerp(0.9, (1 - masks.R) * 0.9, UseMasks) — ТОЧНО КАК В ИГРЕ:
# шейдер: Roughness = (1 - masks.R) * gMaterialParamsVec4.x(=0.9). masks.R = ГЛАДКОСТЬ!
om = MEL.create_material_expression(m, unreal.MaterialExpressionOneMinus, -460, 870)
MEL.connect_material_expressions(mtex, "R", om, "")    # R = гладкость -> 1-R = шероховатость
r09 = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -460, 800)
r09.set_editor_property("r", 0.9)
rmul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -340, 870)
MEL.connect_material_expressions(om, "", rmul, "A")
MEL.connect_material_expressions(r09, "", rmul, "B")
rbase = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -420, 950)
rbase.set_editor_property("r", 0.9)
rl = MEL.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -220, 950)
MEL.connect_material_expressions(rbase, "", rl, "A")
MEL.connect_material_expressions(rmul, "", rl, "B")
MEL.connect_material_expressions(use, "", rl, "Alpha")
MEL.connect_material_property(rl, "", unreal.MaterialProperty.MP_ROUGHNESS)

# AO больше НЕ берём из masks.R — в игре R это МЕТАЛЛИЧНОСТЬ (проверено по маске:
# бронза R=116, камень R=58). Затенение швов уже запечено в диффузе. AO = 1.
abase = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -420, 1100)
abase.set_editor_property("r", 1.0)
MEL.connect_material_property(abase, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

# Metallic = masks.A * gMaterialParamsVec4.y(=0.5) — ПО ШЕЙДЕРУ ИГРЫ (декодирован):
#   ubershader: metallic = _685.w(masks.A) * ParamsVec4.y. masks.A=1.0 у basicparts ->
#   МЕТАЛЛ РАВНОМЕРНЫЙ 0.5 (было masks.G -> кант зеркалил серебром).
metc = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -340, 1250)
metc.set_editor_property("r", 0.5)
mm = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -220, 1250)
MEL.connect_material_expressions(metc, "", mm, "A")
MEL.connect_material_expressions(use, "", mm, "B")
MEL.connect_material_property(mm, "", unreal.MaterialProperty.MP_METALLIC)

# БРОНЗОВЫЙ ЦВЕТ МЕТАЛЛА: для metallic базовый цвет = цвет отражения. У трима диффуз
# серый -> отражает серебром. Делаем базовый цвет металла бронзовым -> отражает ЗОЛОТО.
# Изолируем ТОЛЬКО трим: smoothstep(0.35,0.55, masks.R) -> стена≈0, трим≈1 (бронза R=116).
bronze = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, -300)
bronze.set_editor_property("parameter_name", "BronzeTint")
bronze.set_editor_property("default_value", unreal.LinearColor(0.83, 0.55, 0.22, 1.0))
mstep = MEL.create_material_expression(m, unreal.MaterialExpressionSmoothStep, -460, 120)
mstep.set_editor_property("const_min", 0.35)
mstep.set_editor_property("const_max", 0.55)
MEL.connect_material_expressions(mtex, "R", mstep, "Value")
bcol = MEL.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -120, -100)
MEL.connect_material_expressions(lerp, "", bcol, "A")     # обычный цвет (стена/кирпич)
MEL.connect_material_expressions(bronze, "", bcol, "B")   # бронза на триме
MEL.connect_material_expressions(mstep, "", bcol, "Alpha")

spec = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -220, 1400)
spec.set_editor_property("r", 0.4)
MEL.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

# «подсветка отовсюду»: грань всегда светится долей своего цвета —
# тыльные стороны никогда не чёрные, стоимость нулевая
amb = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, 1500)
amb.set_editor_property("parameter_name", "Ambient")
amb.set_editor_property("default_value", 0.6)  # тыльные стороны — 60% цвета, не чёрные
em = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -80, 1450)
MEL.connect_material_expressions(lerp, "", em, "A")
MEL.connect_material_expressions(amb, "", em, "B")
MEL.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

MEL.recompile_material(m)
unreal.EditorAssetLibrary.save_asset(full)
unreal.log("NMS: M_PartLit готов -> " + full)

# --- M_PartUnlit: тот же вид, но БЕЗ света и теней (ровный, как иконки) ---
U = "M_PartUnlit"
ufull = PKG_MAT + "/" + U
if unreal.EditorAssetLibrary.does_asset_exist(ufull):
    unreal.EditorAssetLibrary.delete_asset(ufull)
um = tools.create_asset(U, PKG_MAT, unreal.Material, unreal.MaterialFactoryNew())
um.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
ub = MEL.create_material_expression(um, unreal.MaterialExpressionTextureSampleParameter2D, -700, -100)
ub.set_editor_property("parameter_name", "BaseTex")
up = MEL.create_material_expression(um, unreal.MaterialExpressionTextureSampleParameter2D, -700, 200)
up.set_editor_property("parameter_name", "PaintMask")
uc = MEL.create_material_expression(um, unreal.MaterialExpressionVectorParameter, -700, 480)
uc.set_editor_property("parameter_name", "Color")
uc.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
uc2 = MEL.create_material_expression(um, unreal.MaterialExpressionVectorParameter, -700, 600)
uc2.set_editor_property("parameter_name", "Color2")
uc2.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
# двухзонная покраска как в игре: маска 1.0 = основной цвет, 0.5 = вторичный (металл)
# + РЕЛЬЕФ в осях поверхности (tangent space): ровная плоскость = ровно 1.0,
#   впадины темнее, выступы светлее — НЕ зависит от ориентации стены (не серит!)
ul = MEL.create_material_expression(um, unreal.MaterialExpressionCustom, -300, 100)
ul.set_editor_property("code", """
float pw = saturate((Mask - 0.72) / 0.28);
float sw = saturate(1.0 - abs(Mask - 0.5) * 4.0);
float3 c = Tex * lerp(float3(1,1,1), C1, pw);
c *= lerp(float3(1,1,1), C2, sw);
float3 L = normalize(float3(0.45, 0.3, 0.85)); // "солнце" в осях поверхности
float ndl = dot(normalize(N), L);
// 0.8437 = ndl ровной поверхности (N=(0,0,1)) -> множитель там ровно 1.0
c *= clamp(1.0 + K * (ndl - 0.8437), 0.62, 1.22);
return c;
""")
ul.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
i1 = unreal.CustomInput(); i1.set_editor_property("input_name", "Tex")
i2 = unreal.CustomInput(); i2.set_editor_property("input_name", "Mask")
i3 = unreal.CustomInput(); i3.set_editor_property("input_name", "C1")
i4 = unreal.CustomInput(); i4.set_editor_property("input_name", "C2")
i5 = unreal.CustomInput(); i5.set_editor_property("input_name", "N")
i6 = unreal.CustomInput(); i6.set_editor_property("input_name", "K")
ul.set_editor_property("inputs", [i1, i2, i3, i4, i5, i6])
MEL.connect_material_expressions(ub, "RGB", ul, "Tex")
MEL.connect_material_expressions(up, "R", ul, "Mask")
MEL.connect_material_expressions(uc, "", ul, "C1")
MEL.connect_material_expressions(uc2, "", ul, "C2")

# запекаем затенение из карты масок (R = AO): швы темнее, камни выделяются —
# глубина без расчёта света. final = цвет * lerp(1, AO, UseMasks)
umt = MEL.create_material_expression(um, unreal.MaterialExpressionTextureSampleParameter2D, -700, 750)
umt.set_editor_property("parameter_name", "MasksTex")
umt.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
wt2 = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
if wt2: umt.set_editor_property("texture", wt2)
uu = MEL.create_material_expression(um, unreal.MaterialExpressionScalarParameter, -700, 1000)
uu.set_editor_property("parameter_name", "UseMasks")
uu.set_editor_property("default_value", 0.0)
uone = MEL.create_material_expression(um, unreal.MaterialExpressionConstant, -420, 850)
uone.set_editor_property("r", 1.0)
uao = MEL.create_material_expression(um, unreal.MaterialExpressionLinearInterpolate, -300, 900)
MEL.connect_material_expressions(uone, "", uao, "A")
MEL.connect_material_expressions(umt, "R", uao, "B")
MEL.connect_material_expressions(uu, "", uao, "Alpha")
ufin = MEL.create_material_expression(um, unreal.MaterialExpressionMultiply, -80, 300)
MEL.connect_material_expressions(ul, "", ufin, "A")
MEL.connect_material_expressions(uao, "", ufin, "B")

# усилитель яркости: одинаково ярко СО ВСЕХ сторон (как передняя на солнце)
ubr = MEL.create_material_expression(um, unreal.MaterialExpressionScalarParameter, -700, 1250)
ubr.set_editor_property("parameter_name", "Brightness")
ubr.set_editor_property("default_value", 0.9)
ufin2 = MEL.create_material_expression(um, unreal.MaterialExpressionMultiply, 40, 320)
MEL.connect_material_expressions(ufin, "", ufin2, "A")
MEL.connect_material_expressions(ubr, "", ufin2, "B")
MEL.connect_material_property(ufin2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(um)
unreal.EditorAssetLibrary.save_asset(ufull)
unreal.log("NMS: M_PartUnlit gotov")
