#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NMSPartData.generated.h"

/**
 * Одна строка таблицы деталей NMS.
 * Импортируется из Content/nms_parts_db.json как Data Table.
 * Поля совпадают с ключами JSON (RowName = поле Name).
 */
USTRUCT(BlueprintType)
struct NMS_BUILDERCORE_API FNMSPartData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NMS Part")
    FString ObjectID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NMS Part")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NMS Part")
    FString Category;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NMS Part")
    FString SubCategory;

    /** Soft-путь к мешу: /Game/NMSBaseBuilder/Features/Models_FBX/ID.ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NMS Part")
    FString ModelPath;

    /** Soft-путь к иконке: /Game/NMSBaseBuilder/Features/UI/Icons/ID.ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NMS Part")
    FString IconPath;

    /** Классы сокетов (входы привязки), напр. ("FLOOR","WALL") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NMS Part|Snap")
    FString SocketClassIDs;

    /** Классы штекеров (выходы привязки) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NMS Part|Snap")
    FString PlugClassIDs;

    /** ID базовой детали, если это вариант */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NMS Part")
    FString VariantOf;

    /** Показывать в меню постройки */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NMS Part")
    bool bShowInDrawer = true;

    FNMSPartData()
        : ObjectID(TEXT(""))
        , DisplayName(TEXT(""))
        , Category(TEXT("uncategorized"))
        , SubCategory(TEXT(""))
        , ModelPath(TEXT(""))
        , IconPath(TEXT(""))
        , SocketClassIDs(TEXT(""))
        , PlugClassIDs(TEXT(""))
        , VariantOf(TEXT(""))
        , bShowInDrawer(true)
    {
    }
};
