// Часть класса SNMSBuilderUI (разнесение по TU, Часть C плана).
// Включения общие для всех TU класса — см. SNMSBuilderUI_Internal.h.
#include "SNMSBuilderUI_Internal.h"
#include "HAL/PlatformProcess.h"   // FPlatformProcess::UserDir() — папка «Документы» без хардкода

#define LOCTEXT_NAMESPACE "NMSBuilder"

// Запоминание последней папки файловых диалогов (между сессиями) — вместо
// жёстко прописанных путей вида C:/Users/.../Documents (правило CLAUDE.md #4).
static FString NMS_LastDirFile()
{
    return FPaths::ProjectSavedDir() / TEXT("NMSUser/last_dir.txt");
}
static FString NMS_LoadLastDir()
{
    FString S;
    if (FFileHelper::LoadFileToString(S, *NMS_LastDirFile()))
    {
        S = S.TrimStartAndEnd();
        if (!S.IsEmpty() && FPaths::DirectoryExists(S)) return S;
    }
    // дефолт — папка «Документы» пользователя (кроссплатформенно, без C:/Users/...)
    return FPlatformProcess::UserDir();
}
static void NMS_SaveLastDir(const FString& Dir)
{
    if (Dir.IsEmpty()) return;
    const FString P = FPaths::ProjectSavedDir() / TEXT("NMSUser");
    IFileManager::Get().MakeDirectory(*P, true);
    FFileHelper::SaveStringToFile(Dir, *NMS_LastDirFile());
}

void SNMSBuilderUI::OnMenuNewBase()
{
    // как в плагине: чистая сцена + сброс свойств базы
    ClearSceneParts();
    CurrentBaseName = TEXT("Untitled Base");
    CurrentGalacticAddress.Empty();
    UE_LOG(LogTemp, Log, TEXT("NMS UI: New Base"));
}

void SNMSBuilderUI::OnMenuOpen()
{
    // Open Base... — выбрать файл базы (.json/.nmsbase/.hg/.blend) и загрузить, как в Blender.
    IDesktopPlatform* DP = FDesktopPlatformModule::Get();
    if (!DP) { OnMenuImportFromNMS(); return; }
    TArray<FString> Picked;
    const bool bPicked = DP->OpenFileDialog(
        nullptr,
        TEXT("Открыть базу"),
        NMS_LoadLastDir(),
        TEXT(""),
        TEXT("Базы NMS (*.json;*.nmsbase;*.hg;*.blend)|*.json;*.nmsbase;*.hg;*.blend"),
        EFileDialogFlags::None,
        Picked);
    if (!bPicked || Picked.Num() == 0) return;
    const FString Path = Picked[0];
    NMS_SaveLastDir(FPaths::GetPath(Path));
    const FString Ext = FPaths::GetExtension(Path).ToLower();
    UNMSBaseManager* Mgr = NewObject<UNMSBaseManager>();
    Mgr->AddToRoot();
    bool bOk = false;
    if (Ext == TEXT("hg"))         bOk = Mgr->ImportNMSSave(Path);
    else if (Ext == TEXT("blend")) bOk = Mgr->ImportFromBlend(Path);
    else { FString Txt; if (FFileHelper::LoadFileToString(Txt, *Path)) bOk = Mgr->ImportFromString(Txt); }
    if (bOk) { SpawnFromManager(Mgr); }
    UE_LOG(LogTemp, Log, TEXT("NMS UI: OpenBase %s -> %s"), *Path, bOk ? TEXT("OK") : TEXT("FAIL"));
    Mgr->RemoveFromRoot();
}

