// Цвета / текстуры / слоты деталей NMS (stateless, кэши живут в .cpp).
// Вынесено из SNMSBuilderUI.cpp без изменения поведения.
#pragma once

#include "CoreMinimal.h"

// Набор текстур одного материал-слота детали.
struct FNMSTexSet { FString Tex, Mask, Norm, Masks, Occ, Mat; bool bUnlit = false; bool bGlass = false; };

// Точные дефолтные пары цветов из таблицы игры (part_colors.json, 1101 деталь).
NMS_BUILDERCORE_API bool NMS_GamePartColors(const FString& ObjectID, FLinearColor& P, FLinearColor& S);

// Родной цвет детали по категории.
NMS_BUILDERCORE_API FLinearColor NMS_DefaultPartColor(const FString& Category, const FString& ObjectID);

// Вторичный цвет (металл/канты) — оригинал игры из карты дефолтов.
NMS_BUILDERCORE_API FLinearColor NMS_DefaultPartColor2(const FString& Category, const FString& ObjectID);

// Точные игровые текстуры детали — из карты part_textures.json.
NMS_BUILDERCORE_API void NMS_PartTexture(const FString& Category, const FString& ObjectID,
                     FString& OutTex, FString& OutMask, FString& OutNorm,
                     FString& OutMasks, FString& OutOcc);

// Текстуры ПО СЛОТАМ: каждый материал-узел детали -> своя текстура (part_slots.json).
NMS_BUILDERCORE_API const TArray<FNMSTexSet>* NMS_PartSlots(const FString& ObjectID);

// Дефолтная пара цветов палитры по индексу (userdata_palette.json).
NMS_BUILDERCORE_API bool NMS_ColourFromIndex(int32 Index, FLinearColor& OutP, FLinearColor& OutS);
