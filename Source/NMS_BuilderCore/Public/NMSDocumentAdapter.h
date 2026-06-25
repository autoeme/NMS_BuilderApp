// Адаптеры между доменной моделью FNMSBaseDocument и существующими слоями
// ввода-вывода (UNMSBaseManager: JSON/.blend/.nmsbase). Это «мост» на время
// инкрементальной миграции: модель уже есть, старый actor-поток ещё работает.
#pragma once

#include "CoreMinimal.h"
#include "NMSBaseDocument.h"

class UNMSBaseManager;

namespace NMSDoc
{
    // Снять документ с менеджера (после ImportNMSSave/ImportFromBlend/...).
    NMS_BUILDERCORE_API FNMSBaseDocument FromManager(const UNMSBaseManager& Mgr);

    // Залить документ в менеджер (его PlacedObjects/BaseName/GalacticAddress),
    // чтобы затем экспортировать через ExportNMSSave/ExportToString.
    NMS_BUILDERCORE_API void ToManager(const FNMSBaseDocument& Doc, UNMSBaseManager& Mgr);
}