void SNMSBuilderUI::OnMenuImportModel()
{
#if WITH_EDITOR
    // Импорт внешней модели .obj/.stl: парсим -> рантайм UStaticMesh -> спавним
    // как обычную деталь (метка NMS_IMPORT_*), чтобы работали выбор/трансформ/подсветка.
    IDesktopPlatform* DP = FDesktopPlatformModule::Get();
    if (!DP) return;
    TArray<FString> Picked;
    const bool bPicked = DP->OpenFileDialog(
        nullptr,
        TEXT("Импорт 3D-модели"),
        NMS_LoadLastDir(),
        TEXT(""),
        TEXT("3D-модели (*.obj;*.stl)|*.obj;*.stl"),
        EFileDialogFlags::None,
        Picked);
    if (!bPicked || Picked.Num() == 0) return;
    const FString Path = Picked[0];
    NMS_SaveLastDir(FPaths::GetPath(Path));

    FNMSImportedMesh Geo;
    FString Err;
    if (!FNMSModelImporter::LoadFromFile(Path, Geo, Err))
    {
        UE_LOG(LogTemp, Warning, TEXT("NMS Import: %s -> ОШИБКА: %s"), *Path, *Err);
        return;
    }

    UWorld* World = ViewportClient.IsValid() ? ViewportClient->GetWorld() : nullptr;
    if (!World) return;

    UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_PartLit.M_PartLit"));
    if (!Mat) Mat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_BasePart.M_BasePart"));

    UStaticMesh* Mesh = FNMSModelImporter::BuildStaticMesh(Geo, Mat, 300.f);
    if (!Mesh) { UE_LOG(LogTemp, Warning, TEXT("NMS Import: не удалось собрать меш")); return; }

    const FString Name = FPaths::GetBaseFilename(Path);
    FActorSpawnParameters Params;
    Params.ObjectFlags = RF_Transient;
    AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(
        AStaticMeshActor::StaticClass(), FTransform::Identity, Params);
    if (!A) return;
    A->SetActorLabel(FString::Printf(TEXT("NMS_IMPORT_%s"), *Name));
    A->Tags.Add(FName(TEXT("IMPORT")));

    if (UStaticMeshComponent* Comp = A->GetStaticMeshComponent())
    {
        Comp->SetMobility(EComponentMobility::Movable);
        Comp->SetCastShadow(false);
        Comp->SetStaticMesh(Mesh);
        Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Comp->SetCollisionObjectType(ECC_WorldStatic);
        Comp->SetCollisionResponseToAllChannels(ECR_Ignore);
        Comp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        if (Mat) Comp->SetMaterial(0, Mat);
    }

    if (ViewportClient.IsValid()) ViewportClient->SetSelectedActor(A);
    if (SceneListView.IsValid()) RebuildSceneTree();
    UE_LOG(LogTemp, Log, TEXT("NMS Import: %s -> %d верт, %d инд OK"),
        *Path, Geo.Positions.Num(), Geo.Indices.Num());
#endif
}

void SNMSBuilderUI::OnMenuImportFromNMS()
{
    // Открываем окно Save Manager (выбор аккаунта/слота/базы из сейва игры).
    TSharedRef<SWindow> Win = SNew(SWindow)
        .Title(FText::FromString(TEXT("Save Manager — базы из сейва игры")))
        .ClientSize(FVector2D(420, 600))
        .SupportsMaximize(false);

    Win->SetContent(BuildSaveManagerPanel());

    if (FSlateApplication::IsInitialized())
    {
        TSharedPtr<SWindow> Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
        if (Parent.IsValid())
            FSlateApplication::Get().AddWindowAsNativeChild(Win, Parent.ToSharedRef());
        else
            FSlateApplication::Get().AddWindow(Win);
    }
}


// Удалить из сцены все детали (актёры с меткой NMS_).
void SNMSBuilderUI::ClearSceneParts()
{
#if WITH_EDITOR
    UWorld* World = ViewportClient.IsValid() ? ViewportClient->GetWorld() : nullptr;
    if (!World) return;
    if (ViewportClient.IsValid()) ViewportClient->SetSelectedActor(nullptr);

    TArray<AActor*> ToDelete;
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        AStaticMeshActor* A = *It;
        if (A && A->GetActorLabel().StartsWith(TEXT("NMS_"))) ToDelete.Add(A);
    }
    for (AActor* A : ToDelete) World->DestroyActor(A);
    UE_LOG(LogTemp, Log, TEXT("NMS UI: cleared %d parts"), ToDelete.Num());
#endif
    if (SceneListView.IsValid()) RebuildSceneTree();
}

