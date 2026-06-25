// Часть класса SNMSBuilderUI (разнесение по TU, Часть C плана).
// Включения общие для всех TU класса — см. SNMSBuilderUI_Internal.h.
#include "SNMSBuilderUI_Internal.h"

#define LOCTEXT_NAMESPACE "NMSBuilder"

AActor* SNMSBuilderUI::GetSelected() const
{
    return ViewportClient.IsValid() ? ViewportClient->GetSelectedActor() : nullptr;
}

float SNMSBuilderUI::GetTransformValue(int32 Kind, int32 Axis) const
{
    AActor* A = GetSelected();
    if (!A) return 0.f;
    if (Kind == 0) { const FVector L = A->GetActorLocation(); return (Axis==0?L.X:Axis==1?L.Y:L.Z); }
    if (Kind == 1) { const FRotator R = A->GetActorRotation(); return (Axis==0?R.Roll:Axis==1?R.Pitch:R.Yaw); }
    const FVector S = A->GetActorScale3D(); return (Axis==0?S.X:Axis==1?S.Y:S.Z);
}

void SNMSBuilderUI::SetTransformValue(int32 Kind, int32 Axis, float V)
{
    AActor* A = GetSelected();
    if (!A) return;
    if (Kind == 0) { FVector L=A->GetActorLocation(); (Axis==0?L.X:Axis==1?L.Y:L.Z)=V; A->SetActorLocation(L); }
    else if (Kind == 1) { FRotator R=A->GetActorRotation(); (Axis==0?R.Roll:Axis==1?R.Pitch:R.Yaw)=V; A->SetActorRotation(R); }
    else { FVector S=A->GetActorScale3D(); (Axis==0?S.X:Axis==1?S.Y:S.Z)=V; A->SetActorScale3D(S); }
}

TSharedRef<SWidget> SNMSBuilderUI::MakeLiveTransformRow(int32 Kind)
{
    // Полное название слева (как на эталоне), поля справа в ряд.
    const FText Label = (Kind==0)?FText::FromString(TEXT("Translate"))
                      : (Kind==1)?FText::FromString(TEXT("Rotate"))
                                 :FText::FromString(TEXT("Scale"));
    TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0,0,8,0))
        [ SNew(SBox).WidthOverride(58.f)
            [ SNew(STextBlock).Text(Label).Font(NMS::Font(8)).ColorAndOpacity(NMS::TextSecondary) ] ];

    // Scale (Kind==2) — ОДНО поле (единый масштаб, как в игре).
    // Translate/Rotate — 3 поля (X,Y,Z), каждое тянется на равную долю ширины.
    const int32 Count = (Kind==2) ? 1 : 3;
    for (int32 i=0;i<Count;++i)
    {
        const int32 Axis = i;
        Row->AddSlot().FillWidth(1.f).Padding(FMargin(0,0,3,0))
        [
            SNew(SNumericEntryBox<float>)
            .AllowSpin(false)
            .MinDesiredValueWidth(0.f)
            .MaxFractionalDigits(2)
            .Value_Lambda([this, Kind, Axis]() -> TOptional<float> {
                if (!GetSelected()) return TOptional<float>();
                return GetTransformValue(Kind, Kind==2 ? 0 : Axis);
            })
            .OnValueCommitted_Lambda([this, Kind, Axis](float NewV, ETextCommit::Type){
                if (Kind==2)
                {
                    if (AActor* A = GetSelected())
                        A->SetActorScale3D(FVector(NewV, NewV, NewV));
                }
                else
                    SetTransformValue(Kind, Axis, NewV);
            })
        ];
    }
    // у Scale одно поле — добавим пустые распорки, чтобы поле было такой же
    // ширины как одно из трёх (а не на всю строку).
    if (Kind == 2)
    {
        Row->AddSlot().FillWidth(2.f)[ SNew(SBox) ];
    }
    return Row;
}

FText SNMSBuilderUI::GetSelectedInfo(int32 Field) const
{
    AActor* A = GetSelected();
    if (!A)
    {
        if (Field==3) {
            int32 Cnt=0;
            UWorld* W = ViewportClient.IsValid() ? ViewportClient->GetWorld() : nullptr;
            if (W) for (TActorIterator<AStaticMeshActor> It(W); It; ++It)
                if (It->GetActorLabel().StartsWith(TEXT("NMS_"))) Cnt++;
            return FText::FromString(FString::FromInt(Cnt));
        }
        return FText::GetEmpty();
    }
    // имя детали из метки актёра (NMS_<ObjectID>)
    if (Field==0)
    {
        FString N = A->GetActorLabel();
        N.RemoveFromStart(TEXT("NMS_"));
        return FText::FromString(N);
    }
    return FText::GetEmpty();
}

// =================== ДЕРЕВО СЦЕНЫ (как в Blender) ===================
void SNMSBuilderUI::ToggleActorHidden(AActor* A)
{
    if (!A) return;
    A->SetActorHiddenInGame(!A->IsHidden());
#if WITH_EDITOR
    A->SetIsTemporarilyHiddenInEditor(!A->IsTemporarilyHiddenInEditor());
#endif
}

