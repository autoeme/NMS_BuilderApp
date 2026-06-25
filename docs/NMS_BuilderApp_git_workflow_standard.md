# Git Workflow Standard: NMS Builder App

Дата: 2026-06-25  
Проект: `autoeme/NMS_BuilderApp`  
Цель документа: описать стандарт работы с Git для команды проекта, включая ветвление, коммиты, pull requests, Git LFS, code review и правила безопасности.

## 1. Core Principles

Работа с Git должна быть предсказуемой, проверяемой и безопасной для больших Unreal Engine проектов.

Основные принципы:

- `main` всегда должен быть в рабочем состоянии;
- изменения делаются через feature branches;
- каждый pull request должен быть маленьким, понятным и проверяемым;
- бинарные ассеты хранятся через Git LFS;
- backup/broken/temp-файлы не коммитятся;
- перед крупным рефакторингом фиксируются тесты на текущее поведение;
- история должна объяснять зачем было сделано изменение, а не только что поменялось.

## 2. Repository Setup

После clone каждый разработчик обязан выполнить:

```bash
git lfs install
git lfs pull
```

Проверить, что ассеты не остались LFS pointer-файлами:

```bash
head -n 1 Content/NMSBaseBuilder/Features/Models_FBX/ABAND_BARREL.uasset
```

Если вывод начинается с:

```text
version https://git-lfs.github.com/spec/v1
```

значит LFS-ассеты не подтянуты, проект в Unreal Engine открывать рано.

## 3. Branching Model

### 3.1 Protected Branches

Основные ветки:

```text
main
develop
```

`main`:

- только стабильные версии;
- direct push запрещён;
- merge только через pull request;
- каждый commit должен собираться.

`develop`:

- интеграционная ветка;
- direct push нежелателен;
- допускает активную разработку, но не должен быть сломан надолго.

Если команда маленькая, можно использовать только `main` и feature branches. В этом случае требования к PR становятся строже.

### 3.2 Working Branches

Формат веток:

```text
feature/<short-description>
fix/<short-description>
refactor/<short-description>
test/<short-description>
docs/<short-description>
chore/<short-description>
```

Примеры:

```text
feature/base-document-model
fix/mac-save-location-provider
refactor/split-builder-ui
test/nms-save-roundtrip
docs/git-workflow-standard
chore/remove-source-backups
```

Правила:

- одна ветка — одна цель;
- не смешивать refactor, feature и formatting без необходимости;
- ветка должна регулярно обновляться от целевой ветки;
- долгоживущие ветки нужно избегать.

## 4. Commit Standard

### 4.1 Commit Size

Коммит должен быть атомарным:

- одна логическая правка;
- можно откатить без отката несвязанных изменений;
- понятно проверить в code review.

Плохой коммит:

```text
fix stuff
```

Хороший коммит:

```text
test: add JSON base roundtrip fixtures
```

### 4.2 Commit Message Format

Использовать Conventional Commits:

```text
<type>(<scope>): <summary>
```

Типы:

```text
feat      новая функциональность
fix       исправление бага
refactor  изменение структуры без изменения поведения
test      тесты
docs      документация
chore     обслуживание проекта
build     сборка, dependencies, Build.cs
ci        CI/CD
perf      производительность
style     форматирование без изменения логики
```

Примеры:

```text
feat(core): introduce base document model
fix(save): resolve macOS save location lookup
refactor(ui): split part catalog panel from main widget
test(blend): add expected part count fixture
docs: add architecture guidelines
chore(source): remove broken backup files
```

### 4.3 Commit Body

Добавлять body, если изменение неочевидно:

```text
refactor(scene): move actor spawning into scene adapter

The base document is now the source of truth. Actors are recreated
from document state instead of storing domain identity in actor labels.
```

## 5. Pull Request Standard

Каждый PR должен содержать:

- цель изменения;
- краткий список изменений;
- как проверялось;
- риски;
- screenshots/video, если менялся UI;
- migration notes, если менялись данные или структура модулей.

Шаблон PR:

```markdown
## Summary

## Changes

## Verification

## Risks

## Screenshots / Video

## Migration Notes
```

## 6. Review Rules

Code review должен проверять:

