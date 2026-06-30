// Часть класса SNMSBuilderUI (разнесение по TU, Часть C плана).
// Включения общие для всех TU класса — см. SNMSBuilderUI_Internal.h.
#include "SNMSBuilderUI_Internal.h"

#define LOCTEXT_NAMESPACE "NMSBuilder"

TSharedRef<SWidget> SNMSBuilderUI::BuildPartListPanel()
{
    return SNew(SBorder)
        .BorderImage(NMS::Box())
        // в режиме перестановки рамка панели становится ЗЕЛЁНОЙ (видно, что режим вкл)
        .BorderBackgroundColor_Lambda([this]()
        {
            return bArrangeMode ? FLinearColor(0.10f, 0.55f, 0.22f, 0.65f)
                                : FLinearColor(0.05f, 0.055f, 0.06f, 0.45f);
        })
        .Padding(FMargin(2.f))
        [
            SNew(SVerticalBox)

            // ЯРКАЯ ПЛАШКА когда режим перестановки включён (как баннер в игре)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SBorder).BorderImage(NMS::Box())
                .BorderBackgroundColor(FLinearColor(0.15f, 0.85f, 0.35f, 0.95f)) // зелёный
                .Padding(FMargin(8.f, 4.f))
                .Visibility_Lambda([this]() { return bArrangeMode ? EVisibility::Visible : EVisibility::Collapsed; })
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ArrangeOn", "● РЕЖИМ ПЕРЕСТАНОВКИ ВКЛЮЧЁН — кликни деталь, затем группу"))
                    .Font(NMS::Font(9, true)).ColorAndOpacity(FLinearColor::White)
                    .Justification(ETextJustify::Center)
                ]
            ]

            // шапка панели — КЛИКАБЕЛЬНА: нажатие сворачивает панель
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SButton)
                .ButtonStyle(&NMS::FlatButton(NMS::PanelHeader, NMS::ItemHover, NMS::AccentDim))
                .OnClicked_Lambda([this]() { TogglePartList(); return FReply::Handled(); })
                .HAlign(HAlign_Left)
                .ContentPadding(FMargin(10.f, 6.f))
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("PartListTitle", "ДЕТАЛИ"))
                    .Font(NMS::Font(10, true)).ColorAndOpacity(NMS::TextPrimary)
                ]
            ]

            // вкладки категорий с иконками игры (по центру, как LB/RB-меню)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 6.f, 0.f, 2.f))
            [
                BuildCategoryColumn()
            ]

            // заголовок активной категории (как «ПРОДВИНУТАЯ ТЕХНОЛОГИЯ» в игре)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 2.f))
            [
                SNew(STextBlock)
                .Text_Lambda([this]()
                {
                    if (Categories.IsValidIndex(ActiveCategoryIndex) && Categories[ActiveCategoryIndex].IsValid())
                        return FText::FromString(Categories[ActiveCategoryIndex]->Category.ToUpper());
                    return FText::GetEmpty();
                })
                .Font(NMS::Font(13, true)).ColorAndOpacity(NMS::TextPrimary)
            ]

            // поиск
            + SVerticalBox::Slot().AutoHeight().Padding(FMargin(8.f, 4.f))
            [
                SNew(SSearchBox)
                .HintText(LOCTEXT("SearchHint", "Search..."))
                .OnTextChanged(this, &SNMSBuilderUI::OnSearchChanged)
            ]

            // сетка деталей с секциями (заголовки «ПОЛ И СТУПЕНЬКИ» из игры)
            + SVerticalBox::Slot().FillHeight(1.f)
            [
                SAssignNew(PartScroll, SScrollBox)
            ]
        ];
}

