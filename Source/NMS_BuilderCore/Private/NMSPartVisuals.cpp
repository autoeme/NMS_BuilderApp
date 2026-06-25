#include "NMSPartVisuals.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

// Точные дефолтные пары цветов из таблицы игры (part_colors.json, 1101 деталь).
bool NMS_GamePartColors(const FString& ObjectID, FLinearColor& P, FLinearColor& S)
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

FLinearColor NMS_DefaultPartColor(const FString& Category, const FString& ObjectID)
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

// Вторичный цвет (металл/канты) — оригинал игры из карты дефолтов.
FLinearColor NMS_DefaultPartColor2(const FString& Category, const FString& ObjectID)
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
void NMS_PartTexture(const FString& Category, const FString& ObjectID,
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
const TArray<FNMSTexSet>* NMS_PartSlots(const FString& ObjectID)
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

// Дефолтная пара цветов палитры по индексу (userdata_palette.json).
bool NMS_ColourFromIndex(int32 Index, FLinearColor& OutP, FLinearColor& OutS)
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
