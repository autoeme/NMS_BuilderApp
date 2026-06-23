#include "SNMSBuilderUI.h"
#include "NMSSaveFile.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SWindow.h"

#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "NMSBaseManager.h"
#include "Editor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SComboButton.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "UObject/ConstructorHelpers.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "NMSModelImporter.h"
#include "Framework/Application/SlateApplication.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SViewport.h"
#include "Slate/SceneViewport.h"
#include "NMSViewportClient.h"
#include "Widgets/Views/STileView.h"
#include "Widgets/Views/STableRow.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateColorBrush.h"

#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"   // список PNG-фонов (кнопка ФОН)
#include "HAL/PlatformApplicationMisc.h" // буфер обмена (импорт/экспорт базы)
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

#define LOCTEXT_NAMESPACE "NMSBuilder"

// ===================== ЛОКАЛИЗАЦИЯ (17 языков) =====================
static bool GNmsI18nLoaded = false;
static FString GNmsLang;
static TArray<TPair<FString,FString>> GNmsLangs;
static TSharedPtr<FJsonObject> GNmsUi;
static TSharedPtr<FJsonObject> GNmsParts;
static TMap<int32, TSharedPtr<FJsonObject>> GNmsCols;
static TMap<int32, FLinearColor> GNmsPalT;
static TMap<int32, FLinearColor> GNmsPalQ;

static TSharedPtr<FJsonObject> NMS_ReadJsonObj(const FString& Full)
{
    FString S; if (!FFileHelper::LoadFileToString(S, *Full)) return nullptr;
    TSharedPtr<FJsonObject> O;
    TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(S);
    FJsonSerializer::Deserialize(R, O);
    return O;
}
static void NMS_LoadI18n()
{
    if (GNmsI18nLoaded) return;
    GNmsI18nLoaded = true;
    const FString Dir = FPaths::ProjectContentDir() / TEXT("NMSData/");
    {
        FString S;
        if (FFileHelper::LoadFileToString(S, *(Dir + TEXT("languages.json"))))
        {
            TArray<TSharedPtr<FJsonValue>> Arr;
            TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(S);
            if (FJsonSerializer::Deserialize(R, Arr))
                for (const TSharedPtr<FJsonValue>& V : Arr)
                {
                    const TSharedPtr<FJsonObject> O = V->AsObject();
                    if (O.IsValid())
                        GNmsLangs.Add(TPair<FString,FString>(O->GetStringField(TEXT("code")), O->GetStringField(TEXT("native"))));
                }
        }
    }
    GNmsUi    = NMS_ReadJsonObj(Dir + TEXT("ui_i18n.json"));
    GNmsParts = NMS_ReadJsonObj(Dir + TEXT("partnames_i18n.json"));
    {
        FString S;
        if (FFileHelper::LoadFileToString(S, *(Dir + TEXT("colour_names_i18n.json"))))
        {
            TArray<TSharedPtr<FJsonValue>> Arr;
            TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(S);
            if (FJsonSerializer::Deserialize(R, Arr))
                for (const TSharedPtr<FJsonValue>& V : Arr)
                {
                    const TSharedPtr<FJsonObject> O = V->AsObject();
                    if (!O.IsValid()) continue;
                    int32 Idx = (int32)O->GetNumberField(TEXT("index"));
                    const TSharedPtr<FJsonObject>* Names;
                    if (O->TryGetObjectField(TEXT("names"), Names))
                        GNmsCols.Add(Idx, *Names);
                }
        }
    }
    {
        FString S;
        if (FFileHelper::LoadFileToString(S, *(Dir + TEXT("userdata_palette.json"))))
        {
            TSharedPtr<FJsonObject> O;
            TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(S);
            if (FJsonSerializer::Deserialize(R, O) && O.IsValid())
                for (const auto& Pair : O->Values)
                {
                    const int32 Idx = FCString::Atoi(*Pair.Key);
                    const TSharedPtr<FJsonObject> E = Pair.Value->AsObject();
                    if (!E.IsValid()) continue;
                    const TArray<TSharedPtr<FJsonValue>>* Arr;
                    if (E->TryGetArrayField(TEXT("T"), Arr) && Arr->Num() >= 3)
                        GNmsPalT.Add(Idx, FLinearColor((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber(), 1.f));
                    if (E->TryGetArrayField(TEXT("Q"), Arr) && Arr->Num() >= 3)
                        GNmsPalQ.Add(Idx, FLinearColor((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber(), 1.f));
                }
        }
    }
    FString Saved;
    if (FFileHelper::LoadFileToString(Saved, *(FPaths::ProjectSavedDir() / TEXT("NMSUser/lang.txt"))))
        GNmsLang = Saved.TrimStartAndEnd();
    if (GNmsLang.IsEmpty()) GNmsLang = TEXT("russian");
}
static FString NMS_Pick(const TSharedPtr<FJsonObject>& O, const FString& Fallback)
{
    if (!O.IsValid()) return Fallback;
    FString V;
    if (O->TryGetStringField(GNmsLang, V) && !V.IsEmpty()) return V;
    if (GNmsLang == TEXT("tencentchinese") && O->TryGetStringField(TEXT("simplifiedchinese"), V) && !V.IsEmpty()) return V;
    if (O->TryGetStringField(TEXT("english"), V) && !V.IsEmpty()) return V;
    return Fallback;
}
static FText NMS_T(const FString& Key, const FString& Fallback)
{
    NMS_LoadI18n();
    if (GNmsUi.IsValid())
    {
        const TSharedPtr<FJsonObject>* Sub;
        if (GNmsUi->TryGetObjectField(Key, Sub))
            return FText::FromString(NMS_Pick(*Sub, Fallback));
    }
    return FText::FromString(Fallback);
}
static FString NMS_PartName(const FString& Oid, const FString& Fallback)
{
    NMS_LoadI18n();
    if (GNmsParts.IsValid())
    {
        const TSharedPtr<FJsonObject>* Sub;
        if (GNmsParts->TryGetObjectField(Oid, Sub))
            return NMS_Pick(*Sub, Fallback);
    }
    return Fallback;
}
static FString NMS_ColName(int32 Idx, const FString& Fallback)
{
    NMS_LoadI18n();
    if (const TSharedPtr<FJsonObject>* O = GNmsCols.Find(Idx))
        return NMS_Pick(*O, Fallback);
    return Fallback;
}
static void NMS_PalTQ(int32 Idx, FLinearColor& OutT, FLinearColor& OutQ)
{
    NMS_LoadI18n();
    if (const FLinearColor* C = GNmsPalT.Find(Idx)) OutT = *C;
    if (const FLinearColor* C = GNmsPalQ.Find(Idx)) OutQ = *C;
}
static void NMS_SetLang(const FString& Code)
{
    NMS_LoadI18n();
    GNmsLang = Code;
    const FString P = FPaths::ProjectSavedDir() / TEXT("NMSUser");
    IFileManager::Get().MakeDirectory(*P, true);
    FFileHelper::SaveStringToFile(Code, *(P / TEXT("lang.txt")));
}
// ===================================================================

// ===========================================================================
//  Цветовая палитра — как меню игры NMS: дымчатый тёмный фон без синевы,
//  полупрозрачные белёсые строки, белый текст.
// ===========================================================================
namespace NMS
{
    static const FLinearColor WindowBg     = FLinearColor(FColor(0x10, 0x14, 0x18));
    static const FLinearColor PanelBg       = FLinearColor(0.07f, 0.08f, 0.09f, 0.80f);
    static const FLinearColor PanelHeader   = FLinearColor(1.f, 1.f, 1.f, 0.14f);  // светлая строка, как в меню игры
    static const FLinearColor InsetBg       = FLinearColor(0.f, 0.f, 0.f, 0.35f);
    static const FLinearColor ItemBg        = FLinearColor(1.f, 1.f, 1.f, 0.08f);
    static const FLinearColor ItemHover     = FLinearColor(1.f, 1.f, 1.f, 0.22f);
    static const FLinearColor CardBg        = FLinearColor(1.f, 1.f, 1.f, 0.10f);
    static const FLinearColor TabActive     = FLinearColor(1.f, 1.f, 1.f, 0.25f);
    static const FLinearColor TabInactive   = FLinearColor(1.f, 1.f, 1.f, 0.06f);
    static const FLinearColor Accent        = FLinearColor(FColor(0xF0, 0xF2, 0xF4)); // белый акцент, как в игре
    static const FLinearColor AccentDim     = FLinearColor(1.f, 1.f, 1.f, 0.30f);
    static const FLinearColor ViewportBlue  = FLinearColor(FColor(0x3A, 0x40, 0x46));
    static const FLinearColor Border        = FLinearColor(1.f, 1.f, 1.f, 0.18f);
    static const FLinearColor TextPrimary   = FLinearColor(FColor(0xF2, 0xF4, 0xF6));
    static const FLinearColor TextSecondary = FLinearColor(FColor(0xA8, 0xAE, 0xB4));
    static const FLinearColor TextCategory  = FLinearColor(FColor(0xD2, 0xD6, 0xDA));
    static const FLinearColor Gold          = FLinearColor(FColor(0xE8, 0xC7, 0x5A));

    static const FSlateBrush* Box()
    {
        return FCoreStyle::Get().GetBrush("GenericWhiteBox");
    }

    static FSlateFontInfo Font(int32 Size, bool bBold = false)
    {
        return FCoreStyle::GetDefaultFontStyle("Bold", Size); // весь текст жирнее для чёткости
    }

    // Плоский стиль кнопки с заданными цветами (без штатной рамки движка).
    static const FButtonStyle& FlatButton(const FLinearColor& Normal,
                                          const FLinearColor& Hover,
                                          const FLinearColor& Pressed)
    {
        static TArray<TSharedPtr<FButtonStyle>> Cache;
        TSharedPtr<FButtonStyle> S = MakeShared<FButtonStyle>();
        S->SetNormal(FSlateColorBrush(Normal));
        S->SetHovered(FSlateColorBrush(Hover));
        S->SetPressed(FSlateColorBrush(Pressed));
        S->SetNormalPadding(FMargin(0));
        S->SetPressedPadding(FMargin(0));
        Cache.Add(S);
        return *Cache.Last();
    }

    static TSharedRef<SWidget> Fill(const FLinearColor& Color)
    {
        return SNew(SBorder).BorderImage(Box()).BorderBackgroundColor(Color);
    }
}

// ---- мелкие строительные хелперы (без состояния) --------------------------

// Родной цвет детали по категории (определена ниже, перед OnPartSelected).
static FLinearColor NMS_DefaultPartColor(const FString& Category, const FString& ObjectID);

// Точные дефолтные пары цветов из таблицы игры (part_colors.json, 1101 деталь).
static bool NMS_GamePartColors(const FString& ObjectID, FLinearColor& P, FLinearColor& S)
{
    static TMap<FString, TPair<FLinearColor, FLinearColor>> Map;
    static bool bLoaded = false;
    if (!bLoaded)
    {
        bLoaded = true;
        FString Raw;
        if (FFileHelper::LoadFileToString(Raw,
            *(FPaths::ProjectContentDir() / TEXT("NMSData/part_colors.json"))))
        {
            TSharedPtr<FJsonObject> Root;
            const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Raw);
            if (FJsonSerializer::Deserialize(R, Root) && Root.IsValid())
                for (const auto& KV : Root->Values)
                {
                    const TSharedPtr<FJsonObject>* O;
                    if (!KV.Value->TryGetObject(O)) continue;
                    auto Col = [&](const TCHAR* K) {
                        const TArray<TSharedPtr<FJsonValue>>* A;
                        if ((*O)->TryGetArrayField(K, A) && A->Num() >= 3)
                            return FLinearColor((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
                        return FLinearColor::White;
                    };
                    Map.Add(FString(KV.Key), TPair<FLinearColor, FLinearColor>(Col(TEXT("p")), Col(TEXT("s"))));
                }
        }
    }
    FString Id = ObjectID; Id.RemoveFromStart(TEXT("^"));
    if (const auto* Hit = Map.Find(Id)) { P = Hit->Key; S = Hit->Value; return true; }
    return false;
}

// Вторичный цвет (металл/канты) — оригинал игры из карты дефолтов.
static FLinearColor NMS_DefaultPartColor2(const FString& Category, const FString& ObjectID)
{
    FLinearColor P, S;
    if (NMS_GamePartColors(ObjectID, P, S)) return S;
    // Нет игрового вторичного (бетон C_/металл M_ и пр. не в таблице) ->
    // вторичный = основной: деталь РАВНОМЕРНАЯ как в игре, без раздела пополам.
    // (раньше возвращался White -> зоны 1,3 белели -> стена дробилась серый/белый).
    return NMS_DefaultPartColor(Category, ObjectID);
}

// Точные игровые текстуры детали — из карты part_textures.json
// (деталь -> её настоящий материал из файлов игры). Пусто = плоский цвет.
struct FNMSTexSet { FString Tex, Mask, Norm, Masks, Occ; bool bUnlit = false; };

static void NMS_PartTexture(const FString& Category, const FString& ObjectID,
                            FString& OutTex, FString& OutMask, FString& OutNorm,
                            FString& OutMasks, FString& OutOcc)
{
    OutTex.Empty(); OutMask.Empty(); OutNorm.Empty(); OutMasks.Empty(); OutOcc.Empty();

    // карта грузится один раз: ID -> (диффуз, маска покраски, рельеф)
    static TMap<FString, FNMSTexSet> Map;
    static bool bLoaded = false;
    if (!bLoaded)
    {
        bLoaded = true;
        FString Raw;
        const FString Path = FPaths::ProjectContentDir() / TEXT("NMSData/part_textures.json");
        if (FFileHelper::LoadFileToString(Raw, *Path))
        {
            TSharedPtr<FJsonObject> Root;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
            if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
            {
                for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : Root->Values)
                {
                    const TSharedPtr<FJsonObject>* O = nullptr;
                    if (!KV.Value->TryGetObject(O)) continue;
                    FNMSTexSet S;
                    (*O)->TryGetStringField(TEXT("tex"), S.Tex);
                    (*O)->TryGetStringField(TEXT("mask"), S.Mask);
                    (*O)->TryGetStringField(TEXT("norm"), S.Norm);
                    (*O)->TryGetStringField(TEXT("masks"), S.Masks);
                    (*O)->TryGetStringField(TEXT("occ"), S.Occ);
                    Map.Add(FString(KV.Key), S);
                }
            }
        }
        UE_LOG(LogTemp, Log, TEXT("NMS: карта текстур деталей: %d"), Map.Num());
    }

    FString Id = ObjectID; Id.RemoveFromStart(TEXT("^"));
    if (const FNMSTexSet* Hit = Map.Find(Id))
    {
        auto Path = [](const FString& N)
            { return FString::Printf(TEXT("/Game/NMSBaseBuilder/PartTex2/T_%s.T_%s"), *N, *N); };
        OutTex = Path(Hit->Tex);
        if (!Hit->Mask.IsEmpty()) OutMask = Path(Hit->Mask);
        if (!Hit->Norm.IsEmpty()) OutNorm = Path(Hit->Norm);
        // карта масок (затенение/шероховатость/металл) — для всех, у кого есть
        if (!Hit->Masks.IsEmpty())
            OutMasks = Path(Hit->Masks);
        // карта затенения швов (пока есть только у камня)
        if (!Hit->Occ.IsEmpty())
            OutOcc = Path(Hit->Occ);
    }
}

// Текстуры ПО СЛОТАМ (пункт 4): каждый материал-узел детали -> своя текстура.
// Убирает «кашу» (раньше один атлас натягивался на весь меш). part_slots.json:
// ID -> [ {tex,masks,norm,cmask}, ... ] в порядке материал-слотов меша.
static const TArray<FNMSTexSet>* NMS_PartSlots(const FString& ObjectID)
{
    static TMap<FString, TArray<FNMSTexSet>> Map;
    static bool bLoaded = false;
    if (!bLoaded)
    {
        bLoaded = true;
        FString Raw;
        const FString Path = FPaths::ProjectContentDir() / TEXT("NMSData/part_slots.json");
        if (FFileHelper::LoadFileToString(Raw, *Path))
        {
            TSharedPtr<FJsonObject> Root;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
            if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
            {
                for (const TPair<FString, TSharedPtr<FJsonValue>>& KV : Root->Values)
                {
                    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
                    if (!KV.Value->TryGetArray(Arr)) continue;
                    TArray<FNMSTexSet> Slots;
                    for (const TSharedPtr<FJsonValue>& V : *Arr)
                    {
                        const TSharedPtr<FJsonObject>* O = nullptr;
                        if (!V->TryGetObject(O)) continue;
                        FNMSTexSet S;
                        (*O)->TryGetStringField(TEXT("tex"),   S.Tex);
                        (*O)->TryGetStringField(TEXT("cmask"), S.Mask);   // покрасочная маска
                        (*O)->TryGetStringField(TEXT("norm"),  S.Norm);
                        (*O)->TryGetStringField(TEXT("masks"), S.Masks);
                        FString Type;
                        (*O)->TryGetStringField(TEXT("type"), Type);
                        S.bUnlit = (Type == TEXT("unlit"));   // эмиссив/лампы из игры (_F07_UNLIT)
                        Slots.Add(S);
                    }
                    if (Slots.Num() > 0) Map.Add(FString(KV.Key), Slots);
                }
            }
        }
        UE_LOG(LogTemp, Log, TEXT("NMS: карта слотов деталей: %d"), Map.Num());
    }
    FString Id = ObjectID; Id.RemoveFromStart(TEXT("^"));
    return Map.Find(Id);
}

static TSharedRef<SWidget> MakeSectionHeader(const FText& Title)
{
    // строка-секция как в меню игры: светлая полупрозрачная полоса, текст слева
    return SNew(SBorder)
        .BorderImage(NMS::Box())
        .BorderBackgroundColor(NMS::PanelHeader)
        .Padding(FMargin(10.f, 6.f))
        .HAlign(HAlign_Left)
        [
            SNew(STextBlock)
            .Text(Title)
            .Font(NMS::Font(10, true))
            .ColorAndOpacity(NMS::TextPrimary)
        ];
}

static TSharedRef<SWidget> MakeField(const FText& Value)
{
    return SNew(SBox).WidthOverride(34.f)
    [
        SNew(SBorder)
        .BorderImage(NMS::Box())
        .BorderBackgroundColor(NMS::InsetBg)
        .Padding(FMargin(6.f, 4.f))
        [
            SNew(STextBlock).Text(Value)
            .Font(NMS::Font(9)).ColorAndOpacity(NMS::TextSecondary)
        ]
    ];
}

static TSharedRef<SWidget> MakeTransformRow(const FText& Label, bool bSingle)
{
    TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 4, 0))
        [
            SNew(SBox).WidthOverride(46.f)
            [
                SNew(STextBlock).Text(Label)
                .Font(NMS::Font(8)).ColorAndOpacity(NMS::TextSecondary)
            ]
        ];

    const int32 Count = bSingle ? 1 : 3;
    for (int32 i = 0; i < Count; ++i)
    {
        Row->AddSlot().AutoWidth().Padding(FMargin(0, 0, 4, 0))
        [
            MakeField(bSingle ? FText::FromString(TEXT("1.0")) : FText::FromString(TEXT("0.0")))
        ];
    }
    return Row;
}

