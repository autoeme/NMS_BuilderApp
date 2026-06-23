# -*- coding: utf-8 -*-
# Генерирует превью-картинки деталей из их 3D-моделей.
# Каждый меш рендерится в Content/NMSData/Thumbs/<ID>.png (256x256).
# Запуск: py "...\generate_thumbnails.py" в консоли Cmd редактора.
# Повторный запуск продолжает с места остановки (готовые пропускаются).

import unreal
import json
import os

CONTENT = unreal.SystemLibrary.get_project_content_directory()
OUT = os.path.join(CONTENT, "NMSData", "Thumbs")
if not os.path.isdir(OUT):
    os.makedirs(OUT)

with open(os.path.join(CONTENT, "nms_parts_db.json"), "r", encoding="utf-8") as f:
    parts = json.load(f)

world = unreal.EditorLevelLibrary.get_editor_world()

rt = unreal.RenderingLibrary.create_render_target2d(
    world, 256, 256, unreal.TextureRenderTargetFormat.RTF_RGBA8)

cap_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.SceneCapture2D, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
cap = cap_actor.capture_component2d
cap.set_editor_property("texture_target", rt)
cap.set_editor_property("primitive_render_mode",
    unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
cap.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
cap.set_editor_property("capture_every_frame", False)
# отключаем небо/туман/блюм в кадре — фон будет чёрным, деталь видна чётко
cap.set_editor_property("show_flags_settings", [
    unreal.EngineShowFlagsSetting(show_flag_name="Atmosphere", enabled=False),
    unreal.EngineShowFlagsSetting(show_flag_name="Fog", enabled=False),
    unreal.EngineShowFlagsSetting(show_flag_name="Bloom", enabled=False),
])

done = 0
skipped = 0
total = len(parts)
for p in parts:
    mp = p.get("ModelPath", "")
    pid = p.get("ObjectID", "").lstrip("^")
    if not mp or not pid:
        skipped += 1
        continue
    out_png = os.path.join(OUT, pid + ".png")
    # перезаписываем существующие (для перегенерации); чтобы пропускать готовые —
    # верни строки: if os.path.exists(out_png): done += 1; continue
    mesh = unreal.EditorAssetLibrary.load_asset(mp)
    if not mesh:
        skipped += 1
        continue
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(0, 0, 200000), unreal.Rotator(0, 0, 0))
    smc = actor.static_mesh_component
    smc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    smc.set_static_mesh(mesh)

    origin, extent = actor.get_actor_bounds(False)
    size = max(extent.x, extent.y, extent.z, 10.0)
    dist = size * 2.0  # ближе — деталь крупнее в кадре
    d = unreal.Vector(-1.0, -0.8, 0.7).normal()
    cam = unreal.Vector(origin.x + d.x * dist, origin.y + d.y * dist, origin.z + d.z * dist)
    rot = unreal.MathLibrary.find_look_at_rotation(cam, origin)
    cap_actor.set_actor_location_and_rotation(cam, rot, False, False)
    cap.clear_show_only_components()
    cap.show_only_actor_components(actor, True)
    cap.capture_scene()
    unreal.RenderingLibrary.export_render_target(world, rt, OUT, pid + ".png")
    unreal.EditorLevelLibrary.destroy_actor(actor)
    done += 1
    if done % 100 == 0:
        unreal.log("NMS thumbs: {}/{}".format(done, total))

unreal.EditorLevelLibrary.destroy_actor(cap_actor)
unreal.log("NMS thumbs: ГОТОВО {} (пропущено {})".format(done, skipped))
