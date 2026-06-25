# Code Review: autoeme/NMS_BuilderApp

Дата ревью: 2026-06-25  
Локальная копия: `/Users/dvaletin/Documents/Codex/2026-06-25/cl/work/NMS_BuilderApp`

## 1. Полнота проекта и достаточность для запуска

Проект пока нельзя считать достаточным для запуска с чистого `git clone` без дополнительных действий и документации.

### P1: Нет README/INSTALL/CI

В корне проекта не найдено `README`, `INSTALL`, `LICENSE`, `.github/workflows` или другой инструкции запуска. Единственный `.md` найден в папке `план/`.

Минимально стоит добавить:

- версию Unreal Engine;
- необходимость Git LFS;
- требования к Xcode/macOS/Windows;
- список нужных Unreal plugins;
- команды/шаги открытия проекта;
- known issues для Mac и Windows;
- минимальный smoke-test после открытия.

### P1: В `Source` лежат backup/broken-файлы

Найдены файлы:

```text
Source/NMS_BuilderApp/NMSBaseManager.cpp.before_rot180
Source/NMS_BuilderApp/NMSBaseManager.cpp.rot180hack
Source/NMS_BuilderApp/NMSViewportClient.cpp.broken_004218
Source/NMS_BuilderApp/NMSViewportClient.h.broken_004218
Source/NMS_BuilderApp/NMS_BuilderApp.Build.cs.before_nms
Source/NMS_BuilderApp/NMS_BuilderApp.cpp.before_tab
Source/NMS_BuilderApp/NMS_BuilderApp.h.before_tab
Source/NMS_BuilderApp/SNMSBuilderUI.cpp.before_tabs
Source/NMS_BuilderApp/SNMSBuilderUI.cpp.broken_004218
Source/NMS_BuilderApp/SNMSBuilderUI.h.before_tabs
Source/NMS_BuilderApp/SNMSBuilderUI.h.broken_004218
```

Их лучше удалить из исходников или перенести в историю Git/отдельную архивную папку вне `Source`.

## 2. Приоритизированные предложения по Clean Architecture

### P0: Добавить тесты для рискованного core-кода до крупных рефакторингов

Сейчас нет автоматических тестов для самых опасных частей проекта: парсинг/запись save-файлов, `.blend` parser, JSON roundtrip и преобразования координат. Без тестов архитектурная чистка будет рискованной: легко сломать импорт/экспорт, визуально заметив это слишком поздно.

Наиболее нуждаются в тестах:

- `.hg` load/save: `/Users/dvaletin/Documents/Codex/2026-06-25/cl/work/NMS_BuilderApp/Source/NMS_BuilderApp/NMSSaveFile.cpp:118`
- `.blend` parser: `/Users/dvaletin/Documents/Codex/2026-06-25/cl/work/NMS_BuilderApp/Source/NMS_BuilderApp/NMSBlendReader.cpp:156`
- JSON import/export и transform conversion: `/Users/dvaletin/Documents/Codex/2026-06-25/cl/work/NMS_BuilderApp/Source/NMS_BuilderApp/NMSBaseManager.cpp:156`

Минимальный набор:

- parse fixture `.json` -> `BaseDocument`;
- export -> import roundtrip;
- transform `Up/At/Position` roundtrip;
- `.hg` load fixture;
- `.blend` fixture against expected part count.

### P0: Разделить Runtime и Editor/UI-модуль

Сейчас `.uproject` объявляет модуль как `Runtime`:

```json
"Name": "NMS_BuilderApp",
"Type": "Runtime"
```

Файл: `/Users/dvaletin/Documents/Codex/2026-06-25/cl/work/NMS_BuilderApp/NMS_BuilderApp.uproject:8`

Но код фактически ведёт себя как editor tool:

- `StartupModule()` регистрирует и автоматически открывает Slate-вкладку.
- `Build.cs` подключает `Slate`, `DesktopPlatform`, условно `UnrealEd`.
- `SNMSBuilderUI.cpp` включает `Editor.h`.

Файлы:

- `/Users/dvaletin/Documents/Codex/2026-06-25/cl/work/NMS_BuilderApp/Source/NMS_BuilderApp/NMS_BuilderApp.cpp:13`
- `/Users/dvaletin/Documents/Codex/2026-06-25/cl/work/NMS_BuilderApp/Source/NMS_BuilderApp/NMS_BuilderApp.Build.cs:16`
- `/Users/dvaletin/Documents/Codex/2026-06-25/cl/work/NMS_BuilderApp/Source/NMS_BuilderApp/SNMSBuilderUI.cpp:22`

Рекомендация:

- `NMS_BuilderCore` или `NMS_BuilderDomain`: чистая модель базы, парсинг, use cases.
- `NMS_BuilderEditor`: Slate, tabs, viewport, file dialogs, actor spawning.
- `NMS_BuilderRuntime` создавать только если действительно нужен standalone/runtime.

### P0: Разобрать god object `SNMSBuilderUI`

`SNMSBuilderUI.cpp` содержит 3684 строки, `SNMSBuilderUI.h` содержит состояние и методы для:

- загрузки деталей;
- меню;
- файловых операций;
- clipboard;
- Undo/Redo;
- base properties;
- part variants;
- favorites/presets;
- viewport;
- palette/coloring;
- save manager;
- scene tree;
- transform panel.

