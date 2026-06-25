// Часть класса SNMSBuilderUI (разнесение по TU, Часть C плана).
// Включения общие для всех TU класса — см. SNMSBuilderUI_Internal.h.
#include "SNMSBuilderUI_Internal.h"

#define LOCTEXT_NAMESPACE "NMSBuilder"

// Читает настоящие игровые палитры (Content/NMSData/palettes.json, вытащено из игры).
void SNMSBuilderUI::LoadPalettes()
{
    PaletteRows.Empty();
    PaletteMaterials.Empty();
    PaletteFinishes.Empty();
    {
        FString FRaw;
        if (FFileHelper::LoadFileToString(FRaw, *(FPaths::ProjectContentDir() / TEXT("NMSData/finishes.json"))))
        {
            TSharedPtr<FJsonObject> FRoot;
            const TSharedRef<TJsonReader<>> FR = TJsonReaderFactory<>::Create(FRaw);
            if (FJsonSerializer::Deserialize(FR, FRoot) && FRoot.IsValid())
                for (const auto& KV : FRoot->Values)
                {
                    const TArray<TSharedPtr<FJsonValue>>* Arr;
                    if (!KV.Value->TryGetArray(Arr)) continue;
                    TArray<TPair<int32, FString>> List;
                    for (const auto& E : *Arr)
                    {
                        const TSharedPtr<FJsonObject>* O;
                        if (!E->TryGetObject(O)) continue;
                        const int32 Idx = (int32)(*O)->GetNumberField(TEXT("idx"));
                        FString Nm; (*O)->TryGetStringField(TEXT("name"), Nm);
                        List.Add(TPair<int32, FString>(Idx, Nm));
                    }
                    PaletteFinishes.Add(FString(KV.Key), List);
                }
        }
    }
    ColourNames.Empty();
    {
        FString CRaw;
        if (FFileHelper::LoadFileToString(CRaw, *(FPaths::ProjectContentDir() / TEXT("NMSData/colour_names.json"))))
        {
            TSharedPtr<FJsonObject> CRoot;
            const TSharedRef<TJsonReader<>> CR = TJsonReaderFactory<>::Create(CRaw);
            if (FJsonSerializer::Deserialize(CR, CRoot) && CRoot.IsValid())
                for (const auto& KV : CRoot->Values)
                {
                    const TSharedPtr<FJsonObject>* O;
                    if (!KV.Value->TryGetObject(O)) continue;
                    FString Nm; if (!(*O)->TryGetStringField(TEXT("ru"), Nm) || Nm.IsEmpty()) (*O)->TryGetStringField(TEXT("en"), Nm);
                    if (!Nm.IsEmpty()) ColourNames.Add(FCString::Atoi(*FString(KV.Key)), Nm);
                }
        }
    }

    FString Raw;
    const FString Path = FPaths::ProjectContentDir() / TEXT("NMSData/palettes.json");
    if (!FFileHelper::LoadFileToString(Raw, *Path)) return;

    TArray<TSharedPtr<FJsonValue>> Arr;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(Reader, Arr)) return;

    TSet<FString> SeenMaterials;
    for (const TSharedPtr<FJsonValue>& V : Arr)
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (!V.IsValid() || !V->TryGetObject(Obj)) continue;

        TSharedPtr<FNMSPaletteRow> Row = MakeShared<FNMSPaletteRow>();
        (*Obj)->TryGetStringField(TEXT("Material"),  Row->Material);
        (*Obj)->TryGetStringField(TEXT("TextLabel"), Row->Label);
        FString P, S;
        (*Obj)->TryGetStringField(TEXT("PrimaryColour"),   P);
        (*Obj)->TryGetStringField(TEXT("SecondaryColour"), S);
        Row->Primary.InitFromString(P);     // формат "(R=..,G=..,B=..,A=..)"
        Row->Secondary.InitFromString(S);
        { FString IdxS; if ((*Obj)->TryGetStringField(TEXT("Index"), IdxS)) Row->ColourIndex = FCString::Atoi(*IdxS); }
        PaletteRows.Add(Row);

        if (!SeenMaterials.Contains(Row->Material))
        {
            SeenMaterials.Add(Row->Material);
            PaletteMaterials.Add(MakeShared<FString>(Row->Material));
        }
    }
    if (PaletteMaterials.Num() > 0) CurrentPaletteMaterial = PaletteMaterials[0];
}

