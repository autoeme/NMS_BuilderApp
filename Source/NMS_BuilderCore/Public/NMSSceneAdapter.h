// SceneAdapter: чистый маппинг доменной детали -> «как её рисовать».
//
// Возвращает ОПИСАНИЕ (данные), не трогая AActor/UWorld/материалы — их
// создаёт editor-слой (SpawnFromManager) уже по этому описанию. Так доменная
// логика (резолв цвета детали, декодирование палитры из UserData) становится
// чистой и тестируемой, без сцены.
#pragma once

#include "CoreMinimal.h"
#include "NMSBaseDocument.h"

// Разрешённый визуал детали: 4 зоны покраски как в игре.
struct FNMSPartRenderInfo
{
    FString      ObjectID;
    FTransform   Transform = FTransform::Identity;
    FLinearColor Color1 = FLinearColor::White;   // основной  (материал-параметр Color)
    FLinearColor Color2 = FLinearColor::White;   // вторичный (материал-параметр Color2)
    // 4 зоны покраски как в игре (материал-параметры ZoneCol0..3).
    FLinearColor ZoneA = FLinearColor::White;    // зона 0
    FLinearColor ZoneB = FLinearColor::White;    // зона 1
    FLinearColor ZoneC = FLinearColor::White;    // зона 2 (палитра T)
    FLinearColor ZoneD = FLinearColor::White;    // зона 3 (палитра Q)
    bool bPaletteColor = false;                  // true: цвет взят из палитры, не дефолт
};

// Маска бит индекса цвета в UserData (NMS пакует индекс палитры в эти биты).
static constexpr uint32 NMS_COLOUR_INDEX_MASK = 0xFCFEFFu;

// Извлечь индекс палитры из UserData детали.
inline int32 NMS_ColourIndexFromUserData(int64 UserData)
{
    return (int32)((uint32)UserData & NMS_COLOUR_INDEX_MASK);
}

// Решить визуал детали: дефолтные цвета по категории + переопределение палитрой
// из UserData. Category берётся из каталога деталей (editor-слой передаёт её).
NMS_BUILDERCORE_API FNMSPartRenderInfo NMS_ResolvePartRender(const FNMSPlacedPart& Part, const FString& Category);
