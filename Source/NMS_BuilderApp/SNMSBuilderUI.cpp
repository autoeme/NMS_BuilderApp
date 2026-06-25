#include "SNMSBuilderUI.h"
#include "SNMSBuilderUI_Internal.h"

#define LOCTEXT_NAMESPACE "NMSBuilder"

void SNMSBuilderUI::Construct(const FArguments& InArgs)
{
    PartsTable = InArgs._PartsTable;

    LoadPartsTable();
    BuildCategories();

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(NMS::Box())
        .BorderBackgroundColor(NMS::WindowBg)
        .Padding(0.f)
        [
            SNew(SVerticalBox)

            // строка вкладок баз
            + SVerticalBox::Slot().AutoHeight()
            [
                BuildTabStrip()
            ]

            // вьюпорт на всё окно; тулбар и правая панель — ПОВЕРХ сцены (как в игре)
            + SVerticalBox::Slot().FillHeight(1.f)
            [
                SNew(SOverlay)

                + SOverlay::Slot()
                [
                    BuildCenterArea()
                ]

                // верхний тулбар поверх сцены (отступ справа — не лезет под правую панель)
                + SOverlay::Slot().VAlign(VAlign_Top)
                  .Padding(TAttribute<FMargin>::CreateLambda([this]()
                      { return FMargin(0.f, 0.f, bShowRightPanel ? RightPanelWidth + 20.f : 20.f, 0.f); }))
                [
                    BuildToolbar()
                ]

                // правая панель поверх сцены: полоска-переключатель + сама панель
                + SOverlay::Slot().HAlign(HAlign_Right)
                [
                    SNew(SHorizontalBox)

                    // вертикальная полоска: клик сворачивает/открывает панель
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                        .ButtonStyle(&NMS::FlatButton(NMS::PanelHeader, NMS::ItemHover, NMS::AccentDim))
                        .OnClicked_Lambda([this]()
                            { bShowRightPanel = !bShowRightPanel; return FReply::Handled(); })
                        .ContentPadding(FMargin(3.f, 18.f))
                        [
                            SNew(STextBlock)
                            .Text_Lambda([this]()
                                { return FText::FromString(bShowRightPanel ? TEXT(">") : TEXT("<")); })
                            .Font(NMS::Font(10, true)).ColorAndOpacity(NMS::TextPrimary)
                        ]
                    ]

                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SBox).WidthOverride(RightPanelWidth)
                        .Visibility_Lambda([this]()
                            { return bShowRightPanel ? EVisibility::Visible : EVisibility::Collapsed; })
                        [
                            BuildRightPanel()
                        ]
                    ]
                ]

                // главное меню — слой ПОВЕРХ сцены в этом же окне: прозрачность работает
                + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
                  .Padding(FMargin(6.f, 2.f, 0.f, 0.f))
                [
                    SNew(SBox)
                    .Visibility_Lambda([this]()
                        { return bShowMainMenu ? EVisibility::Visible : EVisibility::Collapsed; })
                    [
                        MakeMenuContent()
                    ]
                ]
            ]
        ]
    ];

    if (Categories.Num() > 0)
    {
        SelectCategory(0);
    }

    // ===== ПЛАВНЫЙ ПОЛЁТ КАМЕРЫ =====
    // Камера двигается в FNMSViewportClient::Draw, а Draw зовётся только при
    // перерисовке вьюпорта. Slate по умолчанию «придушивает» частоту кадров,
    // когда мышь не двигается и когда зажата кнопка (throttling). Из-за этого
    // dt между кадрами скачет: камера летит рывками или замирает совсем.
    // 1) Выключаем троттлинг Slate — кадры идут с полной частотой даже при
    //    зажатой ПКМ/клавишах WASD.
    if (IConsoleVariable* CVarThrottle = IConsoleManager::Get().FindConsoleVariable(TEXT("Slate.bAllowThrottling")))
        CVarThrottle->Set(0);

    // 2) Непрерывный активный таймер держит виджет «бодрым» и заставляет
    //    вьюпорт перерисовываться каждый кадр -> Draw идёт стабильно, dt ровный.
    RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda(
        [this](double, float) -> EActiveTimerReturnType
        {
            if (SceneViewport.IsValid()) SceneViewport->Invalidate();
            return EActiveTimerReturnType::Continue;
        }));
}

