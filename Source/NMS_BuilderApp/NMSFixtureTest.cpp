// Автотесты ядра на реальных фикстурах (Source/NMS_BuilderApp/TestData):
//  - .nmsprefab / .nmsbase (JSON-форматы плагина) -> ImportFromString/ImportNMSSave
//  - .blend -> ImportFromBlend (парсер NMSBlendReader)
// Запуск: см. [[running-tests]] (категория "NMS.Fixtures").
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "NMSBaseManager.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

static FString FixturePath(const TCHAR* Name)
{
    return FPaths::ProjectDir() / TEXT("Source/NMS_BuilderApp/TestData/") / Name;
}

// .nmsprefab (массив "Prefab") -> 4 детали. Плюс roundtrip сохраняет число.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNMSPrefabFixtureTest,
    "NMS.Fixtures.Prefab",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNMSPrefabFixtureTest::RunTest(const FString& /*Parameters*/)
{
    TStrongObjectPtr<UNMSBaseManager> Mgr(NewObject<UNMSBaseManager>());
    if (!TestTrue(TEXT("import .nmsprefab"), Mgr->ImportNMSSave(FixturePath(TEXT("planetaryprobe.nmsprefab")))))
        return false;
    TestEqual(TEXT("prefab object count"), Mgr->PlacedObjects.Num(), 4);

    TStrongObjectPtr<UNMSBaseManager> B(NewObject<UNMSBaseManager>());
    TestTrue(TEXT("roundtrip re-import"), B->ImportFromString(Mgr->ExportToString()));
    TestEqual(TEXT("roundtrip count"), B->PlacedObjects.Num(), Mgr->PlacedObjects.Num());
    return true;
}

// .nmsbase (массив "Objects") -> 114 деталей. Плюс roundtrip сохраняет число.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNMSBaseFixtureTest,
    "NMS.Fixtures.Base",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNMSBaseFixtureTest::RunTest(const FString& /*Parameters*/)
{
    TStrongObjectPtr<UNMSBaseManager> Mgr(NewObject<UNMSBaseManager>());
    if (!TestTrue(TEXT("import .nmsbase"), Mgr->ImportNMSSave(FixturePath(TEXT("sample_base.nmsbase")))))
        return false;
    TestEqual(TEXT("base object count"), Mgr->PlacedObjects.Num(), 114);

    TStrongObjectPtr<UNMSBaseManager> B(NewObject<UNMSBaseManager>());
    TestTrue(TEXT("roundtrip re-import"), B->ImportFromString(Mgr->ExportToString()));
    TestEqual(TEXT("roundtrip count"), B->PlacedObjects.Num(), Mgr->PlacedObjects.Num());
    return true;
}

// .blend -> парсер NMSBlendReader должен вытащить ровно 15 деталей
// (эталон зафиксирован первым прогоном на sample.blend = ии3.blend).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNMSBlendFixtureTest,
    "NMS.Fixtures.Blend",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNMSBlendFixtureTest::RunTest(const FString& /*Parameters*/)
{
    TStrongObjectPtr<UNMSBaseManager> Mgr(NewObject<UNMSBaseManager>());
    const bool bOk = Mgr->ImportFromBlend(FixturePath(TEXT("sample.blend")));
    AddInfo(FString::Printf(TEXT("blend parts parsed: %d"), Mgr->PlacedObjects.Num()));
    if (!TestTrue(TEXT("ImportFromBlend succeeds"), bOk))
        return false;
    TestEqual(TEXT("blend part count"), Mgr->PlacedObjects.Num(), 15);

    // Каждая деталь имеет непустой ObjectID — парсер заполнил поля.
    bool bAllIds = true;
    for (const FNMSPlacedObject& P : Mgr->PlacedObjects)
        bAllIds = bAllIds && !P.ObjectID.IsEmpty();
    TestTrue(TEXT("all parts have ObjectID"), bAllIds);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
