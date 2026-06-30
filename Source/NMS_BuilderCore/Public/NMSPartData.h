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

/**
 * Деталь — пандус/лестница (семья RAMP / RAMP_H / RAMP_Q_TOP во всех стилях).
 * Меш такой детали авторски развёрнут на 180° вокруг локальной вертикали
 * относительно ориентации игры — это компенсируется доворотом при спавне
 * (и при ручной постановке, и при загрузке базы). RACE_RAMP сюда НЕ входит.
 * У *_RAMP_Q_TOP socket пустой/LEGACYFLOOR, поэтому ловим ещё и по ObjectID.
 */
inline bool NMS_IsRampPart(const FNMSPartData& Part)
{
    return Part.SocketClassIDs.Contains(TEXT("RAMP"))
        || Part.ObjectID.Contains(TEXT("RAMP_Q_TOP"));
}

/**
 * Точечная поправка ориентации (yaw, градусы) для деталей с авторски развёрнутым
 * мешем (как рампы, но адресно по ObjectID). Данные: Content/NMSData/orientation_fix.json
 * ({ "OBJECTID": yaw, ... }, ID без префикса ^). 0 — поправки нет.
 * Применяется доворотом вокруг локальной вертикали на спавне (загрузка + ручная постановка).
 */
NMS_BUILDERCORE_API float NMS_OrientationFixYaw(const FString& ObjectID);

/**
 * Деталь — листва/растение (карточки листьев). Её диффуз RGBA с альфой-вырезом,
 * поэтому нужен masked + двусторонний материал (иначе листья = прямоугольники).
 * Растения биомов: Category «Exotic Decorations» / SubCategory «Decorative Plants»
 * либо явная категория Foliage.
 */
inline bool NMS_IsFoliagePart(const FNMSPartData& Part)
{
    return Part.SubCategory.Contains(TEXT("Decorative Plants"))
        || Part.Category.Equals(TEXT("Foliage"), ESearchCase::IgnoreCase);
}
