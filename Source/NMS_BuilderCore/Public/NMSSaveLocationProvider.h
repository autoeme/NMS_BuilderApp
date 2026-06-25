// NMSSaveLocationProvider.h — абстракция источника пути к папке сейвов NMS.
// Домен (NMS_BuilderCore) НЕ должен знать про %APPDATA% и платформенные пути.
// Конкретные реализации (Windows/Mac) живут в платформенном адаптере
// (Editor/Infrastructure); core работает только с переданным корневым путём.
#pragma once

#include "CoreMinimal.h"

// Источник корневой папки сейвов (HelloGames/NMS, содержит аккаунты st_*).
// Header-only (чистый интерфейс) — без dllexport, иначе линкер ждёт
// определения ctor/dtor из DLL модуля Core, которого нет.
class INMSSaveLocationProvider
{
public:
    virtual ~INMSSaveLocationProvider() = default;

    // Корневая папка HelloGames/NMS. Пусто, если путь не удалось определить.
    virtual FString GetRootSaveFolder() const = 0;
};

// Провайдер с явно заданным путём: UI выбрал папку вручную, либо платформа
// без автоопределения. Не зависит от ОС — поэтому живёт в core.
class FNMSManualSaveLocationProvider final : public INMSSaveLocationProvider
{
public:
    explicit FNMSManualSaveLocationProvider(const FString& InRoot) : Root(InRoot) {}
    virtual FString GetRootSaveFolder() const override { return Root; }

private:
    FString Root;
};
