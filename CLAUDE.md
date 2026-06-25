# CLAUDE.md

This file defines working instructions for AI coding agents contributing to `NMS_BuilderApp`.

## Required Reference Documents

Before making architectural, Git workflow, refactoring, or testing decisions, read and follow these project standards:

- Code review findings: `docs/NMS_BuilderApp_code_review.md`
- Architecture guidelines: `docs/NMS_BuilderApp_architecture_guidelines.md`
- Git workflow standard: `docs/NMS_BuilderApp_git_workflow_standard.md`

These documents are normative for future work unless a human maintainer explicitly overrides them.

## Project Context

`NMS_BuilderApp` is an Unreal Engine 5.8 C++ project for editing No Man's Sky base data.

Current architectural direction:

- separate core/domain logic from Unreal Editor/UI code;
- make `FNMSBaseDocument` or equivalent document model the source of truth;
- keep Unreal actors as scene projections/adapters, not domain storage;
- move editor-only behavior into an Editor module;
- add tests for parser/save/transform behavior before major refactoring.

## Mandatory Development Rules

1. Do not add new business logic to god objects such as `SNMSBuilderUI`.
2. Do not make Domain/Core depend on Slate, `UnrealEd`, `AActor`, `UWorld`, file dialogs, clipboard, or platform paths.
3. Do not use `ActorLabel` or `AActor::Tags` as canonical storage for domain state.
4. Do not introduce hardcoded user paths such as `C:/Users/...` or `%APPDATA%` outside platform-specific adapters.
5. Do not commit backup, broken, temporary, or generated source files.
6. Use Git LFS for Unreal assets and verify LFS state before merge.
7. Add or preserve tests for `.hg`, `.blend`, JSON roundtrip, and transform conversion when touching related code.
8. Prefer small, reviewable changes over broad mixed refactors.

## Architecture Rules

Use the following dependency direction:

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

## Expected Module Direction

Target structure:

```text
Source/
  NMS_BuilderCore/
  NMS_BuilderEditor/
```

Optional only if a real standalone runtime is needed:

```text
Source/
  NMS_BuilderRuntime/
```

## Testing Expectations

Treat tests as `P0` for risky core behavior.

Required coverage areas:

- `.hg` save load/save fixtures;
- `.blend` parser fixture;
- JSON import/export roundtrip;
- `Up/At/Position` transform roundtrip;
- parts catalog load validation;
- scene adapter mapping from document model to actor descriptors.

If Unreal automation tests are not yet available, start with focused C++ automation tests for pure/core code after extracting it from editor dependencies.

## Git Rules

Follow the Git workflow standard linked above.

Commit message format:

```text
<type>(<scope>): <summary>
```

Examples:

```text
test(save): add hg roundtrip fixture
refactor(ui): split part catalog panel
docs: add architecture guidelines
```

Before proposing merge:

```bash
git status --short
git lfs status
git diff --stat
```

## File Hygiene

Do not create or keep files matching:

```text
*.before_*
*.broken_*
*.rot180hack
*.bak
*.bak_*
*_backup.*
```

Do not mix unrelated formatting, asset reimport, code refactor, and behavior changes in the same PR.

## Naming Conventions

Use Unreal-style prefixes:

- `F` for regular structs/classes;
- `U` for `UObject`;
- `A` for actors;
- `S` for Slate widgets;
- `I` for interfaces;
- `E` for enums.

Use the `NMS` project prefix consistently:

```text
FNMSBaseDocument
FNMSPlacedPart
INMSSaveLocationProvider
SNMSPartCatalogPanel
FNMSSceneAdapter
```

Avoid vague names such as `Manager`, `Helper`, `Utils`, or `Data` unless the name is made specific by context.

## Logging

Avoid new `LogTemp` usage in project logic.

Prefer project log categories:

```text
LogNMSBuilderCore
LogNMSBuilderEditor
LogNMSBuilderSave
LogNMSBuilderImport
LogNMSBuilderScene
```

## Definition of Done

A change is not done until:

- it follows the architecture guidelines;
- tests are added or preserved for affected core behavior;
- Git LFS state is clean;
- no backup/temp/source junk is included;
- setup or workflow changes are documented;
- the PR or change summary clearly states verification performed.
