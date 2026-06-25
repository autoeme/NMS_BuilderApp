#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NMSBaseManager.generated.h"

/** Один размещённый объект базы, разобранный из сейва игры. */
USTRUCT(BlueprintType)
struct FNMSPlacedObject
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "NMS")
    FString ObjectID;

    UPROPERTY(BlueprintReadWrite, Category = "NMS")
    FTransform Transform;

    UPROPERTY(BlueprintReadWrite, Category = "NMS")
    int64 UserData = 0;

    UPROPERTY(BlueprintReadWrite, Category = "NMS")
    float Timestamp = 0.f;
};

UCLASS(BlueprintType)
class NMS_BUILDERCORE_API UNMSBaseManager : public UObject
{
    GENERATED_BODY()

public:
    /** Загрузить сейв NMS (JSON) и распарсить массив Objects. */
    UFUNCTION(BlueprintCallable, Category = "NMS|IO")
    bool ImportNMSSave(const FString& FilePath);

    /** Сохранить текущие объекты обратно в JSON-сейв NMS. */
    UFUNCTION(BlueprintCallable, Category = "NMS|IO")
    bool ExportNMSSave(const FString& FilePath);

    /** Разобрать JSON базы из строки (буфер обмена / файл). */
    bool ImportFromString(const FString& Raw);

    /** Прочитать базу прямо из .blend (без Blender). */
    bool ImportFromBlend(const FString& FilePath);

    /** Сериализовать базу в JSON-строку (формат как у Blender-плагина). */
    FString ExportToString() const;

    UPROPERTY(BlueprintReadOnly, Category = "NMS")
    TArray<FNMSPlacedObject> PlacedObjects;

    /** Метаданные базы из сейва (Base Properties, как в плагине). */
    UPROPERTY(BlueprintReadOnly, Category = "NMS")
    FString BaseName;

    UPROPERTY(BlueprintReadOnly, Category = "NMS")
    FString GalacticAddress;

    /** Исходный JSON базы целиком — при экспорте сохраняем все поля игры. */
    TSharedPtr<class FJsonObject> ImportedRoot;

private:
    static FVector NMSToUnrealLocation(const FVector& NmsPos);
    static FVector UnrealToNMSLocation(const FVector& UePos);

    static FTransform BuildTransformFromUpAt(
        const FVector& NmsPos, const FVector& Up, const FVector& At);

    static void DecomposeTransformToUpAt(
        const FTransform& Xform, FVector& OutPos, FVector& OutUp, FVector& OutAt);
};
