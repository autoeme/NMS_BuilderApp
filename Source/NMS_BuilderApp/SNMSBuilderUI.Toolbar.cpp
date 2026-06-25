// Часть класса SNMSBuilderUI (разнесение по TU, Часть C плана).
// Включения общие для всех TU класса — см. SNMSBuilderUI_Internal.h.
#include "SNMSBuilderUI_Internal.h"

#define LOCTEXT_NAMESPACE "NMSBuilder"

TSharedRef<SWidget> SNMSBuilderUI::BuildColorPalette()
{
    LoadPalettes();

    // запасной вариант, если palettes.json не найден
    if (PaletteRows.Num() == 0)
    {
        return SNew(STextBlock).Text(LOCTEXT("NoPalettes", "palettes.json не найден"));
    }

    TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    // единый список "Семейство - Отделка" (как в игре): выбор задаёт и цвета, и materialIndex
    PaletteCombo.Empty(); ComboMap.Empty();
    for (const TSharedPtr<FString>& Fam : PaletteMaterials)
    {
        const FString F = *Fam;
        const TArray<TPair<int32, FString>>* Fins = PaletteFinishes.Find(F);
        if (Fins && Fins->Num() > 0)
            for (const TPair<int32, FString>& Fin : *Fins)
            {
                FString Stem, Rest;
                if (!F.Split(TEXT(" - "), &Stem, &Rest)) Stem = F;   // "Legacy - Concrete" -> "Legacy"
                const FString Opt = (Fins->Num() == 1 && Fin.Value == TEXT("Default"))
                    ? F : (Stem + TEXT(" - ") + Fin.Value);
                PaletteCombo.Add(MakeShared<FString>(Opt));
                ComboMap.Add(Opt, TPair<FString, int32>(F, Fin.Key));
            }
        else { PaletteCombo.Add(MakeShared<FString>(F)); ComboMap.Add(F, TPair<FString, int32>(F, 0)); }
    }
    TSharedPtr<FString> InitSel = PaletteCombo.Num() > 0 ? PaletteCombo[0] : nullptr;
    if (InitSel.IsValid()) { if (auto* P = ComboMap.Find(*InitSel)) { CurrentPaletteMaterial = MakeShared<FString>(P->Key); CurrentMaterialIndex = P->Value; } }
    Box->AddSlot().AutoHeight().Padding(FMargin(0, 0, 0, 5))
    [
        SNew(STextComboBox)
        .OptionsSource(&PaletteCombo)
        .InitiallySelectedItem(InitSel)
        .OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewSel, ESelectInfo::Type)
        {
            if (!NewSel.IsValid()) return;
            if (const TPair<FString, int32>* P = ComboMap.Find(*NewSel))
            {
                CurrentPaletteMaterial = MakeShared<FString>(P->Key);
                SetMaterialIndex(P->Value);  // задаёт отделку, применяет к выбранной детали, перестраивает свотчи
            }
        })
    ];
    Box->AddSlot().AutoHeight()
    [
        SAssignNew(SwatchContainer, SBox)
    ];
    RebuildSwatches();
    return Box;
}