static TSharedRef<SWidget> MakePropertyRow(const FText& Label, const FText& Value)
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
        [
            SNew(STextBlock).Text(Label)
            .Font(NMS::Font(9)).ColorAndOpacity(NMS::TextSecondary)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock).Text(Value)
            .Font(NMS::Font(9)).ColorAndOpacity(NMS::TextPrimary)
        ];
}

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

    NMS_LoadI18n();
    TSharedRef<SVerticalBox> LangBox = SNew(SVerticalBox);
    for (const TPair<FString,FString>& Lg : GNmsLangs)
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

void SNMSBuilderUI::OnMenuNewBase()
{
    // как в плагине: чистая сцена + сброс свойств базы
    ClearSceneParts();
    CurrentBaseName = TEXT("Untitled Base");
    CurrentGalacticAddress.Empty();
    UE_LOG(LogTemp, Log, TEXT("NMS UI: New Base"));
}

void SNMSBuilderUI::OnMenuOpen()
{
    // Open Base... — выбрать файл базы (.json/.nmsbase/.hg/.blend) и загрузить, как в Blender.
    IDesktopPlatform* DP = FDesktopPlatformModule::Get();
    if (!DP) { OnMenuImportFromNMS(); return; }
    TArray<FString> Picked;
    const bool bPicked = DP->OpenFileDialog(
        nullptr,
        TEXT("Открыть базу"),
        TEXT("C:/Users/User/Documents/bases"),
        TEXT(""),
        TEXT("Базы NMS (*.json;*.nmsbase;*.hg;*.blend)|*.json;*.nmsbase;*.hg;*.blend"),
        EFileDialogFlags::None,
        Picked);
    if (!bPicked || Picked.Num() == 0) return;
    const FString Path = Picked[0];
    const FString Ext = FPaths::GetExtension(Path).ToLower();
    UNMSBaseManager* Mgr = NewObject<UNMSBaseManager>();
    Mgr->AddToRoot();
    bool bOk = false;
    if (Ext == TEXT("hg"))         bOk = Mgr->ImportNMSSave(Path);
    else if (Ext == TEXT("blend")) bOk = Mgr->ImportFromBlend(Path);
    else { FString Txt; if (FFileHelper::LoadFileToString(Txt, *Path)) bOk = Mgr->ImportFromString(Txt); }
    if (bOk) { SpawnFromManager(Mgr); }
    UE_LOG(LogTemp, Log, TEXT("NMS UI: OpenBase %s -> %s"), *Path, bOk ? TEXT("OK") : TEXT("FAIL"));
    Mgr->RemoveFromRoot();
}

void SNMSBuilderUI::OnMenuImportModel()
{
#if WITH_EDITOR
    // Импорт внешней модели .obj/.stl: парсим -> рантайм UStaticMesh -> спавним
    // как обычную деталь (метка NMS_IMPORT_*), чтобы работали выбор/трансформ/подсветка.
    IDesktopPlatform* DP = FDesktopPlatformModule::Get();
    if (!DP) return;
    TArray<FString> Picked;
    const bool bPicked = DP->OpenFileDialog(
        nullptr,
        TEXT("Импорт 3D-модели"),
        TEXT("C:/Users/User/Documents"),
        TEXT(""),
        TEXT("3D-модели (*.obj;*.stl)|*.obj;*.stl"),
        EFileDialogFlags::None,
        Picked);
    if (!bPicked || Picked.Num() == 0) return;
    const FString Path = Picked[0];

    FNMSImportedMesh Geo;
    FString Err;
    if (!FNMSModelImporter::LoadFromFile(Path, Geo, Err))
    {
        UE_LOG(LogTemp, Warning, TEXT("NMS Import: %s -> ОШИБКА: %s"), *Path, *Err);
        return;
    }

    UWorld* World = ViewportClient.IsValid() ? ViewportClient->GetWorld() : nullptr;
    if (!World) return;

    UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_PartLit.M_PartLit"));
    if (!Mat) Mat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_BasePart.M_BasePart"));

    UStaticMesh* Mesh = FNMSModelImporter::BuildStaticMesh(Geo, Mat, 300.f);
    if (!Mesh) { UE_LOG(LogTemp, Warning, TEXT("NMS Import: не удалось собрать меш")); return; }

    const FString Name = FPaths::GetBaseFilename(Path);
    FActorSpawnParameters Params;
    Params.ObjectFlags = RF_Transient;
    AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(
        AStaticMeshActor::StaticClass(), FTransform::Identity, Params);
    if (!A) return;
    A->SetActorLabel(FString::Printf(TEXT("NMS_IMPORT_%s"), *Name));
    A->Tags.Add(FName(TEXT("IMPORT")));

    if (UStaticMeshComponent* Comp = A->GetStaticMeshComponent())
    {
        Comp->SetMobility(EComponentMobility::Movable);
        Comp->SetCastShadow(false);
        Comp->SetStaticMesh(Mesh);
        Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Comp->SetCollisionObjectType(ECC_WorldStatic);
        Comp->SetCollisionResponseToAllChannels(ECR_Ignore);
        Comp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        if (Mat) Comp->SetMaterial(0, Mat);
    }

    if (ViewportClient.IsValid()) ViewportClient->SetSelectedActor(A);
    if (SceneListView.IsValid()) RebuildSceneTree();
    UE_LOG(LogTemp, Log, TEXT("NMS Import: %s -> %d верт, %d инд OK"),
        *Path, Geo.Positions.Num(), Geo.Indices.Num());
#endif
}

