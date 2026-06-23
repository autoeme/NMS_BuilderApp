#pragma once

#include "CoreMinimal.h"

/** Одна деталь NMS, извлечённая из .blend (математика — как у Blender-плагина). */
struct FNMSBlendPart
{
    FString ObjectID;            // с "^"
    FVector Position;            // метры NMS (как в сейве)
    FVector Up;                  // несёт масштаб
    FVector At;                  // нормализован
    int32   UserData  = 0;
    int64   Timestamp = 0;
};

namespace NMSBlend
{
    /**
     * Читает .blend НАПРЯМУЮ (без Blender): zstd-распаковка + разбор DNA.
     * Поддержка: Blender 2.8x–5.x (старый 12-байтовый и новый 17-байтовый заголовок).
     * Детали = объекты со свойством ObjectID (как ставит NMS-плагин).
     */
    bool ReadParts(const FString& FilePath, TArray<FNMSBlendPart>& Out, FString& OutError);
}