void SNMSBuilderUI::ToggleActorRenderOff(AActor* A)
{
    if (!A) return;
    const FString Key = A->GetActorNameOrLabel();
    bool Cur = ActorRenderOff.Contains(Key) ? ActorRenderOff[Key] : false;
    Cur = !Cur;
    ActorRenderOff.Add(Key, Cur);
    // "отключить на рендере" — прячем меш-компоненту (видимость рендера)
    if (UStaticMeshComponent* C = A->FindComponentByClass<UStaticMeshComponent>())
        C->SetVisibility(!Cur);
}

void SNMSBuilderUI::RebuildSceneTree()
{
    SceneTreeItems.Reset();
    UWorld* World = ViewportClient.IsValid() ? ViewportClient->GetWorld() : nullptr;
    if (World)
    {
        for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
        {
            AStaticMeshActor* A = *It;
            if (!A) continue;
            FString Label = A->GetActorNameOrLabel();
            if (!Label.StartsWith(TEXT("NMS_"))) continue;
            FString Name = Label; Name.RemoveFromStart(TEXT("NMS_"));
            TSharedPtr<FSceneItem> Item = MakeShared<FSceneItem>();
            Item->Actor = A; Item->Name = Name;
            SceneTreeItems.Add(Item);
        }
    }
    LastSceneItemCount = SceneTreeItems.Num();
    if (SceneListView.IsValid()) SceneListView->RequestListRefresh();
}

void SNMSBuilderUI::SceneTree_OnSelectionChanged(TSharedPtr<FSceneItem> Item, ESelectInfo::Type)
{
    if (Item.IsValid() && Item->Actor.IsValid() && ViewportClient.IsValid())
        ViewportClient->SetSelectedActor(Item->Actor.Get());
}

TSharedRef<ITableRow> SNMSBuilderUI::SceneTree_OnGenerateRow(
    TSharedPtr<FSceneItem> Item, const TSharedRef<STableViewBase>& Owner)
{
    AActor* ActorPtr = Item.IsValid() ? Item->Actor.Get() : nullptr;
    const FString Name = Item.IsValid() ? Item->Name : FString();

    return SNew(STableRow<TSharedPtr<FSceneItem>>, Owner)
        .Style(&FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
    [
        SNew(SHorizontalBox)
        // имя
        + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(4,0,0,0))
        [ SNew(STextBlock).Text(FText::FromString(Name))
            .Font(NMS::Font(8)).ColorAndOpacity(NMS::TextPrimary) ]
        // глаз — скрыть во вьюпорте
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(2,0))
        [
            SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder")
            .ToolTipText(FText::FromString(TEXT("Скрыть во вьюпорте")))
            .OnClicked_Lambda([this, ActorPtr]() { ToggleActorHidden(ActorPtr); return FReply::Handled(); })
            .Content()
            [ SNew(SBox).WidthOverride(18.f).HeightOverride(18.f)
                [ SNew(SImage).Image(GetUIIcon(TEXT("tree_eye"), FVector2D(18,18)))
                    .ColorAndOpacity_Lambda([ActorPtr]() {
                        bool H = ActorPtr ? ActorPtr->IsHidden() : false;
                        return H ? FSlateColor(FLinearColor(0.4f,0.4f,0.4f,0.5f))
                                 : FSlateColor(FLinearColor::White); }) ] ]
        ]
        // камера — отключить рендер
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(2,0,4,0))
        [
            SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder")
            .ToolTipText(FText::FromString(TEXT("Отключить на рендере")))
            .OnClicked_Lambda([this, ActorPtr]() { ToggleActorRenderOff(ActorPtr); return FReply::Handled(); })
            .Content()
            [ SNew(SBox).WidthOverride(18.f).HeightOverride(18.f)
                [ SNew(SImage).Image(GetUIIcon(TEXT("tree_cam"), FVector2D(18,18)))
                    .ColorAndOpacity_Lambda([this, ActorPtr]() {
                        FString K = ActorPtr ? ActorPtr->GetActorNameOrLabel() : FString();
                        bool Off = ActorRenderOff.Contains(K) ? ActorRenderOff[K] : false;
                        return Off ? FSlateColor(FLinearColor(0.4f,0.4f,0.4f,0.5f))
                                   : FSlateColor(FLinearColor::White); }) ] ]
        ]
    ];
}

TSharedRef<SWidget> SNMSBuilderUI::BuildSceneTree()
{
    // Прозрачный фон списка: берём стиль ListView и заменяем фоновые браши на пустые.
    static FTableViewStyle TransparentStyle = FCoreStyle::Get().GetWidgetStyle<FTableViewStyle>("ListView");
    TransparentStyle.BackgroundBrush = *FCoreStyle::Get().GetBrush("NoBrush");

    return SAssignNew(SceneListView, SListView<TSharedPtr<FSceneItem>>)
        .ListItemsSource(&SceneTreeItems)
        .OnGenerateRow(this, &SNMSBuilderUI::SceneTree_OnGenerateRow)
        .OnSelectionChanged(this, &SNMSBuilderUI::SceneTree_OnSelectionChanged)
        .SelectionMode(ESelectionMode::Single)
        .ListViewStyle(&TransparentStyle);
}
#undef LOCTEXT_NAMESPACE
