// NMSSaveFile.h — чтение/запись сейва игры No Man's Sky (.hg), порт save_file.py
// Формат: блоки [16-байт header (MAGIC 0xFEEDA1E5, comp_size, decomp_size, 0)]
//         + LZ4-block сжатые данные. Склейка -> JSON между первой '{' и последней '}'.
// Ключи JSON обфусцированы -> деобфускация по Content/NMSData/save_map_dictionary.json.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Краткая инфа о базе из сейва (для списка выбора).
struct FNMSSaveBaseInfo
{
    int32   BaseIndex = -1;
    FString BaseName;
    FString GalacticAddress;
    FString BaseType;       // HomePlanetBase / PlayerShipBase / ...
    int64   UserData = 0;
    int32   PartCount = 0;  // число деталей (Objects[]) — для превью
};

// Слот сейва: пара save.hg + save2.hg = слот 1 и т.д.
struct FNMSSaveSlot
{
    int32   SlotNumber = 0;       // 1, 2, 3...
    FString PrimaryHg;            // основной .hg (save.hg, save3.hg...)
    FString SecondaryHg;          // парный (save2.hg, save4.hg...)
    FString SaveName;             // имя сейва
    double  LastModified = 0.0;   // для автовыбора последнего активного
};

class FNMSSaveFile
{
public:
    // Путь к .hg
    explicit FNMSSaveFile(const FString& InPath) : Path(InPath) {}

    // Загрузить и деобфусцировать -> JSON (english-ключи). true при успехе.
    bool Load(FString& OutError);

    // Список баз игрока из загруженного JSON.
    TArray<FNMSSaveBaseInfo> ExtractBases() const;

    // Доступ к корневому JSON (english-ключи).
    TSharedPtr<FJsonObject> GetJson() const { return JsonData; }

    // Сохранить (с бэкапом в /blender_backup). Переобфусцирует обратно.
    bool Save(FString& OutError, const FString& OutputPath = TEXT(""));

    // --- статические утилиты поиска сейвов ---
    static FString GetRootSaveFolder();                       // %AppData%/HelloGames/NMS
    static TArray<FString> GetAccounts();                     // папки st_*
    static TArray<FString> GetHgFilesInFolder(const FString& Folder);
    // Слоты аккаунта (пары save.hg). Отсортированы по номеру.
    static TArray<FNMSSaveSlot> GetSaveSlots(const FString& Account);
    // Индекс последнего изменённого слота (для автовыбора). -1 если нет.
    static int32 GetLatestSlotIndex(const TArray<FNMSSaveSlot>& Slots);

private:
    FString Path;
    TSharedPtr<FJsonObject> JsonData;   // english-ключи (после деобфускации)

    // словарь obf<->eng (грузится один раз)
    static void EnsureDictionary();
    static TMap<FString,FString> ObfToEng;  // 'F?0' -> 'PersistentPlayerBases'
    static TMap<FString,FString> EngToObf;  // обратный
    static bool bDictLoaded;
};
