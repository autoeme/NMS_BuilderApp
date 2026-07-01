# -*- coding: utf-8 -*-
# Import newly-extracted PartTex2 PNGs into UE as T_<name> (diffuse/masks/normal/cmask),
# with correct flags. Non-destructive: skips textures already present. No material changes.
import unreal, os

CONTENT = unreal.SystemLibrary.get_project_content_directory()
SRC = os.path.join(CONTENT, "NMSData", "PartTex2")
PKG = "/Game/NMSBaseBuilder/PartTex2"
tools = unreal.AssetToolsHelpers.get_asset_tools()

imported = 0
for fn in sorted(os.listdir(SRC)):
    if not fn.endswith(".png"):
        continue
    name = "T_" + fn[:-4].replace(".", "_")
    full = PKG + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        continue
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SRC, fn)
    task.destination_path = PKG
    task.destination_name = name
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    imported += 1
unreal.log("NMS TEX: imported %d new textures" % imported)

# flags: masks/normal/colourisemask are linear; normal is a normal map; never stream
fixed = 0
for ad in unreal.EditorAssetLibrary.list_assets(PKG):
    tex = unreal.EditorAssetLibrary.load_asset(ad)
    if not isinstance(tex, unreal.Texture2D):
        continue
    low = ad.lower()
    changed = False
    if not tex.get_editor_property("never_stream"):
        tex.set_editor_property("never_stream", True); changed = True
    is_linear = any(s in low for s in ("_masks", "_normal", "_colourisemask", "t_ptm", "t_pto", "t_ptn"))
    if is_linear and tex.get_editor_property("srgb"):
        tex.set_editor_property("srgb", False); changed = True
    if ("_normal" in low or "t_ptn" in low) and tex.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_NORMALMAP:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP); changed = True
    if changed:
        unreal.EditorAssetLibrary.save_loaded_asset(tex); fixed += 1
unreal.log("NMS TEX: flags fixed on %d textures" % fixed)
unreal.EditorAssetLibrary.save_directory(PKG, only_if_is_dirty=False, recursive=False)
unreal.log("NMS TEX: DONE")
unreal.SystemLibrary.quit_editor()