// Tick виджета вызывается Slate каждый кадр — отсюда БЕЗОПАСНО просим
// вьюпорт перерисоваться (в отличие от Draw, где это рекурсия).
void SNMSBuilderUI::Tick(const FGeometry& AllottedGeometry,
    const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    if (SceneViewport.IsValid())
    {
        SceneViewport->Invalidate();
    }

    // Автообновление дерева сцены: если число NMS-деталей изменилось — пересобрать.
    // Проверяем не каждый кадр, а раз в ~0.5 сек, чтобы не нагружать.
    static double SceneTreeAccum = 0.0;
    SceneTreeAccum += InDeltaTime;
    if (SceneListView.IsValid() && SceneTreeAccum > 0.5)
    {
        SceneTreeAccum = 0.0;
        int32 Cnt = 0;
        UWorld* W = ViewportClient.IsValid() ? ViewportClient->GetWorld() : nullptr;
        if (W) for (TActorIterator<AStaticMeshActor> It(W); It; ++It)
            if (It->GetActorNameOrLabel().StartsWith(TEXT("NMS_"))) Cnt++;
        if (Cnt != LastSceneItemCount)
            RebuildSceneTree();
    }

    // Синхронизация выбора: деталь выбрана во вьюпорте -> подсветить строку в списке.
    if (SceneListView.IsValid() && ViewportClient.IsValid())
    {
        // собрать все выделенные детали из viewport
        TSet<AActor*> WantActors;
        for (const TWeakObjectPtr<AActor>& W : ViewportClient->GetSelectedActorsList())
            if (W.IsValid()) WantActors.Add(W.Get());
        if (WantActors.Num() == 0)
            if (AActor* One = ViewportClient->GetSelectedActor()) WantActors.Add(One);

        TArray<TSharedPtr<FSceneItem>> WantItems;
        for (const TSharedPtr<FSceneItem>& It : SceneTreeItems)
            if (It.IsValid() && WantActors.Contains(It->Actor.Get())) WantItems.Add(It);

        // сравнить с текущим выделением списка
        TArray<TSharedPtr<FSceneItem>> CurSel = SceneListView->GetSelectedItems();
        bool bSame = (CurSel.Num() == WantItems.Num());
        if (bSame) { TSet<TSharedPtr<FSceneItem>> C(CurSel); for (const TSharedPtr<FSceneItem>& W : WantItems) if (!C.Contains(W)) { bSame = false; break; } }

        if (!bSame)
        {
            SceneListView->ClearSelection();
            if (WantItems.Num() > 0)
            {
                SceneListView->SetItemSelection(WantItems, true, ESelectInfo::Direct);
                SceneListView->RequestScrollIntoView(WantItems.Last());
            }
        }
    }
}

// ===========================================================================
//  Строка вкладок
// ===========================================================================
TSharedRef<SWidget> SNMSBuilderUI::BuildTabStrip()
{
    TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);

    // гамбургер-меню (настройки/файл) — левый верхний угол
    Tabs->AddSlot().AutoWidth().Padding(FMargin(6, 4, 10, 4)).VAlign(VAlign_Center)
    [
        BuildMenuButton()
    ];

    // ЕДИНСТВЕННАЯ вкладка — имя ОТКРЫТОЙ базы (фейковые вкладки убраны)
    Tabs->AddSlot().AutoWidth().Padding(FMargin(1, 4, 0, 0))
    [
        SNew(SBorder)
        .BorderImage(NMS::Box())
        .BorderBackgroundColor(NMS::TabActive)
        .Padding(FMargin(14, 8))
        [
            SNew(STextBlock)
            .Text_Lambda([this]() { return FText::FromString(CurrentBaseName.IsEmpty() ? FString(TEXT("Untitled Base")) : CurrentBaseName); })
            .Font(NMS::Font(10, true))
            .ColorAndOpacity(NMS::TextPrimary)
        ]
    ];

    return SNew(SBorder)
        .BorderImage(NMS::Box())
        .BorderBackgroundColor(NMS::WindowBg)
        .Padding(FMargin(4, 0))
        [
            Tabs
        ];
}

