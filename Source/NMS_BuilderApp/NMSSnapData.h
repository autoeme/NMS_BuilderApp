#pragma once

#include "CoreMinimal.h"

/**
 * Данные снапа (точек стыковки) деталей No Man's Sky.
 * Источник правды — игра: Content/NMSData/snapping_info.json + snapping_pairs.json,
 * извлечённые из сцен *_snappoints.scene.mbin.
 *
 * snapping_info.json:  ГРУППА -> { parts:[ID...], snap_points:{ ИМЯ:{matrix 4x4, opposite} } }
 *   matrix — локальный трансформ точки относительно центра детали (система NMS, метры).
 *   Загрузчик сразу конвертит его в UE-пространство меша (Z-up, сантиметры).
 * snapping_pairs.json: ГРУППА_A -> ГРУППА_B -> [ "точки A", "точки B" ]
 *   какие точки на детали A валидны для стыковки с деталью B (и наоборот).
 */

// Одна именованная точка стыковки детали (в UE-пространстве меша).
struct FNMSSnapPoint
{
    FString    Name;
    FString    Opposite;          // имя ответной точки (EAST<->WEST, TOP<->BOTTOM); может быть пустым
    FTransform LocalXform;        // локальный трансформ точки в меше детали (Z-up, см)
};

// Группа деталей с общим набором точек стыковки.
struct FNMSSnapGroup
{
    TArray<FString>        Parts;
    TArray<FNMSSnapPoint>  Points;
    const FNMSSnapPoint*   FindPoint(const FString& InName) const;
};

class FNMSSnapData
{
public:
    static FNMSSnapData& Get();

    // Лениво грузит оба JSON из Content/NMSData (повторные вызовы — no-op).
    void EnsureLoaded();
    bool IsLoaded() const { return bLoaded; }

    // Группа детали по её ObjectID (префикс '^' игнорируется). nullptr — деталь не снапится.
    const FString*        GroupForPart(const FString& PartId) const;
    const FNMSSnapGroup*  GetGroup(const FString& GroupName) const;

    // Пара (существующая деталь GroupOld <-> ставимая GroupNew).
    // Возвращает имена валидных точек на старой и новой детали. false — стыковка не разрешена.
    bool GetPair(const FString& GroupOld, const FString& GroupNew,
                 TArray<FString>& OutOldNames, TArray<FString>& OutNewNames) const;

private:
    bool bLoaded = false;

    TMap<FString, FNMSSnapGroup> Groups;       // имя группы -> группа
    TMap<FString, FString>       PartToGroup;   // ObjectID -> имя группы
    // Pairs[A][B] = (точки A, точки B)
    TMap<FString, TMap<FString, TPair<TArray<FString>, TArray<FString>>>> Pairs;

    void LoadInfo(const FString& Path);
    void LoadPairs(const FString& Path);
};