void SNMSBuilderUI::OnMenuImportFromNMS()
{
    // Открываем окно Save Manager (выбор аккаунта/слота/базы из сейва игры).
    TSharedRef<SWindow> Win = SNew(SWindow)
        .Title(FText::FromString(TEXT("Save Manager — базы из сейва игры")))
        .ClientSize(FVector2D(420, 600))
        .SupportsMaximize(false);

    Win->SetContent(BuildSaveManagerPanel());

    if (FSlateApplication::IsInitialized())
    {
        TSharedPtr<SWindow> Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
        if (Parent.IsValid())
            FSlateApplication::Get().AddWindowAsNativeChild(Win, Parent.ToSharedRef());
        else
            FSlateApplication::Get().AddWindow(Win);
    }
}


// Удалить из сцены все детали (актёры с меткой NMS_).
void SNMSBuilderUI::ClearSceneParts()
{
#if WITH_EDITOR
    UWorld* World = ViewportClient.IsValid() ? ViewportClient->GetWorld() : nullptr;
    if (!World) return;
    if (ViewportClient.IsValid()) ViewportClient->SetSelectedActor(nullptr);

    TArray<AActor*> ToDelete;
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        AStaticMeshActor* A = *It;
        if (A && A->GetActorLabel().StartsWith(TEXT("NMS_"))) ToDelete.Add(A);
    }
    for (AActor* A : ToDelete) World->DestroyActor(A);
    UE_LOG(LogTemp, Log, TEXT("NMS UI: cleared %d parts"), ToDelete.Num());
#endif
    if (SceneListView.IsValid()) RebuildSceneTree();
}

// Построить базу из менеджера в сцене: настоящие модели деталей + родные цвета.
// Цвет детали из базы: UserData -> индекс палитры -> RGBA (primary+secondary).
// Таблица из Content/NMSData/userdata_palette.json (сгенерирована из игрового DT_Palettes).
static bool NMS_ColourFromIndex(int32 Index, FLinearColor& OutP, FLinearColor& OutS)
{
    static bool bLoaded = false;
    static TMap<int32, TPair<FLinearColor, FLinearColor>> Tbl;
    if (!bLoaded)
    {
        bLoaded = true;
        FString Raw;
        const FString Path = FPaths::ProjectContentDir() / TEXT("NMSData/userdata_palette.json");
        if (FFileHelper::LoadFileToString(Raw, *Path))
        {
            TSharedPtr<FJsonObject> Root;
            const TSharedRef<TJsonReader<>> Rd = TJsonReaderFactory<>::Create(Raw);
            if (FJsonSerializer::Deserialize(Rd, Root) && Root.IsValid())
            {
                for (const auto& KV : Root->Values)
                {
                    const int32 Idx = FCString::Atoi(*KV.Key);
                    const TSharedPtr<FJsonObject>* O = nullptr;
                    if (!KV.Value->TryGetObject(O)) continue;
                    auto GetC = [&](const TCHAR* Field) -> FLinearColor
                    {
                        const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
                        FLinearColor C(0.8f, 0.8f, 0.8f, 1.f);
                        if ((*O)->TryGetArrayField(Field, A) && A->Num() >= 3)
                        { C.R = (*A)[0]->AsNumber(); C.G = (*A)[1]->AsNumber(); C.B = (*A)[2]->AsNumber(); }
                        return C;
                    };
                    Tbl.Add(Idx, TPair<FLinearColor, FLinearColor>(GetC(TEXT("P")), GetC(TEXT("S"))));
                }
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("NMS PALETTE: загружено цветов = %d"), Tbl.Num());
    }
    if (const TPair<FLinearColor, FLinearColor>* Fnd = Tbl.Find(Index)) { OutP = Fnd->Key; OutS = Fnd->Value; return true; }
    return false;
}

void SNMSBuilderUI::SpawnFromManager(UNMSBaseManager* Mgr)
{
#if WITH_EDITOR
    UWorld* World = ViewportClient.IsValid() ? ViewportClient->GetWorld() : nullptr;
    if (!World || !Mgr) return;

    CurrentBaseName = Mgr->BaseName;
    CurrentGalacticAddress = Mgr->GalacticAddress;

    ClearSceneParts();

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UMaterialInterface* BaseM = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_BasePart.M_BasePart"));
    UMaterialInterface* LitM = LoadObject<UMaterialInterface>(nullptr,
        FNMSViewportClient::bUnlitParts
            ? TEXT("/Game/NMSBaseBuilder/Materials/M_PartUnlit.M_PartUnlit")
            : TEXT("/Game/NMSBaseBuilder/Materials/M_PartLit.M_PartLit"));
    // неосвещаемый материал для эмиссив-слотов (лампы/экраны, флаг _F07_UNLIT в игре)
    UMaterialInterface* UnlitM = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_PartUnlit.M_PartUnlit"));

    int32 Spawned = 0;
    for (const FNMSPlacedObject& Obj : Mgr->PlacedObjects)
    {
        FString Id = Obj.ObjectID;
        Id.RemoveFromStart(TEXT("^"));
        const TSharedPtr<FNMSPartData>* Part = PartById.Find(Id);

        // настоящая модель детали; если не нашли — маленький куб-маркер
        UStaticMesh* Mesh = nullptr;
        if (Part && (*Part).IsValid() && !(*Part)->ModelPath.IsEmpty())
            Mesh = LoadObject<UStaticMesh>(nullptr, *(*Part)->ModelPath);

        FActorSpawnParameters Params;
        Params.ObjectFlags = RF_Transient;
        AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(
            AStaticMeshActor::StaticClass(), Obj.Transform, Params);
        if (!A) continue;
        A->SetActorLabel(FString::Printf(TEXT("NMS_%s"), *Obj.ObjectID));
        A->Tags.Add(FName(*Obj.ObjectID)); // надёжное хранение ID (метки редактор меняет)
        A->Tags.Add(FName(*FString::Printf(TEXT("UD:%lld"), (long long)Obj.UserData))); // цвет/материал детали

        if (UStaticMeshComponent* Comp = A->GetStaticMeshComponent())
        {
            Comp->SetMobility(EComponentMobility::Movable);
            Comp->SetCastShadow(false);
            if (Mesh)
            {
                Comp->SetStaticMesh(Mesh);
                // коллизия для выбора кликом (луч идёт по мешу, т.к. простой коллизии нет)
                Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                Comp->SetCollisionObjectType(ECC_WorldStatic);
                Comp->SetCollisionResponseToAllChannels(ECR_Ignore);
                Comp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
                const FString Cat = Part && (*Part).IsValid() ? (*Part)->Category : FString();
                FLinearColor Col1 = NMS_DefaultPartColor(Cat, Obj.ObjectID);
                FLinearColor Col2 = NMS_DefaultPartColor2(Cat, Obj.ObjectID);
                FLinearColor ZA = Col1, ZB = Col1, ZC = Col1, ZD = Col1; // 4 зоны покраски
                // цвет из базы: декодируем UserData (NMS пакует индекс палитры в биты)
                {
                    const uint32 UD = (uint32)Obj.UserData;
                    const int32 ColourIndex = (int32)(UD & 0xFCFEFFu);
                    FLinearColor PP, SS;
                    const FLinearColor DefCol = Col1;
                    const bool bFound = (ColourIndex != 0 && NMS_ColourFromIndex(ColourIndex, PP, SS));
                    if (bFound) { Col1 = PP; Col2 = SS; ZA = PP; ZB = SS; FLinearColor TT = Col1, QQ = Col1; NMS_PalTQ(ColourIndex, TT, QQ); ZC = TT; ZD = QQ; }
                    static TSet<int32> DbgSeen;
                    if (!DbgSeen.Contains(ColourIndex)) { DbgSeen.Add(ColourIndex);
                        const float Diff = FMath::Abs(Col1.R-DefCol.R)+FMath::Abs(Col1.G-DefCol.G)+FMath::Abs(Col1.B-DefCol.B);
                        UE_LOG(LogTemp, Warning, TEXT("NMS COL idx=%d found=%d Col1=(%.2f,%.2f,%.2f) Def=(%.2f,%.2f,%.2f) dDef=%.2f  (%s)"),
                            ColourIndex, bFound?1:0, Col1.R, Col1.G, Col1.B, DefCol.R, DefCol.G, DefCol.B, Diff, *Obj.ObjectID); }
                }
                // путь к импортированной текстуре по короткому имени атласа
                auto TexPathOf = [](const FString& N) -> FString
                {
                    return N.IsEmpty() ? FString()
                        : FString::Printf(TEXT("/Game/NMSBaseBuilder/PartTex2/T_%s.T_%s"), *N, *N);
                };
                // строитель MID из набора текстур одного слота (diffuse/masks/normal/paint)
                auto BuildMID = [&](const FNMSTexSet& S) -> UMaterialInstanceDynamic*
                {
                    UTexture2D* Tex = S.Tex.IsEmpty() ? nullptr
                        : LoadObject<UTexture2D>(nullptr, *TexPathOf(S.Tex));
                    // эмиссив-слот (лампы/экраны) -> неосвещаемый материал, как в игре
                    UMaterialInterface* Surf = (S.bUnlit && UnlitM) ? UnlitM : LitM;
                    UMaterialInterface* M = (Tex && Surf) ? Surf : BaseM;
                    if (!M) return nullptr;
                    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, Comp);
                    if (Tex && Surf)
                    {
                        MID->SetTextureParameterValue(TEXT("BaseTex"), Tex);
                        // эмиссив-слот не красим маской и не добавляем рельеф — это лампы
                        if (!S.bUnlit)
                        {
                            if (UTexture2D* Mask = LoadObject<UTexture2D>(nullptr, *TexPathOf(S.Mask)))
                                MID->SetTextureParameterValue(TEXT("PaintMask"), Mask);
                            if (UTexture2D* Norm = LoadObject<UTexture2D>(nullptr, *TexPathOf(S.Norm)))
                                MID->SetTextureParameterValue(TEXT("NormalTex"), Norm);
                            if (UTexture2D* Mk = LoadObject<UTexture2D>(nullptr, *TexPathOf(S.Masks)))
                            {
                                MID->SetTextureParameterValue(TEXT("MasksTex"), Mk);
                                MID->SetScalarParameterValue(TEXT("UseMasks"), 1.f);
                            }
                            MID->SetVectorParameterValue(TEXT("Color"), Col1);
                            MID->SetVectorParameterValue(TEXT("Color2"), Col2);
                        }
                    }
                    else MID->SetVectorParameterValue(TEXT("Color"), Col1);
                    // 1-В-1: покраска по умолчанию = РОДНОЙ цвет детали Col1 (дефолт палитры).
                    // Камень Col1=песочный -> бледное каменное тело тонируется в песок (иконка),
                    // бронза в диффузе остаётся. Фибергласс Col1~белый -> беж диффуз виден.
                                        MID->SetVectorParameterValue(TEXT("ZoneCol0"), ZA);
                    MID->SetVectorParameterValue(TEXT("ZoneCol1"), ZB);
                    MID->SetVectorParameterValue(TEXT("ZoneCol2"), ZC);
                    MID->SetVectorParameterValue(TEXT("ZoneCol3"), ZD);
                    return MID;
                };

                const int32 N = Comp->GetNumMaterials();
                // ПУНКТ 4: текстуры ПО СЛОТАМ — ВКЛ для деталей с ПОЛНЫМ покрытием слотов
                // (Slots->Num() >= число материал-слотов меша N). Неполные (корветы) ->
                // откат на одиночный путь, чтобы не белели. Одноматериальные (камень, N=1)
                // тоже идут одиночным путём — наш рабочий результат не трогается.
                const TArray<FNMSTexSet>* Slots = NMS_PartSlots(Obj.ObjectID);
                const bool bFullCover = (Slots && N > 1 && Slots->Num() >= N);
                UE_LOG(LogTemp, Warning, TEXT("NMS PERSLOT %s: N=%d slots=%d cover=%d"),
                    *Obj.ObjectID, N, Slots ? Slots->Num() : -1, bFullCover ? 1 : 0);
                if (bFullCover)
                {
                    for (int32 i = 0; i < N; ++i)
                    {
                        const FNMSTexSet& S = (*Slots)[FMath::Min(i, Slots->Num() - 1)];
                        UMaterialInstanceDynamic* MID = BuildMID(S);
                        UE_LOG(LogTemp, Warning, TEXT("NMS PERSLOT   slot%d tex=%s mid=%d"),
                            i, *S.Tex, MID ? 1 : 0);
                        if (MID) Comp->SetMaterial(i, MID);
                    }
                }
                else
                {
                    // одиночный материал (как раньше): из part_textures.json
                    FString TexPath, MaskPath, NormPath, MasksPath, OccPath;
                    NMS_PartTexture(Cat, Obj.ObjectID, TexPath, MaskPath, NormPath, MasksPath, OccPath);
                    UTexture2D* Tex = TexPath.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *TexPath);
                    UMaterialInterface* M = (Tex && LitM) ? LitM : BaseM;
                    if (M)
                    {
                        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, Comp);
                        if (Tex && LitM)
                        {
                            MID->SetTextureParameterValue(TEXT("BaseTex"), Tex);
                            if (UTexture2D* Mask = LoadObject<UTexture2D>(nullptr, *MaskPath))
                                MID->SetTextureParameterValue(TEXT("PaintMask"), Mask);
                            if (UTexture2D* Norm = LoadObject<UTexture2D>(nullptr, *NormPath))
                                MID->SetTextureParameterValue(TEXT("NormalTex"), Norm);
                            if (UTexture2D* Mk = LoadObject<UTexture2D>(nullptr, *MasksPath))
                            {
                                MID->SetTextureParameterValue(TEXT("MasksTex"), Mk);
                                MID->SetScalarParameterValue(TEXT("UseMasks"), 1.f);
                            }
                            if (UTexture2D* Oc = LoadObject<UTexture2D>(nullptr, *OccPath))
                                MID->SetTextureParameterValue(TEXT("OcclusionTex"), Oc);
                            MID->SetVectorParameterValue(TEXT("Color"), Col1);
                            MID->SetVectorParameterValue(TEXT("Color2"), Col2);
                        }
                        else MID->SetVectorParameterValue(TEXT("Color"), Col1);
                        // 1-В-1: покраска по умолчанию = родной цвет детали Col1 (камень -> песок).
                                                MID->SetVectorParameterValue(TEXT("ZoneCol0"), ZA);
                        MID->SetVectorParameterValue(TEXT("ZoneCol1"), ZB);
                        MID->SetVectorParameterValue(TEXT("ZoneCol2"), ZC);
                        MID->SetVectorParameterValue(TEXT("ZoneCol3"), ZD);
                        for (int32 i = 0; i < N; ++i) Comp->SetMaterial(i, MID);
                    }
                }
            }
            else if (Cube)
            {
                Comp->SetStaticMesh(Cube);
                Comp->SetRelativeScale3D(FVector(0.15f));
            }
        }
        ++Spawned;
    }
    UE_LOG(LogTemp, Log, TEXT("NMS UI: built base '%s' (%d parts)"), *CurrentBaseName, Spawned);
    RebuildSceneTree();   // автообновление дерева сцены после загрузки
    if (!bRestoring) { EditSnapshot = SnapshotScene(); UndoStack.Reset(); RedoStack.Reset(); }