- не нарушено ли направление зависимостей;
- не добавлена ли новая логика в god object;
- есть ли тесты для parser/save/transform изменений;
- нет ли `LogTemp` в новой проектной логике;
- не появились ли hardcoded paths;
- не закоммичены ли backup/generated/temp файлы;
- не остались ли LFS pointer-файлы вместо ассетов;
- не смешаны ли unrelated changes.

Минимум один reviewer должен approve PR перед merge.

## 7. Required Checks Before Merge

Перед merge автор PR обязан выполнить:

```bash
git status --short
git diff --stat
git lfs status
```

Если доступна Unreal build environment:

```bash
# пример, точная команда зависит от установленного UE_5.8
/Users/Shared/Epic\ Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh NMS_BuilderAppEditor Mac Development /path/to/NMS_BuilderApp.uproject
```

Минимальный checklist:

- рабочее дерево не содержит случайных файлов;
- LFS clean;
- проект открывается в UE 5.8;
- изменённый workflow вручную проверен;
- тесты добавлены или явно объяснено, почему тест невозможен.

## 8. Git LFS Standard

### 8.1 Files Managed by LFS

Через Git LFS должны храниться:

```text
*.uasset
*.umap
*.ubulk
*.uexp
*.png
*.tga
*.bmp
*.fbx
*.obj
*.psd
*.dds
*.wav
*.mp3
*.ttf
*.otf
```

Эти правила уже должны быть в `.gitattributes`.

### 8.2 LFS Safety Check

Перед PR:

```bash
git lfs status
git lfs ls-files | wc -l
```

Не коммитить asset-файл, если он выглядит как pointer, но должен быть реальным бинарником в рабочей копии.

### 8.3 Large Assets

Для больших ассетов:

- не переимпортировать ассеты без причины;
- не делать массовый reimport в одном PR с кодом;
- отделять asset-only PR от code PR;
- в PR описывать источник ассетов и способ генерации.

## 9. Files That Must Not Be Committed

Запрещено коммитить:

```text
Binaries/
Build/
DerivedDataCache/
Intermediate/
Saved/
.vs/
.idea/
*.sln
*.suo
*.pdb
*.exe
*.dll
*.bak
*.bak_*
*.before_*
*.broken_*
*.rot180hack
*_backup.*
```

Для исторических заметок использовать:

```text
docs/archive/
```

Для временной работы:

```text
work/
tmp/
```

но не коммитить их без явной причины.

## 10. Handling Refactors

Крупный refactor должен идти этапами:

1. Добавить тесты на текущее поведение.
2. Сделать механическое перемещение файлов без изменения поведения.
3. Запустить сборку/тесты.
4. Затем менять архитектурные границы.
5. Каждый этап оформлять отдельным PR.

Запрещено в одном PR:

- переносить файлы;
- менять поведение;
- форматировать весь файл;
- обновлять ассеты;
- чинить unrelated bugs.

## 11. Conflict Resolution

При конфликтах:

```bash
git fetch origin
git rebase origin/main
```

или, если команда предпочитает merge:

```bash
git merge origin/main
```

Правило проекта должно быть единым. Рекомендация: использовать rebase для feature branches до открытия PR, merge commits — только при merge PR.

Нельзя использовать:

```bash
git reset --hard
git checkout -- .
```

если есть риск удалить чужие или незакоммиченные изменения.

## 12. Release Tags

Релизы тегировать:

```text
vMAJOR.MINOR.PATCH
```

Примеры:

```text
v0.1.0
v0.2.0
v1.0.0
```

Перед тегом:

- `main` green;
- LFS clean;
- release notes готовы;
- проект открывается в UE 5.8;
- smoke-test выполнен.

## 13. Recommended Daily Workflow

```bash
git fetch origin
git switch main
git pull --ff-only
git switch -c feature/<short-description>
```

Работа:

```bash
git status --short
git diff
git add <files>
git commit -m "type(scope): summary"
```

Перед PR:

```bash
git fetch origin
git rebase origin/main
git status --short
git lfs status
git diff --stat origin/main...HEAD
```

Push:

```bash
git push -u origin feature/<short-description>
```

## 14. Definition of Done

Изменение считается готовым, если:

- код соответствует architecture guidelines;
- добавлены или обновлены тесты;
- Unreal project открывается;
- нет случайных generated/backup files;
- LFS-ассеты корректны;
- PR имеет понятное описание;
- reviewer approve получен;
- документация обновлена, если изменился setup/workflow.
