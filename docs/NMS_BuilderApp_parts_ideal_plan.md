# NMS_BuilderApp — План доведения всех деталей до идеала

Статус: активный. Заведён 2026-06-26.

Цель: каждая деталь собирается и переносится 1‑в‑1 как в игре. Сначала довести
**все детали до идеала** (верный меш + геометрия + текстуры + цвет), затем —
ориентация. Источник истины = файлы игры (правила 16/7/31), без эвристики (правило 17).

## Definition of Done одной детали
1. **Верный меш** — стиль‑специфичный из `basebuildingpartstable` (правило 17), не generic.
2. **Целая геометрия** LOD0 — без shadow/collision/LOD1‑3, индексы из `MeshDataStream[VertexDataSize:]`.
3. **Per‑slot текстуры** — каждой секции `usemtl` свой материал из `material_atlas_index`.
4. **Дефолтный цвет** из строительной палитры (115 свотчей, `userdata_palette.json`).
5. **Импортирован** в проект, грузится по `ModelPath`.
6. **Ориентация** верная (пандусы закрыты — см. memory `ramp-needs-180`; остальные направленные — этап 5).

## Базовый аудит (2026-06-26, 2434 детали в `Content/nms_parts_db.json`)
- 2355 — меш .uasset в проекте.
- 24 — меша нет (= ~5 уникальных мешей: `ext_decoration_1x1_0/1`, `airlockconnector`, `robothand`, `corridor_stairs`).
- 55 — `ModelPath` пустой (триаж на этапе 1).
- «Не тот меш» — считается на этапе 0 (быстрый прогон шумит, нужна авторитетная карта).

---

## Этап 0 — Авторитетная карта сборки (только данные, без риска)
Построить достоверную `деталь → (part‑ID, стиль) → сцена → ожидаемый меш/материалы/цвет/снап`
и сверить с проектом. part‑ID из master `PlacementScene`/`variants` + partstable, кросс‑стили
через `legacyitemtable`. Не простым срезом префикса (там ошибки).
- Выход: `build_map.json` (эталон) + `worklist.json` (что чинить, приоритеты).
- Проверка: контроль на S_RAMP / S_WALL / окнах — карта совпадает с фактом.
- Риск: нет (read‑only).

### Результаты Этапа 0 (2026-06-26)
Авторитетная цепочка установлена: `basebuildingobjectstable` (объект → Style + Composites/SinglePartID/Descriptor + PlacementScene) → `basebuildingpartstable[part-ID][Style]` → сцена → меш. part-ID листовой детали = срез буквы стиля (`S_RAMP`→`_RAMP`); композиты (`S_WALL`→`S_WALLB/M/T`) и легаси — отдельно. PlacementScene НЕ годится как ссылка на меш (там generic-превью).

Резолвер (objectstable Style + SinglePartID + срез стиля → partstable, точный стиль без fallback).
Контроли пройдены (пандусы/стены — 0 ложных). Полный worklist: scratchpad/stage0_worklist.json.

- **OK (меш верный): 1073** (включая +198 через SinglePartID).
- **WRONG: 73**, разложены триажем:
  - **`real_wrong` (27) — РЕАЛЬНАЯ ОШИБКА:** декоративные растения биомов стоят контейнерами/expedition‑пропами вместо растений. Семьи: `BASE_DUST` (12), `BASE_HOTPLANT` (5), `BASE_WPLANT` (2), по 1: `BASE_JUNGLE/RADPLANT/SUBZPLANT/TOXPLANT`; плюс `BLD_SKULL` (skull_rex→expeditionrewardskull04), `SWARM_FLAG_R2/G2/B2` (флаг вместо walldrape). Пример: `BASE_DUST01` cur=`expedition_01` → ожид. `desolatepalmlarge`. Ожидаемые сцены ВСЕ существуют → пересобираемо.
  - **`placeholder` (33):** меш назван по ID (фоссилы `FOS_*` → biped/bird/quadruped/…; `SET_*`, `SPEC_FIREWORK*`, `GAMETABLE`, `BASE_ROOT`). Ожидаемые сцены существуют → пересобрать корректно.
  - **`naming_only` (13):** тот же меш, другое имя (`CORRIDOR*_SPACE` удвоено; `W/C/M_WALL`→generic «wall», а у нас стиль‑специфичный — лучше; `SWARM_FLAG_*1`→flagpole). Низкий приоритет.
- **COMPOSITE: 151** — наш единый меш = представление; сверять по под‑частям.
- **EMPTY (ModelPath пуст): 55** — этап 1.
- **НЕ проверено двумя таблицами:** `нет в objectstable` 397 (фоссилы‑части FOS_*_BODY_* 155, фрейтер, большие структуры, под‑части стен WALLB/M/T) + `no_partstable без SinglePartID` 685 (BLD_*/BASE_* декор — вероятно одиночные меши по имени ID). Нужны `legacybasebuildingtable` (есть в PRECACHE_FULL) + фоссил/корвет‑данные. Следующий под‑шаг.

Итого строго подтверждено: 1146 (1073 OK + 73 разобранных). Приоритет на пересборку: 27 real + 33 placeholder = **60 деталей**.