TSharedRef<SWidget> SNMSBuilderUI::BuildToolbar()
{
    // Кнопка в стиле игры: иконка сверху, подпись снизу; активный режим подсвечен.
    auto GameBtn = [this](const FString& IconName, const FString& LKey, const FString& LFb,
                          TFunction<void()> OnClick, TFunction<bool()> IsActive) -> TSharedRef<SWidget>
    {
        const FSlateBrush* Icon = GetUIIcon(IconName, FVector2D(26.f, 26.f));
        TSharedRef<SWidget> IconWidget = Icon
            ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(Icon))
            : StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(30.f).HeightOverride(30.f));
        return SNew(SBox).WidthOverride(96.f).Padding(FMargin(2.f, 0.f))
        [
            SNew(SButton)
            .ButtonStyle(&NMS::FlatButton(NMS::ItemBg, NMS::ItemHover, NMS::AccentDim))
            .ButtonColorAndOpacity_Lambda([IsActive]() -> FSlateColor
                { return (IsActive && IsActive()) ? FSlateColor(NMS::Accent) : FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.85f)); })
            .OnClicked_Lambda([OnClick]() { if (OnClick) OnClick(); return FReply::Handled(); })
            .ContentPadding(FMargin(4.f, 5.f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [ IconWidget ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 3.f, 0.f, 0.f)
                [
                    SNew(STextBlock).Text_Lambda([LKey, LFb]() { return NMS_T(LKey, LFb); }).Font(NMS::Font(8, false))
                    .ColorAndOpacity(NMS::TextPrimary)
                    .Justification(ETextJustify::Center).AutoWrapText(true)
                ]
            ]
        ];
    };
    auto NoOp = []() {};
    auto Never = []() { return false; };
    auto ModeIs = [this](ENMSTransformMode M) { return ViewportClient.IsValid() && ViewportClient->GetTransformMode() == M; };
    auto SetMode = [this](ENMSTransformMode M) { if (ViewportClient.IsValid()) ViewportClient->SetTransformMode(M); };

    TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox);

    // --- группа 1: постройка/цвета/трансформации ---
    Bar->AddSlot().AutoWidth()[ GameBtn(TEXT("buildmenu__putdown"), TEXT("MODE_BUILD"), TEXT("ПОСТРОИТЬ"),
        [this]() { TogglePartList(); }, [this]() { return bShowPartList; }) ];
    Bar->AddSlot().AutoWidth()[ GameBtn(TEXT("buildmenu__recolour"), TEXT("HDR_COLORS"), TEXT("ЦВЕТА"), NoOp, Never) ];
    Bar->AddSlot().AutoWidth()[ GameBtn(TEXT("buildmenu__rotate"), TEXT("HDR_ROTATE"), TEXT("ПОВЕРНУТЬ"),
        [SetMode]() { SetMode(ENMSTransformMode::Rotate); }, [ModeIs]() { return ModeIs(ENMSTransformMode::Rotate); }) ];
    {
        const FSlateBrush* CIcon = GetUIIcon(TEXT("buildmenu__rotatey"), FVector2D(26.f, 26.f));
        auto CurveItem = [this](const FText& Label, ENMSCurveType Type) -> TSharedRef<SWidget>
        {
            return SNew(SButton)
                .ButtonStyle(&NMS::FlatButton(FLinearColor(0.f,0.f,0.f,0.f), NMS::ItemHover, NMS::AccentDim))
                .ContentPadding(FMargin(14.f, 7.f)).HAlign(HAlign_Left)
                .OnClicked_Lambda([this, Type]() {
                    if (ViewportClient.IsValid()) { ViewportClient->CurveType = Type; ViewportClient->bCurveMode = true; ViewportClient->CurvePoints.Reset(); } UpdateCurvePreview();
                    if (FSlateApplication::IsInitialized()) FSlateApplication::Get().DismissAllMenus();
                    return FReply::Handled();
                })
                [ SNew(STextBlock).Text(Label).Font(NMS::Font(9)).ColorAndOpacity(NMS::TextPrimary) ];
        };
        Bar->AddSlot().AutoWidth().VAlign(VAlign_Top)
        [
            SNew(SBox).WidthOverride(112.f).Padding(FMargin(2.f, 0.f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                SNew(SComboButton).HasDownArrow(false)
                .ButtonStyle(&NMS::FlatButton(NMS::ItemBg, NMS::ItemHover, NMS::AccentDim))
                .OnGetMenuContent_Lambda([this, CurveItem]()
                {
                    return SNew(SBorder).BorderImage(NMS::Box()).BorderBackgroundColor(NMS::WindowBg).Padding(FMargin(2.f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()[ CurveItem(FText::FromString(TEXT("Плавная (Catmull)")), ENMSCurveType::Catmull) ]
                        + SVerticalBox::Slot().AutoHeight()[ CurveItem(FText::FromString(TEXT("Ломаная")),           ENMSCurveType::Polyline) ]
                        + SVerticalBox::Slot().AutoHeight()[ CurveItem(FText::FromString(TEXT("Дуга / Круг")),       ENMSCurveType::Circle) ]
                        + SVerticalBox::Slot().AutoHeight()[ CurveItem(FText::FromString(TEXT("Безье")),             ENMSCurveType::Bezier) ]
                        + SVerticalBox::Slot().AutoHeight()[ CurveItem(FText::FromString(TEXT("Прямоугольник")),     ENMSCurveType::Rect) ]
                    ];
                })
                .ButtonContent()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [ CIcon ? StaticCastSharedRef<SWidget>(SNew(SImage).Image(CIcon)) : StaticCastSharedRef<SWidget>(SNew(SBox).WidthOverride(30.f).HeightOverride(30.f)) ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6.f,0.f,0.f,0.f))
                    [ SNew(STextBlock).Text_Lambda([]() { return NMS_T(TEXT("MODE_CURVE"), TEXT("КРИВАЯ")); }).Font(NMS::Font(9)).ColorAndOpacity(NMS::TextPrimary) ]
                ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,3.f,0.f,0.f))
                [
                    SNew(SBorder).BorderImage(NMS::Box())
                    .BorderBackgroundColor(FLinearColor(0.05f,0.055f,0.06f,0.92f))
                    .Visibility_Lambda([this](){ return (ViewportClient.IsValid() && ViewportClient->bCurveMode) ? EVisibility::Visible : EVisibility::Collapsed; })
                    .Padding(FMargin(5.f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [ SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth()
                            [ SNew(SBox).WidthOverride(36.f)[ SNew(SEditableTextBox).Font(NMS::Font(8)).Justification(ETextJustify::Center)
                                .Text_Lambda([this](){ return FText::AsNumber(ViewportClient.IsValid()?FMath::RoundToInt((ViewportClient->CurveOverlap/0.92f)*100.f):0); })
                                .OnTextCommitted_Lambda([this](const FText& Tx, ETextCommit::Type){ if(ViewportClient.IsValid()) ViewportClient->CurveOverlap=FMath::Clamp((float)FCString::Atof(*Tx.ToString()),0.f,100.f)/100.f*0.92f; UpdateCurvePreview(); }) ] ]
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(2.f,0.f,0.f,0.f))
                            [ SNew(STextBlock).Text(FText::FromString(TEXT("%"))).Font(NMS::Font(8)).ColorAndOpacity(NMS::TextSecondary) ]
                            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(6.f,0.f,0.f,0.f))
                            [ SNew(STextBlock).Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("%d шт"), CurvePartCount())); }).Font(NMS::Font(8)).ColorAndOpacity(NMS::TextPrimary) ] ]
                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,3.f))
                        [ SNew(SSlider)
                            .Value_Lambda([this](){ return ViewportClient.IsValid() ? ViewportClient->CurveOverlap/0.92f : 0.f; })
                            .OnValueChanged_Lambda([this](float V){ if (ViewportClient.IsValid()) ViewportClient->CurveOverlap = V*0.92f; UpdateCurvePreview(); }) ]
                        + SVerticalBox::Slot().AutoHeight()
                        [ SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                            [ SNew(STextBlock).Text(FText::FromString(TEXT("Накл"))).Font(NMS::Font(8)).ColorAndOpacity(NMS::TextSecondary) ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [ SNew(SBox).WidthOverride(38.f)[ SNew(SEditableTextBox).Font(NMS::Font(8)).Justification(ETextJustify::Center)
                                .Text_Lambda([this](){ return FText::AsNumber(ViewportClient.IsValid()?FMath::RoundToInt(ViewportClient->CurveTilt):0); })
                                .OnTextCommitted_Lambda([this](const FText& Tx, ETextCommit::Type){ if(ViewportClient.IsValid()) ViewportClient->CurveTilt=FMath::Clamp((float)FCString::Atof(*Tx.ToString()),-90.f,90.f); UpdateCurvePreview(); }) ] ] ]
                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,2.f))
                        [ SNew(SSlider)
                            .Value_Lambda([this](){ return ViewportClient.IsValid() ? (ViewportClient->CurveTilt+90.f)/180.f : 0.5f; })
                            .OnValueChanged_Lambda([this](float V){ if (ViewportClient.IsValid()) ViewportClient->CurveTilt = (V-0.5f)*180.f; UpdateCurvePreview(); }) ]
                        + SVerticalBox::Slot().AutoHeight()
                        [ SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                            [ SNew(STextBlock).Text(FText::FromString(TEXT("Крен"))).Font(NMS::Font(8)).ColorAndOpacity(NMS::TextSecondary) ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [ SNew(SBox).WidthOverride(38.f)[ SNew(SEditableTextBox).Font(NMS::Font(8)).Justification(ETextJustify::Center)
                                .Text_Lambda([this](){ return FText::AsNumber(ViewportClient.IsValid()?FMath::RoundToInt(ViewportClient->CurveRoll):0); })
                                .OnTextCommitted_Lambda([this](const FText& Tx, ETextCommit::Type){ if(ViewportClient.IsValid()) ViewportClient->CurveRoll=FMath::Clamp((float)FCString::Atof(*Tx.ToString()),-90.f,90.f); UpdateCurvePreview(); }) ] ] ]
                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,2.f))
                        [ SNew(SSlider)
                            .Value_Lambda([this](){ return ViewportClient.IsValid() ? (ViewportClient->CurveRoll+90.f)/180.f : 0.5f; })
                            .OnValueChanged_Lambda([this](float V){ if (ViewportClient.IsValid()) ViewportClient->CurveRoll = (V-0.5f)*180.f; UpdateCurvePreview(); }) ]
                        + SVerticalBox::Slot().AutoHeight()
                        [ SNew(SHorizontalBox)
                            .Visibility_Lambda([this](){ return (ViewportClient.IsValid() && ViewportClient->CurveType==ENMSCurveType::Circle) ? EVisibility::Visible : EVisibility::Collapsed; })
                            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                            [ SNew(STextBlock).Text(FText::FromString(TEXT("Рад"))).Font(NMS::Font(8)).ColorAndOpacity(NMS::TextSecondary) ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [ SNew(SBox).WidthOverride(48.f)[ SNew(SEditableTextBox).Font(NMS::Font(8)).Justification(ETextJustify::Center)
                                .Text_Lambda([this](){ return FText::AsNumber(ViewportClient.IsValid()?FMath::RoundToInt(ViewportClient->CurveRadius):0); })
                                .OnTextCommitted_Lambda([this](const FText& Tx, ETextCommit::Type){ if(ViewportClient.IsValid()) ViewportClient->CurveRadius=FMath::Max(0.f,(float)FCString::Atof(*Tx.ToString())); UpdateCurvePreview(); }) ] ] ]
                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,2.f))
                        [ SNew(SSlider)
                            .Visibility_Lambda([this](){ return (ViewportClient.IsValid() && ViewportClient->CurveType==ENMSCurveType::Circle) ? EVisibility::Visible : EVisibility::Collapsed; })
                            .Value_Lambda([this](){ return ViewportClient.IsValid() ? FMath::Clamp(ViewportClient->CurveRadius/4000.f,0.f,1.f) : 0.f; })
                            .OnValueChanged_Lambda([this](float V){ if (ViewportClient.IsValid()) ViewportClient->CurveRadius = V*4000.f; UpdateCurvePreview(); }) ]
                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,2.f,0.f,0.f))
                        [ SNew(SButton).ButtonStyle(&NMS::FlatButton(NMS::AccentDim, NMS::ItemHover, NMS::AccentDim)).HAlign(HAlign_Center).ContentPadding(FMargin(4.f,4.f))
                            .OnClicked_Lambda([this](){ BuildAlongCurve(); return FReply::Handled(); })
                            [ SNew(STextBlock).Text(FText::FromString(TEXT("Применить"))).Font(NMS::Font(8,true)).ColorAndOpacity(NMS::TextPrimary) ] ]
                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,2.f,0.f,0.f))
                        [ SNew(SButton).ButtonStyle(&NMS::FlatButton(NMS::ItemBg, NMS::ItemHover, NMS::AccentDim)).HAlign(HAlign_Center).ContentPadding(FMargin(4.f,3.f))
                            .OnClicked_Lambda([this](){ if (ViewportClient.IsValid()) ViewportClient->CurvePoints.Reset(); UpdateCurvePreview(); return FReply::Handled(); })
                            [ SNew(STextBlock).Text(FText::FromString(TEXT("Очистить"))).Font(NMS::Font(8)).ColorAndOpacity(NMS::TextSecondary) ] ]
                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,2.f,0.f,0.f))
                        [ SNew(SButton).ButtonStyle(&NMS::FlatButton(NMS::ItemBg, NMS::ItemHover, NMS::AccentDim)).HAlign(HAlign_Center).ContentPadding(FMargin(4.f,3.f))
                            .OnClicked_Lambda([this](){ if (ViewportClient.IsValid()) { ViewportClient->CurvePoints.Reset(); ViewportClient->bCurveMode = false; } UpdateCurvePreview(); return FReply::Handled(); })
                            [ SNew(STextBlock).Text(FText::FromString(TEXT("Выход"))).Font(NMS::Font(8)).ColorAndOpacity(NMS::TextSecondary) ] ]
                    ]
                ]
            ]
        ];
    }
    Bar->AddSlot().AutoWidth()[ GameBtn(TEXT("buildmenu__scale"), TEXT("HDR_SCALE"), TEXT("МАСШТАБ"),
        [SetMode]() { SetMode(ENMSTransformMode::Scale); }, [ModeIs]() { return ModeIs(ENMSTransformMode::Scale); }) ];

    // разделитель между группами
    Bar->AddSlot().AutoWidth().Padding(FMargin(14.f, 0.f))[ SNew(SBox).WidthOverride(1.f) ];

    // --- группа 2: режимы/камера/проводка ---
    Bar->AddSlot().AutoWidth()[ GameBtn(TEXT("buildmenu__snapping"), TEXT("MODE_FREE"), TEXT("СВОБОДНОЕ РАЗМЕЩЕНИЕ"),
        [this]() { if (ViewportClient.IsValid()) ViewportClient->SetMoveSnap((ViewportClient->GetMoveSnap() > 1.f) ? 1.f : 50.f); },
        [this]() { return ViewportClient.IsValid() && ViewportClient->GetMoveSnap() <= 1.f; }) ];
    Bar->AddSlot().AutoWidth()[ GameBtn(TEXT("buildmenu__move"), TEXT("MODE_EDIT"), TEXT("РЕДАКТИРОВАНИЕ"),
        [SetMode]() { SetMode(ENMSTransformMode::Move); }, [ModeIs]() { return ModeIs(ENMSTransformMode::Move); }) ];
    Bar->AddSlot().AutoWidth()[ GameBtn(TEXT("buildmenu__cam"), TEXT("HDR_CAMERA"), TEXT("КАМЕРА"),
        [this]() { if (ViewportClient.IsValid()) ViewportClient->ResetCamera(); },
        Never) ];
    Bar->AddSlot().AutoWidth()[ GameBtn(TEXT("buildmenu__wiring"), TEXT("MODE_WIRING"), TEXT("ПРОВОДКА"),
        [this]() { if (ViewportClient.IsValid()) ViewportClient->ToggleWiringMode(); },
        [this]() { return ViewportClient.IsValid() && ViewportClient->IsWiringMode(); }) ];

    // --- группа: отменить / повторить / зеркало ---
    Bar->AddSlot().AutoWidth()[ GameBtn(TEXT("buildmenu__undo"), TEXT("BTN_UNDO"), TEXT("ОТМЕНИТЬ"),
        [this]() { DoUndo(); }, Never) ];
    Bar->AddSlot().AutoWidth()[ GameBtn(TEXT("buildmenu__redo"), TEXT("BTN_REDO"), TEXT("ПОВТОРИТЬ"),
        [this]() { DoRedo(); }, Never) ];
    Bar->AddSlot().AutoWidth()[ GameBtn(TEXT("buildmenu__mirror"), TEXT("BTN_MIRROR"), TEXT("ЗЕРКАЛО"),
        [this]() { if (ViewportClient.IsValid()) ViewportClient->MirrorSelection(0); }, Never) ];

    // Полупрозрачная плашка по центру сверху — как панель постройки в игре.
    // Контейнер не перехватывает клики (SelfHitTestInvisible) — мимо плашки кликается сцена.
    return SNew(SBorder)
        .BorderImage(NMS::Box())
        .BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.f))
        .Visibility(EVisibility::SelfHitTestInvisible)
        .Padding(FMargin(8.f, 4.f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)
            [
                SNew(SBorder).BorderImage(NMS::Box())
                .BorderBackgroundColor(FLinearColor(0.05f, 0.055f, 0.06f, 0.45f)) // прозрачнее
                .Padding(FMargin(10.f, 6.f))
                [ Bar ]
            ]
        ];
}