// Построить базу из менеджера в сцене: настоящие модели деталей + родные цвета.
// Цвет детали из базы: UserData -> индекс палитры -> RGBA (primary+secondary).
// Таблица из Content/NMSData/userdata_palette.json (сгенерирована из игрового DT_Palettes).
// Единое применение материалов к компоненту детали (per-slot: стекло/листва/unlit
// + покраска), как в SpawnFromManager. Зовётся и при ручной постановке (OnPartSelected),
// чтобы стекло/мультислот выглядели одинаково с загрузкой базы.
void SNMSBuilderUI::ApplyPartMaterialsToComponent(UStaticMeshComponent* Comp,
                                                  const FNMSPartData& PD, int64 UserData)
{
    if (!Comp) return;
    const FString Cat = PD.Category;
    const bool bFoliage = NMS_IsFoliagePart(PD);
    UMaterialInterface* BaseM = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_BasePart.M_BasePart"));
    UMaterialInterface* LitM = LoadObject<UMaterialInterface>(nullptr,
        FNMSViewportClient::bUnlitParts
            ? TEXT("/Game/NMSBaseBuilder/Materials/M_PartUnlit.M_PartUnlit")
            : TEXT("/Game/NMSBaseBuilder/Materials/M_PartLit.M_PartLit"));
    UMaterialInterface* UnlitM = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_PartUnlit.M_PartUnlit"));
    UMaterialInterface* FoliageM = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_PartFoliage.M_PartFoliage"));
    UMaterialInterface* GlassM = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_PartGlass.M_PartGlass"));

    FNMSPlacedPart Obj; Obj.ObjectID = PD.ObjectID; Obj.UserData = UserData;
    const FNMSPartRenderInfo RI = NMS_ResolvePartRender(Obj, Cat);
    const FLinearColor Col1 = RI.Color1, Col2 = RI.Color2,
        ZA = RI.ZoneA, ZB = RI.ZoneB, ZC = RI.ZoneC, ZD = RI.ZoneD;

    auto TexPathOf = [](const FString& N) -> FString {
        return N.IsEmpty() ? FString()
            : FString::Printf(TEXT("/Game/NMSBaseBuilder/PartTex2/T_%s.T_%s"), *N, *N);
    };
    auto BuildMID = [&](const FNMSTexSet& S) -> UMaterialInstanceDynamic* {
        UTexture2D* Tex = S.Tex.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *TexPathOf(S.Tex));
        UMaterialInterface* Surf = (S.bGlass && GlassM) ? GlassM
                                 : (S.bUnlit && UnlitM) ? UnlitM
                                 : ((bFoliage && FoliageM) ? FoliageM : LitM);
        UMaterialInterface* M = (Tex && Surf) ? Surf : BaseM;
        if (!M) return nullptr;
        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, Comp);
        if (Tex && Surf)
        {
            MID->SetTextureParameterValue(TEXT("BaseTex"), Tex);
            if (!S.bUnlit)
            {
                if (UTexture2D* Mk = LoadObject<UTexture2D>(nullptr, *TexPathOf(S.Mask)))
                    MID->SetTextureParameterValue(TEXT("PaintMask"), Mk);
                if (UTexture2D* Nm = LoadObject<UTexture2D>(nullptr, *TexPathOf(S.Norm)))
                    MID->SetTextureParameterValue(TEXT("NormalTex"), Nm);
                if (UTexture2D* Ms = LoadObject<UTexture2D>(nullptr, *TexPathOf(S.Masks)))
                { MID->SetTextureParameterValue(TEXT("MasksTex"), Ms); MID->SetScalarParameterValue(TEXT("UseMasks"), 1.f); }
                MID->SetVectorParameterValue(TEXT("Color"), Col1);
                MID->SetVectorParameterValue(TEXT("Color2"), Col2);
            }
        }
        else MID->SetVectorParameterValue(TEXT("Color"), Col1);
        MID->SetVectorParameterValue(TEXT("ZoneCol0"), ZA);
        MID->SetVectorParameterValue(TEXT("ZoneCol1"), ZB);
        MID->SetVectorParameterValue(TEXT("ZoneCol2"), ZC);
        MID->SetVectorParameterValue(TEXT("ZoneCol3"), ZD);
        return MID;
    };

    const int32 N = Comp->GetNumMaterials();
    const TArray<FNMSTexSet>* Slots = NMS_PartSlots(PD.ObjectID);
    const bool bFullCover = (Slots && N > 1 && Slots->Num() >= N);
    if (bFullCover)
    {
        for (int32 i = 0; i < N; ++i)
        {
            const FNMSTexSet& S = (*Slots)[FMath::Min(i, Slots->Num() - 1)];
            if (UMaterialInstanceDynamic* MID = BuildMID(S)) Comp->SetMaterial(i, MID);
        }
    }
    else
    {
        FString TexPath, MaskPath, NormPath, MasksPath, OccPath;
        NMS_PartTexture(Cat, PD.ObjectID, TexPath, MaskPath, NormPath, MasksPath, OccPath);
        UTexture2D* Tex = TexPath.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *TexPath);
        const bool bGlass1 = Slots && Slots->Num() > 0 && (*Slots)[0].bGlass;
        UMaterialInterface* Surf1 = (bGlass1 && GlassM) ? GlassM : ((bFoliage && FoliageM) ? FoliageM : LitM);
        UMaterialInterface* M = (Tex && Surf1) ? Surf1 : BaseM;
        if (M)
        {
            UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, Comp);
            if (Tex && Surf1)
            {
                MID->SetTextureParameterValue(TEXT("BaseTex"), Tex);
                if (UTexture2D* Mk = LoadObject<UTexture2D>(nullptr, *MaskPath)) MID->SetTextureParameterValue(TEXT("PaintMask"), Mk);
                if (UTexture2D* Nm = LoadObject<UTexture2D>(nullptr, *NormPath)) MID->SetTextureParameterValue(TEXT("NormalTex"), Nm);
                if (UTexture2D* Ms = LoadObject<UTexture2D>(nullptr, *MasksPath)) { MID->SetTextureParameterValue(TEXT("MasksTex"), Ms); MID->SetScalarParameterValue(TEXT("UseMasks"), 1.f); }
                if (UTexture2D* Oc = LoadObject<UTexture2D>(nullptr, *OccPath)) MID->SetTextureParameterValue(TEXT("OcclusionTex"), Oc);
                MID->SetVectorParameterValue(TEXT("Color"), Col1);
                MID->SetVectorParameterValue(TEXT("Color2"), Col2);
            }
            else MID->SetVectorParameterValue(TEXT("Color"), Col1);
            MID->SetVectorParameterValue(TEXT("ZoneCol0"), ZA);
            MID->SetVectorParameterValue(TEXT("ZoneCol1"), ZB);
            MID->SetVectorParameterValue(TEXT("ZoneCol2"), ZC);
            MID->SetVectorParameterValue(TEXT("ZoneCol3"), ZD);
            for (int32 i = 0; i < N; ++i) Comp->SetMaterial(i, MID);
        }
    }
}

