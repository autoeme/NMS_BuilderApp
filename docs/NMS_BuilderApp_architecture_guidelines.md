# Architecture Guidelines: NMS Builder App

Дата: 2026-06-25  
Проект: `autoeme/NMS_BuilderApp`  
Цель документа: задать стандарт архитектуры, расположения файлов, подходов к разработке и naming conventions для дальнейшего развития проекта.

## 1. Architectural Principles

Проект должен развиваться по принципам Clean Architecture:

- бизнес-правила и модель базы не зависят от Unreal Engine UI, Slate, Editor API, файловых диалогов или конкретных форматов хранения;
- зависимости направлены внутрь: UI и инфраструктура зависят от application/core, но core не зависит от UI и инфраструктуры;
- внешний мир подключается через интерфейсы и адаптеры;
- каждый слой имеет одну ответственность и может тестироваться отдельно;
- любые рискованные изменения начинаются с тестов на текущее поведение.

Основное правило: `BaseDocument` и операции над базой должны быть понятны и тестируемы без запуска Unreal Editor.

## 2. Architecture Layers

### 2.1 Domain Layer

Domain содержит чистую модель предметной области и базовые правила.

Разрешено:

- value objects;
- domain entities;
- чистые функции преобразования;
- доменные ошибки;
- интерфейсы, если они описывают потребности домена.

Запрещено:

- Slate;
- `AActor`, `UWorld`, `UStaticMesh`, `FPreviewScene`;
- файловые диалоги;
- чтение из `FPaths::ProjectContentDir()`;
- прямые зависимости от `%APPDATA%`, macOS paths, clipboard, editor labels.

Примеры сущностей:

- `FBaseDocument`
- `FPlacedPart`
- `FPartId`
- `FPartTransform`
- `FPartUserData`
- `FBaseMetadata`

### 2.2 Application Layer

Application содержит use cases проекта. Это слой сценариев, который координирует домен, репозитории и адаптеры.

Примеры use cases:

- `FOpenBaseUseCase`
- `FSaveBaseUseCase`
- `FImportBlendUseCase`
- `FImportNmsSaveUseCase`
- `FExportClipboardUseCase`
- `FBuildAlongCurveUseCase`
- `FUndoHistoryService`

Application layer не должен знать о Slate widget layout или конкретных Unreal actors. Он может работать с интерфейсами:

- `IPartsCatalogRepository`
- `IBaseDocumentRepository`
- `ISaveGameRepository`
- `IFilePicker`
- `IClipboardService`
- `ISceneGateway`

### 2.3 Infrastructure Layer

Infrastructure реализует работу с внешним миром:

- JSON;
- `.hg` save files;
- `.blend` reader;
- Git/LFS-independent assets lookup;
- platform-specific save locations;
- filesystem;
- clipboard;
- file dialogs.

Infrastructure зависит от Domain/Application, но Domain/Application не зависят от Infrastructure.

Примеры:

- `FJsonBaseDocumentRepository`
- `FNmssaveFileRepository`
- `FBlendBaseImporter`
- `FWindowsSaveLocationProvider`
- `FMacSaveLocationProvider`
- `FDesktopFilePicker`
- `FPlatformClipboardService`

### 2.4 Presentation Layer

Presentation отвечает за UI state, Slate widgets, view models и user interactions.

Примеры:

- `SNMSBuilderMainWidget`
- `SNMSPartCatalogPanel`
- `SNMSSaveManagerPanel`
- `SNMSRightInspectorPanel`
- `FNMSBuilderViewModel`

Presentation вызывает use cases, но не выполняет парсинг save-файлов, не сериализует базу и не хранит бизнес-правила.

### 2.5 Unreal Scene Adapter Layer

Этот слой связывает чистую модель базы с Unreal-сценой.

Он отвечает за:

- создание actors из `FBaseDocument`;
- синхронизацию actor transform обратно в документ;
- загрузку meshes/materials/textures;
- viewport selection;
- scene tree;
- preview/ghost actors.

Примеры:

- `FNMSSceneAdapter`
- `FNMSActorFactory`
- `FNMSMaterialFactory`
- `FNMSViewportClient`

Важно: `AActor::Tags` и `ActorLabel` не должны быть primary storage для domain data. Они могут использоваться только как технический механизм отображения. Источник истины — `FBaseDocument`.

