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

// Точное сравнение деталей (для детекции изменений в Undo/Redo).
inline bool operator==(const FNMSPlacedPart& A, const FNMSPlacedPart& B)
{
    return A.ObjectID == B.ObjectID
        && A.UserData == B.UserData
        && A.Timestamp == B.Timestamp
        && A.Transform.GetLocation()  == B.Transform.GetLocation()
        && A.Transform.GetRotation()  == B.Transform.GetRotation()
        && A.Transform.GetScale3D()   == B.Transform.GetScale3D();
}
inline bool operator!=(const FNMSPlacedPart& A, const FNMSPlacedPart& B) { return !(A == B); }

// Документы равны, если совпадает СЦЕНА (список деталей по порядку).
// Имя/адрес базы на состояние сцены не влияют — в сравнение не входят.
inline bool operator==(const FNMSBaseDocument& A, const FNMSBaseDocument& B)
{
    if (A.Parts.Num() != B.Parts.Num()) return false;
    for (int32 i = 0; i < A.Parts.Num(); ++i)
        if (A.Parts[i] != B.Parts[i]) return false;
    return true;
}
inline bool operator!=(const FNMSBaseDocument& A, const FNMSBaseDocument& B) { return !(A == B); }
