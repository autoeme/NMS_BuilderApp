# План разделения `SNMSBuilderUI` (3684 строки → ~7 файлов класса + 4 общих)

## Принцип
Класс и **всё его состояние** (заголовок `SNMSBuilderUI.h`) остаются нетронутыми.
Меняется только физическое размещение: stateless-код → reusable-файлы,
тела методов → несколько `.cpp` одного класса. Поведение не меняется.

Расположение исходников: `Source/NMS_BuilderApp/`

---

## Часть A. Новые общие файлы (вынос stateless-кода)

### A1. `NMSStyle.h` — тема и стилевые билдеры (header-only)
Переносится `namespace NMS` (строки **207–257**): цветовые константы + `Box() / Font() / FlatButton() / Fill()`.
- Используется **93 раза** во всех UI-методах → обязан быть в заголовке.
- Константы делаем `inline const FLinearColor` (C++17, единое определение, без ODR-проблем).
- `FlatButton()` с внутренним `static Cache` → вынести **тело в `NMSStyle.cpp`** (чтобы кэш был один на всё приложение, а не по копии на TU).

### A2. `NMSLocalization.h/.cpp` — i18n (строки **66–206**)
- В `.cpp`: file-static глобалы `GNms*`, `NMS_ReadJsonObj`, `NMS_LoadI18n`, `NMS_Pick`, `NMS_SetLang`.
- В `.h` (публичное API): `NMS_T` (19×), `NMS_PartName` (2×), `NMS_ColName`, `NMS_PalTQ`, `NMS_SetLang`, список языков.
- Глобалы **остаются приватными** внутри `.cpp` — наружу только функции.

### A3. `NMSPartVisuals.h/.cpp` — цвета/текстуры/слоты деталей (строки **262–413** + **2526** + **1218–1255**)
- `.h`: struct `FNMSTexSet`; объявления `NMS_DefaultPartColor`, `NMS_DefaultPartColor2`, `NMS_GamePartColors`, `NMS_PartTexture`, `NMS_PartSlots`, `NMS_ColourFromIndex`.
- `.cpp`: их тела + статические кэши-карты (`part_colors.json`, `part_textures.json`, `part_slots.json`).
- ⚠️ `NMS_DefaultPartColor` физически переезжает с 2526 — убрать forward-декларацию (262), она больше не нужна.

### A4. `NMSUIHelpers.h/.cpp` — мелкие UI-фабрики (строки **414–483**)
- `MakeSectionHeader / MakeField / MakeTransformRow / MakePropertyRow` (8 использований).
- Зависят от `NMSStyle.h` → включают его.

**Итого вынесено из файла: ≈ 525 строк, нулевой риск (код не трогает члены класса).**

---

## Часть B. Общий приватный заголовок включений

### B1. `SNMSBuilderUI_Internal.h`
Содержит блок из ~60 `#include` (строки **1–63**) + новые 4 заголовка (A1–A4).
Каждый `.cpp` класса включает только его — чтобы не дублировать список инклудов семь раз.
- `LOCTEXT_NAMESPACE` **не** кладём сюда: `#define`/`#undef` повторяем в каждом `.cpp` (требование UE).

---

## Часть C. Разнесение методов класса по `.cpp` (один класс — несколько TU)

| Новый файл | Методы (по карте) | ~строк |
|---|---|---|
| `SNMSBuilderUI.cpp` (каркас) | `Construct`, `Tick`, `BuildCenterArea`, `BuildViewport`, `BuildTabStrip`, `BuildMenuButton`, `MakeMenuContent`, `OnMenuButtonClicked`, `ApplyBackground`, `CycleBackground`, `GetUIIcon`, `GetPartThumb` | ~600 |
| `SNMSBuilderUI.Parts.cpp` | браузер: `BuildPartListPanel`, `BuildCategoryColumn`, `MakePartCard`, `MakeMoveMenu`, `MakeVariantMenu`, `RebuildSectionedGrid`, `OnGeneratePartTile`, `SelectCategory`, `ApplyFilter`, `OnSearchChanged`, `TogglePartList`, `OnPartSelected`, `MovePartToCategory` | ~620 |
| `SNMSBuilderUI.Data.cpp` | загрузка/persistence: `LoadPartsTable`, `LoadFromDataTable`, `LoadFromJsonFallback`, `LoadPartVariants`, `LoadPartSections`, `LoadFavorites/SaveFavorites`, `AddToFavorites`, `AddToPreset`, `LoadUserCategories/SaveUserCategories`, `BuildCategories` | ~285 |
| `SNMSBuilderUI.Curve.cpp` | `UpdateCurvePreview`, `TessellateCurve`, `CurvePartCount`, `BuildAlongCurve` | ~215 |
| `SNMSBuilderUI.BaseIO.cpp` | меню-действия + сцена: `OnMenuNewBase/Open/ImportModel/ImportFromNMS/SaveBaseAs/ImportClipboard/ExportClipboard/ExportToNMS`, `ClearSceneParts`, `SpawnFromManager`, `CollectSceneToManager`, undo/redo (`Snapshot/Restore/CommitEdit/DoUndo/DoRedo`) | ~485 |
| `SNMSBuilderUI.SaveManager.cpp` | все `SM*` (11 методов) | ~353 |
| `SNMSBuilderUI.Toolbar.cpp` | `BuildToolbar`, `BuildRightPanel`, `BuildColorPalette` | ~290 |
| `SNMSBuilderUI.Palette.cpp` | `LoadPalettes`, `RebuildSwatches`, `SetMaterialIndex`, `OnColorClicked` | ~245 |
| `SNMSBuilderUI.RightPanel.cpp` | `GetSelected`, `GetTransformValue/SetTransformValue`, `MakeLiveTransformRow`, `GetSelectedInfo`, `ToggleActorHidden/RenderOff`, `RebuildSceneTree`, `SceneTree_OnGenerateRow`, `SceneTree_OnSelectionChanged`, `BuildSceneTree` | ~202 |