// ===========================================================================
//  Гамбургер-меню (левый верхний угол)
// ===========================================================================
TSharedRef<SWidget> SNMSBuilderUI::BuildMenuButton()
{
    return SNew(SBox)
        [
            SNew(SButton)
            .ButtonStyle(FCoreStyle::Get(), "NoBorder")
            .OnClicked(this, &SNMSBuilderUI::OnMenuButtonClicked)
            .ContentPadding(FMargin(0))
            [
                SNew(SBox).WidthOverride(30.f).HeightOverride(30.f)
                [
                    SNew(SBorder)
                    .BorderImage(NMS::Box())
                    .BorderBackgroundColor(NMS::AccentDim)
                    .HAlign(HAlign_Center).VAlign(VAlign_Center)
                    [
                        SNew(SImage)
                        .Image(GetUIIcon(TEXT("icon_tools"), FVector2D(28.f, 28.f)))
                        .ColorAndOpacity(NMS::TextPrimary)
                    ]
                ]
            ]
        ];
}

FReply SNMSBuilderUI::OnMenuButtonClicked()
{
    // меню — слой поверх сцены (см. Construct), просто показываем/прячем
    bShowMainMenu = !bShowMainMenu;
    return FReply::Handled();
}

TSharedRef<SWidget> SNMSBuilderUI::MakeMenuContent()
{
    // Своё всплывающее меню — полупрозрачное, как остальные панели (стиль меню игры).
    // bKeepOpen=true — пункт не закрывает меню (удобно листать фоны).
    auto Item = [this](const FText& Label, TFunction<void()> Action, bool bKeepOpen = false) -> TSharedRef<SWidget>
    {
        return SNew(SButton)
            .ButtonStyle(&NMS::FlatButton(FLinearColor(0.f,0.f,0.f,0.f), NMS::ItemHover, NMS::AccentDim))
            .ContentPadding(FMargin(14.f, 7.f))
            .HAlign(HAlign_Left)
            .OnClicked_Lambda([this, Action, bKeepOpen]()
            {
                if (!bKeepOpen) bShowMainMenu = false;
                if (Action) Action();
                return FReply::Handled();
            })
            [
                SNew(STextBlock).Text(Label)
                .Font(NMS::Font(9)).ColorAndOpacity(NMS::TextPrimary)
            ];
    };
    auto Header = [](const FText& T) -> TSharedRef<SWidget>
    {
        return SNew(SBorder).BorderImage(NMS::Box())
            .BorderBackgroundColor(NMS::PanelHeader)
            .Padding(FMargin(10.f, 4.f))
            [
                SNew(STextBlock).Text(T)
                .Font(NMS::Font(8, true)).ColorAndOpacity(NMS::TextPrimary)
            ];
    };

    TSharedRef<SVerticalBox> LangBox = SNew(SVerticalBox);
    for (const TPair<FString,FString>& Lg : NMS_Languages())
    {
        const FString Code = Lg.Key;
        LangBox->AddSlot().AutoHeight()
        [ Item(FText::FromString(Lg.Value),
            [this, Code]() { NMS_SetLang(Code); RebuildSectionedGrid(); RebuildSwatches(); }) ];
    }

    return SNew(SBox).WidthOverride(230.f)
    [
        SNew(SBorder)
        .BorderImage(NMS::Box())
        .BorderBackgroundColor(FLinearColor(0.05f, 0.055f, 0.06f, 0.70f)) // полупрозрачное
        .Padding(FMargin(3.f))
        [
            SNew(SVerticalBox)
            // === точная копия панелей Blender-плагина ===
            + SVerticalBox::Slot().AutoHeight()[ Header(NMS_T(TEXT("HDR_FILE"), TEXT("ФАЙЛ"))) ]
            + SVerticalBox::Slot().AutoHeight()[ Item(NMS_T(TEXT("FILE_NEW_BASE"), TEXT("New Base...")),
                [this]() { OnMenuNewBase(); }) ]
            + SVerticalBox::Slot().AutoHeight()[ Item(NMS_T(TEXT("FILE_SAVE_BASE"), TEXT("Save Base As...")),
                [this]() { OnMenuSaveBaseAs(); }) ]
            + SVerticalBox::Slot().AutoHeight()[ Item(NMS_T(TEXT("FILE_OPEN_BASE"), TEXT("Open Base...")),
                [this]() { OnMenuOpen(); }) ]
            + SVerticalBox::Slot().AutoHeight()[ Item(NMS_T(TEXT("MENU_IMPORT_MODEL_EXT"), TEXT("\u0418\u043c\u043f\u043e\u0440\u0442 3D-\u043c\u043e\u0434\u0435\u043b\u0438 (.obj/.stl)")),
                [this]() { OnMenuImportModel(); }) ]
            + SVerticalBox::Slot().AutoHeight()[ Header(NMS_T(TEXT("HDR_IMPEXP"), TEXT("IMPORT & EXPORT"))) ]
            + SVerticalBox::Slot().AutoHeight()[ Item(NMS_T(TEXT("CLIP_IMPORT"), TEXT("Import from Clipboard")),
                [this]() { OnMenuImportClipboard(); }) ]
            + SVerticalBox::Slot().AutoHeight()[ Item(NMS_T(TEXT("CLIP_EXPORT"), TEXT("Export to Clipboard")),
                [this]() { OnMenuExportClipboard(); }) ]
            + SVerticalBox::Slot().AutoHeight()[ Header(NMS_T(TEXT("HDR_SAVEMGR"), TEXT("SAVE MANAGER (\u0438\u0433\u0440\u0430)"))) ]
            + SVerticalBox::Slot().AutoHeight()[ Item(NMS_T(TEXT("SM_OPEN"), TEXT("\u041e\u0442\u043a\u0440\u044b\u0442\u044c \u0431\u0430\u0437\u0443 \u0438\u0437 \u0441\u0435\u0439\u0432\u0430 \u0438\u0433\u0440\u044b")),
                [this]() { OnMenuImportFromNMS(); }) ]
            + SVerticalBox::Slot().AutoHeight()[ Item(NMS_T(TEXT("SM_SAVE"), TEXT("\u0421\u043e\u0445\u0440\u0430\u043d\u0438\u0442\u044c \u0431\u0430\u0437\u0443 \u0432 \u0441\u0435\u0439\u0432 \u0438\u0433\u0440\u044b")),
                [this]() { OnMenuExportToNMS(); }) ]
            + SVerticalBox::Slot().AutoHeight()[ Header(NMS_T(TEXT("HDR_VIEW"), TEXT("\u0412\u0418\u0414"))) ]
            + SVerticalBox::Slot().AutoHeight()[ Item(NMS_T(TEXT("MENU_BACKGROUND"), TEXT("Сменить фон")),
                [this]() { CycleBackground(); }, /*bKeepOpen=*/true) ]
            + SVerticalBox::Slot().AutoHeight()[ Item(NMS_T(TEXT("LIGHT_TOGGLE"), TEXT("Свет: вкл/выкл")),
                [this]() { FNMSViewportClient::bUnlitParts = !FNMSViewportClient::bUnlitParts; },
                /*bKeepOpen=*/true) ]
            // режим перестановки: клик по детали кладёт её в Избранное
            + SVerticalBox::Slot().AutoHeight()[ Item(
                NMS_T(TEXT("REARRANGE_MODE"), TEXT("Режим перестановки деталей")),
                [this]() { bArrangeMode = !bArrangeMode; PendingMoveItem.Reset(); },
                /*bKeepOpen=*/true) ]
            // === ЯЗЫК / LANGUAGE (выпадающий список) ===
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SComboButton)
                .ButtonStyle(&NMS::FlatButton(FLinearColor(0.f,0.f,0.f,0.f), NMS::ItemHover, NMS::AccentDim))
                .ContentPadding(FMargin(14.f, 7.f))
                .HasDownArrow(true)
                .ButtonContent()
                [
                    SNew(STextBlock).Text(NMS_T(TEXT("MENU_LANGUAGE"), TEXT("Язык")))
                    .Font(NMS::Font(9)).ColorAndOpacity(NMS::TextPrimary)
                ]
                .MenuContent()[ LangBox ]
            ]
        ]
    ];
}