#endif
}

// ===========================================================================
//  Undo / Redo — снимки сцены через экспорт/импорт базы
// ===========================================================================
FString SNMSBuilderUI::SnapshotScene()
{
    UNMSBaseManager* Mgr = NewObject<UNMSBaseManager>(); Mgr->AddToRoot();
    CollectSceneToManager(Mgr);
    FString S = Mgr->ExportToString();
    Mgr->RemoveFromRoot();
    return S;
}
void SNMSBuilderUI::RestoreScene(const FString& S)
{
    bRestoring = true;
    ClearSceneParts();
    UNMSBaseManager* Mgr = NewObject<UNMSBaseManager>(); Mgr->AddToRoot();
    if (Mgr->ImportFromString(S)) SpawnFromManager(Mgr);
    Mgr->RemoveFromRoot();
    bRestoring = false;
}
void SNMSBuilderUI::CommitEdit()
{
    if (bRestoring) return;
    FString S = SnapshotScene();
    if (EditSnapshot.IsEmpty()) { EditSnapshot = S; return; } // базовая точка
    if (S == EditSnapshot) return;                           // ничего не изменилось
    UndoStack.Add(EditSnapshot);
    if (UndoStack.Num() > 100) UndoStack.RemoveAt(0);
    RedoStack.Reset();
    EditSnapshot = S;
}
void SNMSBuilderUI::DoUndo()
{
    if (UndoStack.Num() == 0) return;
    RedoStack.Add(EditSnapshot);
    FString S = UndoStack.Pop();
    EditSnapshot = S;
    RestoreScene(S);
}
void SNMSBuilderUI::DoRedo()
{
    if (RedoStack.Num() == 0) return;
    UndoStack.Add(EditSnapshot);
    FString S = RedoStack.Pop();
    EditSnapshot = S;
    RestoreScene(S);
}

// Сцена -> PlacedObjects (для сохранения/экспорта). ObjectID берём из метки NMS_^ID.
void SNMSBuilderUI::CollectSceneToManager(UNMSBaseManager* Mgr)
{
#if WITH_EDITOR
    UWorld* World = ViewportClient.IsValid() ? ViewportClient->GetWorld() : nullptr;
    if (!World || !Mgr) return;

    Mgr->PlacedObjects.Reset();
    Mgr->BaseName = CurrentBaseName;
    Mgr->GalacticAddress = CurrentGalacticAddress;

    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        AStaticMeshActor* A = *It;
        if (!A) continue;
        if (A->ActorHasTag(FName(TEXT("NMS_CABLE")))) continue; // кабель — отдельная сущность
        FString Label = A->GetActorLabel();
        if (!Label.RemoveFromStart(TEXT("NMS_"))) continue;

        FNMSPlacedObject P;
        // ID из тега (точный); метка — запасной путь
        P.ObjectID = (A->Tags.Num() > 0) ? A->Tags[0].ToString() : Label;
        P.Transform = A->GetActorTransform();
        P.Timestamp = (float)FDateTime::UtcNow().ToUnixTimestamp();
        // цвет/материал: читаем сохранённый при спавне UserData (тег "UD:<число>")
        for (const FName& Tg : A->Tags)
        {
            FString S = Tg.ToString();
            if (S.RemoveFromStart(TEXT("UD:"))) { P.UserData = FCString::Atoi64(*S); break; }
        }
        Mgr->PlacedObjects.Add(P);
    }
#endif
}

// Save Base As... — сохранить базу в JSON-файл (формат Blender-плагина).
void SNMSBuilderUI::OnMenuSaveBaseAs()
{
    IDesktopPlatform* DP = FDesktopPlatformModule::Get();
    if (!DP) return;

    TArray<FString> Picked;
    const bool bPicked = DP->SaveFileDialog(
        nullptr,
        TEXT("Сохранить базу как"),
        TEXT("C:/Users/User/Documents/bases"),
        CurrentBaseName.IsEmpty() ? TEXT("base.json") : CurrentBaseName + TEXT(".json"),
        TEXT("База NMS (*.json)|*.json"),
        EFileDialogFlags::None,
        Picked);
    if (!bPicked || Picked.Num() == 0) return;

    UNMSBaseManager* Mgr = NewObject<UNMSBaseManager>();
    Mgr->AddToRoot();
    CollectSceneToManager(Mgr);
    const bool bOk = Mgr->ExportNMSSave(Picked[0]);
    UE_LOG(LogTemp, Log, TEXT("NMS UI: SaveBaseAs %s -> %s (%d parts)"),
        *Picked[0], bOk ? TEXT("OK") : TEXT("FAIL"), Mgr->PlacedObjects.Num());
    Mgr->RemoveFromRoot();
}

// Import from Clipboard — вставить базу из буфера обмена (как в плагине).
void SNMSBuilderUI::OnMenuImportClipboard()
{
    FString Clip;
    FPlatformApplicationMisc::ClipboardPaste(Clip);
    if (Clip.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("NMS UI: буфер обмена пуст"));
        return;
    }

    UNMSBaseManager* Mgr = NewObject<UNMSBaseManager>();
    Mgr->AddToRoot();
    if (Mgr->ImportFromString(Clip))
    {
        SpawnFromManager(Mgr);
    }
    Mgr->RemoveFromRoot();
}

// Export to Clipboard — скопировать базу в буфер обмена (как в плагине).
void SNMSBuilderUI::OnMenuExportClipboard()
{
    UNMSBaseManager* Mgr = NewObject<UNMSBaseManager>();
    Mgr->AddToRoot();
    CollectSceneToManager(Mgr);
    const FString Out = Mgr->ExportToString();
    FPlatformApplicationMisc::ClipboardCopy(*Out);
    UE_LOG(LogTemp, Log, TEXT("NMS UI: база скопирована в буфер (%d деталей)"),
        Mgr->PlacedObjects.Num());
    Mgr->RemoveFromRoot();
}

void SNMSBuilderUI::OnMenuExportToNMS()
{
    OnMenuExportClipboard();
}