void SNMSBuilderUI::SpawnFromManager(UNMSBaseManager* Mgr)
{
#if WITH_EDITOR
    UWorld* World = ViewportClient.IsValid() ? ViewportClient->GetWorld() : nullptr;
    if (!World || !Mgr) return;

    // Источник правды — доменный документ (актёры строятся как его проекция).
    const FNMSBaseDocument Doc = NMSDoc::FromManager(*Mgr);
    CurrentBaseName = Doc.Name;
    CurrentGalacticAddress = Doc.GalacticAddress;

    ClearSceneParts();

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UMaterialInterface* BaseM = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_BasePart.M_BasePart"));
    UMaterialInterface* LitM = LoadObject<UMaterialInterface>(nullptr,
        FNMSViewportClient::bUnlitParts
            ? TEXT("/Game/NMSBaseBuilder/Materials/M_PartUnlit.M_PartUnlit")
            : TEXT("/Game/NMSBaseBuilder/Materials/M_PartLit.M_PartLit"));
    // неосвещаемый материал для эмиссив-слотов (лампы/экраны, флаг _F07_UNLIT в игре)
    UMaterialInterface* UnlitM = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_PartUnlit.M_PartUnlit"));
    // листва: masked + двусторонний (альфа диффуза вырезает форму листа)
    UMaterialInterface* FoliageM = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_PartFoliage.M_PartFoliage"));
    // стекло: translucent (игровой флаг _F30_REFRACTION) — иначе стекла не видно
    UMaterialInterface* GlassM = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_PartGlass.M_PartGlass"));

    int32 Spawned = 0;
    for (const FNMSPlacedPart& Obj : Doc.Parts)
    {
        FString Id = Obj.ObjectID;
        Id.RemoveFromStart(TEXT("^"));
        const TSharedPtr<FNMSPartData>* Part = PartById.Find(Id);

        // настоящая модель детали; если не нашли — маленький куб-маркер
        UStaticMesh* Mesh = nullptr;
        if (Part && (*Part).IsValid() && !(*Part)->ModelPath.IsEmpty())
            Mesh = LoadObject<UStaticMesh>(nullptr, *(*Part)->ModelPath);

        FActorSpawnParameters Params;
        Params.ObjectFlags = RF_Transient;
        AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(
            AStaticMeshActor::StaticClass(), Obj.Transform, Params);
        if (!A) continue;
        // Пандусы/лестницы авторски развёрнуты на 180° вокруг локальной вертикали
        // относительно ориентации игры (см. NMS_IsRampPart). Доворачиваем ПОСЛЕ
        // Up/At-трансформа, в локальном пространстве детали, чтобы база из игры
        // вставала так же, как в игре.
        if (Part && (*Part).IsValid() && NMS_IsRampPart(**Part))
            A->AddActorLocalRotation(FRotator(0.f, 180.f, 0.f));
        // Точечная поправка ориентации (детали с авторски развёрнутым мешем) — orientation_fix.json
        if (const float OFix = NMS_OrientationFixYaw(Obj.ObjectID))
            A->AddActorLocalRotation(FRotator(0.f, OFix, 0.f));
        A->SetActorLabel(FString::Printf(TEXT("NMS_%s"), *Obj.ObjectID));
        A->Tags.Add(FName(*Obj.ObjectID)); // надёжное хранение ID (метки редактор меняет)
        A->Tags.Add(FName(*FString::Printf(TEXT("UD:%lld"), (long long)Obj.UserData))); // цвет/материал детали

        if (UStaticMeshComponent* Comp = A->GetStaticMeshComponent())
        {
            Comp->SetMobility(EComponentMobility::Movable);
            Comp->SetCastShadow(false);
            if (Mesh)
            {
                Comp->SetStaticMesh(Mesh);
                // коллизия для выбора кликом (луч идёт по мешу, т.к. простой коллизии нет)
                Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                Comp->SetCollisionObjectType(ECC_WorldStatic);
                Comp->SetCollisionResponseToAllChannels(ECR_Ignore);
                Comp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
                const FString Cat = Part && (*Part).IsValid() ? (*Part)->Category : FString();
                const bool bFoliage = Part && (*Part).IsValid() && NMS_IsFoliagePart(**Part);
                // Резолв цвета/зон детали — через чистый SceneAdapter (шаг 4.2/4.3):
                // дефолтные цвета по категории + переопределение палитрой из UserData.
                const FNMSPartRenderInfo RI = NMS_ResolvePartRender(Obj, Cat);
                FLinearColor Col1 = RI.Color1, Col2 = RI.Color2;
                FLinearColor ZA = RI.ZoneA, ZB = RI.ZoneB, ZC = RI.ZoneC, ZD = RI.ZoneD;
                // путь к импортированной текстуре по короткому имени атласа
                auto TexPathOf = [](const FString& N) -> FString
                {
                    return N.IsEmpty() ? FString()
                        : FString::Printf(TEXT("/Game/NMSBaseBuilder/PartTex2/T_%s.T_%s"), *N, *N);
                };
                // строитель MID из набора текстур одного слота (diffuse/masks/normal/paint)
                auto BuildMID = [&](const FNMSTexSet& S) -> UMaterialInstanceDynamic*
                {
                    UTexture2D* Tex = S.Tex.IsEmpty() ? nullptr
                        : LoadObject<UTexture2D>(nullptr, *TexPathOf(S.Tex));
                    // эмиссив-слот (лампы/экраны) -> неосвещаемый материал, как в игре
                    UMaterialInterface* Surf = (S.bGlass && GlassM) ? GlassM
                                             : (S.bUnlit && UnlitM) ? UnlitM
                                             : ((bFoliage && FoliageM) ? FoliageM : LitM);
                    UMaterialInterface* M = (Tex && Surf) ? Surf : BaseM;
                    if (!M) return nullptr;
                    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, Comp);
                    if (Tex && Surf)
                    {
                        MID->SetTextureParameterValue(TEXT("BaseTex"), Tex);
                        // эмиссив-слот не красим маской и не добавляем рельеф — это лампы
                        if (!S.bUnlit)
                        {
                            if (UTexture2D* Mask = LoadObject<UTexture2D>(nullptr, *TexPathOf(S.Mask)))
                                MID->SetTextureParameterValue(TEXT("PaintMask"), Mask);
                            if (UTexture2D* Norm = LoadObject<UTexture2D>(nullptr, *TexPathOf(S.Norm)))
                                MID->SetTextureParameterValue(TEXT("NormalTex"), Norm);
                            if (UTexture2D* Mk = LoadObject<UTexture2D>(nullptr, *TexPathOf(S.Masks)))
                            {
                                MID->SetTextureParameterValue(TEXT("MasksTex"), Mk);
                                MID->SetScalarParameterValue(TEXT("UseMasks"), 1.f);
                            }
                            MID->SetVectorParameterValue(TEXT("Color"), Col1);
                            MID->SetVectorParameterValue(TEXT("Color2"), Col2);
                        }
                    }
                    else MID->SetVectorParameterValue(TEXT("Color"), Col1);
                    // 1-В-1: покраска по умолчанию = РОДНОЙ цвет детали Col1 (дефолт палитры).
                    // Камень Col1=песочный -> бледное каменное тело тонируется в песок (иконка),
                    // бронза в диффузе остаётся. Фибергласс Col1~белый -> беж диффуз виден.
                                        MID->SetVectorParameterValue(TEXT("ZoneCol0"), ZA);
                    MID->SetVectorParameterValue(TEXT("ZoneCol1"), ZB);
                    MID->SetVectorParameterValue(TEXT("ZoneCol2"), ZC);
                    MID->SetVectorParameterValue(TEXT("ZoneCol3"), ZD);
                    return MID;
                };

                const int32 N = Comp->GetNumMaterials();
                // ПУНКТ 4: текстуры ПО СЛОТАМ — ВКЛ для деталей с ПОЛНЫМ покрытием слотов
                // (Slots->Num() >= число материал-слотов меша N). Неполные (корветы) ->
                // откат на одиночный путь, чтобы не белели. Одноматериальные (камень, N=1)
                // тоже идут одиночным путём — наш рабочий результат не трогается.
                const TArray<FNMSTexSet>* Slots = NMS_PartSlots(Obj.ObjectID);
                const bool bFullCover = (Slots && N > 1 && Slots->Num() >= N);
                UE_LOG(LogTemp, Warning, TEXT("NMS PERSLOT %s: N=%d slots=%d cover=%d"),
                    *Obj.ObjectID, N, Slots ? Slots->Num() : -1, bFullCover ? 1 : 0);
                if (bFullCover)
                {
                    for (int32 i = 0; i < N; ++i)
                    {
                        const FNMSTexSet& S = (*Slots)[FMath::Min(i, Slots->Num() - 1)];
                        UMaterialInstanceDynamic* MID = BuildMID(S);
                        UE_LOG(LogTemp, Warning, TEXT("NMS PERSLOT   slot%d tex=%s mid=%d"),
                            i, *S.Tex, MID ? 1 : 0);
                        if (MID) Comp->SetMaterial(i, MID);
                    }
                }
                else
                {
                    // одиночный материал (как раньше): из part_textures.json
                    FString TexPath, MaskPath, NormPath, MasksPath, OccPath;
                    NMS_PartTexture(Cat, Obj.ObjectID, TexPath, MaskPath, NormPath, MasksPath, OccPath);
                    UTexture2D* Tex = TexPath.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *TexPath);
                    const bool bGlass1 = Slots && Slots->Num() > 0 && (*Slots)[0].bGlass;
                    UMaterialInterface* Surf1 = (bGlass1 && GlassM) ? GlassM : ((bFoliage && FoliageM) ? FoliageM : LitM);
                    UMaterialInterface* M = (Tex && Surf1) ? Surf1 : BaseM;
                    if (M)
                    {
                        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, Comp);
                        if (Tex && Surf1)
                        {
                            MID->SetTextureParameterValue(TEXT("BaseTex"), Tex);
                            if (UTexture2D* Mask = LoadObject<UTexture2D>(nullptr, *MaskPath))
                                MID->SetTextureParameterValue(TEXT("PaintMask"), Mask);
                            if (UTexture2D* Norm = LoadObject<UTexture2D>(nullptr, *NormPath))
                                MID->SetTextureParameterValue(TEXT("NormalTex"), Norm);
                            if (UTexture2D* Mk = LoadObject<UTexture2D>(nullptr, *MasksPath))
                            {
                                MID->SetTextureParameterValue(TEXT("MasksTex"), Mk);
                                MID->SetScalarParameterValue(TEXT("UseMasks"), 1.f);
                            }
                            if (UTexture2D* Oc = LoadObject<UTexture2D>(nullptr, *OccPath))
                                MID->SetTextureParameterValue(TEXT("OcclusionTex"), Oc);
                            MID->SetVectorParameterValue(TEXT("Color"), Col1);
                            MID->SetVectorParameterValue(TEXT("Color2"), Col2);
                        }
                        else MID->SetVectorParameterValue(TEXT("Color"), Col1);
                        // 1-В-1: покраска по умолчанию = родной цвет детали Col1 (камень -> песок).
                                                MID->SetVectorParameterValue(TEXT("ZoneCol0"), ZA);
                        MID->SetVectorParameterValue(TEXT("ZoneCol1"), ZB);
                        MID->SetVectorParameterValue(TEXT("ZoneCol2"), ZC);
                        MID->SetVectorParameterValue(TEXT("ZoneCol3"), ZD);
                        for (int32 i = 0; i < N; ++i) Comp->SetMaterial(i, MID);
                    }
                }
            }
            else if (Cube)
            {
                Comp->SetStaticMesh(Cube);
                Comp->SetRelativeScale3D(FVector(0.15f));
            }
        }
        ++Spawned;
    }
    UE_LOG(LogTemp, Log, TEXT("NMS UI: built base '%s' (%d parts)"), *CurrentBaseName, Spawned);
    RebuildSceneTree();   // автообновление дерева сцены после загрузки
    if (!bRestoring) { EditSnapshot = SnapshotScene(); bHasBaseline = true; UndoStack.Reset(); RedoStack.Reset(); }
