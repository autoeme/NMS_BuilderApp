// NMSSaveFile.cpp — порт save_file.py + save_editor_utils.py на C++/UE.
#include "NMSSaveFile.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Compression/lz4.h"

TMap<FString,FString> FNMSSaveFile::ObfToEng;
TMap<FString,FString> FNMSSaveFile::EngToObf;
bool FNMSSaveFile::bDictLoaded = false;

static const uint32 NMS_HG_MAGIC = 0xFEEDA1E5u;
static const int32  NMS_HG_CHUNK = 0x80000;

void FNMSSaveFile::EnsureDictionary()
{
    if (bDictLoaded) return;
    bDictLoaded = true;
    const FString DictPath = FPaths::Combine(
        FPaths::ProjectContentDir(), TEXT("NMSData/save_map_dictionary.json"));
    FString Raw;
    if (!FFileHelper::LoadFileToString(Raw, *DictPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("NMS SaveFile: no dict: %s"), *DictPath);
        return;
    }
    TSharedPtr<FJsonObject> Obj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
    {
        for (const auto& Pair : Obj->Values)
        {
            const FString Obf = FString(Pair.Key);
            FString Eng;
            if (Pair.Value.IsValid() && Pair.Value->TryGetString(Eng))
            {
                ObfToEng.Add(Obf, Eng);
                EngToObf.Add(Eng, Obf);
            }
        }
    }
    UE_LOG(LogTemp, Log, TEXT("NMS SaveFile: dict pairs: %d"), ObfToEng.Num());
}

TArray<FString> FNMSSaveFile::GetHgFilesInFolder(const FString& Folder)
{
    TArray<FString> Out;
    IFileManager& FM = IFileManager::Get();
    TArray<FString> Found;
    FM.FindFiles(Found, *FPaths::Combine(Folder, TEXT("save*.hg")), true, false);
    for (const FString& F : Found)
        Out.Add(FPaths::Combine(Folder, F));
    return Out;
}

TArray<FString> FNMSSaveFile::GetAccounts(const FString& RootSaveFolder)
{
    TArray<FString> Out;
    const FString Root = RootSaveFolder;
    if (Root.IsEmpty()) return Out;
    IFileManager& FM = IFileManager::Get();
    TArray<FString> Dirs;
    FM.FindFiles(Dirs, *FPaths::Combine(Root, TEXT("*")), false, true);
    for (const FString& D : Dirs)
    {
        if (D.StartsWith(TEXT("st_")))
        {
            const FString Full = FPaths::Combine(Root, D);
            if (GetHgFilesInFolder(Full).Num() > 0)
                Out.Add(Full);
        }
    }
    return Out;
}

// --- рекурсивный перевод ключей JSON ---
static TSharedPtr<FJsonValue> TranslateValue(
    const TSharedPtr<FJsonValue>& In, const TMap<FString,FString>& Map);

static TSharedPtr<FJsonObject> TranslateObject(
    const TSharedPtr<FJsonObject>& In, const TMap<FString,FString>& Map)
{
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    for (const auto& Pair : In->Values)
    {
        const FString KeyStr = FString(Pair.Key);
        const FString* Mapped = Map.Find(KeyStr);
        FString NewKey = Mapped ? *Mapped : KeyStr;
        Out->SetField(NewKey, TranslateValue(Pair.Value, Map));
    }
    return Out;
}

static TSharedPtr<FJsonValue> TranslateValue(
    const TSharedPtr<FJsonValue>& In, const TMap<FString,FString>& Map)
{
    if (!In.IsValid()) return In;
    if (In->Type == EJson::Object)
        return MakeShared<FJsonValueObject>(TranslateObject(In->AsObject(), Map));
    if (In->Type == EJson::Array)
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        for (const auto& E : In->AsArray())
            Arr.Add(TranslateValue(E, Map));
        return MakeShared<FJsonValueArray>(Arr);
    }
    return In;
}