### 2.6 Editor Module Layer

Editor module содержит всё, что существует только в Unreal Editor:

- tab registration;
- `FGlobalTabmanager`;
- `UnrealEd`;
- editor-only Slate tools;
- editor commands.

Runtime/Core модуль не должен зависеть от Editor module.

## 3. Recommended Module Structure

Рекомендуемая структура модулей:

```text
Source/
  NMS_BuilderCore/
    NMS_BuilderCore.Build.cs
    Public/
    Private/

  NMS_BuilderRuntime/
    NMS_BuilderRuntime.Build.cs
    Public/
    Private/

  NMS_BuilderEditor/
    NMS_BuilderEditor.Build.cs
    Public/
    Private/
```

Если standalone runtime не нужен, `NMS_BuilderRuntime` можно не создавать. Минимальный целевой вариант:

```text
Source/
  NMS_BuilderCore/
  NMS_BuilderEditor/
```

## 4. File System Layout

### 4.1 Source Layout

Рекомендуемая структура:

```text
Source/
  NMS_BuilderCore/
    Public/
      Domain/
        BaseDocument.h
        PlacedPart.h
        PartId.h
        BaseMetadata.h
        TransformConversion.h

      Application/
        OpenBaseUseCase.h
        SaveBaseUseCase.h
        ImportBlendUseCase.h
        ImportNmsSaveUseCase.h
        UndoHistoryService.h

      Ports/
        BaseDocumentRepository.h
        PartsCatalogRepository.h
        SaveLocationProvider.h
        ClipboardService.h

    Private/
      Domain/
        TransformConversion.cpp

      Application/
        OpenBaseUseCase.cpp
        SaveBaseUseCase.cpp
        ImportBlendUseCase.cpp
        ImportNmsSaveUseCase.cpp
        UndoHistoryService.cpp

      Infrastructure/
        Json/
          JsonBaseDocumentRepository.cpp
        NmsSave/
          NmssaveFile.cpp
          NmssaveFileRepository.cpp
        Blend/
          BlendReader.cpp
          BlendBaseImporter.cpp
        PartsCatalog/
          JsonPartsCatalogRepository.cpp

  NMS_BuilderEditor/
    Public/
      NMS_BuilderEditorModule.h

    Private/
      Module/
        NMS_BuilderEditorModule.cpp

      UI/
        Main/
          SNMSBuilderMainWidget.h
          SNMSBuilderMainWidget.cpp
        Parts/
          SNMSPartCatalogPanel.h
          SNMSPartCatalogPanel.cpp
        SaveManager/
          SNMSSaveManagerPanel.h
          SNMSSaveManagerPanel.cpp
        Inspector/
          SNMSInspectorPanel.h
          SNMSInspectorPanel.cpp

      ViewModels/
        NMSBuilderViewModel.h
        NMSBuilderViewModel.cpp

      Scene/
        NMSSceneAdapter.h
        NMSSceneAdapter.cpp
        NMSActorFactory.h
        NMSActorFactory.cpp
        NMSMaterialFactory.h
        NMSMaterialFactory.cpp
        NMSViewportClient.h
        NMSViewportClient.cpp

      Infrastructure/
        DesktopFilePicker.h
        DesktopFilePicker.cpp
        PlatformClipboardService.h
        PlatformClipboardService.cpp
```

### 4.2 Content Layout

Recommended `Content` layout:

```text
Content/
  NMSBaseBuilder/
    Materials/
    Meshes/
    Textures/
    UI/

  NMSData/
    Catalog/
      nms_parts_db.json
      part_variants.json
      part_sections.json
      part_slots.json

    Palettes/
      palettes.json
      finishes.json
      colour_names.json
      userdata_palette.json

    Localization/
      languages.json
      ui_i18n.json
      partnames_i18n.json
      colour_names_i18n.json

    Save/
      save_map_dictionary.json

    Thumbnails/
    GameIcons/
```

Avoid mixing generated, backup, source, runtime and user files in the same folder.

### 4.3 User Data Layout

User-generated files should go under `Saved/NMSUser`, not `Content`.

```text
Saved/
  NMSUser/
    favorites.json
    user_categories.json
    base_screenshots.json
    lang.txt
```

### 4.4 Generated and Import Scripts

Python import/reimport scripts should be grouped by purpose:

```text
Tools/
  Python/
    Import/
    Reimport/
    Materials/
    Thumbnails/
    Validation/
```

Scripts must not be required for normal app startup unless documented in `README.md`.

### 4.5 Backup Files

Backup files must not be committed into `Source`.

Do not keep files like:

```text
*.before_*
*.broken_*
*.rot180hack
*.bak
```

Use Git history, branches, tags or `docs/archive/` for historical notes.

## 5. Architectural Approaches

### 5.1 Test-First for Risky Core Behavior

Before refactoring parsers, save logic or transform conversion, add tests that lock current behavior.

Minimum required test groups:

- `.hg` load/save fixtures;
- `.blend` parser fixture;
- JSON import/export roundtrip;
- `Up/At/Position` transform roundtrip;
- parts catalog load validation;
- scene adapter mapping from `BaseDocument` to actor descriptors.

### 5.2 Dependency Rule

Allowed dependency direction:

```text
Editor/UI -> Application -> Domain
Infrastructure -> Application/Domain
SceneAdapter -> Application/Domain
```

Forbidden dependency direction:

```text
Domain -> UI
Domain -> Unreal Editor
Domain -> Filesystem
Domain -> AActor/UWorld
Application -> Slate widget layout
Application -> DesktopPlatform file dialog
```

### 5.3 Ports and Adapters

When application logic needs external behavior, define a port in Core and implement it outside.

Example:

```text
Core/Public/Ports/SaveLocationProvider.h
Editor/Private/Infrastructure/MacSaveLocationProvider.cpp
Editor/Private/Infrastructure/WindowsSaveLocationProvider.cpp
```

### 5.4 Source of Truth

The source of truth for the edited base is `FBaseDocument`.

Unreal actors are views/projections of that document. They may cache IDs for lookup, but they must not be the canonical storage of part identity, userdata or base metadata.

### 5.5 Error Handling

Core operations should return explicit results, not only log to `LogTemp`.

Preferred pattern:

```text
TExpected<T, FNMSDomainError>
```

or a project-specific result type:

```text
TNMSResult<T>
FNMSOperationError
```

Rules:

- Domain/Application returns structured errors.
- UI decides how to display errors.
- Infrastructure adds context such as file path and parser stage.
- Logs are supplemental, not the error API.

### 5.6 Logging

Avoid `LogTemp` for project logic.

Define project log categories:

```text
LogNMSBuilderCore
LogNMSBuilderEditor
LogNMSBuilderSave
LogNMSBuilderImport
LogNMSBuilderScene
```

### 5.7 Configuration

Hardcoded paths such as `C:/Users/User/Documents` or `%APPDATA%` must not live in UI or domain logic.

Use:

- platform providers;
- user settings;
- `Saved/Config`;
- explicit file picker defaults.

### 5.8 UI Composition

Slate widgets should be small and focused.

Recommended maximum:

- widget header: under 150 lines;
- widget implementation: under 500 lines;
- if a widget crosses this, split panels, view model, services or child widgets.

### 5.9 View Models

Slate widgets should bind to view models rather than store all business state.

Example:

```text
FNMSBuilderViewModel
  CurrentBaseName
  SelectedPart
  VisibleParts
  AvailableCategories
  Commands
```

View models call use cases and expose UI-ready state.

### 5.10 Asset Loading

Asset paths must be centralized.

Do not scatter `LoadObject` calls with raw string paths across UI code. Use:

- `FNMSAssetRegistry`
- `FNMSPartAssetResolver`
- `FNMSMaterialFactory`

### 5.11 Platform Support

Platform-specific behavior must be isolated:

```text
Windows:
  APPDATA/HelloGames/NMS

macOS:
  documented provider path or manual folder picker

Manual:
  user-selected save root
```

Core code should receive paths as input.

## 6. Naming Conventions

### 6.1 General C++ Naming

Follow Unreal Engine naming conventions:

- `U` prefix for `UObject` classes: `UNMSBaseManager`
- `A` prefix for actors: `ANMSPartActor`
- `S` prefix for Slate widgets: `SNMSPartCatalogPanel`
- `F` prefix for structs and regular classes: `FBaseDocument`
- `I` prefix for interfaces: `ISaveLocationProvider`
- `E` prefix for enums: `ENMSTransformMode`
- `T` prefix only for templates, following Unreal style.

### 6.2 Project Prefix

