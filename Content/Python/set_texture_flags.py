# -*- coding: utf-8 -*-
# Правка флагов игровых текстур (запускать в редакторе):
#   T_PTN## (нормали)  -> Flip Green Channel (NMS -Y против UE +Y)
#   T_PTM## (маски)    -> SRGB OFF (линейные данные)
#   T_PTO## (затенение)-> SRGB OFF
# Если после запуска впадины на эталонных стенах стали ВЫПУКЛОСТЯМИ —
# поставь FLIP_GREEN = False и запусти ещё раз.
import unreal

FLIP_GREEN = False

reg = unreal.AssetRegistryHelpers.get_asset_registry()
assets = reg.get_assets_by_path("/Game/NMSBaseBuilder/PartTex2", recursive=True)
n_flip = n_srgb = 0
for a in assets:
    name = str(a.asset_name)
    tex = a.get_asset()
    if not isinstance(tex, unreal.Texture2D):
        continue
    changed = False
    if name.startswith("T_PTN"):
        if tex.get_editor_property("flip_green_channel") != FLIP_GREEN:
            tex.set_editor_property("flip_green_channel", FLIP_GREEN)
            changed = True; n_flip += 1
    elif name.startswith("T_PTM") or name.startswith("T_PTO"):
        if tex.get_editor_property("srgb"):
            tex.set_editor_property("srgb", False)
            changed = True; n_srgb += 1
    if changed:
        tex.post_edit_change()
        unreal.EditorAssetLibrary.save_loaded_asset(tex)

print("Flip Green переключён: %d | SRGB исправлен: %d" % (n_flip, n_srgb))
print("ГОТОВО. Проверь эталонные стены: впадины должны остаться впадинами.")