// Одна карточка детали (превью игры + имя + значок «▌▌» вариантов).
TSharedRef<SWidget> SNMSBuilderUI::MakePartCard(TSharedPtr<FNMSPartData> Item)
{
    FString Id;
    if (Item.IsValid()) { Id = Item->ObjectID; Id.RemoveFromStart(TEXT("^")); }
    const FText Name = Item.IsValid()
        ? FText::FromString(NMS_PartName(Id, Item->DisplayName)) : LOCTEXT("Unknown", "???");
    const FSlateBrush* Thumb = Item.IsValid() ? GetPartThumb(Id) : nullptr;
    const bool bHasVariants = PartVariants.Contains(Id);

    TSharedRef<SWidget> ThumbWidget = Thumb
        ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Thumb))
        : StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(FText::FromString(TEXT("◰")))
            .Font(NMS::Font(20)).ColorAndOpacity(NMS::TextSecondary));

    // Карточка: обычный клик ставит деталь. В режиме перестановки (bArrangeMode)
    // клик по карточке = добавить её в Избранное (простой и надёжный способ
    // «перекладывать» детали без зависающего drag-drop).
    TSharedRef<SWidget> Card =
        SNew(SButton)
        .ButtonStyle(&NMS::FlatButton(FLinearColor(1, 1, 1, 0.06f), NMS::ItemHover, NMS::AccentDim))
        .ContentPadding(FMargin(3.f))
        .OnClicked_Lambda([this, Item]() -> FReply
        {
            if (bArrangeMode)
            {
                // «взять» деталь — следующий клик по категории переместит её туда
                PendingMoveItem = Item;
                RebuildSectionedGrid();      // перерисовать: зелёный ореол на взятой
            }
            else
            {
                OnPartSelected(Item, ESelectInfo::Direct);   // обычный режим — ставим деталь
            }
            return FReply::Handled();
        })
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(0, 0, 0, 3))
            [
                SNew(SBorder).BorderImage(NMS::Box())
                .BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.30f))
                .HAlign(HAlign_Center).VAlign(VAlign_Center)
                [ ThumbWidget ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(FMargin(1, 0))
            [
                SNew(STextBlock).Text(Name)
                .Font(NMS::Font(8, true)).ColorAndOpacity(NMS::TextPrimary)
                .Justification(ETextJustify::Center).AutoWrapText(true)
            ]
        ];

    TSharedRef<SWidget> Content = Card;
    if (bHasVariants)
    {
        Content = SNew(SOverlay)
        + SOverlay::Slot() [ Card ]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0, 3, 3, 0))
        [
            SNew(SComboButton).HasDownArrow(false)
            .ComboButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FComboButtonStyle>("ComboButton"))
            .ContentPadding(FMargin(3, 1))
            .OnGetMenuContent_Lambda([this, Item]() { return MakeVariantMenu(Item); })
            .ButtonContent()
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("▌▌")))
                .Font(NMS::Font(9, true)).ColorAndOpacity(FLinearColor(1.f, 0.78f, 0.18f))
            ]
        ];
    }
    // фикс. размер карточки — как в игре (крупнее): 132×132.
    // Если деталь «взята» в режиме перестановки — зелёный ореол (как выделение в игре).
    const bool bPicked = bArrangeMode && PendingMoveItem.IsValid() && PendingMoveItem.Pin() == Item;
    return SNew(SBox).WidthOverride(132.f).HeightOverride(132.f)
    [
        SNew(SBorder).BorderImage(NMS::Box())
        .BorderBackgroundColor(bPicked ? FLinearColor(0.20f, 1.0f, 0.45f, 0.95f)  // зелёный ореол NMS
                                       : FLinearColor(0, 0, 0, 0))
        .Padding(FMargin(bPicked ? 3.f : 3.f)) [ Content ]
    ];
}

// Меню по долгому нажатию: добавить деталь в Избранное / пресет / новый пресет.
TSharedRef<SWidget> SNMSBuilderUI::MakeMoveMenu(TSharedPtr<FNMSPartData> Item)
{
    FString Id;
    if (Item.IsValid()) { Id = Item->ObjectID; Id.RemoveFromStart(TEXT("^")); }
    TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

    auto AddRow = [&](const FString& Label, TFunction<void()> Act)
    {
        Box->AddSlot().AutoHeight().Padding(2)
        [
            SNew(SButton)
            .ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
            .ContentPadding(FMargin(8, 4))
            .OnClicked_Lambda([Act]() { Act(); FSlateApplication::Get().DismissAllMenus(); return FReply::Handled(); })
            [ SNew(STextBlock).Text(FText::FromString(Label)).Font(NMS::Font(10)).ColorAndOpacity(NMS::TextPrimary) ]
        ];
    };

    AddRow(TEXT("★  В избранное"), [this, Id]() { AddToFavorites(Id); });
    // существующие пресеты
    for (const auto& Pair : Presets)
    {
        const FString Name = Pair.Key;
        AddRow(FString::Printf(TEXT("   → пресет «%s»"), *Name), [this, Name, Id]() { AddToPreset(Name, Id); });
    }
    // новый пресет (имя по времени; переименование — позже)
    AddRow(TEXT("＋  Новый пресет"), [this, Id]()
    {
        const FString Name = FString::Printf(TEXT("Набор %d"), Presets.Num() + 1);
        AddToPreset(Name, Id);
        CurrentPreset = Name;
    });

    return SNew(SBorder)
        .BorderImage(NMS::Box())
        .BorderBackgroundColor(FLinearColor(0.05f, 0.08f, 0.15f, 0.97f))
        .Padding(4)
        [ SNew(SBox).MinDesiredWidth(200.f) [ Box ] ];
}

