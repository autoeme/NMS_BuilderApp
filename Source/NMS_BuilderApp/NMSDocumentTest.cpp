// Автотесты доменной модели и адаптера manager<->document (категория "NMS.Document").
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "NMSBaseDocument.h"
#include "NMSDocumentAdapter.h"
#include "NMSBaseManager.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

static FString DocFixturePath(const TCHAR* Name)
{
    return FPaths::ProjectDir() / TEXT("Source/NMS_BuilderApp/TestData/") / Name;
}

// Менеджер -> документ: метаданные и детали переносятся 1:1.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNMSDocumentFromManagerTest,
    "NMS.Document.FromManager",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNMSDocumentFromManagerTest::RunTest(const FString& /*Parameters*/)
{
    TStrongObjectPtr<UNMSBaseManager> Mgr(NewObject<UNMSBaseManager>());
    if (!TestTrue(TEXT("import fixture"), Mgr->ImportNMSSave(DocFixturePath(TEXT("sample_base.nmsbase")))))
        return false;

    const FNMSBaseDocument Doc = NMSDoc::FromManager(*Mgr);
    TestEqual(TEXT("part count matches manager"), Doc.Num(), Mgr->PlacedObjects.Num());
    TestEqual(TEXT("base count is 114"), Doc.Num(), 114);
    if (Doc.Num() == Mgr->PlacedObjects.Num() && Doc.Num() > 0)
    {
        TestEqual(TEXT("ObjectID[0]"), Doc.Parts[0].ObjectID, Mgr->PlacedObjects[0].ObjectID);
        TestTrue(TEXT("Transform[0] matches"),
            Doc.Parts[0].Transform.Equals(Mgr->PlacedObjects[0].Transform, 1e-4f));
    }
    return true;
}

// Документ -> менеджер -> документ: всё сохраняется (включая трансформы).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNMSDocumentRoundtripTest,
    "NMS.Document.AdapterRoundtrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNMSDocumentRoundtripTest::RunTest(const FString& /*Parameters*/)
{
    // Собираем документ вручную (без сцены/файлов) — чистая модель.
    FNMSBaseDocument Doc;
    Doc.Name = TEXT("RoundtripBase");
    Doc.GalacticAddress = TEXT("999");
    {
        FNMSPlacedPart A; A.ObjectID = TEXT("^WALLA");
        A.Transform = FTransform(FRotator(10.f, 20.f, 30.f), FVector(100.f, -50.f, 25.f), FVector(1.f));
        A.UserData = 42; A.Timestamp = 123.f;
        Doc.Parts.Add(A);

        FNMSPlacedPart B; B.ObjectID = TEXT("^CUBEFRAME");
        B.Transform = FTransform(FRotator::ZeroRotator, FVector(-7.f, 8.f, 9.f), FVector(1.f));
        B.UserData = 7; B.Timestamp = 456.f;
        Doc.Parts.Add(B);
    }

    TStrongObjectPtr<UNMSBaseManager> Mgr(NewObject<UNMSBaseManager>());
    NMSDoc::ToManager(Doc, *Mgr);
    TestEqual(TEXT("manager got all parts"), Mgr->PlacedObjects.Num(), Doc.Num());

    const FNMSBaseDocument Back = NMSDoc::FromManager(*Mgr);
    if (!TestEqual(TEXT("roundtrip count"), Back.Num(), Doc.Num()))
        return false;
    TestEqual(TEXT("name preserved"), Back.Name, Doc.Name);
    TestEqual(TEXT("galactic address preserved"), Back.GalacticAddress, Doc.GalacticAddress);
    for (int32 i = 0; i < Doc.Num(); ++i)
    {
        TestEqual(*FString::Printf(TEXT("ObjectID[%d]"), i), Back.Parts[i].ObjectID, Doc.Parts[i].ObjectID);
        TestEqual(*FString::Printf(TEXT("UserData[%d]"), i), Back.Parts[i].UserData, Doc.Parts[i].UserData);
        TestTrue(*FString::Printf(TEXT("Transform[%d]"), i),
            Back.Parts[i].Transform.Equals(Doc.Parts[i].Transform, 1e-4f));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
