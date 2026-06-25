// Автотесты SceneAdapter (категория "NMS.SceneAdapter"): чистый резолв визуала.
#include "Misc/AutomationTest.h"
#include "NMSSceneAdapter.h"
#include "NMSPartVisuals.h"

#if WITH_DEV_AUTOMATION_TESTS

// Декод индекса палитры из UserData — битовая маска.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNMSColourIndexDecodeTest,
    "NMS.SceneAdapter.ColourIndexDecode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNMSColourIndexDecodeTest::RunTest(const FString& /*Parameters*/)
{
    TestEqual(TEXT("UserData=0 -> index 0"), NMS_ColourIndexFromUserData(0), 0);
    // старшие байты вне маски 0xFCFEFF должны отбрасываться
    TestEqual(TEXT("mask applied"),
        NMS_ColourIndexFromUserData((int64)0xFF000000u | 0x00ABCDu),
        (int32)(0x00ABCDu & NMS_COLOUR_INDEX_MASK));
    return true;
}

// UserData=0 -> цвет по умолчанию (палитра не применяется), поля копируются.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNMSResolveDefaultTest,
    "NMS.SceneAdapter.ResolveDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNMSResolveDefaultTest::RunTest(const FString& /*Parameters*/)
{
    FNMSPlacedPart Part;
    Part.ObjectID  = TEXT("^WALLA");
    Part.Transform = FTransform(FRotator(5.f, 10.f, 15.f), FVector(1.f, 2.f, 3.f), FVector(1.f));
    Part.UserData  = 0;

    const FString Cat = TEXT("Timber Structures");
    const FNMSPartRenderInfo R = NMS_ResolvePartRender(Part, Cat);

    TestFalse(TEXT("no palette colour at UserData=0"), R.bPaletteColor);
    TestEqual(TEXT("ObjectID copied"), R.ObjectID, Part.ObjectID);
    TestTrue(TEXT("Transform copied"), R.Transform.Equals(Part.Transform, 1e-4f));
    // дефолтные цвета совпадают с прямым вызовом доменных функций
    TestTrue(TEXT("Color1 == default"),
        R.Color1.Equals(NMS_DefaultPartColor(Cat, Part.ObjectID), 1e-4f));
    TestTrue(TEXT("Color2 == default2"),
        R.Color2.Equals(NMS_DefaultPartColor2(Cat, Part.ObjectID), 1e-4f));
    return true;
}

// Детерминизм: одинаковый вход -> одинаковый результат.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNMSResolveDeterministicTest,
    "NMS.SceneAdapter.ResolveDeterministic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNMSResolveDeterministicTest::RunTest(const FString& /*Parameters*/)
{
    FNMSPlacedPart Part;
    Part.ObjectID = TEXT("^CUBEFRAME");
    Part.UserData = 0x00ABCD;  // какой-то индекс палитры
    const FString Cat = TEXT("Alloy Structures");

    const FNMSPartRenderInfo A = NMS_ResolvePartRender(Part, Cat);
    const FNMSPartRenderInfo B = NMS_ResolvePartRender(Part, Cat);
    TestTrue(TEXT("Color1 deterministic"), A.Color1.Equals(B.Color1, 1e-6f));
    TestTrue(TEXT("Color2 deterministic"), A.Color2.Equals(B.Color2, 1e-6f));
    TestEqual(TEXT("palette flag deterministic"), A.bPaletteColor, B.bPaletteColor);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