#endif
}

// ===========================================================================
//  Undo / Redo — снимки сцены ДОМЕННЫМ документом (FNMSBaseDocument).
//  Раньше сериализовали всю сцену в JSON (ExportToString) на каждую правку —
//  дорого и завязано на формат экспорта. Теперь снимок = копия структур,
//  трансформы хранятся точно (без decompose/recompose через Up/At).
// ===========================================================================
FNMSBaseDocument SNMSBuilderUI::SnapshotScene()
{
    UNMSBaseManager* Mgr = NewObject<UNMSBaseManager>(); Mgr->AddToRoot();
    CollectSceneToManager(Mgr);
    FNMSBaseDocument Doc = NMSDoc::FromManager(*Mgr);
    Mgr->RemoveFromRoot();
    return Doc;
}
void SNMSBuilderUI::RestoreScene(const FNMSBaseDocument& Doc)
{
    bRestoring = true;
    ClearSceneParts();
    UNMSBaseManager* Mgr = NewObject<UNMSBaseManager>(); Mgr->AddToRoot();
    NMSDoc::ToManager(Doc, *Mgr);
    SpawnFromManager(Mgr);
    Mgr->RemoveFromRoot();
    bRestoring = false;
}
void SNMSBuilderUI::CommitEdit()
{
    if (bRestoring) return;
    FNMSBaseDocument S = SnapshotScene();
    if (!bHasBaseline) { EditSnapshot = MoveTemp(S); bHasBaseline = true; return; } // базовая точка
    if (S == EditSnapshot) return;                           // ничего не изменилось
    UndoStack.Add(EditSnapshot);
    if (UndoStack.Num() > 100) UndoStack.RemoveAt(0);
    RedoStack.Reset();
    EditSnapshot = MoveTemp(S);
}
void SNMSBuilderUI::DoUndo()
{
    if (UndoStack.Num() == 0) return;
    RedoStack.Add(EditSnapshot);
    FNMSBaseDocument S = UndoStack.Pop();
    EditSnapshot = S;
    RestoreScene(S);
}
void SNMSBuilderUI::DoRedo()
{
    if (RedoStack.Num() == 0) return;
    UndoStack.Add(EditSnapshot);
    FNMSBaseDocument S = RedoStack.Pop();
    EditSnapshot = S;
    RestoreScene(S);
}