После Части C ни один файл не превышает ~620 строк (было 3684).

---

## Порядок выполнения (каждый шаг = отдельная сборка для проверки)

1. **Шаг 0** — бэкап `SNMSBuilderUI.cpp` (в проекте уже принята практика `.bak`).
2. **Шаг 1** — создать `NMSStyle.h/.cpp`, вырезать `namespace NMS`, добавить `#include "NMSStyle.h"` в исходный файл. → собрать.
3. **Шаг 2** — `NMSLocalization.h/.cpp`. → собрать.
4. **Шаг 3** — `NMSPartVisuals.h/.cpp` (+ убрать forward-декл, перенести тело с 2526). → собрать.
5. **Шаг 4** — `NMSUIHelpers.h/.cpp`. → собрать.
   *(после шагов 1–4 исходный файл ≈ 3160 строк, поведение идентично)*
6. **Шаг 5** — создать `SNMSBuilderUI_Internal.h`, перевести исходный файл на него.
7. **Шаги 6–13** — по одному выносить TU из таблицы Части C, **каждый со своим** include-блоком (`#include "SNMSBuilderUI_Internal.h"` + `LOCTEXT_NAMESPACE`). Собирать после каждого.
8. **Шаг 14** — финальная сборка + прогон приложения (вкладка открывается, список деталей, покраска, Save Manager, дерево сцены работают).

---

## Сборка проекта (Build.cs / project files)
- **`NMS_BuilderApp.Build.cs` менять не нужно** — модуль тот же, UBT автоматически подхватывает все `.cpp` в `Source/`.
- После добавления файлов — **перегенерировать VS-проект** (ПКМ по `.uproject` → *Generate Visual Studio project files*), иначе новые файлы не появятся в `.sln`.

---

## Риски и подводные камни (учтены в плане)
- **`LOCTEXT_NAMESPACE`** — `#define` в начале и `#undef` в конце **каждого** нового `.cpp`.
- **File-static глобалы i18n** (`GNms*`) — переезжают целиком в `NMSLocalization.cpp`, наружу не светятся → нет дублирования состояния.
- **Статические кэши в функциях** (`NMS_GamePartColors`, `NMS_PartTexture`, `FlatButton`) — должны жить в **одном** TU (поэтому их тела в `.cpp`, не в заголовке), иначе кэш задвоится.
- **`inline const` константы** темы в `NMSStyle.h` — безопасны для включения в много TU (C++17).
- **Порядок переноса** — сначала stateless (A), потом разнесение (C): на каждом шаге можно собраться и убедиться, что ничего не сломалось.

---

## Связь с общей стратегией
Это **Этапы 1–2**: вынос stateless-кода + разнесение методов по TU.
Состояние и поведение не трогаются.

**Этап 3 (отдельным планом, позже):** превращение самых изолированных панелей
в настоящие под-виджеты с делегатами наружу:
- `SNMSSaveManager` — владеет `SM*`, отдаёт `OnLoadBase(BaseIndex)`.
- `SNMSColorPalette` — владеет `Palette*`, отдаёт `OnColorPicked / OnMaterialChanged`.

Это сократит и заголовок (сейчас 262 строки), и развяжет состояние.
Остальные панели (дерево сцены, transform) сильно завязаны на `ViewportClient` —
их выносить дороже, оставляем в составе класса.