// Перестроить сетку: детали сгруппированы по секциям с жёлтыми заголовками.
void SNMSBuilderUI::RebuildSectionedGrid()
{
    if (!PartScroll.IsValid()) return;
    PartScroll->ClearChildren();

    // сгруппировать VisibleParts по названию секции (с сохранением порядка idx)
    TArray<FString> SecOrder;
    TMap<FString, TArray<TSharedPtr<FNMSPartData>>> BySec;
    TMap<FString, int32> SecIdx;
    for (const TSharedPtr<FNMSPartData>& P : VisibleParts)
    {
        if (!P.IsValid()) continue;
        FString Id = P->ObjectID; Id.RemoveFromStart(TEXT("^"));
        FString Sec = TEXT("ПРОЧЕЕ"); int32 Idx = 9999;
        if (const FNMSSecInfo* S = PartSection.Find(Id)) { Sec = S->Title; Idx = S->Idx; }
        if (!BySec.Contains(Sec)) { SecOrder.Add(Sec); SecIdx.Add(Sec, Idx); }
        BySec.FindOrAdd(Sec).Add(P);
    }
    SecOrder.Sort([&](const FString& A, const FString& B) { return SecIdx[A] < SecIdx[B]; });

    for (const FString& Sec : SecOrder)
    {
        // жёлтый заголовок секции (как в игре)
        PartScroll->AddSlot().Padding(FMargin(8, 8, 8, 2))
        [
            SNew(STextBlock).Text(FText::FromString(Sec))
            .Font(NMS::Font(10, true)).ColorAndOpacity(FLinearColor(1.f, 0.78f, 0.18f))
        ];
        // карточки секции в перенос-сетку
        TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(true);
        for (const TSharedPtr<FNMSPartData>& P : BySec[Sec])
            Wrap->AddSlot() [ MakePartCard(P) ];
        PartScroll->AddSlot().Padding(FMargin(4, 0)) [ Wrap ];
    }
}

