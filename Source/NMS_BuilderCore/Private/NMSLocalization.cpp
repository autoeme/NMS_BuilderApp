#include "NMSLocalization.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

// ===================== ЛОКАЛИЗАЦИЯ (17 языков) =====================
// Глобалы и приватные хелперы живут только в этом TU — наружу торчат
// лишь публичные функции из NMSLocalization.h.
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
FText NMS_T(const FString& Key, const FString& Fallback)
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
FString NMS_PartName(const FString& Oid, const FString& Fallback)
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
FString NMS_ColName(int32 Idx, const FString& Fallback)
{
    NMS_LoadI18n();
    if (const TSharedPtr<FJsonObject>* O = GNmsCols.Find(Idx))
        return NMS_Pick(*O, Fallback);
    return Fallback;
}
void NMS_PalTQ(int32 Idx, FLinearColor& OutT, FLinearColor& OutQ)
{
    NMS_LoadI18n();
    if (const FLinearColor* C = GNmsPalT.Find(Idx)) OutT = *C;
    if (const FLinearColor* C = GNmsPalQ.Find(Idx)) OutQ = *C;
}
void NMS_SetLang(const FString& Code)
{
    NMS_LoadI18n();
    GNmsLang = Code;
    const FString P = FPaths::ProjectSavedDir() / TEXT("NMSUser");
    IFileManager::Get().MakeDirectory(*P, true);
    FFileHelper::SaveStringToFile(Code, *(P / TEXT("lang.txt")));
}
const TArray<TPair<FString, FString>>& NMS_Languages()
{
    NMS_LoadI18n();
    return GNmsLangs;
}
// ===================================================================