const FSlateBrush* SNMSBuilderUI::GetPartThumb(const FString& PartId)
{
    if (TSharedPtr<FSlateDynamicImageBrush>* Found = ThumbBrushes.Find(PartId))
        return Found->Get();
    // приоритет: родная иконка из игры, затем рендер модели
    FString Path = FPaths::ProjectContentDir() / TEXT("NMSData/GameIcons") / (PartId + TEXT(".png"));
    if (!FPaths::FileExists(Path))
        Path = FPaths::ProjectContentDir() / TEXT("NMSData/Thumbs") / (PartId + TEXT(".png"));
    if (!FPaths::FileExists(Path)) return nullptr;
    TSharedPtr<FSlateDynamicImageBrush> Brush =
        MakeShareable(new FSlateDynamicImageBrush(FName(*Path), FVector2D(128.f, 96.f)));
    ThumbBrushes.Add(PartId, Brush);
    return Brush.Get();
}

const FSlateBrush* SNMSBuilderUI::GetUIIcon(const FString& Name, const FVector2D& Size)
{
    const FString Path = FPaths::ProjectContentDir() / TEXT("NMSData/UI") / (Name + TEXT(".png"));
    if (!FPaths::FileExists(Path)) return nullptr;
    TSharedPtr<FSlateDynamicImageBrush> Brush =
        MakeShareable(new FSlateDynamicImageBrush(FName(*Path), Size));
    IconBrushes.Add(Brush);
    return Brush.Get();
}