// колонка категорий слева
// Иконка игровой вкладки для категории деталей
static FString NMS_CategoryIcon(const FString& Cat)
{
    if (Cat.Contains(TEXT("Furnish")))    return TEXT("tabs__buildtab_furniture");
    if (Cat.Contains(TEXT("Legacy")))     return TEXT("tabs__buildtab_basic");
    if (Cat.Contains(TEXT("Advanced")))   return TEXT("tabs__buildtab_tech");
    if (Cat.Contains(TEXT("Exotic")))     return TEXT("tabs__buildtab_exoticdecoration");
    if (Cat.Contains(TEXT("Deployable"))) return TEXT("tabs__buildtab_deployable");
    if (Cat.Contains(TEXT("Decoration"))) return TEXT("tabs__buildtab_decoration");
    if (Cat.Contains(TEXT("Large")))      return TEXT("tabs__buildtab_structures");
    if (Cat.Contains(TEXT("Wall Art")))   return TEXT("tabs__buildtab_wallart");
    if (Cat.Contains(TEXT("Corvette")))   return TEXT("tabs__buildtab_freighter");
    if (Cat.Contains(TEXT("Builder")))    return TEXT("tabs__buildtab_builders");
    if (Cat.Contains(TEXT("Alloy")))      return TEXT("tabs__buildtab_fibreglass");
    if (Cat.Contains(TEXT("Timber")))     return TEXT("tabs__buildtab_timber");
    if (Cat.Contains(TEXT("Stone")))      return TEXT("tabs__buildtab_stone");
    if (Cat.Contains(TEXT("Power")))      return TEXT("tabs__buildtab_powerindustry");
    if (Cat.Contains(TEXT("Fossil")))     return TEXT("tabs__buildtab_fossil");      // окаменелости (череп) из игры
    if (Cat.Contains(TEXT("Settlement"))) return TEXT("tabs__buildtab_settlement");  // поселенцы (флаг) из игры
    if (Cat.Contains(TEXT("Избранное")))  return TEXT("tabs__buildtab_favorites");   // звезда из игры
    if (Cat.Contains(TEXT("Пресет")))     return TEXT("tabs__buildtab_biggs");       // пресеты (по фото)
    if (Cat.Contains(TEXT("Wooden")))     return TEXT("tabs__buildtab_timber");
    if (Cat.Contains(TEXT("Other")))      return TEXT("tabs__buildtab_deployable");
    if (Cat.Contains(TEXT("Freighter")))  return TEXT("tabs__buildtab_freighter");
    if (Cat.Contains(TEXT("Foliage")))    return TEXT("tabs__buildtab_exoticdecoration");
    if (Cat.Contains(TEXT("Rooms")))      return TEXT("tabs__buildtab_structures");
    if (Cat.Contains(TEXT("Event")))      return TEXT("tabs__buildtab_favorites");
    if (Cat.Contains(TEXT("Space")))      return TEXT("tabs__buildtab_freighter.tech");
    if (Cat.Contains(TEXT("Basic")))      return TEXT("tabs__buildtab_basic");
    if (Cat.Contains(TEXT("Special")))    return TEXT("tabs__buildtab_othertech");
    if (Cat.Contains(TEXT("Underwater"))) return TEXT("tabs__buildtab_freighter.bio");
    return TEXT("groups__buildgroup");
}

TSharedRef<SWidget> SNMSBuilderUI::BuildCategoryColumn()
{
    // Горизонтальный ряд вкладок-иконок, как игровое LB/RB-меню
    TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);

    for (int32 i = 0; i < Categories.Num(); ++i)
    {
        const FString CatName = Categories[i]->Category;
        const FSlateBrush* Icon = GetUIIcon(NMS_CategoryIcon(CatName), FVector2D(30.f, 30.f));
        // активная вкладка -> золотая иконка (как в игре); остальные -> приглушённо-белые
        auto IconTint = [this, i]() -> FSlateColor
        {
            return ActiveCategoryIndex == i
                ? FSlateColor(NMS::Gold)
                : FSlateColor(FLinearColor(1.f, 1.f, 1.f, 1.f)); // как есть, не приглушать
        };
        TSharedRef<SWidget> IconWidget = Icon
            ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Icon).ColorAndOpacity_Lambda(IconTint))
            : StaticCastSharedRef<SWidget>(SNew(STextBlock)
                .Text(FText::FromString(CatName.Left(2).ToUpper()))
                .Font(NMS::Font(10, true))
                .ColorAndOpacity_Lambda([this, i]() -> FSlateColor {
                    return ActiveCategoryIndex == i ? FSlateColor(NMS::Gold) : FSlateColor(NMS::TextPrimary); }));

        Row->AddSlot().AutoWidth().Padding(FMargin(2.f, 0.f))
        [
            SNew(SBox).WidthOverride(46.f).HeightOverride(40.f)
            [
                SNew(SButton)
                .ButtonStyle(&NMS::FlatButton(NMS::ItemBg, NMS::ItemHover, NMS::AccentDim))
                .ButtonColorAndOpacity_Lambda([this, i]() -> FSlateColor
                {
                    return ActiveCategoryIndex == i
                        ? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 1.f))
                        : FSlateColor(FLinearColor(1.f, 1.f, 1.f, 1.f));
                })
                .OnClicked_Lambda([this, i]()
                {
                    // в режиме перестановки + есть взятая деталь -> переместить её сюда
                    if (bArrangeMode && PendingMoveItem.IsValid() && Categories.IsValidIndex(i))
                    {
                        MovePartToCategory(PendingMoveItem.Pin(), Categories[i]->Category);
                        PendingMoveItem.Reset();
                    }
                    else
                    {
                        SelectCategory(i);
                    }
                    return FReply::Handled();
                })
                .HAlign(HAlign_Center).VAlign(VAlign_Center)
                .ContentPadding(FMargin(2.f))
                .ToolTipText(FText::FromString(CatName))
                [ IconWidget ]
            ]
        ];
    }

    return SNew(SScrollBox).Orientation(Orient_Horizontal)
        + SScrollBox::Slot()[ Row ];
}