bool FNMSSaveFile::Load(FString& OutError)
{
    EnsureDictionary();
    TArray<uint8> Raw;
    if (!FFileHelper::LoadFileToArray(Raw, *Path))
    { OutError = FString::Printf(TEXT("Не открыть: %s"), *Path); return false; }

    TArray<uint8> Dec;
    int32 Offset = 0; const int32 N = Raw.Num();
    while (Offset < N)
    {
        if (Offset + 16 > N) { OutError = TEXT("Битый header"); return false; }
        uint32 Magic, CompSize, DecompSize, Zero;
        FMemory::Memcpy(&Magic,      Raw.GetData()+Offset,    4);
        FMemory::Memcpy(&CompSize,   Raw.GetData()+Offset+4,  4);
        FMemory::Memcpy(&DecompSize, Raw.GetData()+Offset+8,  4);
        FMemory::Memcpy(&Zero,       Raw.GetData()+Offset+12, 4);
        if (Magic != NMS_HG_MAGIC)
        { OutError = FString::Printf(TEXT("magic @%d 0x%08X"), Offset, Magic); return false; }
        Offset += 16;
        if (Offset + (int32)CompSize > N) { OutError = TEXT("блок за границей"); return false; }
        const int32 Start = Dec.Num();
        Dec.AddUninitialized((int32)DecompSize);
        const int32 Got = LZ4_decompress_safe(
            (const char*)(Raw.GetData()+Offset),
            (char*)(Dec.GetData()+Start),
            (int32)CompSize, (int32)DecompSize);
        if (Got < 0) { OutError = FString::Printf(TEXT("LZ4 err @%d"), Offset); return false; }
        Offset += (int32)CompSize;
    }

    int32 JS = INDEX_NONE, JE = INDEX_NONE;
    for (int32 i=0;i<Dec.Num();++i){ if (Dec[i]=='{'){ JS=i; break; } }
    for (int32 i=Dec.Num()-1;i>=0;--i){ if (Dec[i]=='}'){ JE=i; break; } }
    if (JS==INDEX_NONE || JE<=JS) { OutError = TEXT("нет JSON"); return false; }

    FString JsonStr;
    FFileHelper::BufferToString(JsonStr, Dec.GetData()+JS, JE-JS+1);

    TSharedPtr<FJsonObject> ObfObj;
    TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(R, ObfObj) || !ObfObj.IsValid())
    { OutError = TEXT("парс JSON"); return false; }

    JsonData = TranslateObject(ObfObj, ObfToEng);
    return true;
}

TArray<FNMSSaveBaseInfo> FNMSSaveFile::ExtractBases() const
{
    TArray<FNMSSaveBaseInfo> Out;
    if (!JsonData.IsValid()) return Out;

    // BaseContext -> PlayerStateData -> PersistentPlayerBases[]
    const TSharedPtr<FJsonObject>* BaseCtx;
    if (!JsonData->TryGetObjectField(TEXT("BaseContext"), BaseCtx)) return Out;
    const TSharedPtr<FJsonObject>* PSD;
    if (!(*BaseCtx)->TryGetObjectField(TEXT("PlayerStateData"), PSD)) return Out;
    const TArray<TSharedPtr<FJsonValue>>* Bases;
    if (!(*PSD)->TryGetArrayField(TEXT("PersistentPlayerBases"), Bases)) return Out;

    int32 Index = 0;
    for (const auto& V : *Bases)
    {
        const TSharedPtr<FJsonObject> B = V->AsObject();
        if (!B.IsValid()) { ++Index; continue; }

        // BaseType.PersistentBaseTypes
        FString BType;
        const TSharedPtr<FJsonObject>* BT;
        if (B->TryGetObjectField(TEXT("BaseType"), BT))
            (*BT)->TryGetStringField(TEXT("PersistentBaseTypes"), BType);

        if (BType == TEXT("ExternalPlanetBase")) break; // как в плагине

        FNMSSaveBaseInfo Info;
        Info.BaseIndex = Index;
        B->TryGetStringField(TEXT("Name"), Info.BaseName);
        Info.BaseType = BType;
        // GalacticAddress может быть числом или строкой
        if (const TSharedPtr<FJsonValue>* GAp = B->Values.Find(TEXT("GalacticAddress")))
        {
            const TSharedPtr<FJsonValue>& GA = *GAp;
            if (GA.IsValid())
                Info.GalacticAddress = (GA->Type==EJson::String) ? GA->AsString()
                                      : FString::Printf(TEXT("%lld"), (int64)GA->AsNumber());
        }
        double UD = 0; B->TryGetNumberField(TEXT("UserData"), UD);
        Info.UserData = (int64)UD;

        // число деталей для превью
        const TArray<TSharedPtr<FJsonValue>>* ObjArr;
        if (B->TryGetArrayField(TEXT("Objects"), ObjArr))
            Info.PartCount = ObjArr->Num();

        if (BType == TEXT("HomePlanetBase") || BType == TEXT("PlayerShipBase"))
            Out.Add(Info);
        ++Index;
    }
    return Out;
}

