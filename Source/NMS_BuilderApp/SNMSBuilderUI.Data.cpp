// Часть класса SNMSBuilderUI (разнесение по TU, Часть C плана).
// Включения общие для всех TU класса — см. SNMSBuilderUI_Internal.h.
#include "SNMSBuilderUI_Internal.h"

#define LOCTEXT_NAMESPACE "NMSBuilder"

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

// ======================= SAVE MANAGER реализация =======================
#undef LOCTEXT_NAMESPACE
