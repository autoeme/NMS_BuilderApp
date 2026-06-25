// NMSPlatformSaveLocationProvider.h — платформенный адаптер пути к сейвам NMS.
// Здесь (вне домена) можно зависеть от ОС: %APPDATA% на Windows,
// ~/Library/Application Support на Mac. Реализует core-интерфейс
// INMSSaveLocationProvider, чтобы NMS_BuilderCore не знал про платформу.
#pragma once

#include "CoreMinimal.h"
#include "NMSSaveLocationProvider.h"

// Автоопределение корневой папки HelloGames/NMS по текущей ОС.
class FNMSPlatformSaveLocationProvider final : public INMSSaveLocationProvider
{
public:
    virtual FString GetRootSaveFolder() const override;
};
