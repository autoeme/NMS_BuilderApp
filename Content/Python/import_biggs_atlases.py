import unreal, os, glob

# Импорт biggs-атласов корветов из PNG (UE не читал их DDS-формат -> конвертнули в PNG).
# Источник: <проект>\TexSrcBiggs\T_biggs_*.png  | Цель: PartTex2.
# Настройки игровые: diffuse srgb ON; masks/normal/colourise srgb OFF.

CONTENT = unreal.SystemLibrary.get_project_content_directory()
PROJ = os.path.normpath(os.path.join(CONTENT, ".."))
SRC  = os.path.join(PROJ, "TexSrcBiggs")
DEST = "/Game/NMSBaseBuilder/PartTex2"

tools = unreal.AssetToolsHelpers.get_asset_tools()
eas   = unreal.EditorAssetSubsystem()

def kind(n):
    n = n.lower()
    if "normal" in n:        return False, unreal.TextureCompressionSettings.TC_NORMALMAP
    if "masks" in n:         return False, unreal.TextureCompressionSettings.TC_MASKS
    if "colourisemask" in n: return False, unreal.TextureCompressionSettings.TC_MASKS
    if "occlusion" in n:     return False, unreal.TextureCompressionSettings.TC_MASKS
    return True, unreal.TextureCompressionSettings.TC_DEFAULT

files = glob.glob(os.path.join(SRC, "T_biggs_*.png"))
unreal.log("NMS BIGGS PNG: naydeno = %d" % len(files))
ok = 0; fail = 0
for f in files:
    name = os.path.splitext(os.path.basename(f))[0]
    srgb, comp = kind(name)
    try:
        task = unreal.AssetImportTask()
        task.filename = f
        task.destination_path = DEST
        task.destination_name = name
        task.replace_existing = True
        task.automated = True
        task.save = True
        tools.import_asset_tasks([task])
        obj = eas.load_asset(DEST + "/" + name)
        if obj:
            obj.set_editor_property("srgb", srgb)
            obj.set_editor_property("compression_settings", comp)
            obj.set_editor_property("never_stream", False)
            eas.save_asset(DEST + "/" + name)
            ok += 1
        else:
            fail += 1
    except Exception as e:
        fail += 1
        unreal.log_warning("NMS BIGGS FAIL %s: %s" % (name, str(e)[:60]))

unreal.log("NMS BIGGS PNG: GOTOVO. OK=%d Fail=%d. Napishi Klodu chisla." % (ok, fail))