bool FNMSSaveFile::Save(FString& OutError, const FString& OutputPath)
{
    if (!JsonData.IsValid()) { OutError = TEXT("нет данных"); return false; }
    EnsureDictionary();

    // бэкап оригинала
    const FString Target = OutputPath.IsEmpty() ? Path : OutputPath;
    {
        const FString Dir = FPaths::GetPath(Path);
        const FString Name = FPaths::GetCleanFilename(Path);
        const FString BkDir = FPaths::Combine(Dir, TEXT("blender_backup"));
        IFileManager::Get().MakeDirectory(*BkDir, true);
        const FString Bk = FPaths::Combine(BkDir, Name + TEXT(".blender.bak"));
        IFileManager::Get().Copy(*Bk, *Path);
    }

    // переобфускация ключей обратно
    TSharedPtr<FJsonObject> Obf = TranslateObject(JsonData, EngToObf);

    // сериализация в компактный JSON
    FString JsonStr;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonStr);
    FJsonSerializer::Serialize(Obf.ToSharedRef(), W);

    // UTF-8 байты
    FTCHARToUTF8 Utf8(*JsonStr);
    const uint8* Src = (const uint8*)Utf8.Get();
    const int32 SrcLen = Utf8.Length();

    // блоки LZ4
    TArray<uint8> OutBytes;
    for (int32 i=0;i<SrcLen;i+=NMS_HG_CHUNK)
    {
        const int32 ChunkLen = FMath::Min(NMS_HG_CHUNK, SrcLen-i);
        const int32 Bound = LZ4_compressBound(ChunkLen);
        TArray<uint8> Comp; Comp.AddUninitialized(Bound);
        const int32 CompLen = LZ4_compress_default(
            (const char*)(Src+i), (char*)Comp.GetData(), ChunkLen, Bound);
        if (CompLen <= 0) { OutError = TEXT("LZ4 compress fail"); return false; }
        uint32 Hdr[4] = { NMS_HG_MAGIC, (uint32)CompLen, (uint32)ChunkLen, 0u };
        OutBytes.Append((uint8*)Hdr, 16);
        OutBytes.Append(Comp.GetData(), CompLen);
    }

    if (!FFileHelper::SaveArrayToFile(OutBytes, *Target))
    { OutError = TEXT("не записать файл"); return false; }
    return true;
}

// --- слоты аккаунта: пары save.hg + save2.hg ---
TArray<FNMSSaveSlot> FNMSSaveFile::GetSaveSlots(const FString& Account)
{
    TArray<FNMSSaveSlot> Slots;
    TArray<FString> Hgs = GetHgFilesInFolder(Account);
    // карта номер->путь. save.hg = 1, save2.hg = 2, save3.hg = 3...
    // слот N = пара (2N-1, 2N): save.hg+save2.hg=слот1, save3+save4=слот2
    TMap<int32,FString> ByNumber;
    for (const FString& F : Hgs)
    {
        const FString Name = FPaths::GetCleanFilename(F); // save.hg / save2.hg
        FString Digits;
        for (const TCHAR C : Name) { if (FChar::IsDigit(C)) Digits.AppendChar(C); }
        const int32 Num = Digits.IsEmpty() ? 1 : FCString::Atoi(*Digits);
        ByNumber.Add(Num, F);
    }
    // собираем слоты
    TArray<int32> Nums; ByNumber.GetKeys(Nums); Nums.Sort();
    for (int32 Num : Nums)
    {
        if (Num % 2 == 0)
        {
            FNMSSaveSlot S;
            S.SlotNumber  = Num / 2;
            S.SecondaryHg = ByNumber[Num];
            const int32 PrimNum = Num - 1;
            if (ByNumber.Contains(PrimNum)) S.PrimaryHg = ByNumber[PrimNum];
            // время изменения основного
            const FString Active = S.PrimaryHg.IsEmpty() ? S.SecondaryHg : S.PrimaryHg;
            S.LastModified = IFileManager::Get().GetTimeStamp(*Active).ToUnixTimestamp();
            Slots.Add(S);
        }
    }
    // если только save.hg (нечётное, без пары) — тоже слот 1
    if (Slots.Num() == 0 && ByNumber.Contains(1))
    {
        FNMSSaveSlot S; S.SlotNumber = 1; S.PrimaryHg = ByNumber[1];
        S.LastModified = IFileManager::Get().GetTimeStamp(*S.PrimaryHg).ToUnixTimestamp();
        Slots.Add(S);
    }
    Slots.Sort([](const FNMSSaveSlot& A, const FNMSSaveSlot& B){ return A.SlotNumber < B.SlotNumber; });
    return Slots;
}

int32 FNMSSaveFile::GetLatestSlotIndex(const TArray<FNMSSaveSlot>& Slots)
{
    int32 Best = -1; double BestTime = -1.0;
    for (int32 i = 0; i < Slots.Num(); ++i)
        if (Slots[i].LastModified > BestTime) { BestTime = Slots[i].LastModified; Best = i; }
    return Best;
}