Файл: `/Users/dvaletin/Documents/Codex/2026-06-25/cl/work/NMS_BuilderApp/Source/NMS_BuilderApp/SNMSBuilderUI.h:52`

Это нарушает Single Responsibility Principle и делает UI центром бизнес-логики.

Рекомендация:

- `PartsCatalogService`
- `PaletteRepository`
- `BaseDocumentService`
- `SaveGameImportService`
- `SceneAdapter`
- `UndoHistory`
- `BuilderPresenter`/`ViewModel`

UI должен только отображать состояние и отправлять команды.

### P0: Ввести чистую доменную модель вместо зависимости от `AActor`

Сейчас экспорт сцены собирается напрямую из Unreal actors:

```cpp
for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
```

Файл: `/Users/dvaletin/Documents/Codex/2026-06-25/cl/work/NMS_BuilderApp/Source/NMS_BuilderApp/SNMSBuilderUI.cpp:1495`

ObjectID и UserData хранятся через `ActorLabel` и `Tags`:

```cpp
FString Label = A->GetActorLabel();
P.ObjectID = (A->Tags.Num() > 0) ? A->Tags[0].ToString() : Label;
```

Файл: `/Users/dvaletin/Documents/Codex/2026-06-25/cl/work/NMS_BuilderApp/Source/NMS_BuilderApp/SNMSBuilderUI.cpp:1510`

Это инвертирует зависимости: доменная база зависит от UE-сцены и editor labels.

Рекомендация:

```text
BaseDocument
  PlacedPart[]
    PartId
    Transform
    UserData
    Timestamp
```

Unreal actors должны быть только view/adapters для отображения `BaseDocument`.

### P1: Инкапсулировать `FNMSViewportClient`

`FNMSViewportClient` раскрывает большую часть состояния публичными полями:

- camera;
- grid;
- transform mode;
- curve settings;
- callbacks;
- pending textures/colors;
- wiring mode.

Файл: `/Users/dvaletin/Documents/Codex/2026-06-25/cl/work/NMS_BuilderApp/Source/NMS_BuilderApp/NMSViewportClient.h:38`

Рекомендация:

- заменить публичные поля на команды: `SetTransformMode`, `StartPlacement`, `SetCurveSettings`;
- события оформить через интерфейс или event dispatcher;
- viewport не должен знать о бизнес-сценариях вроде Undo/Redo, только сообщать о пользовательском действии.

### P1: Убрать платформенную логику из домена

`FNMSSaveFile::GetRootSaveFolder()` жёстко привязан к Windows `%APPDATA%`:

```cpp
FString AppData = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
return FPaths::Combine(AppData, TEXT("HelloGames/NMS"));
```

Файл: `/Users/dvaletin/Documents/Codex/2026-06-25/cl/work/NMS_BuilderApp/Source/NMS_BuilderApp/NMSSaveFile.cpp:48`

На Mac это не найдёт save folder.

Рекомендация:

- `ISaveLocationProvider`;
- реализации `WindowsSaveLocationProvider`, `MacSaveLocationProvider`, `ManualSaveLocationProvider`;
- UI выбирает provider, core работает только с переданным path.

### P1: Перепроектировать Undo/Redo

Undo/Redo сейчас делается сериализацией всей сцены в JSON строку:

```cpp
FString S = SnapshotScene();
UndoStack.Add(EditSnapshot);
```

Файл: `/Users/dvaletin/Documents/Codex/2026-06-25/cl/work/NMS_BuilderApp/Source/NMS_BuilderApp/SNMSBuilderUI.cpp:1449`

Риски:

- дорого на больших базах;
- Undo зависит от формата экспорта;
- любое изменение сериализации влияет на историю;
- сцена и документ смешаны.

Рекомендация:

- short-term: snapshot чистого `BaseDocument`, не actors/JSON;
- long-term: command history (`PlacePartCommand`, `MovePartCommand`, `DeletePartCommand`, `PaintPartCommand`).

### P2: Вынести глобальные кэши локализации/палитр из UI

В `SNMSBuilderUI.cpp` есть статические глобальные кэши:

```cpp
static bool GNmsI18nLoaded = false;
static FString GNmsLang;
static TArray<TPair<FString,FString>> GNmsLangs;
static TSharedPtr<FJsonObject> GNmsUi;
```

Файл: `/Users/dvaletin/Documents/Codex/2026-06-25/cl/work/NMS_BuilderApp/Source/NMS_BuilderApp/SNMSBuilderUI.cpp:66`

Рекомендация:

- `LocalizationService`;
- `PaletteService`;
- явные ошибки загрузки;
- возможность тестировать без Slate.

## 3. Рекомендуемый порядок работ

1. Добавить `README.md` с запуском, Git LFS, UE 5.8, plugins и smoke-test.
2. Добавить тесты на парсеры и roundtrip для текущего поведения.
3. Удалить backup/broken-файлы из `Source`.
4. Разделить модуль на Core и Editor.
5. Ввести `BaseDocument` как центр модели.
6. Перенести actor-spawning в `SceneAdapter`.
7. Разобрать `SNMSBuilderUI` на use cases/services.

## 4. Что не было проверено

Сборка и запуск в Unreal Engine не выполнялись, потому что на текущей машине не найден UE 5.8, а Git LFS-ассеты не были подтянуты до конца.
