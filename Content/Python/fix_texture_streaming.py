# -*- coding: utf-8 -*-
# ФИКС МУТИ: снять never_stream со всех текстур деталей.
# Причина мути: 1044 текстуры с never_stream=True переполняли пул стриминга UE
# (программный лимит ~1000 МБ, не зависит от мощности GPU) -> движок сбрасывал
# их в низкий мип. Включаем нормальный стриминг: видимые грузятся в полном
# разрешении, остальные — по мере надобности. Память не забивается (для слабых ПК).
# Запуск в редакторе: py "...\fix_texture_streaming.py"
import unreal

DST = "/Game/NMSBaseBuilder/PartTex2"
reg = unreal.AssetRegistryHelpers.get_asset_registry()
assets = unreal.EditorAssetLibrary.list_assets(DST, recursive=True)
done = 0
for path in assets:
    obj = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(obj, unreal.Texture2D):
        continue
    try:
        obj.set_editor_property("never_stream", False)
        # подстраховка: не занижать резкость
        obj.set_editor_property("lod_bias", 0)
        obj.post_edit_change()
        done += 1
        if done % 200 == 0:
            unreal.log("NMS TEX: обработано {}".format(done))
    except Exception as e:
        unreal.log_warning("NMS TEX: {} -> {}".format(path, e))

unreal.EditorAssetLibrary.save_directory(DST, only_if_is_dirty=True, recursive=True)
# увеличим пул на всякий случай (для редактора), чтобы наверняка резко
unreal.SystemLibrary.execute_console_command(None, "r.Streaming.PoolSize 3000")
unreal.log("NMS TEX: never_stream снят с {} текстур. Перезапусти вкладку строителя.".format(done))
