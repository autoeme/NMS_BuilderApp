import unreal, os, json
CONTENT = unreal.SystemLibrary.get_project_content_directory()
PROJ = os.path.normpath(os.path.join(CONTENT, ".."))
MESHSRC = os.path.join(PROJ, "MeshSrc")
PKG = "/Game/NMSBaseBuilder/GameMeshes"
names = json.load(open(os.path.join(CONTENT,"Python","aligned_list.json")))
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.OBJ 1")
tools = unreal.AssetToolsHelpers.get_asset_tools()
def make_options():
    pl = unreal.InterchangeGenericAssetsPipeline(); mp = pl.mesh_pipeline
    for prop, val in (("build_nanite", False),("build_reversed_index_buffer", False),
                      ("collision", False),("generate_lightmap_u_vs", False),("combine_static_meshes", True)):
        try: mp.set_editor_property(prop, val)
        except Exception: pass
    ov = unreal.InterchangePipelineStackOverride(); ov.add_pipeline(pl); return ov
unreal.log("NMS_ALIGN: start %d" % len(names))
imp=0; miss=0
for name in names:
    path = os.path.join(MESHSRC, name + ".obj")
    if not os.path.exists(path): miss+=1; unreal.log("NMS_ALIGN miss "+name); continue
    t=unreal.AssetImportTask(); t.filename=path; t.destination_path=PKG; t.destination_name=name
    t.automated=True; t.save=True; t.replace_existing=True; t.options=make_options()
    tools.import_asset_tasks([t]); imp+=1
    unreal.log("NMS_ALIGN ok %d/%d %s"%(imp,len(names),name))
unreal.log("NMS_ALIGN GOTOVO imported=%d miss=%d"%(imp,miss))