// Сетка цветов выбранного материала (как страница материала в игре).
void SNMSBuilderUI::RebuildSwatches()
{
    if (!SwatchContainer.IsValid()) return;

    const int32 Cols = 4;
    TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(2.f));
    int32 i = 0;
    for (const TSharedPtr<FNMSPaletteRow>& Row : PaletteRows)
    {
        if (!CurrentPaletteMaterial.IsValid() || Row->Material != *CurrentPaletteMaterial) continue;
        const FLinearColor Pri = Row->Primary;
        const FLinearColor Sec = Row->Secondary;
        Grid->AddSlot(i % Cols, i / Cols)
        [
            SNew(SButton)
            // плоский стиль без рамки движка — никакого чёрного ореола
            .ButtonStyle(&NMS::FlatButton(FLinearColor(0.f,0.f,0.f,0.f),
                FLinearColor(1.f,1.f,1.f,0.35f), NMS::AccentDim))
            .ContentPadding(FMargin(0.f))
            .ToolTipText(FText::FromString(NMS_ColName(Row->ColourIndex, ColourNames.Contains(Row->ColourIndex) ? ColourNames[Row->ColourIndex] : Row->Label)))
            .OnClicked_Lambda([this, Pri, Sec, Idx=Row->ColourIndex]()
            {
                OnColorClicked(Pri, Idx);
                // вторичный цвет пары — на зону металла/кантов
                if (AActor* A = ViewportClient.IsValid() ? ViewportClient->GetSelectedActor() : nullptr)
                    if (UStaticMeshComponent* C = A->FindComponentByClass<UStaticMeshComponent>())
                        if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0)))
                        {
                            FLinearColor Ter = Sec, Qua = Sec; NMS_PalTQ(Idx, Ter, Qua);
                            MID->SetVectorParameterValue(TEXT("Color2"), Sec);
                            MID->SetVectorParameterValue(TEXT("ZoneCol1"), Sec);  // S -> зона 1
                            MID->SetVectorParameterValue(TEXT("ZoneCol2"), Ter);  // T -> зона 2
                            MID->SetVectorParameterValue(TEXT("ZoneCol3"), Qua);  // Q -> зона 3
                        }
                return FReply::Handled();
            })
            [
                // плашка: сверху основной цвет, снизу полоска вторичного — как в игре
                SNew(SVerticalBox)
                + SVerticalBox::Slot().FillHeight(1.f)
                [
                    SNew(SBorder).BorderImage(NMS::Box()).BorderBackgroundColor(Pri)
                    [ SNew(SBox).HeightOverride(13.f) ]
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SBorder).BorderImage(NMS::Box()).BorderBackgroundColor(Sec)
                    [ SNew(SBox).HeightOverride(4.f) ]
                ]
            ]
        ];
        ++i;
    }
    SwatchContainer->SetContent(Grid);
}

void SNMSBuilderUI::SetMaterialIndex(int32 MatIdx)
{
    CurrentMaterialIndex = MatIdx;
    if (ViewportClient.IsValid())
        if (AActor* A = ViewportClient->GetSelectedActor())
        {
            int64 UD = 0; int32 udPos = -1;
            for (int32 ti = 0; ti < A->Tags.Num(); ++ti)
            {
                FString S = A->Tags[ti].ToString();
                if (S.RemoveFromStart(TEXT("UD:"))) { UD = FCString::Atoi64(*S); udPos = ti; break; }
            }
            uint32 U = (uint32)UD;
            U = (U & ~(0xFFu << 24)) | (((uint32)MatIdx & 0xFFu) << 24);
            const FName NewTag(*FString::Printf(TEXT("UD:%lld"), (long long)(int64)U));
            if (udPos >= 0) A->Tags[udPos] = NewTag; else A->Tags.Add(NewTag);
        }
    RebuildSwatches();
}

void SNMSBuilderUI::OnColorClicked(FLinearColor Color, int32 ColourIndex)
{
    if (!ViewportClient.IsValid()) return;
    AActor* A = ViewportClient->GetSelectedActor();
    if (!A) { UE_LOG(LogTemp, Log, TEXT("NMS: нет выбранной детали для покраски")); return; }
    // записать выбранный цвет в UserData детали (тег "UD:<число>") — чтобы покраска ушла в сейв
    if (ColourIndex >= 0)
    {
        int64 UD = 0; int32 udPos = -1;
        for (int32 ti = 0; ti < A->Tags.Num(); ++ti)
        {
            FString S = A->Tags[ti].ToString();
            if (S.RemoveFromStart(TEXT("UD:"))) { UD = FCString::Atoi64(*S); udPos = ti; break; }
        }
        const uint32 COLMASK = 0xFCFEFFu; // 24 бита минус зарезервированные 8/16/17
        uint32 U = (uint32)UD;
        U = (U & ~COLMASK) | ((uint32)ColourIndex & COLMASK);                       // цвет
        U = (U & ~(0xFFu << 24)) | (((uint32)CurrentMaterialIndex & 0xFFu) << 24);  // отделка
        UD = (int64)U;
        const FName NewTag(*FString::Printf(TEXT("UD:%lld"), (long long)UD));
        if (udPos >= 0) A->Tags[udPos] = NewTag; else A->Tags.Add(NewTag);
    }
    UStaticMeshComponent* Comp = A->FindComponentByClass<UStaticMeshComponent>();
    if (!Comp) return;

    // Если на детали уже наш динамический материал (текстурный или плоский) —
    // просто меняем параметр Color: текстуры и вид НЕ слетают.
    if (UMaterialInstanceDynamic* Existing =
        Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0)))
    {
        Existing->SetVectorParameterValue(TEXT("Color"), Color);
        Existing->SetVectorParameterValue(TEXT("ZoneCol0"), Color);  // основной -> зона 0
        CommitEdit();
        return;
    }

    // Иначе (деталь ещё без нашего материала) — плоский цветной.
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_BasePart.M_BasePart"));
    if (!Base)
    {
        UE_LOG(LogTemp, Warning, TEXT("NMS: M_BasePart не найден — запусти Content/Python/create_base_material.py"));
        return;
    }
    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Comp);
    MID->SetVectorParameterValue(TEXT("Color"), Color);
    const int32 N = Comp->GetNumMaterials();
    for (int32 i = 0; i < N; ++i) Comp->SetMaterial(i, MID);
    CommitEdit();
}

// ===========================================================================
//  Construct
// ===========================================================================
#undef LOCTEXT_NAMESPACE