// ===========================================================================
//  Верхний тулбар
// ===========================================================================
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
    auto ModeIs = [this](ENMSTransformMode M) { return ViewportClient.IsValid() && ViewportClient->TransformMode == M; };
    auto SetMode = [this](ENMSTransformMode M) { if (ViewportClient.IsValid()) ViewportClient->TransformMode = M; };

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
        [this]() { if (ViewportClient.IsValid()) ViewportClient->MoveSnap = (ViewportClient->MoveSnap > 1.f) ? 1.f : 50.f; },
        [this]() { return ViewportClient.IsValid() && ViewportClient->MoveSnap <= 1.f; }) ];
    Bar->AddSlot().AutoWidth()[ GameBtn(TEXT("buildmenu__move"), TEXT("MODE_EDIT"), TEXT("РЕДАКТИРОВАНИЕ"),
        [SetMode]() { SetMode(ENMSTransformMode::Move); }, [ModeIs]() { return ModeIs(ENMSTransformMode::Move); }) ];
    Bar->AddSlot().AutoWidth()[ GameBtn(TEXT("buildmenu__cam"), TEXT("HDR_CAMERA"), TEXT("КАМЕРА"),
        [this]() { if (ViewportClient.IsValid()) { ViewportClient->CameraLocation = FVector(-600.f, 0.f, 400.f);
                   ViewportClient->CameraRotation = FRotator(-25.f, 0.f, 0.f); ViewportClient->FocusPoint = FVector::ZeroVector; } },
        Never) ];
    Bar->AddSlot().AutoWidth()[ GameBtn(TEXT("buildmenu__wiring"), TEXT("MODE_WIRING"), TEXT("ПРОВОДКА"),
        [this]() { if (ViewportClient.IsValid()) { ViewportClient->CancelPlacing(); ViewportClient->bWiringMode = !ViewportClient->bWiringMode; } },
        [this]() { return ViewportClient.IsValid() && ViewportClient->bWiringMode; }) ];

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
static FLinearColor NMS_DefaultPartColor(const FString& Category, const FString& ObjectID)
{
    // сперва — точный дефолт из таблицы игры
    FLinearColor P, S;
    if (NMS_GamePartColors(ObjectID, P, S)) return P;

    FString Id = ObjectID; Id.RemoveFromStart(TEXT("^"));

    if (Category == TEXT("Timber Structures"))  return FLinearColor(0.731f, 0.626f, 0.511f); // дуб
    if (Category == TEXT("Stone Structures"))   return FLinearColor(0.875f, 0.796f, 0.678f); // Pink & Natural — оригинал палитры игры
    if (Category == TEXT("Alloy Structures"))   return FLinearColor(0.85f, 0.86f, 0.88f);    // сплав
    if (Category == TEXT("Corvette"))           return FLinearColor(0.92f, 0.92f, 0.92f);    // корвет
    if (Category == TEXT("Builder Structures")) return FLinearColor(0.90f, 0.91f, 0.92f);    // фрейтер-стиль

    if (Category == TEXT("Legacy Structures") || Category == TEXT("Large Structures"))
    {
        if (Id.StartsWith(TEXT("W_"))) return FLinearColor(0.55f, 0.40f, 0.26f);  // дерево
        if (Id.StartsWith(TEXT("C_"))) return FLinearColor(0.88f, 0.86f, 0.82f);  // бетон
        if (Id.StartsWith(TEXT("M_"))) return FLinearColor(0.52f, 0.54f, 0.57f);  // металл
        if (Id.StartsWith(TEXT("S_"))) return FLinearColor(0.62f, 0.58f, 0.52f);  // камень (legacy)
    }
    if (Id.StartsWith(TEXT("FRE_"))) return FLinearColor(0.92f, 0.92f, 0.92f);    // фрейтер

    return FLinearColor(0.78f, 0.78f, 0.74f); // прочее — нейтральный, как раньше
}

void SNMSBuilderUI::OnPartSelected(TSharedPtr<FNMSPartData> Item, ESelectInfo::Type)
{
    if (!Item.IsValid()) return;
    CurveBuildPart = Item;   // запомнить для раскладки по кривой
    LastPartItem = Item;     // для непрерывной установки
    if (ViewportClient.IsValid()) { if (UStaticMesh* CM = LoadObject<UStaticMesh>(nullptr, *Item->ModelPath)) { const FVector S = CM->GetBoundingBox().GetSize(); ViewportClient->CurvePartLen = FMath::Max(FMath::Max(S.X, S.Y), 1.f); } }
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
    AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(
        AStaticMeshActor::StaticClass(), FTransform(FRotator(0.f, 90.f, 0.f), SpawnLoc), Params);
    if (!Actor) return;
#if WITH_EDITOR
    Actor->SetActorLabel(FString::Printf(TEXT("NMS_%s"), *Item->ObjectID));
#endif
    Actor->Tags.Add(FName(*Item->ObjectID)); // ID для сохранения/экспорта базы
    if (UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent())
    {
        Comp->SetMobility(EComponentMobility::Movable);
        Comp->SetStaticMesh(Mesh);
        // коллизия для повторного выбора кликом после установки
        Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Comp->SetCollisionObjectType(ECC_WorldStatic);
        Comp->SetCollisionResponseToAllChannels(ECR_Ignore);
        Comp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Comp->SetCastShadow(false); // тени вешают GPU (Virtual Shadow Maps) на этой видеокарте
        // Зелёная «голограмма» размещения (как в игре); после установки заменится на обычный.
        UMaterialInterface* Ghost = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/NMSBaseBuilder/Materials/M_Ghost.M_Ghost"));
        if (!Ghost) Ghost = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/NMSBaseBuilder/Materials/M_BasePart.M_BasePart"));
        if (!Ghost) Ghost = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
        if (Ghost)
        {
            const int32 N = Comp->GetNumMaterials();
            for (int32 i = 0; i < N; ++i) Comp->SetMaterial(i, Ghost);
        }
    }

    // 3) Режим размещения: голограмма следует за курсором, ЛКМ ставит (как в игре).
    //    Родной цвет/атлас материала применится при подтверждении постановки.
    ViewportClient->PendingPartColor = NMS_DefaultPartColor(Item->Category, Item->ObjectID);
    ViewportClient->PendingPartColor2 = NMS_DefaultPartColor2(Item->Category, Item->ObjectID);
    NMS_PartTexture(Item->Category, Item->ObjectID,
        ViewportClient->PendingBaseTex, ViewportClient->PendingPaintMask,
        ViewportClient->PendingNormalTex, ViewportClient->PendingMasksTex,
        ViewportClient->PendingOccTex);
    ViewportClient->StartPlacing(Actor);
}


// Живой меш-предпросмотр раскладки через InstancedStaticMesh (видна сама деталь, обновляется с ползунком).
void SNMSBuilderUI::UpdateCurvePreview()
{
    if (!ViewportClient.IsValid()) return;
    UWorld* World = ViewportClient->GetWorld(); if (!World) return;
    if (!CurvePreviewISM)
    {
        AActor* Host = World->SpawnActor<AActor>(); if (!Host) return;
        Host->SetFlags(RF_Transient);
        USceneComponent* Root = NewObject<USceneComponent>(Host);
        Host->SetRootComponent(Root); Root->RegisterComponent();
        CurvePreviewISM = NewObject<UInstancedStaticMeshComponent>(Host);
        CurvePreviewISM->SetupAttachment(Root);
        CurvePreviewISM->RegisterComponent();
        CurvePreviewISM->SetCastShadow(false);
        CurvePreviewISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    CurvePreviewISM->ClearInstances();
    if (!ViewportClient->bCurveMode || !CurveBuildPart.IsValid()) return;
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *CurveBuildPart->ModelPath); if (!Mesh) return;
    CurvePreviewISM->SetStaticMesh(Mesh);

    FString TexP,MaskP,NormP,MasksP,OccP;
    NMS_PartTexture(CurveBuildPart->Category, CurveBuildPart->ObjectID, TexP,MaskP,NormP,MasksP,OccP);
    UMaterialInterface* LitM  = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NMSBaseBuilder/Materials/M_PartLit.M_PartLit"));
    UMaterialInterface* BaseM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NMSBaseBuilder/Materials/M_BasePart.M_BasePart"));
    UTexture2D* BodyTex = TexP.IsEmpty()  ? nullptr : LoadObject<UTexture2D>(nullptr, *TexP);
    UTexture2D* MaskTex = MaskP.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *MaskP);
    UTexture2D* NormTex = NormP.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *NormP);
    UTexture2D* MaskaTex= MasksP.IsEmpty()? nullptr : LoadObject<UTexture2D>(nullptr, *MasksP);
    const FLinearColor Col1 = NMS_DefaultPartColor(CurveBuildPart->Category, CurveBuildPart->ObjectID);
    const FLinearColor Col2 = NMS_DefaultPartColor2(CurveBuildPart->Category, CurveBuildPart->ObjectID);
    UMaterialInterface* M = (BodyTex && LitM) ? LitM : BaseM;
    if (M)
    {
        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, CurvePreviewISM);
        if (BodyTex && LitM)
        {
            MID->SetTextureParameterValue(TEXT("BaseTex"), BodyTex);
            if (MaskTex)  MID->SetTextureParameterValue(TEXT("PaintMask"), MaskTex);
            if (NormTex)  MID->SetTextureParameterValue(TEXT("NormalTex"), NormTex);
            if (MaskaTex) { MID->SetTextureParameterValue(TEXT("MasksTex"), MaskaTex); MID->SetScalarParameterValue(TEXT("UseMasks"), 1.f); }
            MID->SetVectorParameterValue(TEXT("Color"), Col1);
            MID->SetVectorParameterValue(TEXT("Color2"), Col2);
        }
        else MID->SetVectorParameterValue(TEXT("Color"), Col1);
        MID->SetVectorParameterValue(TEXT("ZoneCol0"), Col1);
        MID->SetVectorParameterValue(TEXT("ZoneCol1"), Col1);
        MID->SetVectorParameterValue(TEXT("ZoneCol2"), Col1);
        MID->SetVectorParameterValue(TEXT("ZoneCol3"), Col1);
        const int32 NM = CurvePreviewISM->GetNumMaterials();
        for (int32 i=0;i<NM;++i) CurvePreviewISM->SetMaterial(i, MID);
    }

    TArray<FVector> Curve = TessellateCurve(); if (Curve.Num() < 2) return;
    const FVector Sz = Mesh->GetBoundingBox().GetSize();
    float PartLen = FMath::Max(FMath::Max(Sz.X, Sz.Y), 1.f);
    const float Step = FMath::Max(PartLen * (1.f - ViewportClient->CurveOverlap), 1.f);
    TArray<float> Seg; float Total=0.f;
    for (int32 i=0;i+1<Curve.Num();++i){ const float dd=FVector::Dist2D(Curve[i],Curve[i+1]); Seg.Add(dd); Total+=dd; }
    const int32 NSteps = FMath::Min(FMath::Max(1, FMath::RoundToInt(Total / Step)), 5000);
    const float AStep = Total / NSteps;
    for (int32 ki=0; ki<=NSteps; ++ki)
    {
        const float d = ki * AStep;
        float acc=0.f; int32 si=0; while(si+1<Seg.Num() && acc+Seg[si] < d){ acc+=Seg[si]; ++si; }
        const float f = (Seg.IsValidIndex(si) && Seg[si]>1e-3f) ? (d-acc)/Seg[si] : 0.f;
        const int32 ni = FMath::Min(si+1, Curve.Num()-1);
        const FVector Pos = FMath::Lerp(Curve[si], Curve[ni], f);
        FVector T = Curve[ni]-Curve[si]; T.Z=0.f; if (T.IsNearlyZero()) T=FVector(1,0,0); T.Normalize();
        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X));
        CurvePreviewISM->AddInstance(FTransform(FRotator(ViewportClient->CurveTilt, Yaw, ViewportClient->CurveRoll), FVector(Pos.X, Pos.Y, 0.f)), true);
    }
}

// Построить плотную линию пути по типу кривой (Catmull/Ломаная/Круг/Прямоугольник).
TArray<FVector> SNMSBuilderUI::TessellateCurve()
{
    TArray<FVector> Curve;
    if (!ViewportClient.IsValid()) return Curve;
    const TArray<FVector> Pts = ViewportClient->CurvePoints;
    if (Pts.Num() < 2) return Curve;
    const ENMSCurveType Type = ViewportClient->CurveType;
    auto CM = [](float a1,float a2,float a3,float a4,float t){ return (a1*((-t+2)*t-1)*t + a2*(((3*t-5)*t)*t+2) + a3*((-3*t+4)*t+1)*t + a4*((t-1)*t*t))*0.5f; };
    if (Type == ENMSCurveType::Polyline) { Curve = Pts; }
    else if (Type == ENMSCurveType::Rect)
    {
        const FVector A = Pts[0], C = Pts[1];
        Curve = { A, FVector(C.X,A.Y,0.f), C, FVector(A.X,C.Y,0.f), A };
    }
    else if (Type == ENMSCurveType::Circle)
    {
        const FVector C = Pts[0]; const float R = (ViewportClient->CurveRadius > 1.f) ? ViewportClient->CurveRadius : FVector::Dist2D(C, Pts[1]); const int32 N = 64;
        for (int32 i=0;i<=N;++i){ const float a=2.f*PI*i/N; Curve.Add(C+FVector(R*FMath::Cos(a),R*FMath::Sin(a),0.f)); }
    }
    else
    {
        TArray<FVector> P; P.Reserve(Pts.Num()+2); P.Add(Pts[0]); P.Append(Pts); P.Add(Pts.Last());
        const int32 Seg=20;
        for (int32 i=1;i+2<P.Num();++i) for (int32 s=0;s<Seg;++s){ const float t=(float)s/Seg; Curve.Add(FVector(CM(P[i-1].X,P[i].X,P[i+1].X,P[i+2].X,t),CM(P[i-1].Y,P[i].Y,P[i+1].Y,P[i+2].Y,t),0.f)); }
        Curve.Add(Pts.Last());
    }
    return Curve;
}