// Ставит фон по номеру (PNG из Content/NMSData/UI/Backgrounds, с зацикливанием).
TSharedRef<SWidget> SNMSBuilderUI::BuildRightPanel()
{
    return SNew(SBorder)
        .BorderImage(NMS::Box())
        .BorderBackgroundColor(FLinearColor(0.05f, 0.055f, 0.06f, 0.45f)) // прозрачная, сцена видна сквозь
        .Padding(FMargin(0.f))
        [
            SNew(SScrollBox).Orientation(Orient_Vertical)

            // --- TRANSFORM ---
            + SScrollBox::Slot()[ MakeSectionHeader(LOCTEXT("Transform", "TRANSFORM")) ]
            + SScrollBox::Slot().Padding(FMargin(12, 8))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 3))
                [ MakeLiveTransformRow(0) ]   // Translate (живой)
                + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 3))
                [ MakeLiveTransformRow(1) ]   // Rotate
                + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 3))
                [ MakeLiveTransformRow(2) ]   // Scale
            ]

            // --- MATERIAL & COLOURS ---
            + SScrollBox::Slot()[ MakeSectionHeader(LOCTEXT("Material", "MATERIAL & COLOURS")) ]
            + SScrollBox::Slot().Padding(FMargin(12, 8))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [ BuildColorPalette() ]
            ]

            // --- ДЕРЕВО СЦЕНЫ (под палитрой красок) ---
            + SScrollBox::Slot()[ MakeSectionHeader(LOCTEXT("SceneTree", "\u0421\u0426\u0415\u041d\u0410 (\u0434\u0435\u0442\u0430\u043b\u0438)")) ]
            + SScrollBox::Slot()[ SNew(SBox).HeightOverride(260.f)[ BuildSceneTree() ] ]

                    ];
}

// ===========================================================================
//  Логика
// ===========================================================================
#undef LOCTEXT_NAMESPACE
