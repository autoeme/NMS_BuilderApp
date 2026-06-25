#pragma once

#include "NMSSaveFile.h"
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Engine/DataTable.h"
#include "NMSPartData.h"
#include "NMSBaseDocument.h"   // снимки Undo/Redo хранят доменный документ

template <typename ItemType> class STileView;
class ITableRow;
class STableViewBase;
class SWidget;

/** Категория + детали в ней. */
struct FNMSCategoryGroup
{
    FString Category;
    TArray<TSharedPtr<FNMSPartData>> Parts;
};

/** Один цвет игровой палитры (из NMSData/palettes.json — вытащено из игры). */
struct FNMSPaletteRow
{
    FString Material;     // группа материала: "Timber - Polished Timber" и т.п.
    FString Label;        // имя цвета в игре: "White & Orange"
    FLinearColor Primary   = FLinearColor::White;
    FLinearColor Secondary = FLinearColor::White;
    int32 ColourIndex = -1;  // индекс свотча 0..114 -> пишется в UserData при покраске
};

/**
 * Главный интерфейс приложения NMS Base Builder.
 * Тёмно-синяя тема в стиле оригинального приложения:
 *  - строка вкладок баз
 *  - верхний тулбар
 *  - центральная область вьюпорта (заглушка) + панель списка деталей
 *  - правая панель Transform / Material & Colours / Visibility / Properties
 */
class SNMSBuilderUI : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SNMSBuilderUI) {}
        SLATE_ARGUMENT(UDataTable*, PartsTable)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // непрерывная перерисовка вьюпорта (для анимации камеры)
    virtual void Tick(const FGeometry& AllottedGeometry,
        const double InCurrentTime, const float InDeltaTime) override;