// ===========================================================================
//  Правая панель
// ===========================================================================
void SNMSBuilderUI::SelectCategory(int32 Index)
{
    if (!Categories.IsValidIndex(Index)) return;
    ActiveCategoryIndex = Index;
    SearchText.Empty();
    ApplyFilter();
}

void SNMSBuilderUI::ApplyFilter()
{
    VisibleParts.Reset();
    // Если в поиске что-то введено — ищем по ВСЕМ деталям (глобально),
    // иначе показываем только активную категорию.
    const bool bGlobalSearch = !SearchText.IsEmpty();
    const TArray<TSharedPtr<FNMSPartData>>* Src = nullptr;
    if (bGlobalSearch)
    {
        Src = &AllParts;
    }
    else if (Categories.IsValidIndex(ActiveCategoryIndex))
    {
        Src = &Categories[ActiveCategoryIndex]->Parts;
    }
    if (Src)
    {
        for (const TSharedPtr<FNMSPartData>& P : *Src)
        {
            if (!P.IsValid()) continue;
            FString CleanId = P->ObjectID; CleanId.RemoveFromStart(TEXT("^"));
            // скрытые ярусы прячем, но при поиске по ID — показываем
            if (PartHidden.Contains(CleanId)
                && (SearchText.IsEmpty() || !CleanId.Contains(SearchText)))
                continue;
            if (SearchText.IsEmpty()
                || P->DisplayName.Contains(SearchText)
                || P->ObjectID.Contains(SearchText))
            {
                VisibleParts.Add(P);
            }
        }
    }
    RebuildSectionedGrid();
}

void SNMSBuilderUI::OnSearchChanged(const FText& NewText)
{
    SearchText = NewText.ToString();
    ApplyFilter();
}

void SNMSBuilderUI::TogglePartList()
{
    bShowPartList = !bShowPartList;
    Invalidate(EInvalidateWidgetReason::Layout);
}

TSharedRef<ITableRow> SNMSBuilderUI::OnGeneratePartTile(
    TSharedPtr<FNMSPartData> Item, const TSharedRef<STableViewBase>& Owner)
{
    const FText Name = Item.IsValid()
        ? FText::FromString(Item->DisplayName) : LOCTEXT("Unknown", "???");

    // превью из сгенерированных миниатюр (NMSData/Thumbs/<ID>.png)
    const FSlateBrush* Thumb = nullptr;
    if (Item.IsValid())
    {
        FString Id = Item->ObjectID;
        Id.RemoveFromStart(TEXT("^"));
        Thumb = GetPartThumb(Id);
    }
    TSharedRef<SWidget> ThumbWidget = Thumb
        ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Thumb))
        : StaticCastSharedRef<SWidget>(SNew(STextBlock)
            .Text(FText::FromString(TEXT("◰")))
            .Font(NMS::Font(18)).ColorAndOpacity(NMS::TextSecondary));

    // есть ли у детали стопка вариантов (ярусы B/M/T — как «▌▌» в игре)
    FString CleanId;
    if (Item.IsValid()) { CleanId = Item->ObjectID; CleanId.RemoveFromStart(TEXT("^")); }
    const bool bHasVariants = PartVariants.Contains(CleanId);

    TSharedRef<SWidget> Card =
        SNew(SBorder)
        .BorderImage(NMS::Box())
        .BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.08f)) // полупрозрачная карточка
        .Padding(FMargin(3.f))
        [
            SNew(SVerticalBox)

            // превью детали — занимает всё место над надписью
            + SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(0, 0, 0, 3))
            [
                SNew(SBorder).BorderImage(NMS::Box())
                .BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.30f))
                .HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    ThumbWidget
                ]
            ]

            // имя детали — карточка заканчивается на надписи, без пустоты снизу
            + SVerticalBox::Slot().AutoHeight().Padding(FMargin(1, 0))
            [
                SNew(STextBlock).Text(Name)
                .Font(NMS::Font(8, true)).ColorAndOpacity(NMS::TextPrimary)
                .Justification(ETextJustify::Center)
                .AutoWrapText(true)
            ]
        ];

    TSharedRef<SWidget> Content = Card;
    if (bHasVariants)
    {
        // значок «▌▌» в правом верхнем углу; клик открывает выбор варианта
        Content = SNew(SOverlay)
        + SOverlay::Slot() [ Card ]
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0, 2, 2, 0))
        [
            SNew(SComboButton)
            .HasDownArrow(false)
            .ComboButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FComboButtonStyle>("ComboButton"))
            .ContentPadding(FMargin(3, 1))
            .OnGetMenuContent_Lambda([this, Item]() { return MakeVariantMenu(Item); })
            .ButtonContent()
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("▌▌"))) // ▌▌
                .Font(NMS::Font(9, true))
                .ColorAndOpacity(FLinearColor(1.f, 0.78f, 0.18f)) // жёлтый, как в игре
            ]
        ];
    }

    return SNew(STableRow<TSharedPtr<FNMSPartData>>, Owner)
    .Padding(FMargin(2.f))
    [
        Content
    ];
}

