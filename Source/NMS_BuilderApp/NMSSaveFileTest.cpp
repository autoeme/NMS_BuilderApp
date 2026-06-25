// Автотест чтения игрового сейва .hg (категория "NMS.SaveFile").
// Фикстура sample_save.hg — ЛИЧНЫЙ сейв игрока, в репозиторий не коммитится
// (см. .gitignore: *.hg). Если её нет — тест корректно пропускается.
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "NMSSaveFile.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNMSSaveFileLoadTest,
    "NMS.SaveFile.LoadHg",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNMSSaveFileLoadTest::RunTest(const FString& /*Parameters*/)
{
    const FString Path = FPaths::ProjectDir() / TEXT("Source/NMS_BuilderApp/TestData/sample_save.hg");
    if (!IFileManager::Get().FileExists(*Path))
    {
        AddWarning(TEXT("Нет фикстуры sample_save.hg — тест .hg пропущен. "
                        "Положи небольшой save.hg в Source/NMS_BuilderApp/TestData/."));
        return true; // graceful skip — не падаем там, где фикстуры нет
    }

    // Load: распаковка LZ4 + деобфускация ключей по save_map_dictionary.json.
    FNMSSaveFile SaveFile(Path);
    FString Err;
    if (!TestTrue(TEXT("Load .hg succeeds"), SaveFile.Load(Err)))
    {
        AddError(FString::Printf(TEXT("Load error: %s"), *Err));
        return false;
    }
    TestTrue(TEXT("deobfuscated JSON is valid"), SaveFile.GetJson().IsValid());

    // Базы игрока из сейва.
    const TArray<FNMSSaveBaseInfo> Bases = SaveFile.ExtractBases();
    AddInfo(FString::Printf(TEXT("bases in save: %d"), Bases.Num()));
    TestTrue(TEXT("save has at least one base"), Bases.Num() > 0);
    for (const FNMSSaveBaseInfo& B : Bases)
        TestTrue(TEXT("base has a type"), !B.BaseType.IsEmpty());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