// Сколько деталей встанет при текущем нахлёсте (для счётчика).
int32 SNMSBuilderUI::CurvePartCount()
{
    if (!ViewportClient.IsValid() || !CurveBuildPart.IsValid()) return 0;
    TArray<FVector> Curve = TessellateCurve();
    if (Curve.Num() < 2) return 0;
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *CurveBuildPart->ModelPath);
    if (!Mesh) return 0;
    const FVector Sz = Mesh->GetBoundingBox().GetSize();
    float PartLen = FMath::Max(Sz.X, Sz.Y); if (PartLen < 1.f) PartLen = 100.f;
    const float Step = FMath::Max(PartLen * (1.f - ViewportClient->CurveOverlap), 1.f);
    float Total = 0.f;
    for (int32 i=0;i+1<Curve.Num();++i) Total += FVector::Dist2D(Curve[i], Curve[i+1]);
    return FMath::Max(1, FMath::RoundToInt(Total / Step)) + 1;
}

// Разложить выбранную деталь вдоль нарисованной кривой (встык/с нахлёстом, поворот по касательной).
void SNMSBuilderUI::BuildAlongCurve()
{
    if (!ViewportClient.IsValid() || !CurveBuildPart.IsValid()) return;
    UWorld* World = ViewportClient->GetWorld(); if (!World) return;
    TArray<FVector> Curve = TessellateCurve();
    if (Curve.Num() < 2) return;

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *CurveBuildPart->ModelPath);
    if (!Mesh) return;
    const FVector Sz = Mesh->GetBoundingBox().GetSize();
    float PartLen = FMath::Max(Sz.X, Sz.Y); if (PartLen < 1.f) PartLen = 100.f;
    const float Step = FMath::Max(PartLen * (1.f - ViewportClient->CurveOverlap), 1.f);

    FString TexP, MaskP, NormP, MasksP, OccP;
    NMS_PartTexture(CurveBuildPart->Category, CurveBuildPart->ObjectID, TexP, MaskP, NormP, MasksP, OccP);
    UMaterialInterface* LitM  = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NMSBaseBuilder/Materials/M_PartLit.M_PartLit"));
    UMaterialInterface* BaseM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NMSBaseBuilder/Materials/M_BasePart.M_BasePart"));
    UTexture2D* BodyTex = TexP.IsEmpty()  ? nullptr : LoadObject<UTexture2D>(nullptr, *TexP);
    UTexture2D* MaskTex = MaskP.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *MaskP);
    UTexture2D* NormTex = NormP.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *NormP);
    UTexture2D* MaskaTex= MasksP.IsEmpty()? nullptr : LoadObject<UTexture2D>(nullptr, *MasksP);
    const FLinearColor Col1 = NMS_DefaultPartColor(CurveBuildPart->Category, CurveBuildPart->ObjectID);
    const FLinearColor Col2 = NMS_DefaultPartColor2(CurveBuildPart->Category, CurveBuildPart->ObjectID);

    TArray<float> Seg; float Total=0.f;
    for (int32 i=0;i+1<Curve.Num();++i){ const float dd=FVector::Dist2D(Curve[i],Curve[i+1]); Seg.Add(dd); Total+=dd; }
    int32 Count=0;
    const int32 NSteps = FMath::Max(1, FMath::RoundToInt(Total / Step));
    const float AStep = Total / NSteps;
    for (int32 ki=0; ki<=NSteps; ++ki)
    {
        const float d = ki * AStep;
        float acc=0.f; int32 si=0;
        while (si+1<Seg.Num() && acc+Seg[si] < d) { acc+=Seg[si]; ++si; }
        const float f = (Seg.IsValidIndex(si) && Seg[si]>1e-3f) ? (d-acc)/Seg[si] : 0.f;
        const int32 ni = FMath::Min(si+1, Curve.Num()-1);
        const FVector Pos = FMath::Lerp(Curve[si], Curve[ni], f);
        FVector Tang = Curve[ni] - Curve[si]; Tang.Z = 0.f;
        if (Tang.IsNearlyZero()) Tang = FVector(1,0,0); Tang.Normalize();
        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Tang.Y, Tang.X));

        FActorSpawnParameters SP; SP.ObjectFlags = RF_Transient;
        AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
            FTransform(FRotator(ViewportClient->CurveTilt, Yaw, ViewportClient->CurveRoll), FVector(Pos.X, Pos.Y, 0.f)), SP);
        if (A)
        {
#if WITH_EDITOR
            A->SetActorLabel(FString::Printf(TEXT("NMS_%s"), *CurveBuildPart->ObjectID));
#endif
            A->Tags.Add(FName(*CurveBuildPart->ObjectID));
            if (UStaticMeshComponent* Comp = A->GetStaticMeshComponent())
            {
                Comp->SetMobility(EComponentMobility::Movable);
                Comp->SetStaticMesh(Mesh);
                Comp->SetCastShadow(false);
                Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                Comp->SetCollisionObjectType(ECC_WorldStatic);
                Comp->SetCollisionResponseToAllChannels(ECR_Ignore);
                Comp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
                UMaterialInterface* M = (BodyTex && LitM) ? LitM : BaseM;
                if (M)
                {
                    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, Comp);
                    if (BodyTex && LitM)
                    {
                        MID->SetTextureParameterValue(TEXT("BaseTex"), BodyTex);
                        if (MaskTex)  MID->SetTextureParameterValue(TEXT("PaintMask"), MaskTex);
                        if (NormTex)  MID->SetTextureParameterValue(TEXT("NormalTex"), NormTex);
                        if (MaskaTex) { MID->SetTextureParameterValue(TEXT("MasksTex"), MaskaTex); MID->SetScalarParameterValue(TEXT("UseMasks"), 1.f); }
                        MID->SetVectorParameterValue(TEXT("Color"), Col1);
                        MID->SetVectorParameterValue(TEXT("Color2"), Col2);
                    }
                    else MID->SetVectorParameterValue(TEXT("Color"), Col1);
                    MID->SetVectorParameterValue(TEXT("ZoneCol0"), Col1);
                    MID->SetVectorParameterValue(TEXT("ZoneCol1"), Col1);
                    MID->SetVectorParameterValue(TEXT("ZoneCol2"), Col1);
                    MID->SetVectorParameterValue(TEXT("ZoneCol3"), Col1);
                    const int32 NM = Comp->GetNumMaterials();
                    for (int32 i=0;i<NM;++i) Comp->SetMaterial(i, MID);
                }
            }
            ++Count;
        }
    }
    UE_LOG(LogTemp, Log, TEXT("NMS: curve built %d parts (%s)"), Count, *CurveBuildPart->ObjectID);
    ViewportClient->CurvePoints.Reset();
    ViewportClient->bCurveMode = false;
    UpdateCurvePreview();
}

// ===========================================================================
//  Загрузка данных (без изменений)
// ===========================================================================
void SNMSBuilderUI::LoadPartsTable()
{
    // JSON-файл всегда содержит актуальные ПОЛНЫЕ данные (с ModelPath/IconPath),
    // поэтому читаем его в первую очередь. DataTable-ассет может быть устаревшим.
    if (LoadFromJsonFallback())
    {
        return;
    }
    // Запасной путь — DataTable-ассет.
    if (!PartsTable)
    {
        PartsTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/nms_parts_db.nms_parts_db"));
    }
    LoadFromDataTable();
}

bool SNMSBuilderUI::LoadFromDataTable()
{
    if (!PartsTable) return false;
    AllParts.Reset();
    static const FString Ctx(TEXT("NMSBuilderUI"));
    TArray<FNMSPartData*> Rows;
    PartsTable->GetAllRows<FNMSPartData>(Ctx, Rows);
    for (FNMSPartData* Row : Rows)
    {
        if (Row) AllParts.Add(MakeShared<FNMSPartData>(*Row));
    }
    UE_LOG(LogTemp, Log, TEXT("NMS UI: loaded %d parts from DataTable"), AllParts.Num());
    return AllParts.Num() > 0;
}

bool SNMSBuilderUI::LoadFromJsonFallback()
{
    AllParts.Reset();
    const FString Path = FPaths::ProjectContentDir() / TEXT("nms_parts_db.json");
    FString Raw;
    if (!FFileHelper::LoadFileToString(Raw, *Path))
    {
        UE_LOG(LogTemp, Error, TEXT("NMS UI: cannot read %s"), *Path);
        return false;
    }
    TArray<TSharedPtr<FJsonValue>> Arr;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(Reader, Arr))
    {
        UE_LOG(LogTemp, Error, TEXT("NMS UI: JSON parse failed"));
        return false;
    }
    for (const TSharedPtr<FJsonValue>& V : Arr)
    {
        const TSharedPtr<FJsonObject> O = V->AsObject();
        if (!O.IsValid()) continue;
        TSharedPtr<FNMSPartData> P = MakeShared<FNMSPartData>();
        auto GetStr = [&O](const TCHAR* Key) -> FString { FString S; O->TryGetStringField(Key, S); return S; };
        P->ObjectID       = GetStr(TEXT("ObjectID"));
        P->DisplayName    = GetStr(TEXT("DisplayName"));
        P->Category       = GetStr(TEXT("Category"));
        P->SubCategory    = GetStr(TEXT("SubCategory"));
        P->ModelPath      = GetStr(TEXT("ModelPath"));
        P->IconPath       = GetStr(TEXT("IconPath"));
        P->SocketClassIDs = GetStr(TEXT("SocketClassIDs"));
        P->PlugClassIDs   = GetStr(TEXT("PlugClassIDs"));
        P->VariantOf      = GetStr(TEXT("VariantOf"));
        O->TryGetBoolField(TEXT("bShowInDrawer"), P->bShowInDrawer);
        AllParts.Add(P);
    }
    UE_LOG(LogTemp, Log, TEXT("NMS UI: loaded %d parts from JSON fallback"), AllParts.Num());
    return AllParts.Num() > 0;
}

// Стопки вариантов из таблицы игры (Content/NMSData/part_variants.json):
// { "S_WALL": {"v":["S_WALLB","S_WALLM","S_WALLT"]}, "S_WALLB": {"hide":1}, ... }
void SNMSBuilderUI::LoadPartVariants()
{
    PartVariants.Reset();
    PartHidden.Reset();
    const FString Path = FPaths::ProjectContentDir() / TEXT("NMSData/part_variants.json");
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *Path)) return;
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()) return;
    for (const auto& Pair : Root->Values)
    {
        const FString Key(Pair.Key); // в UE 5.8 ключи JSON — FSharedString
        const TSharedPtr<FJsonObject>* O;
        if (!Pair.Value->TryGetObject(O)) continue;
        const TArray<TSharedPtr<FJsonValue>>* Arr;
        if ((*O)->TryGetArrayField(TEXT("v"), Arr))
        {
            TArray<FString>& V = PartVariants.Add(Key);
            for (const TSharedPtr<FJsonValue>& Jv : *Arr) V.Add(Jv->AsString());
        }
        if ((*O)->HasField(TEXT("hide"))) PartHidden.Add(Key);
    }
    UE_LOG(LogTemp, Log, TEXT("NMS UI: стопки вариантов: %d, скрытых ярусов: %d"),
        PartVariants.Num(), PartHidden.Num());
}