void SNMSBuilderUI::ApplyBackground(int32 Index)
{
    if (!ViewportClient.IsValid()) return;

    const FString Dir = FPaths::ProjectContentDir() / TEXT("NMSData/UI/Backgrounds");
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.png")), true, false);
    if (Files.Num() == 0) return;
    Files.Sort();

    BackgroundIndex = ((Index % Files.Num()) + Files.Num()) % Files.Num();
    ViewportClient->SetBackgroundImage(Dir / Files[BackgroundIndex]);
    UE_LOG(LogTemp, Log, TEXT("NMS: фон -> %s"), *Files[BackgroundIndex]);
}

// Пункт меню «Сменить фон»: следующий по кругу.
void SNMSBuilderUI::CycleBackground()
{
    ApplyBackground(BackgroundIndex + 1);
}

// ===========================================================================
//  Центральная область: вьюпорт-заглушка + панель деталей снизу
// ===========================================================================
// ===========================================================================
//  Встроенный 3D-вьюпорт (шаг 1: проверка связки виджет->вьюпорт->клиент)
// ===========================================================================
TSharedRef<SWidget> SNMSBuilderUI::BuildViewport()
{
    ViewportClient = MakeShareable(new FNMSViewportClient());
    ViewportClient->OnEdited = [this]() { CommitEdit(); };
    ViewportClient->OnUndo   = [this]() { DoUndo(); };
    ViewportClient->OnRedo   = [this]() { DoRedo(); };
    ViewportClient->OnPlaceContinue = [this]() { if (LastPartItem.IsValid()) OnPartSelected(LastPartItem, ESelectInfo::Direct); };
    ViewportClient->OnCurveApply = [this]() { BuildAlongCurve(); };
    ViewportClient->OnCurveChanged = [this]() { UpdateCurvePreview(); };
    // Клик в сцену -> явно ставим клавиатурный фокус на вьюпорт, чтобы WASD/QE
    // летали (как в Unreal/Blender). Без этого мышь захвачена, а клавиши уходят
    // другому виджету -> «не лечу».
    ViewportClient->OnRequestFocus = [this]()
    {
        if (ViewportWidget.IsValid())
            FSlateApplication::Get().SetKeyboardFocus(ViewportWidget, EFocusCause::Mouse);
    };

    ViewportWidget = SNew(SViewport)
        .EnableGammaCorrection(false)
        .IgnoreTextureAlpha(true);
    // фокус клавиатуры вьюпорт получает через FSceneViewport при клике мыши

    SceneViewport = MakeShareable(new FSceneViewport(ViewportClient.Get(), ViewportWidget));
    ViewportWidget->SetViewportInterface(SceneViewport.ToSharedRef());

    // Фон по умолчанию — космос-панорама (файл 25_HDRI_*).
    {
        const FString Dir = FPaths::ProjectContentDir() / TEXT("NMSData/UI/Backgrounds");
        TArray<FString> Files;
        IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.png")), true, false);
        Files.Sort();
        for (int32 i = 0; i < Files.Num(); ++i)
        {
            if (Files[i].StartsWith(TEXT("25_"))) { ApplyBackground(i); break; }
        }
    }

    return ViewportWidget.ToSharedRef();
}