// Меню выбора варианта стопки: сама деталь + её ярусы (низ/середина/верх).
TSharedRef<SWidget> SNMSBuilderUI::MakeVariantMenu(TSharedPtr<FNMSPartData> Item)
{
    TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    if (!Item.IsValid()) return Box;
    FString CleanId = Item->ObjectID; CleanId.RemoveFromStart(TEXT("^"));

    TArray<TSharedPtr<FNMSPartData>> Options;
    Options.Add(Item); // первый пункт — сама деталь
    if (const TArray<FString>* Vars = PartVariants.Find(CleanId))
    {
        for (const FString& Vid : *Vars)
        {
            if (const TSharedPtr<FNMSPartData>* P = PartById.Find(Vid))
                Options.Add(*P);
        }
    }

    for (const TSharedPtr<FNMSPartData>& Opt : Options)
    {
        FString OptId = Opt->ObjectID; OptId.RemoveFromStart(TEXT("^"));
        const FSlateBrush* Thumb = GetPartThumb(OptId);
        Box->AddSlot().AutoHeight().Padding(2)
        [
            SNew(SButton)
            .ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
            .ContentPadding(FMargin(4, 3))
            .OnClicked_Lambda([this, Opt]()
            {
                OnPartSelected(Opt, ESelectInfo::Direct);
                FSlateApplication::Get().DismissAllMenus();
                return FReply::Handled();
            })
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
                [
                    Thumb
                        ? StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(36).HeightOverride(36)
                            [ SNew(SImage).Image(Thumb) ])
                        : StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(36).HeightOverride(36))
                ]
                + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                [
                    SNew(STextBlock).Text(FText::FromString(Opt->DisplayName))
                    .Font(NMS::Font(9)).ColorAndOpacity(NMS::TextPrimary)
                ]
            ]
        ];
    }

    return SNew(SBorder)
        .BorderImage(NMS::Box())
        .BorderBackgroundColor(FLinearColor(0.05f, 0.08f, 0.15f, 0.96f)) // тёмная панель в стиле меню
        .Padding(4)
        [
            SNew(SBox).MinDesiredWidth(220.f) [ Box ]
        ];
}

