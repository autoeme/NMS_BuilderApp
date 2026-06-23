// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"

class SDockTab;
class FSpawnTabArgs;

class FNMS_BuilderAppModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    TSharedRef<SDockTab> OnSpawnTab(const FSpawnTabArgs& Args);

    // отложенное открытие, когда Slate готов
    bool TryOpenTab(float DeltaTime);
    FTSTicker::FDelegateHandle OpenTickerHandle;

    static const FName TabId;
};