// Сцена -> PlacedObjects (для сохранения/экспорта). ObjectID берём из метки NMS_^ID.
void SNMSBuilderUI::CollectSceneToManager(UNMSBaseManager* Mgr)
{
#if WITH_EDITOR
    UWorld* World = ViewportClient.IsValid() ? ViewportClient->GetWorld() : nullptr;
    if (!World || !Mgr) return;

    Mgr->PlacedObjects.Reset();
    Mgr->BaseName = CurrentBaseName;
    Mgr->GalacticAddress = CurrentGalacticAddress;

    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        AStaticMeshActor* A = *It;
        if (!A) continue;
        if (A->ActorHasTag(FName(TEXT("NMS_CABLE")))) continue; // кабель — отдельная сущность
        FString Label = A->GetActorLabel();
        if (!Label.RemoveFromStart(TEXT("NMS_"))) continue;

        FNMSPlacedObject P;
        // ID из тега (точный); метка — запасной путь
        P.ObjectID = (A->Tags.Num() > 0) ? A->Tags[0].ToString() : Label;
        P.Transform = A->GetActorTransform();
        // 1:1: убрать ДИСПЛЕЙНУЮ поправку ориентации (ramp +180 и orientation_fix),
        // которую добавили на спавне, чтобы в базу ушёл игровой Up/At. Ручной доворот
        // пользователя сохраняется (вычитаем только фиксированную поправку детали).
        {
            FString Id = P.ObjectID; Id.RemoveFromStart(TEXT("^"));
            float Off = NMS_OrientationFixYaw(P.ObjectID);
            if (const TSharedPtr<FNMSPartData>* Part = PartById.Find(Id))
                if ((*Part).IsValid() && NMS_IsRampPart(**Part)) Off += 180.f;
            if (Off != 0.f)
                P.Transform.SetRotation(P.Transform.GetRotation() * FRotator(0.f, -Off, 0.f).Quaternion());
        }
        P.Timestamp = (float)FDateTime::UtcNow().ToUnixTimestamp();
        // цвет/материал: читаем сохранённый при спавне UserData (тег "UD:<число>")
        for (const FName& Tg : A->Tags)
        {
            FString S = Tg.ToString();
            if (S.RemoveFromStart(TEXT("UD:"))) { P.UserData = FCString::Atoi64(*S); break; }
        }
        Mgr->PlacedObjects.Add(P);
    }
