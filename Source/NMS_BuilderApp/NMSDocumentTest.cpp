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

// Равенство документов — основа детекции изменений в Undo/Redo.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNMSDocumentEqualityTest,
    "NMS.Document.Equality",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNMSDocumentEqualityTest::RunTest(const FString& /*Parameters*/)
{
    auto MakePart = [](const TCHAR* Id, const FVector& Loc, int64 UD)
    {
        FNMSPlacedPart P;
        P.ObjectID = Id;
        P.Transform = FTransform(FRotator::ZeroRotator, Loc, FVector(1.f));
        P.UserData = UD;
        return P;
    };

    FNMSBaseDocument A;
    A.Name = TEXT("Base");
    A.Parts.Add(MakePart(TEXT("^WALLA"), FVector(1, 2, 3), 10));
    A.Parts.Add(MakePart(TEXT("^CUBEFRAME"), FVector(4, 5, 6), 20));

    // полная копия -> равны
    FNMSBaseDocument B = A;
    TestTrue(TEXT("identical docs are equal"), A == B);

    // имя/адрес не влияют на равенство сцены
    B.Name = TEXT("Other"); B.GalacticAddress = TEXT("999");
    TestTrue(TEXT("name/address ignored"), A == B);

    // сдвиг позиции -> не равны
    FNMSBaseDocument C = A;
    C.Parts[0].Transform.AddToTranslation(FVector(0, 0, 1));
    TestTrue(TEXT("moved part -> not equal"), A != C);

    // другой UserData (цвет) -> не равны
    FNMSBaseDocument D = A;
    D.Parts[1].UserData = 21;
    TestTrue(TEXT("changed UserData -> not equal"), A != D);

    // другое число деталей -> не равны
    FNMSBaseDocument E = A;
    E.Parts.RemoveAt(1);
    TestTrue(TEXT("different count -> not equal"), A != E);

    // другой ObjectID -> не равны
    FNMSBaseDocument F = A;
    F.Parts[0].ObjectID = TEXT("^FLOORA");
    TestTrue(TEXT("changed ObjectID -> not equal"), A != F);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