// Секции внутри категории (Content/NMSData/part_sections.json):
// { "S_WALL": {"sec":"СТЕНЫ, ДВЕРИ И ОКНА","idx":0}, ... } — названия из игры.
void SNMSBuilderUI::LoadPartSections()
{
    PartSection.Reset();
    const FString Path = FPaths::ProjectContentDir() / TEXT("NMSData/part_sections.json");
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *Path)) return;
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()) return;
    for (const auto& Pair : Root->Values)
    {
        const TSharedPtr<FJsonObject>* O;
        if (!Pair.Value->TryGetObject(O)) continue;
        FNMSSecInfo S;
        (*O)->TryGetStringField(TEXT("sec"), S.Title);
        double Idx = 0.0; (*O)->TryGetNumberField(TEXT("idx"), Idx); S.Idx = (int32)Idx;
        if (!S.Title.IsEmpty()) PartSection.Add(FString(Pair.Key), S);
    }
    UE_LOG(LogTemp, Log, TEXT("NMS UI: деталей с секцией: %d"), PartSection.Num());
}

// === Избранное и Пресеты (хранение в Content/NMSData/favorites.json) ===
// Путь к пользовательским настройкам — ЗАПИСЫВАЕМАЯ папка Saved (работает и в
// редакторе, и в автономном exe; Content в standalone только для чтения).
static FString NMS_UserFile(const TCHAR* Name)
{
    return FPaths::ProjectSavedDir() / TEXT("NMSUser") / Name;
}

void SNMSBuilderUI::LoadFavorites()
{
    FavoriteIDs.Reset(); Presets.Reset();
    FString Json;
    const FString Path = NMS_UserFile(TEXT("favorites.json"));
    if (!FFileHelper::LoadFileToString(Json, *Path)) return;
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()) return;
    const TArray<TSharedPtr<FJsonValue>>* Fav;
    if (Root->TryGetArrayField(TEXT("favorites"), Fav))
        for (const auto& V : *Fav) FavoriteIDs.AddUnique(V->AsString());
    const TSharedPtr<FJsonObject>* Pr;
    if (Root->TryGetObjectField(TEXT("presets"), Pr))
        for (const auto& Pair : (*Pr)->Values)
        {
            const TArray<TSharedPtr<FJsonValue>>* Arr;
            if (Pair.Value->TryGetArray(Arr))
            {
                TArray<FString>& L = Presets.Add(FString(Pair.Key));
                for (const auto& V : *Arr) L.Add(V->AsString());
            }
        }
}

void SNMSBuilderUI::SaveFavorites()
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Fav;
    for (const FString& Id : FavoriteIDs) Fav.Add(MakeShared<FJsonValueString>(Id));
    Root->SetArrayField(TEXT("favorites"), Fav);
    TSharedRef<FJsonObject> Pr = MakeShared<FJsonObject>();
    for (const auto& Pair : Presets)
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        for (const FString& Id : Pair.Value) Arr.Add(MakeShared<FJsonValueString>(Id));
        Pr->SetArrayField(Pair.Key, Arr);
    }
    Root->SetObjectField(TEXT("presets"), Pr);
    FString Out;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Out));
    FFileHelper::SaveStringToFile(Out, *NMS_UserFile(TEXT("favorites.json")));
}

void SNMSBuilderUI::AddToFavorites(const FString& Id)
{
    FavoriteIDs.AddUnique(Id);
    SaveFavorites();
    BuildCategories();           // пересобрать категории (★ Избранное наполнится)
    Invalidate(EInvalidateWidgetReason::Layout);
}

void SNMSBuilderUI::AddToPreset(const FString& Preset, const FString& Id)
{
    Presets.FindOrAdd(Preset).AddUnique(Id);
    SaveFavorites();
    BuildCategories();
    Invalidate(EInvalidateWidgetReason::Layout);
}

// Пользовательские переопределения категорий (порядок наводим сами,
// оригинал из игры не трогаем). Файл: Content/NMSData/user_categories.json
void SNMSBuilderUI::LoadUserCategories()
{
    UserCategory.Reset();
    FString Json;
    const FString Path = NMS_UserFile(TEXT("user_categories.json"));
    if (!FFileHelper::LoadFileToString(Json, *Path)) return;
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()) return;
    for (const auto& Pair : Root->Values)
        UserCategory.Add(FString(Pair.Key), Pair.Value->AsString());
}

void SNMSBuilderUI::SaveUserCategories()
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    for (const auto& Pair : UserCategory) Root->SetStringField(Pair.Key, Pair.Value);
    FString Out;
    FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Out));
    FFileHelper::SaveStringToFile(Out, *NMS_UserFile(TEXT("user_categories.json")));
}

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

void SNMSBuilderUI::BuildCategories()
{
    Categories.Reset();
    PartById.Reset();
    LoadPartVariants();
    LoadPartSections();
    LoadFavorites();
    LoadUserCategories();
    // применить пользовательские переназначения категорий к деталям
    for (const TSharedPtr<FNMSPartData>& P : AllParts)
    {
        if (!P.IsValid()) continue;
        FString Id = P->ObjectID; Id.RemoveFromStart(TEXT("^"));
        if (const FString* C = UserCategory.Find(Id)) P->Category = *C;
    }
    for (const TSharedPtr<FNMSPartData>& P : AllParts)
    {
        if (!P.IsValid()) continue;
        FString Id = P->ObjectID;
        Id.RemoveFromStart(TEXT("^"));
        PartById.Add(Id, P);
    }

    // --- группа «★ Избранное» (первая, наполняется из favorites.json) ---
    {
        TSharedRef<FNMSCategoryGroup> Fav = MakeShared<FNMSCategoryGroup>();
        Fav->Category = TEXT("★ Избранное");
        for (const FString& Id : FavoriteIDs)
            if (const TSharedPtr<FNMSPartData>* P = PartById.Find(Id)) Fav->Parts.Add(*P);
        Categories.Add(Fav);
    }
    // --- группа «Пресеты» (наборы; показывает детали активного пресета) ---
    {
        TSharedRef<FNMSCategoryGroup> Pre = MakeShared<FNMSCategoryGroup>();
        Pre->Category = TEXT("Пресеты");
        const TArray<FString>* Sel = CurrentPreset.IsEmpty() ? nullptr : Presets.Find(CurrentPreset);
        if (Sel)
            for (const FString& Id : *Sel)
                if (const TSharedPtr<FNMSPartData>* P = PartById.Find(Id)) Pre->Parts.Add(*P);
        Categories.Add(Pre);
    }

    TMap<FString, TSharedPtr<FNMSCategoryGroup>> Map;
    for (const TSharedPtr<FNMSPartData>& P : AllParts)
    {
        if (!P.IsValid()) continue;
        TSharedPtr<FNMSCategoryGroup>& G = Map.FindOrAdd(P->Category);
        if (!G.IsValid())
        {
            G = MakeShared<FNMSCategoryGroup>();
            G->Category = P->Category;
            Categories.Add(G);
        }
        G->Parts.Add(P);
    }
    UE_LOG(LogTemp, Log, TEXT("NMS UI: built %d categories"), Categories.Num());
}

#undef LOCTEXT_NAMESPACE
// ======================= SAVE MANAGER реализация =======================
void SNMSBuilderUI::SMRefreshAccounts()
{
    SMAccounts = FNMSSaveFile::GetAccounts();
    SMSlots.Reset(); SMBases.Reset();
    SMAccountIdx = INDEX_NONE; SMSlotIdx = INDEX_NONE;

    // автовыбор: если один аккаунт — берём его
    if (SMAccounts.Num() == 1)
        SMSelectAccount(0);
    else if (SMAccounts.Num() > 1)
    {
        // автовыбор аккаунта с самым свежим сейвом
        int32 BestAcc = 0; double BestTime = -1;
        for (int32 i=0;i<SMAccounts.Num();++i)
        {
            TArray<FNMSSaveSlot> S = FNMSSaveFile::GetSaveSlots(SMAccounts[i]);
            int32 L = FNMSSaveFile::GetLatestSlotIndex(S);
            if (L>=0 && S[L].LastModified > BestTime) { BestTime=S[L].LastModified; BestAcc=i; }
        }
        SMSelectAccount(BestAcc);
    }
    if (SMStatusText.IsValid())
        SMStatusText->SetText(FText::FromString(FString::Printf(
            TEXT("Аккаунтов: %d"), SMAccounts.Num())));
}

void SNMSBuilderUI::SMSelectAccount(int32 Idx)
{
    if (!SMAccounts.IsValidIndex(Idx)) return;
    SMAccountIdx = Idx;
    SMSlots = FNMSSaveFile::GetSaveSlots(SMAccounts[Idx]);
    SMBases.Reset(); SMSlotIdx = INDEX_NONE;

    // АВТОВЫБОР последнего активного слота
    const int32 Latest = FNMSSaveFile::GetLatestSlotIndex(SMSlots);
    if (Latest >= 0) SMSelectSlot(Latest);
}

void SNMSBuilderUI::SMSelectSlot(int32 Idx)
{
    if (!SMSlots.IsValidIndex(Idx)) return;
    SMSlotIdx = Idx;
    SMBases.Reset();

    const FString& Hg = SMSlots[Idx].PrimaryHg.IsEmpty()
        ? SMSlots[Idx].SecondaryHg : SMSlots[Idx].PrimaryHg;
    FNMSSaveFile SF(Hg);
    FString Err;
    if (SF.Load(Err))
    {
        SMBases = SF.ExtractBases();
        UE_LOG(LogTemp, Log, TEXT("NMS SaveMgr: слот %d, баз: %d"),
            SMSlots[Idx].SlotNumber, SMBases.Num());
    }
    else
        UE_LOG(LogTemp, Error, TEXT("NMS SaveMgr: %s"), *Err);
    SMRebuildBaseList();
}

void SNMSBuilderUI::SMLoadBaseIntoEditor(int32 BaseIndex)
{
    if (SMSlotIdx == INDEX_NONE || !SMSlots.IsValidIndex(SMSlotIdx)) return;
    const FString& Hg = SMSlots[SMSlotIdx].PrimaryHg.IsEmpty()
        ? SMSlots[SMSlotIdx].SecondaryHg : SMSlots[SMSlotIdx].PrimaryHg;
    FNMSSaveFile SF(Hg);
    FString Err;
    if (!SF.Load(Err)) { UE_LOG(LogTemp,Error,TEXT("NMS SaveMgr load: %s"),*Err); return; }

    // достаём JSON базы по индексу и грузим её Objects в редактор
    TSharedPtr<FJsonObject> Root = SF.GetJson();
    if (!Root.IsValid()) return;
    const TSharedPtr<FJsonObject>* BC; if(!Root->TryGetObjectField(TEXT("BaseContext"),BC)) return;
    const TSharedPtr<FJsonObject>* PSD; if(!(*BC)->TryGetObjectField(TEXT("PlayerStateData"),PSD)) return;
    const TArray<TSharedPtr<FJsonValue>>* Bases;
    if(!(*PSD)->TryGetArrayField(TEXT("PersistentPlayerBases"),Bases)) return;
    if(!Bases->IsValidIndex(BaseIndex)) return;

    TSharedPtr<FJsonObject> Base = (*Bases)[BaseIndex]->AsObject();
    if(!Base.IsValid()) return;

    // имя/адрес базы -> в свойства
    Base->TryGetStringField(TEXT("Name"), CurrentBaseName);

    // собираем JSON {Objects:[...]} и грузим через существующий импорт
    TSharedPtr<FJsonObject> Wrap = MakeShared<FJsonObject>();
    const TArray<TSharedPtr<FJsonValue>>* Objs;
    if (Base->TryGetArrayField(TEXT("Objects"), Objs))
    {
        Wrap->SetArrayField(TEXT("Objects"), *Objs);
        FString JsonStr;
        TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&JsonStr);
        FJsonSerializer::Serialize(Wrap.ToSharedRef(), W);
        // грузим через тот же путь, что Import from Clipboard
        UNMSBaseManager* Mgr = NewObject<UNMSBaseManager>();
        Mgr->AddToRoot();
        if (Mgr->ImportFromString(JsonStr))
            SpawnFromManager(Mgr);
        Mgr->RemoveFromRoot();
        UE_LOG(LogTemp, Warning, TEXT("NMS SaveMgr: база '%s' загружена (%d деталей)"),
            *CurrentBaseName, Objs->Num());
    }
}