#endif
}

// Save Base As... — сохранить базу в JSON-файл (формат Blender-плагина).
void SNMSBuilderUI::OnMenuSaveBaseAs()
{
    IDesktopPlatform* DP = FDesktopPlatformModule::Get();
    if (!DP) return;

    TArray<FString> Picked;
    const bool bPicked = DP->SaveFileDialog(
        nullptr,
        TEXT("Сохранить базу как"),
        NMS_LoadLastDir(),
        CurrentBaseName.IsEmpty() ? TEXT("base.json") : CurrentBaseName + TEXT(".json"),
        TEXT("База NMS (*.json)|*.json"),
        EFileDialogFlags::None,
        Picked);
    if (!bPicked || Picked.Num() == 0) return;
    NMS_SaveLastDir(FPaths::GetPath(Picked[0]));

    UNMSBaseManager* Mgr = NewObject<UNMSBaseManager>();
    Mgr->AddToRoot();
    CollectSceneToManager(Mgr);
    const bool bOk = Mgr->ExportNMSSave(Picked[0]);
    UE_LOG(LogTemp, Log, TEXT("NMS UI: SaveBaseAs %s -> %s (%d parts)"),
        *Picked[0], bOk ? TEXT("OK") : TEXT("FAIL"), Mgr->PlacedObjects.Num());
    Mgr->RemoveFromRoot();
}

