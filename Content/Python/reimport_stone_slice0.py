import unreal, os

# Переимпорт каменного атласа: слой 0 (песочный, 2048x2048) из TexSrcAll.
# Источники уже обрезаны до верхнего слоя. Игра использует именно слой 0.

PROJ = unreal.Paths.project_dir()           # .../NMS_BuilderApp/
SRC  = os.path.join(PROJ, "TexSrcAll")
DEST = "/Game/NMSBaseBuilder/PartTex2"

# имя файла-источника -> (имя ассета, srgb, compression)
MAPS = [
    ("T_basicparts_stonetrim_1.png",              True,  unreal.TextureCompressionSettings.TC_DEFAULT),
    ("T_basicparts_stonetrim_masks_1.png",        False, unreal.TextureCompressionSettings.TC_MASKS),
    ("T_basicparts_stonetrim_normal_1.png",       False, unreal.TextureCompressionSettings.TC_NORMALMAP),
    ("T_basicparts_stonetrim_colourisemask_1.png",False, unreal.TextureCompressionSettings.TC_MASKS),
]

tools = unreal.AssetToolsHelpers.get_asset_tools()
eas   = unreal.EditorAssetSubsystem()

for fname, srgb, comp in MAPS:
    path = os.path.join(SRC, fname)
    name = os.path.splitext(fname)[0]
    if not os.path.exists(path):
        unreal.log_error("NMS: net fajla " + path); continue

    task = unreal.AssetImportTask()
    task.filename        = path
    task.destination_path= DEST
    task.destination_name= name
    task.replace_existing= True
    task.automated       = True
    task.save            = True
    tools.import_asset_tasks([task])

    # выставить правильные настройки текстуры
    obj = eas.load_asset(DEST + "/" + name)
    if obj:
        obj.set_editor_property("srgb", srgb)
        obj.set_editor_property("compression_settings", comp)
        obj.set_editor_property("never_stream", False)
        eas.save_asset(DEST + "/" + name)
        unreal.log("NMS: pereimport OK -> " + name + " (2048x2048 sloy0)")

unreal.log("NMS: STONE slice0 GOTOVO. Perezapusti vkladku detali.")
