// Локализация UI билдера NMS (17 языков). Публичное API.
// Глобалы и загрузка остаются приватными внутри NMSLocalization.cpp.
#pragma once

#include "CoreMinimal.h"

// Локализованный UI-текст по ключу (с фолбэком, если перевода нет).
NMS_BUILDERCORE_API FText NMS_T(const FString& Key, const FString& Fallback);

// Имя детали по ObjectID.
NMS_BUILDERCORE_API FString NMS_PartName(const FString& Oid, const FString& Fallback);

// Имя цвета палитры по индексу.
NMS_BUILDERCORE_API FString NMS_ColName(int32 Idx, const FString& Fallback);

// Третичный/четвертичный цвета палитры по индексу (из userdata_palette.json).
NMS_BUILDERCORE_API void NMS_PalTQ(int32 Idx, FLinearColor& OutT, FLinearColor& OutQ);

// Сменить язык (сохраняется в Saved/NMSUser/lang.txt).
NMS_BUILDERCORE_API void NMS_SetLang(const FString& Code);

// Список доступных языков: пары (code, native). Гарантирует загрузку i18n.
NMS_BUILDERCORE_API const TArray<TPair<FString, FString>>& NMS_Languages();