TSharedRef<SWidget> SNMSBuilderUI::BuildCenterArea()
{
    return SNew(SVerticalBox)

        // вьюпорт + панель деталей ПОВЕРХ него (прозрачная, как в игре)
        + SVerticalBox::Slot().FillHeight(1.f)
        [
            SNew(SOverlay)

            + SOverlay::Slot()
            [
                BuildViewport()
            ]

            // выдвижная панель деталей у нижнего края, сквозь неё видно сцену
            // (отступ справа — чтобы не заходила под правую панель с красками)
            + SOverlay::Slot().VAlign(VAlign_Bottom)
              .Padding(TAttribute<FMargin>::CreateLambda([this]()
                  { return FMargin(0.f, 0.f, bShowRightPanel ? RightPanelWidth + 20.f : 20.f, 0.f); }))
            [
                SNew(SBox).HeightOverride(360.f)
                .Visibility_Lambda([this]()
                {
                    return bShowPartList ? EVisibility::Visible : EVisibility::Collapsed;
                })
                [
                    BuildPartListPanel()
                ]
            ]
        ]

        // нижняя плашка ДЕТАЛИ
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SButton)
            .ButtonStyle(&NMS::FlatButton(NMS::PanelHeader, NMS::ItemHover, NMS::AccentDim))
            .OnClicked_Lambda([this]() { TogglePartList(); return FReply::Handled(); })
            .HAlign(HAlign_Center).VAlign(VAlign_Center)
            .ContentPadding(FMargin(0, 8))
            [
                SNew(STextBlock)
                .Text(LOCTEXT("PartListBar", "ДЕТАЛИ"))
                .Font(NMS::Font(10, true)).ColorAndOpacity(NMS::TextPrimary)
            ]
        ];
}

// ===========================================================================
//  Панель деталей: слева категории, справа поиск + сетка карточек
// ===========================================================================
#undef LOCTEXT_NAMESPACE