private:
    UDataTable* PartsTable = nullptr;

    TArray<TSharedPtr<FNMSPartData>> AllParts;
    TArray<TSharedPtr<FNMSCategoryGroup>> Categories;

    int32 ActiveCategoryIndex = INDEX_NONE;

    // детали, реально показываемые в сетке (с учётом фильтра поиска)
    TArray<TSharedPtr<FNMSPartData>> VisibleParts;
    TSharedPtr<STileView<TSharedPtr<FNMSPartData>>> PartTileView;

    FString SearchText;
    bool bShowPartList = false; // PART LIST закрыт по умолчанию

    // --- загрузка данных ---
    void LoadPartsTable();
    bool LoadFromDataTable();
    bool LoadFromJsonFallback();
    void BuildCategories();

    // --- сборка крупных блоков UI ---
    TSharedRef<SWidget> BuildTabStrip();

    // --- гамбургер-меню (левый верхний угол) ---
    TSharedRef<SWidget> BuildMenuButton();
    FReply OnMenuButtonClicked();
    TSharedRef<SWidget> MakeMenuContent();
    void OnMenuImportFromNMS();
    void OnMenuExportToNMS();
    void OnMenuNewBase();
    void OnMenuOpen();
    void OnMenuImportModel(); // import .obj/.stl

    // --- функции из Blender-плагина: файл базы + буфер обмена ---
    void OnMenuSaveBaseAs();        // Save Base As... (JSON формата плагина)
    void OnMenuImportClipboard();   // Import from Clipboard
    void OnMenuExportClipboard();   // Export to Clipboard
    void SpawnFromManager(class UNMSBaseManager* Mgr);   // построить базу в сцене
    void CollectSceneToManager(class UNMSBaseManager* Mgr); // сцена -> PlacedObjects
    // --- Undo/Redo (снимки сцены доменным документом FNMSBaseDocument) ---
    TArray<FNMSBaseDocument> UndoStack;
    TArray<FNMSBaseDocument> RedoStack;
    FNMSBaseDocument EditSnapshot;
    bool bHasBaseline = false;          // EditSnapshot уже задан (пустая сцена — валидна)
    bool bRestoring = false;
    FNMSBaseDocument SnapshotScene();
    void RestoreScene(const FNMSBaseDocument& Doc);
    void CommitEdit();
    void DoUndo();
    void DoRedo();
    void ClearSceneParts();         // удалить все детали NMS_ из сцены

    // Base Properties (как в плагине)
    FString CurrentBaseName;
    FString CurrentGalacticAddress;

    // быстрый поиск детали по ObjectID (без ^) — для спавна из сейва
    TMap<FString, TSharedPtr<FNMSPartData>> PartById;

    // --- стопки вариантов (значок «▌▌», как в игре) ---
    // деталь -> варианты ярусов (CompositePartObjectIDs из таблицы игры)
    TMap<FString, TArray<FString>> PartVariants;
    // детали, скрытые из общего списка (ShowInBuildMenu=false в игре)
    TSet<FString> PartHidden;
    void LoadPartVariants();
    TSharedRef<SWidget> MakeVariantMenu(TSharedPtr<FNMSPartData> Item);

    // --- секции внутри категории (заголовки «ПОЛ И СТУПЕНЬКИ» и т.п. из игры) ---
    struct FNMSSecInfo { FString Title; int32 Idx = 0; };
    TMap<FString, FNMSSecInfo> PartSection;   // ID детали -> {название секции, порядок}
    void LoadPartSections();
    TSharedPtr<class SScrollBox> PartScroll;  // прокручиваемый список секций
    void RebuildSectionedGrid();              // перестроить сетку с заголовками секций
    TSharedRef<SWidget> MakePartCard(TSharedPtr<FNMSPartData> Item); // одна карточка детали

    // --- Избранное и Пресеты (перетаскивание детали долгим нажатием 3 сек) ---
    TArray<FString> FavoriteIDs;                       // избранные детали (★)
    TMap<FString, TArray<FString>> Presets;            // именованные наборы
    FString CurrentPreset;                             // активный пресет (для просмотра)
    void LoadFavorites();                              // из NMSData/favorites.json
    void SaveFavorites();                              // в NMSData/favorites.json
    void AddToFavorites(const FString& Id);
    void AddToPreset(const FString& Preset, const FString& Id);
    TSharedRef<SWidget> MakeMoveMenu(TSharedPtr<FNMSPartData> Item); // меню «переместить в»
    // режим перестановки деталей (вкл. кнопкой в меню):
    //   клик по детали — «взять» её, клик по категории — переместить туда.
    bool bArrangeMode = false;
    TWeakPtr<FNMSPartData> PendingMoveItem;
    TSharedPtr<FNMSPartData> CurveBuildPart;
    TSharedPtr<FNMSPartData> LastPartItem;   // последняя деталь (для непрерывной установки)     // деталь для раскладки по кривой
    void BuildAlongCurve();                      // разложить деталь вдоль кривой
    TArray<FVector> TessellateCurve();           // путь по типу кривой
    int32 CurvePartCount();                       // сколько деталей при текущем нахлёсте
    void UpdateCurvePreview();                    // живой меш-предпросмотр раскладки (ISM)
    class UInstancedStaticMeshComponent* CurvePreviewISM = nullptr;                 // взятая для перемещения деталь
    TMap<FString, FString> UserCategory;                    // ID -> своя категория (override)
    void LoadUserCategories();                              // из NMSData/user_categories.json
    void SaveUserCategories();
    void MovePartToCategory(TSharedPtr<FNMSPartData> Item, const FString& Cat);

    TSharedRef<SWidget> BuildToolbar();
    TSharedRef<SWidget> BuildCenterArea();

    // --- встроенный 3D-вьюпорт (способ 2: свой клиент, standalone-совместимо) ---
    TSharedRef<SWidget> BuildViewport();
    TSharedPtr<class FNMSViewportClient> ViewportClient;
    TSharedPtr<class FSceneViewport> SceneViewport;
    TSharedPtr<class SViewport> ViewportWidget;
    TSharedRef<SWidget> BuildPartListPanel();
    TSharedRef<SWidget> BuildCategoryColumn();
    TSharedRef<SWidget> BuildRightPanel();

    // --- логика ---
    void SelectCategory(int32 Index);
    void ApplyFilter();
    void OnSearchChanged(const FText& NewText);
    void TogglePartList();

    TSharedRef<ITableRow> OnGeneratePartTile(
        TSharedPtr<FNMSPartData> Item, const TSharedRef<STableViewBase>& Owner);
    void OnPartSelected(TSharedPtr<FNMSPartData> Item, ESelectInfo::Type);

    // --- выбор фона сцены (PNG из Content/NMSData/UI/Backgrounds, по кругу) ---
    void CycleBackground();
    void ApplyBackground(int32 Index); // ставит фон по номеру (с зацикливанием)
    int32 BackgroundIndex = -1;

    // --- всплывающее главное меню (overlay поверх сцены — прозрачное) ---
    bool bShowMainMenu = false;

    // --- правая панель: можно сворачивать, как нижнюю ---
    bool bShowRightPanel = true;
    static constexpr float RightPanelWidth = 230.f; // ширина правой панели

    // --- покраска: настоящие игровые палитры + применение цвета к детали ---
    TSharedRef<SWidget> BuildColorPalette();
    void OnColorClicked(FLinearColor Color, int32 ColourIndex = -1);
    void LoadPalettes();      // читает NMSData/palettes.json
    void RebuildSwatches();   // перестраивает сетку цветов под выбранный материал
    void SetMaterialIndex(int32 MatIdx);  // выбрать отделку, применить к выбранной детали
    TArray<TSharedPtr<FNMSPaletteRow>> PaletteRows;
    TArray<TSharedPtr<FString>> PaletteMaterials;
    TSharedPtr<FString> CurrentPaletteMaterial;
    int32 CurrentMaterialIndex = 0;  // выбранная отделка (materialIndex 0-3) -> в UserData
    TMap<FString, TArray<TPair<int32, FString>>> PaletteFinishes;  // группа цвета -> [(idx, имя отделки)]
    TArray<TSharedPtr<FString>> PaletteCombo;        // выпадающий список "Семейство - Отделка"
    TMap<FString, TPair<FString, int32>> ComboMap;   // строка опции -> (семейство, materialIndex)
    TMap<int32, FString> ColourNames;                // colourIndex -> название цвета (рус)
    TSharedPtr<class SBox> SwatchContainer;

    // --- иконки игрового UI (PNG из Content/NMSData/UI, грузятся с диска) ---
    TArray<TSharedPtr<struct FSlateDynamicImageBrush>> IconBrushes;
    const FSlateBrush* GetUIIcon(const FString& Name, const FVector2D& Size);

    // --- превью деталей (PNG из Content/NMSData/Thumbs, кэш по ID) ---
    TMap<FString, TSharedPtr<struct FSlateDynamicImageBrush>> ThumbBrushes;
    const FSlateBrush* GetPartThumb(const FString& PartId);

    // =================== SAVE MANAGER (сейв игры .hg) ===================
    FString SMManualRoot;                     // вручную выбранная папка сейвов (пусто = авто по ОС)
    TArray<FString>          SMAccounts;      // пути st_*
    TArray<FNMSSaveSlot>     SMSlots;         // слоты выбранного аккаунта
    TArray<FNMSSaveBaseInfo> SMBases;         // базы выбранного слота
    int32 SMAccountIdx = INDEX_NONE;
    int32 SMSlotIdx    = INDEX_NONE;
    bool  SMShowCorvettes = false;            // переключатель Corvette/Основа

    void SMRefreshAccounts();                 // кнопка обновить + автозагрузка
    void SMSelectAccount(int32 Idx);          // выбрал аккаунт -> грузим слоты
    void SMSelectSlot(int32 Idx);             // выбрал слот -> грузим базы
    void SMLoadBaseIntoEditor(int32 BaseIndex); // загрузить базу в редактор
    TSharedRef<class SWidget> BuildSaveManagerPanel(); // строит панель
    TSharedPtr<class SVerticalBox> SMBaseListBox;      // контейнер списка баз
    void SMRebuildBaseList();                          // перестроить список (фильтр)
    FString SMSearchText;                              // фильтр по имени
    TSharedPtr<class STextBlock> SMStatusText;         // статус/превью

    // привязка база(имя) -> путь к скриншоту + кэш загруженных картинок
    TMap<FString,FString> SMBaseScreens;               // имя базы -> путь png/jpg
    TMap<FString, TSharedPtr<struct FSlateDynamicImageBrush>> SMScreenBrushes;
    void SMLoadScreenLinks();                          // читает base_screenshots.json
    void SMSaveScreenLinks();                          // пишет base_screenshots.json
    const FSlateBrush* SMGetScreenBrush(const FString& BaseName); // картинка базы или null
    void SMPickScreenshot(const FString& BaseName);    // выбрать скриншот для базы
    TSharedRef<class SToolTip> SMMakeBaseTooltip(const FNMSSaveBaseInfo& B); // tooltip с фото

    // =================== ПРАВЫЕ ПАНЕЛИ (выбранная деталь) ===================
    // Живой Transform: чтение/запись позиции/вращения/масштаба выбранной детали.
    AActor* GetSelected() const;             // выбранный актёр из ViewportClient
    TSharedRef<class SWidget> MakeLiveTransformRow(int32 Kind); // 0=Translate,1=Rotate,2=Scale
    float  GetTransformValue(int32 Kind, int32 Axis) const;     // читает компоненту
    void   SetTransformValue(int32 Kind, int32 Axis, float V);  // пишет компоненту
    // Свойства выбранной (PROPERTIES)
    FText  GetSelectedInfo(int32 Field) const; // 0=имя,1=адрес,2=userdata,3=partcount

    // =================== ДЕРЕВО СЦЕНЫ (Unreal SListView) ===================
    // Элемент списка = деталь сцены.
    struct FSceneItem { TWeakObjectPtr<AActor> Actor; FString Name; };
    TArray<TSharedPtr<FSceneItem>> SceneTreeItems;
    TSharedPtr<class SListView<TSharedPtr<FSceneItem>>> SceneListView;

    TSharedRef<class SWidget> BuildSceneTree();   // панель со списком
    void RebuildSceneTree();                      // пересобрать массив + обновить вид
    TSharedRef<class ITableRow> SceneTree_OnGenerateRow(
        TSharedPtr<FSceneItem> Item, const TSharedRef<class STableViewBase>& Owner);
    void SceneTree_OnSelectionChanged(TSharedPtr<FSceneItem> Item, ESelectInfo::Type);
    void ToggleActorHidden(AActor* A);            // глаз: скрыть во вьюпорте
    void ToggleActorRenderOff(AActor* A);         // камера: отключить рендер
    TMap<FString,bool> ActorRenderOff;            // "отключён на рендере" по имени
    int32 LastSceneItemCount = -1;                // для автообновления по Tick
    float SceneTreeHeight = 240.f;                // высота списка (регулируется +/-)
};
