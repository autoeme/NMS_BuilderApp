import unreal, os, glob

# Импорт НОВЫХ мешей: все OBJ из MeshSrc, у которых ещё нет .uasset в GameMeshes.
# Покрывает 22 диагонали/крыши/окна по стилям + всё, что доконвертировано.

CONTENT = unreal.SystemLibrary.get_project_content_directory()
PROJ = os.path.normpath(os.path.join(CONTENT, ".."))
MESHSRC = os.path.join(PROJ, "MeshSrc")
PKG = "/Game/NMSBaseBuilder/GameMeshes"
PKG_DIR = os.path.join(CONTENT, "NMSBaseBuilder", "GameMeshes")

unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.OBJ 1")
tools = unreal.AssetToolsHelpers.get_asset_tools()

def make_options():
    pl = unreal.InterchangeGenericAssetsPipeline()
    mp = pl.mesh_pipeline
    for prop, val in (("build_nanite", False), ("build_reversed_index_buffer", False),
                      ("collision", False), ("generate_lightmap_u_vs", False),
                      ("combine_static_meshes", True)):
        try: mp.set_editor_property(prop, val)
        except Exception: pass
    ov = unreal.InterchangePipelineStackOverride(); ov.add_pipeline(pl)
    return ov

objs = glob.glob(os.path.join(MESHSRC, "*.obj"))
new = []
for o in objs:
    name = os.path.splitext(os.path.basename(o))[0]
    if not os.path.exists(os.path.join(PKG_DIR, name + ".uasset")):
        new.append((name, o))

unreal.log("NMS: novyh meshey k importu: %d" % len(new))
imp = 0
for name, path in new:
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = PKG
    task.destination_name = name
    task.automated = True; task.save = True; task.replace_existing = True
    task.options = make_options()
    tools.import_asset_tasks([task])
    imp += 1
    if imp % 50 == 0: unreal.log("NMS: imported %d / %d" % (imp, len(new)))

unreal.log("NMS: GOTOVO. Importirovano novyh meshey: %d. Perezapusti vkladku." % imp)