### Выполнено — конвертация+импорт (2026-06-26)
Пройдена полная цепочка для приоритетных деталей:
- Конвертер: `NMS_EXTRACT/10_TOOLS/meshwork/convert_lod0.py` (numpy — запускать в Blender; пути в скрипте linux-песочницы, переопределять на Windows). Карта: `geo_scene_map.json`. Данные: сцены `SCENES_PARTS_NEW`, геометрия `MBINCompiler/UNPACK_ALL_MESHES`.
- **Сконвертировано 46/52 OBJ** (растения биомов, дисплеи фоссилов, фейерверки, walldrape и т.д.). Не вышли: FOS_QUAD/FOS_WORM (empty), GAMETABLE/BASE_ROOT (nonodes) + 8 SET_* без геометрии — отложены.
- **Импортировано 43/43 уникальных меша** в `/Game/NMSBaseBuilder/GameMeshes` (0 ошибок).
  - Импорт UE headless: **полный** `UnrealEditor.exe ... -ExecutePythonScript=<скрипт БЕЗ пробелов в пути> -nosound -unattended -nosplash` (НЕ `-run=pythonscript` — Interchange падает в commandlet без Slate; путь скрипта с пробелами тоже ломается — класть в scratchpad). Скрипт сам пишет результат и `quit_editor()`. Шаблон: `Content/Python/reimport_stage2.py` + список `stage2_import_list.json`.
- **Обновлено 46 ModelPath** в `Content/nms_parts_db.json` (напр. `BASE_DUST01: expedition_01 → desolatepalmlarge`). Приложение читает JSON ПЕРВЫМ (`SNMSBuilderUI.Data.cpp::LoadFromJsonFallback`) → переимпорт DataTable НЕ нужен, видно после перезапуска.

### Выполнено — текстуры растений (Этап 4, 2026-06-26)
- Источник PNG: `MBINCompiler/TEXBIOMESCOMMON_FULL/.../foliage/<имя>.<sub>.png` (sub: diffuse без суффикса / masks / normal / colourisemask). Индекс по атлас-имени `<папка>_<файл>_<sub>`.
- **Импортировано 61 текстура** в `/Game/NMSBaseBuilder/PartTex2` как `T_<atlasname>` (флаги: normal→sRGB off+TC_NORMALMAP+flip green; masks/colourise→sRGB off). Скрипт `scratchpad/import_tex_stage4.py` (NB: у Texture2D в UE5.8 НЕТ `post_edit_change()` — не вызывать; `set_editor_property` + `save_directory` достаточно).
- **30 записей в `part_textures.json`** (ObjectID→{tex,masks,norm,mask=colourisemask}) — все растения биомов теперь с текстурами.
- Не покрыто текстурами (16): фоссил-дисплеи (interior trim — текстуры в TexSpace), BLD_SKULL (poi_bone), SET_STAFFBUILD (robots) — их PNG в других дампах, follow-up.

Осталось из приоритетных: 6 неконвертнутых (фоссил-туши/модульные) + 8 SET_* (геометрия под другим именем). Дальше: легаси/фрейтер/фоссил-части (397), композиты (151), текстуры для 16 непокрытых дисплеев.

## Этап 1 — Триаж пустых и недостающих
55 пустых + 24 без меша → метки `legit_no_mesh` (decals/posters/DESCRIPTOR‑процедурные/menu‑root)
/ `need_convert` / `need_reference_assembly` (модульные). 24 свести к ~5 уникальным сценам.
- Выход: `to_convert.json`.

## Этап 2 — Конвертация мешей (Blender MCP)
`convert_lod0`: scene+geometry → OBJ, только LOD0, отброс shadow/collision/LOD1‑3, нарезка по
`usemtl`, применение node‑Transform. Модульные (REFERENCE) — рекурсивный обход. «Монстры» (40,
100к+ верш.) — отдельным заходом. Автопроверка: Σтреуг = IndexDataSize/6, индексы < N, bbox ок.
- Выход: OBJ в `MeshSrc/` + `slots`‑карта.

## Этап 3 — Импорт в UE (редактор закрыт)
`reimport`: `combine_static_meshes=True`, `nanite=False`, `generate_lightmap_uvs=False`,
`collision=False`, `replace_existing=True`, UE `-nosound`. Тяжёлые — отдельной пачкой.
Обновить `ModelPath` для исправленных деталей.

## Этап 4 — Текстуры и цвет (per‑slot)
Секция `usemtl` → diffuse/masks/normal/colourise (PNG) из атласа. Ubershader: Roughness=(1-masks.R)*0.9,
Metallic=masks.G, AO=1. Дефолтный цвет запечён в диффуз. Контроль — «камень‑идеал» из заметок.

## Этап 5 — Ориентация (добивка)
Объективный скан «асимметрия меша vs снап‑точки» по направленным (двери/декор) → список
180°/зеркальных. Расширить подход `NMS_IsRampPart` в общий механизм коррекции (таблица
«деталь→доворот»), без глобального хака.

## Этап 6 — Финал
Тесты (`.hg` roundtrip / JSON / transform), Git LFS чист, нет backup/temp, осмысленные коммиты.

---

## Источники данных (где что лежит)
- Каталог: `Content/nms_parts_db.json`; паспорт: `Content/NMSData/master_parts_data.json`.
- Стиль‑модели: `Content/NMSData/partstable_stylemodels.json`.
- Сцены: `C:\Users\User\Desktop\NMS_EXTRACT\SCENES_PARTS_NEW` (.SCENE.MXML).
- Геометрия: `C:\Users\User\Desktop\MBINCompiler\UNPACK_ALL_MESHES`.
- OBJ: `MeshSrc/`. Материалы: `material_atlas_index(_FULL).json`. Цвет: `userdata_palette.json`. Снап: `snapping_info.json`.
- Свежие таблицы после апдейта: `MBINCompiler\PRECACHE_FULL` (basebuildingpartstable/objectstable).
- Полные заметки/правила: `C:\Users\User\Desktop\NMS_EXTRACT\6_LOGIC_NOTES`.
