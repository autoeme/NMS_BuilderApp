# NMS_BuilderApp

Редактор баз No Man's Sky на Unreal Engine 5.8 (C++). Встроенный 3D-вьюпорт,
каталог деталей, покраска по игровой палитре, импорт/экспорт баз и Save Manager
для чтения/записи игровых сейвов.

> Тип модуля в `.uproject` — `Runtime`, но фактически это **editor-tool**:
> используются Slate, `UnrealEd`, `DesktopPlatform`. Запускать нужно через
> редактор Unreal (см. ниже). Разделение на Core/Editor — в планах (см. `docs/`).

## Требования

- **Unreal Engine 5.8** (`EngineAssociation` в `.uproject` = `5.8`).
- **Visual Studio 2022/2023** с тулчейном C++ для разработки под Windows
  (проект собирается компилятором MSVC 14.5x).
- **Git LFS** — обязательно. Все бинарные ассеты (`*.uasset`, `*.umap`,
  текстуры, fbx, звук, шрифты) хранятся в LFS (см. `.gitattributes`).
- Windows 10/11. (На macOS Save Manager не найдёт сейвы — путь к ним сейчас
  завязан на Windows `%APPDATA%`, это известное ограничение.)

### Плагины

Включены: `ModelingToolsEditorMode`, `FbxAutomationTestBuilder`,
`DatasmithFBXImporter`. Остальные (`Fab`, `Bridge`, `QuixelTools`,
`MegascansPlugin`) выключены.

## Получение исходников (важно про LFS)

```bash
git clone <repo-url>
cd NMS_BuilderApp
git lfs install
git lfs pull        # подтянуть реальные ассеты вместо указателей
```

Если проект уже склонирован, а ассеты остались LFS-указателями (UE ругается
«Недопустимая сводка для пакета … повреждён») — разверни их из локального кэша:

```bash
git lfs checkout
```

Проверка, что ассеты на месте (а не указатели по ~130 байт):

```bash
git lfs ls-files | grep " - "   # строки с " - " = НЕ выгруженные указатели; должно быть пусто
```

## Сборка

Перегенерировать файлы проекта: ПКМ по `NMS_BuilderApp.uproject` →
*Generate Visual Studio project files*. Затем собрать из IDE или из консоли:

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
    NMS_BuilderAppEditor Win64 Development `
    -Project="<полный путь>\NMS_BuilderApp.uproject" -WaitMutex
```

UBT автоматически подхватывает все `.cpp` в `Source/` — менять `Build.cs` при
добавлении файлов не нужно (но для появления файлов в `.sln` нужна регенерация
проекта).

## Запуск

Открыть `NMS_BuilderApp.uproject` в Unreal Editor 5.8. При старте модуль
автоматически регистрирует и открывает Slate-вкладку билдера.

## Smoke-test после открытия

1. Редактор открылся без ошибок ассетов в *Журнале сообщений*.
2. Открылась вкладка билдера; виден каталог деталей слева.
3. Выбор детали → постановка призрака в сцене → ЛКМ ставит деталь.
4. **Полёт камеры:** кликнуть в сцену, зажать ПКМ + WASD/QE — плавный облёт;
   ПКМ + мышь — обзор; колесо — зум; ESC — отмена текущего действия/выбора.
5. Покраска работает (палитра справа меняет цвет выбранной детали).
6. Save Manager открывает базу из игрового сейва и пишет обратно (Windows).
7. Дерево сцены справа отражает поставленные детали.

## Структура

```
Source/
  NMS_BuilderCore/       — доменное/парсинг-ядро (БЕЗ Slate/UnrealEd):
                           модель базы, цвета/локализация, .hg/.blend, JSON, zstd
    Public/  Private/
  NMS_BuilderApp/        — редакторный модуль: Slate UI, вьюпорт, диалоги,
                           спавн актёров; зависит от NMS_BuilderCore
Content/                 — ассеты (Git LFS)
docs/                    — стандарты проекта (архитектура, git, code review)
план/                    — рабочие планы рефакторинга
```

Граница зависимостей (`Editor -> Core`, но не наоборот) теперь **форсится сборкой**:
`NMS_BuilderCore.Build.cs` не подключает Slate/UnrealEd/DesktopPlatform, поэтому
доменный код не может случайно от них зависеть.

## Известные ограничения

- Save Manager привязан к Windows-пути сейвов (`%APPDATA%/HelloGames/NMS`).
- Нет CI и автоматических тестов парсеров (в планах — см. `docs/`).
- `SNMSBuilderUI` — крупный god-object; разбор по сервисам запланирован.
