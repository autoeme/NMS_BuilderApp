import unreal, os

# Чистый переимпорт ПОЛНЫХ атласов (без резки) для timber/builders/alloy
# с ПРИНУДИТЕЛЬНЫМИ игровыми настройками (как у камня, что дало идеал).
# Флаги sRGB сверены по материалам игры: diffuse=true, masks/normal/colourise=false.

PROJ = unreal.Paths.project_dir()
SRC  = os.path.join(PROJ, "TexSrcAll")
DEST = "/Game/NMSBaseBuilder/PartTex2"

STYLES = ["woodtrim", "builderstrim", "fiberglasstrim"]

# суффикс -> (srgb, compression)
KIND = {
    "":             (True,  unreal.TextureCompressionSettings.TC_DEFAULT),
    "_masks":       (False, unreal.TextureCompressionSettings.TC_MASKS),
    "_normal":      (False, unreal.TextureCompressionSettings.TC_NORMALMAP),
    "_colourisemask":(False, unreal.TextureCompressionSettings.TC_MASKS),
}

tools = unreal.AssetToolsHelpers.get_asset_tools()
eas   = unreal.EditorAssetSubsystem()

ok = 0
for style in STYLES:
    for suf, (srgb, comp) in KIND.items():
        name = "T_basicparts_%s%s" % (style, suf)
        path = os.path.join(SRC, name + ".png")
        if not os.path.exists(path):
            unreal.log_error("NMS: net fajla " + path); continue

        task = unreal.AssetImportTask()
        task.filename         = path
        task.destination_path = DEST
        task.destination_name = name
        task.replace_existing = True
        task.automated        = True
        task.save             = True
        tools.import_asset_tasks([task])

        obj = eas.load_asset(DEST + "/" + name)
        if obj:
            obj.set_editor_property("srgb", srgb)
            obj.set_editor_property("compression_settings", comp)
            obj.set_editor_property("never_stream", False)
            eas.save_asset(DEST + "/" + name)
            ok += 1
            unreal.log("NMS: OK -> %s (srgb=%s)" % (name, srgb))

unreal.log("NMS: STYLES GOTOVO, pereimport %d tekstur. Perezapusti vkladku." % ok)