// Родной цвет материала детали — как выглядит свежепоставленная деталь в игре.
// Берём дефолтные цвета игровых палитр (palettes.json, первый цвет каждой группы).
void SNMSBuilderUI::OnPartSelected(TSharedPtr<FNMSPartData> Item, ESelectInfo::Type)
{
    if (!Item.IsValid()) return;
    CurveBuildPart = Item;   // запомнить для раскладки по кривой
    LastPartItem = Item;     // для непрерывной установки
    if (ViewportClient.IsValid()) { if (UStaticMesh* CM = LoadObject<UStaticMesh>(nullptr, *Item->ModelPath)) { const FVector S = CM->GetBoundingBox().GetSize(); ViewportClient->SetCurvePartLen(FMath::Max(FMath::Max(S.X, S.Y), 1.f)); } }
    UE_LOG(LogTemp, Log, TEXT("Selected part: %s (%s) model=%s"),
        *Item->DisplayName, *Item->ObjectID, *Item->ModelPath);

    if (!ViewportClient.IsValid()) return;
    UWorld* World = ViewportClient->GetWorld();
    if (!World) return;

    // завершить предыдущее действие: убрать призрак и снять выделение
    ViewportClient->CancelPlacing();
    ViewportClient->ClearSelection();

    // 1) Загрузить меш по пути из базы (/Game/NMSBaseBuilder/Features/Models_FBX/ID.ID)
    UStaticMesh* Mesh = nullptr;
    if (!Item->ModelPath.IsEmpty())
        Mesh = LoadObject<UStaticMesh>(nullptr, *Item->ModelPath);
    if (!Mesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("NMS: меш не найден: %s"), *Item->ModelPath);
        return;
    }

    // 2) Поставить деталь туда, куда смотрит камера (точка на сетке Z=0)
    const FVector SpawnLoc = ViewportClient->GetGroundFocusPoint();
    FActorSpawnParameters Params;
    Params.ObjectFlags = RF_Transient;
    // при постройке деталь ставим боком (поворот 90° вокруг вертикали), а не ребром
    float SpawnYaw = 90.f;
    // Пандусы/лестницы авторски развёрнуты на 180° (см. NMS_IsRampPart) —
    // компенсируем, чтобы при постановке смотрели так же, как в игре/при загрузке.
    if (NMS_IsRampPart(*Item))
        SpawnYaw += 180.f;
    // Точечная поправка ориентации (детали с авторски развёрнутым мешем) — orientation_fix.json
    SpawnYaw += NMS_OrientationFixYaw(Item->ObjectID);
    AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(
        AStaticMeshActor::StaticClass(), FTransform(FRotator(0.f, SpawnYaw, 0.f), SpawnLoc), Params);
    if (!Actor) return;
#if WITH_EDITOR
    Actor->SetActorLabel(FString::Printf(TEXT("NMS_%s"), *Item->ObjectID));
#endif
    Actor->Tags.Add(FName(*Item->ObjectID)); // ID для сохранения/экспорта базы
    if (UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent())
    {
        Comp->SetMobility(EComponentMobility::Movable);
        Comp->SetStaticMesh(Mesh);
        // коллизия для выбора кликом
        Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Comp->SetCollisionObjectType(ECC_WorldStatic);
        Comp->SetCollisionResponseToAllChannels(ECR_Ignore);
        Comp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Comp->SetCastShadow(false); // тени вешают GPU (Virtual Shadow Maps) на этой видеокарте
    }

    // 3) Деталь следует за курсором (снап/сетка), ЛКМ ставит — но выглядит как
    //    обычная деталь (настоящий материал + гизмо), без зелёной голограммы.
    // Материалы как при загрузке базы: ПО СЛОТАМ (стекло/листва/unlit + покраска),
    // чтобы вручную поставленная деталь выглядела 1-в-1 с загруженной (стекло видно).
    if (UStaticMeshComponent* PartComp = Actor->GetStaticMeshComponent())
        ApplyPartMaterialsToComponent(PartComp, *Item, 0);
    ViewportClient->StartPlacing(Actor);
}


// Живой меш-предпросмотр раскладки через InstancedStaticMesh (видна сама деталь, обновляется с ползунком).
void SNMSBuilderUI::MovePartToCategory(TSharedPtr<FNMSPartData> Item, const FString& Cat)
{
    if (!Item.IsValid()) return;
    FString Id = Item->ObjectID; Id.RemoveFromStart(TEXT("^"));
    if (Cat.Contains(TEXT("Избранное")))     // в Избранное — отдельный список
    {
        AddToFavorites(Id);
        return;
    }
    Item->Category = Cat;                     // переназначить категорию детали
    UserCategory.Add(Id, Cat);               // запомнить override
    SaveUserCategories();
    BuildCategories();                       // пересобрать группы
    Invalidate(EInvalidateWidgetReason::Layout);
}
#undef LOCTEXT_NAMESPACE