void SNMSBuilderUI::SMRebuildBaseList()
{
    if (!SMBaseListBox.IsValid()) return;
    SMBaseListBox->ClearChildren();

    for (int32 i = 0; i < SMBases.Num(); ++i)
    {
        const FNMSSaveBaseInfo& B = SMBases[i];
        // фильтр Corvette/Основа
        const bool bCorv = (B.BaseType == TEXT("PlayerShipBase"));
        if (bCorv != SMShowCorvettes) continue;
        // фильтр по поиску
        if (!SMSearchText.IsEmpty() &&
            !B.BaseName.Contains(SMSearchText)) continue;

        const int32 BaseIdx = B.BaseIndex;
        const FString Label = B.BaseName.IsEmpty()
            ? FString::Printf(TEXT("(без имени) [%d]"), BaseIdx) : B.BaseName;
        const FString Tip = FString::Printf(TEXT("Деталей: %d  |  Тип: %s  |  Адрес: %s"),
            B.PartCount, *B.BaseType, *B.GalacticAddress);

        const FNMSSaveBaseInfo BCopy = B;
        SMBaseListBox->AddSlot().AutoHeight().Padding(2,1)
        [
            SNew(SButton)
            .ToolTip(SMMakeBaseTooltip(BCopy))   // превью с ФОТО при наведении
            .OnClicked_Lambda([this, BaseIdx]() {
                SMLoadBaseIntoEditor(BaseIdx);
                return FReply::Handled();
            })
            .Content()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                [ SNew(STextBlock).Text(FText::FromString(Label)) ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [ SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("  %d дет."), B.PartCount)))
                    .ColorAndOpacity(FSlateColor(FLinearColor(0.6f,0.6f,0.6f))) ]
                // кнопка привязки скриншота
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4,0,0,0)
                [ SNew(SButton)
                    .ToolTipText(FText::FromString(TEXT("Привязать скриншот базы")))
                    .Text(FText::FromString(SMBaseScreens.Contains(B.BaseName) ? TEXT("\u0444\u043e\u0442\u043e\u2713") : TEXT("\u0444\u043e\u0442\u043e")))
                    .OnClicked_Lambda([this, BName=B.BaseName]() {
                        SMPickScreenshot(BName);
                        return FReply::Handled();
                    }) ]
            ]
        ];
    }
    if (SMStatusText.IsValid())
    {
        int32 Shown = 0;
        for (const auto& B : SMBases)
            if (((B.BaseType==TEXT("PlayerShipBase"))==SMShowCorvettes) &&
                (SMSearchText.IsEmpty()||B.BaseName.Contains(SMSearchText))) Shown++;
        SMStatusText->SetText(FText::FromString(FString::Printf(
            TEXT("%s: %d"), SMShowCorvettes?TEXT("Корветов"):TEXT("Баз"), Shown)));
    }
}

TSharedRef<SWidget> SNMSBuilderUI::BuildSaveManagerPanel()
{
    // загрузка привязок скриншотов + аккаунтов
    SMLoadScreenLinks();
    SMRefreshAccounts();

    return SNew(SVerticalBox)
    // строка: заголовок + кнопка обновить
    + SVerticalBox::Slot().AutoHeight().Padding(4,2)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
        [ SNew(STextBlock).Text(FText::FromString(TEXT("SAVE MANAGER")))
            .ColorAndOpacity(FSlateColor(FLinearColor(1.f,0.78f,0.18f))) ]
        + SHorizontalBox::Slot().AutoWidth()
        [ SNew(SButton).Text(FText::FromString(TEXT("Обновить")))
            .OnClicked_Lambda([this](){ SMRefreshAccounts(); SMRebuildBaseList(); return FReply::Handled(); }) ]
    ]
    // Select Account
    + SVerticalBox::Slot().AutoHeight().Padding(4,2)
    [
        SNew(SComboButton)
        .ButtonContent()[ SNew(STextBlock).Text_Lambda([this](){
            return FText::FromString(SMAccounts.IsValidIndex(SMAccountIdx)
                ? FPaths::GetCleanFilename(SMAccounts[SMAccountIdx])
                : TEXT("Select Account")); }) ]
        .OnGetMenuContent_Lambda([this](){
            FMenuBuilder MB(true, nullptr);
            for (int32 i=0;i<SMAccounts.Num();++i){
                const int32 Idx=i;
                MB.AddMenuEntry(FText::FromString(FPaths::GetCleanFilename(SMAccounts[i])),
                    FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda(
                        [this,Idx](){ SMSelectAccount(Idx); SMRebuildBaseList(); })));
            }
            return MB.MakeWidget();
        })
    ]
    // Select Slot
    + SVerticalBox::Slot().AutoHeight().Padding(4,2)
    [
        SNew(SComboButton)
        .ButtonContent()[ SNew(STextBlock).Text_Lambda([this](){
            return FText::FromString(SMSlots.IsValidIndex(SMSlotIdx)
                ? FString::Printf(TEXT("Slot %d"), SMSlots[SMSlotIdx].SlotNumber)
                : TEXT("Select Save Slot")); }) ]
        .OnGetMenuContent_Lambda([this](){
            FMenuBuilder MB(true, nullptr);
            for (int32 i=0;i<SMSlots.Num();++i){
                const int32 Idx=i;
                MB.AddMenuEntry(FText::FromString(FString::Printf(TEXT("Slot %d"), SMSlots[i].SlotNumber)),
                    FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda(
                        [this,Idx](){ SMSelectSlot(Idx); })));
            }
            return MB.MakeWidget();
        })
    ]
    // переключатель Основа / Корветы
    + SVerticalBox::Slot().AutoHeight().Padding(4,2)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.f)
        [ SNew(SButton).Text(FText::FromString(TEXT("Основа")))
            .OnClicked_Lambda([this](){ SMShowCorvettes=false; SMRebuildBaseList(); return FReply::Handled(); }) ]
        + SHorizontalBox::Slot().FillWidth(1.f)
        [ SNew(SButton).Text(FText::FromString(TEXT("Корветы")))
            .OnClicked_Lambda([this](){ SMShowCorvettes=true; SMRebuildBaseList(); return FReply::Handled(); }) ]
    ]
    // поиск
    + SVerticalBox::Slot().AutoHeight().Padding(4,2)
    [
        SNew(SEditableTextBox)
        .HintText(FText::FromString(TEXT("Поиск базы...")))
        .OnTextChanged_Lambda([this](const FText& T){ SMSearchText=T.ToString(); SMRebuildBaseList(); })
    ]
    // статус
    + SVerticalBox::Slot().AutoHeight().Padding(4,2)
    [ SAssignNew(SMStatusText, STextBlock).Text(FText::FromString(TEXT(""))) ]
    // список баз (прокрутка)
    + SVerticalBox::Slot().FillHeight(1.f).Padding(4,2)
    [
        SNew(SScrollBox)
        + SScrollBox::Slot()[ SAssignNew(SMBaseListBox, SVerticalBox) ]
    ];
}

// ============ СКРИНШОТЫ БАЗ (превью при наведении) ============
void SNMSBuilderUI::SMLoadScreenLinks()
{
    SMBaseScreens.Reset();
    const FString F = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("NMSUser/base_screenshots.json"));
    FString Raw;
    if (!FFileHelper::LoadFileToString(Raw, *F)) return;
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Raw);
    if (FJsonSerializer::Deserialize(R, Obj) && Obj.IsValid())
        for (const auto& Pair : Obj->Values)
        {
            FString V; if (Pair.Value->TryGetString(V))
                SMBaseScreens.Add(FString(Pair.Key), V);
        }
}

void SNMSBuilderUI::SMSaveScreenLinks()
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    for (const auto& Pair : SMBaseScreens)
        Obj->SetStringField(Pair.Key, Pair.Value);
    FString Out;
    TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), W);
    const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NMSUser"));
    IFileManager::Get().MakeDirectory(*Dir, true);
    FFileHelper::SaveStringToFile(Out, *FPaths::Combine(Dir, TEXT("base_screenshots.json")));
}

const FSlateBrush* SNMSBuilderUI::SMGetScreenBrush(const FString& BaseName)
{
    const FString* PathP = SMBaseScreens.Find(BaseName);
    if (!PathP || PathP->IsEmpty() || !FPaths::FileExists(*PathP)) return nullptr;

    if (TSharedPtr<FSlateDynamicImageBrush>* Cached = SMScreenBrushes.Find(BaseName))
        return Cached->Get();

    // загружаем картинку как brush (превью ~320x180)
    TSharedPtr<FSlateDynamicImageBrush> Brush =
        MakeShareable(new FSlateDynamicImageBrush(FName(**PathP), FVector2D(320, 180)));
    SMScreenBrushes.Add(BaseName, Brush);
    return Brush.Get();
}

void SNMSBuilderUI::SMPickScreenshot(const FString& BaseName)
{
    IDesktopPlatform* DP = FDesktopPlatformModule::Get();
    if (!DP) return;
    const void* H = nullptr;
    if (FSlateApplication::IsInitialized())
    {
        TSharedPtr<SWindow> W = FSlateApplication::Get().GetActiveTopLevelWindow();
        if (W.IsValid() && W->GetNativeWindow().IsValid())
            H = W->GetNativeWindow()->GetOSWindowHandle();
    }
    // стартовая папка — скриншоты Steam NMS
    FString StartDir = TEXT("C:/Program Files (x86)/Steam/userdata");
    TArray<FString> Picked;
    if (DP->OpenFileDialog(H, TEXT("Выбрать скриншот базы"), StartDir, TEXT(""),
        TEXT("Изображения (*.jpg;*.png)|*.jpg;*.png"), EFileDialogFlags::None, Picked)
        && Picked.Num() > 0)
    {
        SMBaseScreens.Add(BaseName, Picked[0]);
        SMScreenBrushes.Remove(BaseName); // сброс кэша
        SMSaveScreenLinks();
        SMRebuildBaseList();
    }
}

TSharedRef<SToolTip> SNMSBuilderUI::SMMakeBaseTooltip(const FNMSSaveBaseInfo& B)
{
    const FString Tip = FString::Printf(TEXT("Деталей: %d  |  Тип: %s  |  Адрес: %s"),
        B.PartCount, *B.BaseType, *B.GalacticAddress);
    const FSlateBrush* Screen = SMGetScreenBrush(B.BaseName);

    TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    // фото (если привязано)
    if (Screen)
    {
        Box->AddSlot().AutoHeight().Padding(2)
        [
            SNew(SBox).WidthOverride(320).HeightOverride(180)
            [ SNew(SImage).Image(Screen) ]
        ];
    }
    // имя базы
    Box->AddSlot().AutoHeight().Padding(2)
    [ SNew(STextBlock).Text(FText::FromString(B.BaseName))
        .ColorAndOpacity(FSlateColor(FLinearColor(1.f,0.85f,0.3f))) ];
    // инфо
    Box->AddSlot().AutoHeight().Padding(2)
    [ SNew(STextBlock).Text(FText::FromString(Tip)) ];
    if (!Screen)
        Box->AddSlot().AutoHeight().Padding(2)
        [ SNew(STextBlock)
            .Text(FText::FromString(TEXT("(ПКМ — привязать скриншот)")))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.55f,0.55f,0.55f))) ];

    return SNew(SToolTip)[ Box ];
}

// =================== ПРАВЫЕ ПАНЕЛИ: TRANSFORM выбранной ===================
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
