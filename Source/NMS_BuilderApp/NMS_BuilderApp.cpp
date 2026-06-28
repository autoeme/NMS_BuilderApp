// Copyright Epic Games, Inc. All Rights Reserved.

#include "NMS_BuilderApp.h"
#include "NMSBuilderLog.h"
#include "SNMSBuilderUI.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Textures/SlateIcon.h"
#include "Misc/App.h"

DEFINE_LOG_CATEGORY(LogNMSBuilderEditor);
DEFINE_LOG_CATEGORY(LogNMSBuilderScene);
DEFINE_LOG_CATEGORY(LogNMSBuilderSave);
DEFINE_LOG_CATEGORY(LogNMSBuilderImport);

const FName FNMS_BuilderAppModule::TabId(TEXT("NMSBuilderAppWindow"));

void FNMS_BuilderAppModule::StartupModule()
{
    // Регистрируем nomad-вкладку. Сама регистрация безопасна и рано.
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        TabId,
        FOnSpawnTab::CreateRaw(this, &FNMS_BuilderAppModule::OnSpawnTab))
        .SetDisplayName(NSLOCTEXT("NMSBuilder", "TabTitle", "NMS Base Builder"));

    // Авто-открытие вкладки — только в интерактивном редакторе. В headless
    // (-nullrhi / commandlet / автотесты) создание Slate-окна падает
    // ("GetRestoredDimensions is not expected to be called on this platform"),
    // поэтому там вкладку не открываем.
    if (!FApp::CanEverRender())
        return;

    // Открытие откладываем: на момент StartupModule Slate-приложение
    // может быть ещё не готово, прямой вызов TryInvokeTab уронит.
    OpenTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FNMS_BuilderAppModule::TryOpenTab),
        0.0f);
}

void FNMS_BuilderAppModule::ShutdownModule()
{
    if (OpenTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(OpenTickerHandle);
        OpenTickerHandle.Reset();
    }
    if (FSlateApplication::IsInitialized())
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
    }
}

bool FNMS_BuilderAppModule::TryOpenTab(float /*DeltaTime*/)
{
    // ждём, пока Slate реально поднимется
    if (!FSlateApplication::IsInitialized())
    {
        return true; // продолжаем тикать
    }

    FGlobalTabmanager::Get()->TryInvokeTab(TabId);

    // открыли — тикер больше не нужен
    OpenTickerHandle.Reset();
    return false; // снять тикер
}

TSharedRef<SDockTab> FNMS_BuilderAppModule::OnSpawnTab(const FSpawnTabArgs& /*Args*/)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SNMSBuilderUI)
        ];
}

IMPLEMENT_PRIMARY_GAME_MODULE(FNMS_BuilderAppModule, NMS_BuilderApp, "NMS_BuilderApp");
