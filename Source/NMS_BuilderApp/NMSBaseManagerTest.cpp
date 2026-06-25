// Автотесты ядра: JSON import/export roundtrip и Up/At/Position transform.
// Запуск: окно Session Frontend -> Automation -> категория "NMS.BaseManager",
// либо headless:  -ExecCmds="Automation RunTests NMS.BaseManager; Quit".
#include "Misc/AutomationTest.h"
#include "NMSBaseManager.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// Фикстура: минимальная база в формате Blender-плагина (Objects[], Vec3=[x,y,z]).
static const TCHAR* GNMSFixtureJson = TEXT(R"JSON(
{
  "Name": "TestBase",
  "GalacticAddress": "12345",
  "Objects": [
    { "ObjectID": "^WALLA", "Position": [10.0, -3.5, 2.0], "Up": [0.0, 1.0, 0.0], "At": [1.0, 0.0, 0.0], "UserData": 123456, "Timestamp": 1700000000 },
    { "ObjectID": "^CUBEFRAME", "Position": [-4.0, 8.0, 0.5], "Up": [0.0, 0.0, 1.0], "At": [0.0, 1.0, 0.0], "UserData": 7, "Timestamp": 1700000111 }
  ]
}
)JSON");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNMSBaseManagerParseTest,
    "NMS.BaseManager.ParseFixture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNMSBaseManagerParseTest::RunTest(const FString& /*Parameters*/)
{
    TStrongObjectPtr<UNMSBaseManager> Mgr(NewObject<UNMSBaseManager>());
    TestTrue(TEXT("ImportFromString succeeds"), Mgr->ImportFromString(GNMSFixtureJson));
    TestEqual(TEXT("object count"), Mgr->PlacedObjects.Num(), 2);
    if (Mgr->PlacedObjects.Num() == 2)
    {
        TestEqual(TEXT("ObjectID[0]"), Mgr->PlacedObjects[0].ObjectID, FString(TEXT("^WALLA")));
        TestEqual(TEXT("ObjectID[1]"), Mgr->PlacedObjects[1].ObjectID, FString(TEXT("^CUBEFRAME")));
        TestEqual(TEXT("UserData[0]"), Mgr->PlacedObjects[0].UserData, (int64)123456);
        TestEqual(TEXT("UserData[1]"), Mgr->PlacedObjects[1].UserData, (int64)7);
    }
    TestEqual(TEXT("BaseName parsed"), Mgr->BaseName, FString(TEXT("TestBase")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNMSBaseManagerRoundtripTest,
    "NMS.BaseManager.JsonTransformRoundtrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNMSBaseManagerRoundtripTest::RunTest(const FString& /*Parameters*/)
{
    // import -> export -> import; объекты и трансформы должны совпасть.
    TStrongObjectPtr<UNMSBaseManager> A(NewObject<UNMSBaseManager>());
    if (!TestTrue(TEXT("first import"), A->ImportFromString(GNMSFixtureJson)))
        return false;

    const FString Exported = A->ExportToString();
    TestTrue(TEXT("export is non-empty"), !Exported.IsEmpty());
    TestTrue(TEXT("export contains Objects"), Exported.Contains(TEXT("Objects")));

    TStrongObjectPtr<UNMSBaseManager> B(NewObject<UNMSBaseManager>());
    if (!TestTrue(TEXT("re-import of exported"), B->ImportFromString(Exported)))
        return false;

    if (!TestEqual(TEXT("count preserved"), B->PlacedObjects.Num(), A->PlacedObjects.Num()))
        return false;

    for (int32 i = 0; i < A->PlacedObjects.Num(); ++i)
    {
        const FNMSPlacedObject& PA = A->PlacedObjects[i];
        const FNMSPlacedObject& PB = B->PlacedObjects[i];
        TestEqual(*FString::Printf(TEXT("ObjectID[%d]"), i), PB.ObjectID, PA.ObjectID);
        TestEqual(*FString::Printf(TEXT("UserData[%d]"), i), PB.UserData, PA.UserData);

        // Up/At/Position roundtrip: трансформ после export->import совпадает с исходным.
        const bool bLoc = PB.Transform.GetLocation().Equals(PA.Transform.GetLocation(), 0.05f);
        const bool bRot = PB.Transform.GetRotation().Equals(PA.Transform.GetRotation(), 0.001f);
        const bool bScale = PB.Transform.GetScale3D().Equals(PA.Transform.GetScale3D(), 0.001f);
        TestTrue(*FString::Printf(TEXT("transform location[%d]"), i), bLoc);
        TestTrue(*FString::Printf(TEXT("transform rotation[%d]"), i), bRot);
        TestTrue(*FString::Printf(TEXT("transform scale[%d]"), i), bScale);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
