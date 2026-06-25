// Чистая доменная модель базы NMS — источник правды для редактора.
//
// СОЗНАТЕЛЬНО без зависимостей от Slate / UnrealEd / AActor / UWorld / файлов:
// только Core-типы (FString, FTransform, TArray). Сцена (актёры), ввод-вывод
// и UI работают ЧЕРЕЗ адаптеры (см. NMSDocumentAdapter.h), а не наоборот.
//
// Это «Этап 4» из docs/code review: BaseDocument как модель, актёры — проекции.
#pragma once

#include "CoreMinimal.h"

// Одна размещённая деталь базы.
struct FNMSPlacedPart
{
    FString    ObjectID;                 // ^WALLA и т.п. (id детали игры)
    FTransform Transform = FTransform::Identity; // мировой трансформ редактора (Z-up, см)
    int64      UserData  = 0;            // битовые флаги детали (цвет/вариант и пр.)
    float      Timestamp = 0.f;         // отметка времени из сейва
};

// База целиком: метаданные + список деталей.
struct FNMSBaseDocument
{
    FString Name;
    FString GalacticAddress;
    TArray<FNMSPlacedPart> Parts;

    void Reset()
    {
        Name.Empty();
        GalacticAddress.Empty();
        Parts.Reset();
    }

    int32 Num() const { return Parts.Num(); }
    bool IsEmpty() const { return Parts.Num() == 0; }
};