// Import from Clipboard — вставить базу из буфера обмена (как в плагине).
void SNMSBuilderUI::OnMenuImportClipboard()
{
    FString Clip;
    FPlatformApplicationMisc::ClipboardPaste(Clip);
    if (Clip.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("NMS UI: буфер обмена пуст"));
        return;
    }

    UNMSBaseManager* Mgr = NewObject<UNMSBaseManager>();
    Mgr->AddToRoot();
    if (Mgr->ImportFromString(Clip))
    {
        SpawnFromManager(Mgr);
    }
    Mgr->RemoveFromRoot();
}

// Export to Clipboard — скопировать базу в буфер обмена (как в плагине).
void SNMSBuilderUI::OnMenuExportClipboard()
{
    UNMSBaseManager* Mgr = NewObject<UNMSBaseManager>();
    Mgr->AddToRoot();
    CollectSceneToManager(Mgr);
    const FString Out = Mgr->ExportToString();
    FPlatformApplicationMisc::ClipboardCopy(*Out);
    UE_LOG(LogTemp, Log, TEXT("NMS UI: база скопирована в буфер (%d деталей)"),
        Mgr->PlacedObjects.Num());
    Mgr->RemoveFromRoot();
}

void SNMSBuilderUI::OnMenuExportToNMS()
{
    OnMenuExportClipboard();
}

// ===========================================================================
//  Верхний тулбар
// ===========================================================================
#undef LOCTEXT_NAMESPACE