Use `NMS` consistently for project-specific types.

Preferred:

```text
FNMSBaseDocument
FNMSPlacedPart
FNMSPartCatalog
INMSSaveLocationProvider
SNMSPartCatalogPanel
```

Avoid generic names:

```text
FManager
FUtils
FData
FHelper
```

### 6.3 Layer-Specific Names

Domain:

```text
FNMSBaseDocument
FNMSPlacedPart
FNMSPartId
FNMSBaseMetadata
FNMSUserData
```

Application:

```text
FNMSOpenBaseUseCase
FNMSSaveBaseUseCase
FNMSImportBlendUseCase
FNMSUndoHistoryService
```

Ports:

```text
INMSBaseDocumentRepository
INMSPartsCatalogRepository
INMSSaveLocationProvider
INMSClipboardService
```

Infrastructure:

```text
FNMSJsonBaseDocumentRepository
FNMSSaveGameRepository
FNMSBlendReader
FNMSDesktopFilePicker
```

Presentation:

```text
SNMSBuilderMainWidget
SNMSPartCatalogPanel
SNMSSaveManagerPanel
FNMSBuilderViewModel
```

Scene:

```text
FNMSSceneAdapter
FNMSActorFactory
FNMSMaterialFactory
FNMSViewportClient
```

### 6.4 File Naming

One primary type per file.

Preferred:

```text
NMSBaseDocument.h
NMSBaseDocument.cpp
NMSOpenBaseUseCase.h
NMSOpenBaseUseCase.cpp
SNMSPartCatalogPanel.h
SNMSPartCatalogPanel.cpp
```

Avoid files named:

```text
Utils.cpp
Helpers.cpp
Manager.cpp
Data.cpp
Everything.cpp
```

### 6.5 Function Naming

Use verbs for commands:

```text
OpenBase()
SaveBase()
ImportFromBlend()
ExportToJson()
ApplyTransform()
BuildAlongCurve()
```

Use `Try` for operations that may fail without throwing:

```text
TryLoadBase()
TryParseSaveFile()
TryResolvePartAsset()
```

Use `Get` for cheap accessors and `Load` for I/O:

```text
GetSelectedPart()
LoadPartsCatalog()
```

### 6.6 Boolean Naming

Boolean variables should read naturally:

```text
bIsDirty
bHasSelection
bCanUndo
bShowGrid
bUseUnlitMaterials
```

Avoid ambiguous names:

```text
bState
bMode
bFlag
```

### 6.7 Enum Naming

Enums use `E` prefix and scoped enum classes:

```cpp
enum class ENMSTransformMode : uint8
{
    Move,
    Rotate,
    Scale
};
```

### 6.8 Constants

Constants should be named and centralized near the layer that owns them.

```cpp
namespace NMS::Transform
{
    constexpr float NmsToUnrealScale = 100.0f;
}
```

Avoid anonymous magic numbers in UI or scene code.

### 6.9 Logs

Use project log categories and concise messages.

Preferred:

```cpp
UE_LOG(LogNMSBuilderImport, Warning, TEXT("Failed to import blend file: %s"), *Path);
```

Avoid:

```cpp
UE_LOG(LogTemp, Warning, TEXT("fail"));
```

## 7. Review Checklist

Before merging a change, verify:

- Does core logic compile without Slate/Editor dependencies?
- Are new parser/save/transform behaviors covered by tests?
- Is `FBaseDocument` the source of truth?
- Are platform-specific paths isolated?
- Are file dialogs and clipboard behind ports?
- Did the change avoid adding more logic to a god widget?
- Are project log categories used instead of `LogTemp`?
- Are backup/generated files excluded from `Source`?
- Does README mention any new setup requirement?

## 8. Migration Strategy

Recommended incremental order:

1. Add test fixtures for current save/import/export behavior.
2. Create `NMS_BuilderCore` module.
3. Move pure structs and transform conversion into Core.
4. Introduce `FNMSBaseDocument`.
5. Make JSON import/export work against `FNMSBaseDocument`.
6. Add scene adapter that maps `FNMSBaseDocument` to actors.
7. Split `SNMSBuilderUI` into focused panels.
8. Move editor-only code into `NMS_BuilderEditor`.
9. Replace raw paths and `LogTemp` usage.
10. Add CI/smoke checks once Unreal build environment is available.
