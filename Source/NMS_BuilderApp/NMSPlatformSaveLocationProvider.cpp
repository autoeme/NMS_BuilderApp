// NMSPlatformSaveLocationProvider.cpp — платформенное определение папки сейвов.
#include "NMSPlatformSaveLocationProvider.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"

FString FNMSPlatformSaveLocationProvider::GetRootSaveFolder() const
{
#if PLATFORM_WINDOWS
    const FString AppData = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
    if (AppData.IsEmpty()) return FString();
    return FPaths::Combine(AppData, TEXT("HelloGames/NMS"));
#elif PLATFORM_MAC
    const FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
    if (Home.IsEmpty()) return FString();
    return FPaths::Combine(Home, TEXT("Library/Application Support/HelloGames/NMS"));
#else
    // Linux/прочее: автоопределения нет — пусть UI задаёт путь вручную
    // через FNMSManualSaveLocationProvider.
    return FString();
#endif
}
