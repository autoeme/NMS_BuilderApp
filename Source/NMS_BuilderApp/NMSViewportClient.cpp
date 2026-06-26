#include "NMSViewportClient.h"
#include "NMSSnapData.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "CanvasTypes.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "LegacyScreenPercentageDriver.h"
#include "EngineModule.h"        // GetRendererModule()
#include "RendererInterface.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"  // LOD/рёбра для контура выделения
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ImageUtils.h"          // ImportFileAsTexture2D (смена фона)
#include "Engine/Texture2D.h"    // игровые атласы деталей
#include "Engine/TextureCube.h"  // кубемап окружения для отражений металла
#include "Misc/Paths.h"          // имя файла фона (HDRI -> панорама)
#include "GameFramework/WorldSettings.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/World.h"
#include "Components/LineBatchComponent.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Actor.h"

bool FNMSViewportClient::bUnlitParts = false; // СВЕТ ВКЛ по умолчанию: насыщенные цвета, бронза, контраст (lit M_PartLit). Переключатель в меню ВИД остаётся.

FNMSViewportClient::FNMSViewportClient()
{
    PreviewScene = MakeUnique<FPreviewScene>(
        FPreviewScene::ConstructionValues()
            .SetCreatePhysicsScene(true)   // нужна для line trace (выбор детали кликом)
            .SetLightBrightness(3.f)
            .SetSkyBrightness(1.f));

    UWorld* W = PreviewScene->GetWorld();
    if (!W) return;

    // --- НЕБО воссоздано из Engine-компонентов (standalone-совместимо) ---

    // ВАЖНО: атмосферное солнце должно быть ОДНО, иначе чёрный экран + варнинг
    // про конкуренцию. Поэтому гасим встроенный свет превью-сцены В НОЛЬ и
    // оставляем единственным солнцем — наш DirectionalLight.
    PreviewScene->SetLightBrightness(0.f);

    // СВЕТ «КАК В ИГРЕ, НО ДЁШЕВО»: одно солнце БЕЗ ТЕНЕЙ + небесная подсветка.
    // Тени — самое дорогое в свете; сам расчёт освещения поверхностей дёшев.
    // Один источник => нет предупреждения ForwardShadingPriority.
    {
        ADirectionalLight* Sun = W->SpawnActor<ADirectionalLight>();
        if (Sun)
        {
            Sun->SetActorRotation(FRotator(-35.f, -45.f, 0.f));
            if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
            {
                L->SetMobility(EComponentMobility::Movable);
                L->SetIntensity(9.f);           // яркость как в игре (солнце планеты)
                L->SetLightColor(FLinearColor(1.0f, 0.89f, 0.74f)); // ТЁПЛЫЙ игровой тон
                L->SetCastShadows(false);       // БЕЗ теней
                L->SetAtmosphereSunLight(true); // единственное солнце атмосферы
                SunComp = L;
            }
        }
    }

    // Атмосфера — градиент неба.
    {
        AActor* AtmoActor = W->SpawnActor<AActor>();
        if (AtmoActor)
        {
            USkyAtmosphereComponent* Atmo = NewObject<USkyAtmosphereComponent>(AtmoActor);
            AtmoActor->SetRootComponent(Atmo);
            Atmo->RegisterComponent();
        }
    }

    // SkyLight — РАВНОМЕРНАЯ подсветка СО ВСЕХ СТОРОН, включая снизу (как Blender Solid).
    // Для стройки: ни одна грань не чернеет, можно точно стыковать детали под любым углом.
    {
        ASkyLight* Sky = W->SpawnActor<ASkyLight>();
        if (Sky)
        {
            if (USkyLightComponent* SC = Cast<USkyLightComponent>(Sky->GetLightComponent()))
            {
                SC->SetMobility(EComponentMobility::Movable);
                SC->SetIntensity(4.0f);                       // отражения металла + ровная заливка
                // Тёплое окружение (как небо планеты): металл (masks.G) отражает его и
                // становится золотым. Умеренно тёплый, чтобы не пережелтить камень.
                SC->SetLightColor(FLinearColor(1.0f, 0.90f, 0.78f));
                if (UTextureCube* EnvCube = LoadObject<UTextureCube>(nullptr,
                        TEXT("/Engine/MapTemplates/Sky/SunsetAmbientCubemap.SunsetAmbientCubemap")))
                {
                    SC->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
                    SC->Cubemap = EnvCube;
                    SC->SourceCubemapAngle = 0.f;
                }
                SC->RecaptureSky();
            }
        }
    }

    // 5) Небо — стандартная sky sphere движка (/Engine/EngineSky): голубой купол
    //    с горизонтом. Блики/ореол солнца убраны через show-флаги (Bloom/LensFlares).
    {
        UStaticMesh* SkyMesh = LoadObject<UStaticMesh>(nullptr,
            TEXT("/Engine/EngineSky/SM_SkySphere.SM_SkySphere"));
        if (SkyMesh)
        {
            AStaticMeshActor* SkyActor = W->SpawnActor<AStaticMeshActor>();
            if (SkyActor)
            {
                if (UStaticMeshComponent* SM = SkyActor->GetStaticMeshComponent())
                {
                    SM->SetMobility(EComponentMobility::Movable);
                    SM->SetStaticMesh(SkyMesh);
                    SM->SetWorldScale3D(FVector(400.f));
                    SM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    SM->SetCastShadow(false);
                    // Фон меню игры (M_SkyBG, создаётся скриптом create_bg_material.py);
                    // если его нет — стандартное голубое небо движка.
                    UMaterialInterface* SkyMat = LoadObject<UMaterialInterface>(nullptr,
                        TEXT("/Game/NMSBaseBuilder/Materials/M_SkyBG.M_SkyBG"));
                    if (!SkyMat) SkyMat = LoadObject<UMaterialInterface>(nullptr,
                        TEXT("/Engine/EngineSky/M_Sky_Panning_Clouds2.M_Sky_Panning_Clouds2"));
                    if (SkyMat) SM->SetMaterial(0, SkyMat);
                    SkySphereComp = SM; // запомнили купол — для смены фона кнопкой
                }
            }
        }
    }

    // Пол убран.
}

FNMSViewportClient::~FNMSViewportClient()
{
    PreviewScene.Reset();
}

// Смена фона сцены: PNG с диска -> текстура -> MID от M_SkyBG на купол.
void FNMSViewportClient::SetBackgroundImage(const FString& PngPath)
{
    if (!SkySphereComp.IsValid()) return;

    UTexture2D* Tex = FImageUtils::ImportFileAsTexture2D(PngPath);
    if (!Tex)
    {
        UE_LOG(LogTemp, Warning, TEXT("NMS: не удалось загрузить фон %s"), *PngPath);
        return;
    }

    // HDRI-панорамы (в имени "HDRI") оборачиваются вокруг сцены и вращаются
    // с камерой (M_SkyPano); остальные — статичный задник на весь экран (M_SkyBG).
    const bool bPano = FPaths::GetCleanFilename(PngPath).Contains(TEXT("HDRI"));
    UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, bPano
        ? TEXT("/Game/NMSBaseBuilder/Materials/M_SkyPano.M_SkyPano")
        : TEXT("/Game/NMSBaseBuilder/Materials/M_SkyBG.M_SkyBG"));
    if (!Base) Base = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_SkyBG.M_SkyBG"));
    if (!Base) return;

    UMaterialInstanceDynamic* MID =
        UMaterialInstanceDynamic::Create(Base, SkySphereComp.Get());
    MID->SetTextureParameterValue(TEXT("BGTex"), Tex);
    SkySphereComp->SetMaterial(0, MID);
}

UWorld* FNMSViewportClient::GetWorld() const
{
    return PreviewScene.IsValid() ? PreviewScene->GetWorld() : nullptr;
}

void FNMSViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
    if (!Canvas || !Viewport || !PreviewScene.IsValid()) return;

    const FIntPoint Size = Viewport->GetSizeXY();
    if (Size.X < 1 || Size.Y < 1) { Canvas->Clear(SkyColor); return; }

    // dt по реальному времени (у FViewportClient нет своего тика)
    const double Now = FPlatformTime::Seconds();
    float Dt = (LastDrawTime > 0.0) ? (float)(Now - LastDrawTime) : 0.016f;
    LastDrawTime = Now;
    Dt = FMath::Clamp(Dt, 0.0f, 0.1f);
    UpdateCamera(Dt);

    // ФОН-ГРАДИЕНТ по референсу: светлый верх -> синий низ (без атмосферы).
    {
        const float W = (float)Size.X;
        const float H = (float)Size.Y;
        const FLinearColor Top   (0.52f, 0.70f, 0.88f, 1.f); // светло-голубой верх
        const FLinearColor Bottom(0.20f, 0.32f, 0.62f, 1.f); // насыщенно-синий низ
        const int32 Bands = 96;
        for (int32 i = 0; i < Bands; ++i)
        {
            const float t  = (float)i / (Bands - 1);
            const float y0 = (float)i / Bands * H;
            const float y1 = (float)(i + 1) / Bands * H;
            const FLinearColor C = FMath::Lerp(Top, Bottom, t);
            FCanvasTileItem Tile(FVector2D(0, y0), FVector2D(W, y1 - y0 + 1.f), C);
            Tile.BlendMode = SE_BLEND_Opaque;
            Canvas->DrawItem(Tile);
        }
    }

    // Источник сцены: НАША preview-сцена (своё небо движка + детали).
    // Раньше тут подменялось на мир редактора — это тянуло Lumen/рейтрейсинг
    // editor-мира и вешало GPU (DXGI_DEVICE_HUNG), а также показывало чужое небо.
    FSceneInterface* RenderScene = PreviewScene->GetScene();

    // СОЛНЦЕ СЛЕДУЕТ ЗА КАМЕРОЙ: свет идёт вдоль взгляда (+ небольшой наклон сверху-сбоку),
    // поэтому грань, обращённая к тебе, всегда освещённая — как ни крути камеру.
    if (bSunFollowsCamera && SunComp.IsValid())
    {
        FRotator SunRot = CameraRotation;
        SunRot.Pitch -= 25.f; // солнце чуть выше линии взгляда — мягкий объём
        SunRot.Yaw   -= 20.f; // и чуть сбоку — грани читаются, но тёмной стороны к тебе нет
        SunComp->SetWorldRotation(SunRot);
    }

    FSceneViewFamilyContext ViewFamily(
        FSceneViewFamily::ConstructionValues(
            Viewport,
            RenderScene,
            FEngineShowFlags(ESFIM_Game))
        .SetTime(FGameTime::GetTimeSinceAppStart()));

    // Параметры вида/камеры.
    FSceneViewInitOptions ViewInit;
    ViewInit.ViewFamily = &ViewFamily;
    ViewInit.SetViewRectangle(FIntRect(0, 0, Size.X, Size.Y));
    ViewInit.ViewOrigin = CameraLocation;

    // матрица поворота вида из FRotator камеры
    ViewInit.ViewRotationMatrix = FInverseRotationMatrix(CameraRotation)
        * FMatrix(
            FPlane(0, 0, 1, 0),
            FPlane(1, 0, 0, 0),
            FPlane(0, 1, 0, 0),
            FPlane(0, 0, 0, 1));

    const float HalfFOV = FMath::DegreesToRadians(CameraFOV * 0.5f);
    const float AspectRatio = (float)Size.X / (float)Size.Y;
    ViewInit.ProjectionMatrix = FReversedZPerspectiveMatrix(
        HalfFOV, AspectRatio, 1.0f, GNearClippingPlane);

    FSceneView* View = new FSceneView(ViewInit);
    ViewFamily.Views.Add(View);

    // Кэш матриц для депроекции курсора (выбор детали) на следующих кадрах.
    CachedInvView = View->ViewMatrices.GetInvViewMatrix();
    CachedInvProj = View->ViewMatrices.GetInvProjectionMatrix();
    CachedViewProj = View->ViewMatrices.GetViewProjectionMatrix();
    CachedViewRect = FIntRect(0, 0, Size.X, Size.Y);
    bViewCached = true;

    // --- СТАБИЛЬНОСТЬ GPU ---
    // Полный рендер мира редактора (Lumen + аппаратный рейтрейсинг + облака),
    // да ещё дважды за кадр (вьюпорт редактора + наш), вешал GPU:
    // DXGI_ERROR_DEVICE_HUNG / «Failed to create raytracing RTPSO».
    // Для превью-вьюпорта эти фичи не нужны — отключаем их.
    {
        FEngineShowFlags& SF = ViewFamily.EngineShowFlags;
        SF.DisableAdvancedFeatures();          // Lumen-пост, DOF, bloom, motion blur, AA и пр.
        SF.SetLumenGlobalIllumination(false);  // Lumen GI (использует HW ray tracing)
        SF.SetLumenReflections(false);         // Lumen-отражения (HW ray tracing)
        SF.SetScreenSpaceReflections(false);
        SF.SetDistanceFieldAO(false);
        SF.SetContactShadows(false);
        SF.SetVolumetricFog(false);
        SF.SetTemporalAA(false);               // без TSR/TAA
        SF.SetMotionBlur(false);
        SF.SetBloom(false);                    // убрать свечение/ореол от солнца
        SF.SetLensFlares(false);               // убрать блики объектива
        SF.SetEyeAdaptation(false);            // без авто-экспозиции (не пересвечивает)
    }

    // screen percentage (обязательно в новых версиях UE)
    ViewFamily.EngineShowFlags.ScreenPercentage = true;
    ViewFamily.SetScreenPercentageInterface(
        new FLegacyScreenPercentageDriver(ViewFamily, 1.0f));

    // Грид-сетка как 3D-линии в сцене (с глубиной) — рисуем ДО рендера сцены.
    DrawGrid();
    // Гизмо перемещения (цветные оси) поверх — добавляется к тем же линиям.
    DrawGizmo();
    DrawCurvePreview();
    DrawSnapDebug();   // кадры точек стыковки (после Flush в DrawGrid)

    GetRendererModule().BeginRenderingViewFamily(Canvas, &ViewFamily);

    // Рамка выбранной детали — overlay поверх сцены (так и должно быть).
    DrawSelection(Canvas, View);
    DrawCompass(Canvas, Size);
    DrawMarquee(Canvas, Size);

    // Индикатор режима снапа при наличии выбранной детали.
    if (SelectedActor.IsValid())
    {
        if (UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr)
        {
            const FString Txt = bSnapToParts ? TEXT("СНАП: ВКЛ  (V)") : TEXT("СНАП: ВЫКЛ  (V)");
            const FLinearColor Col = bSnapToParts ? FLinearColor(0.35f, 1.f, 0.45f) : FLinearColor(1.f, 0.6f, 0.3f);
            FCanvasTextItem T(FVector2D(16.f, 16.f), FText::FromString(Txt), Font, Col);
            T.EnableShadow(FLinearColor::Black);
            Canvas->DrawItem(T);
        }
    }

    // НЕ вызываем InvalidateDisplay здесь — это рекурсия (Draw->invalidate->Draw)
    // и stack overflow. Непрерывный тик вьюпорта сделаем правильно через
    // SNMSBuilderUI::Tick (на уровне виджета), а не из Draw.
}


// ===========================================================================
//  Грид-сетка постройки (стиль NMS) — рисуется на плоскости Z=0
//  Центрируется на проекции камеры и снапится к ячейке -> ощущение бесконечной.
//  Линии разбиты на сегменты для радиального затухания от камеры.
// ===========================================================================
void FNMSViewportClient::DrawGrid()
{
    if (!bShowGrid || GridMode == 0 || !PreviewScene.IsValid()) return;
    UWorld* World = PreviewScene->GetWorld();
    if (!World) return;
    ULineBatchComponent* LB = World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent);
    if (!LB) return;
    LB->Flush(); // очистить линии прошлого кадра

    const float Cell   = FMath::Max(GridCellSize, 1.f);
    const float Extent = Cell * GridHalfCount;
    // Сетка СТАТИЧНА: закреплена в мировом нуле и не следует за камерой.
    const float CenterX = 0.f;
    const float CenterY = 0.f;
    const int32 Seg = 10;

    auto FadeAt = [this](const FVector& P) -> float
    {
        const float d = FVector::Dist2D(P, CameraLocation);
        return FMath::Clamp(1.f - (d - GridFadeStart) / FMath::Max(GridFadeEnd - GridFadeStart, 1.f), 0.f, 1.f);
    };
    auto DrawFadedLine = [&](const FVector& P0, const FVector& P1, const FLinearColor& Col, float Width)
    {
        for (int32 s = 0; s < Seg; ++s)
        {
            const FVector A = FMath::Lerp(P0, P1, (float)s / Seg);
            const FVector B = FMath::Lerp(P0, P1, (float)(s + 1) / Seg);
            const float Fade = FadeAt((A + B) * 0.5f);
            if (Fade <= 0.02f) continue;
            FLinearColor C = Col; C.A *= Fade;
            // 3D-линия с глубиной (SDPG_World) — детали её перекрывают
            LB->DrawLine(A, B, C, SDPG_World, Width, 0.f);
        }
    };

    // Режим 2 = «максимально чёткая»: ярче и толще линии для точной постройки.
    const bool bSharp = (GridMode >= 2);
    FLinearColor MinorC = GridMinorColor;
    FLinearColor MajorC = GridMajorColor;
    if (bSharp)
    {
        MinorC.A = 0.55f;          // тонкие линии чёткие
        MajorC = FLinearColor(0.30f, 1.00f, 0.55f, 0.95f); // основные — яркий зелёный
    }
    else
    {
        // обычный режим — теперь тоже выразительный и чёткий
        MinorC.A = FMath::Max(MinorC.A, 0.42f);
        MajorC = FLinearColor(0.55f, 0.78f, 1.00f, 0.90f); // основные — ясный голубой
    }
    const float MinorW = bSharp ? 1.7f : 1.1f;
    const float MajorW = bSharp ? 3.4f : 2.7f;

    for (int32 i = -GridHalfCount; i <= GridHalfCount; ++i)
    {
        const float Off = i * Cell;
        const bool bMajor = (GridMajorEvery > 0) && (i % GridMajorEvery == 0);
        const FLinearColor Col = bMajor ? MajorC : MinorC;
        const float Width = bMajor ? MajorW : MinorW;
        DrawFadedLine(FVector(CenterX + Off, CenterY - Extent, 0.f),
                      FVector(CenterX + Off, CenterY + Extent, 0.f), Col, Width);
        DrawFadedLine(FVector(CenterX - Extent, CenterY + Off, 0.f),
                      FVector(CenterX + Extent, CenterY + Off, 0.f), Col, Width);
    }
}

// ===========================================================================
//  Рамка выбранной детали (12 рёбер AABB, overlay)
// ===========================================================================
// Кэш «жёстких» рёбер меша (силуэт/грани, без диагоналей триангуляции) в локальных координатах.
static const TArray<FVector>& NMS_GetHardEdges(UStaticMesh* Mesh)
{
    static TMap<UStaticMesh*, TArray<FVector>> Cache;
    if (TArray<FVector>* Found = Cache.Find(Mesh)) return *Found;
    TArray<FVector>& Out = Cache.Add(Mesh);
    if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.Num() == 0) return Out;
    const FStaticMeshLODResources& LOD = Mesh->GetRenderData()->LODResources[0];
    const FPositionVertexBuffer& PVB = LOD.VertexBuffers.PositionVertexBuffer;
    if (PVB.GetNumVertices() == 0) return Out;
    FIndexArrayView Idx = LOD.IndexBuffer.GetArrayView();
    const int32 NumTris = Idx.Num() / 3;
    struct FE { FVector N; FVector A; FVector B; int32 Count; bool bHard; };
    TMap<TPair<FIntVector, FIntVector>, FE> Map;
    auto Q = [](const FVector& v){ return FIntVector(FMath::RoundToInt(v.X*10.f), FMath::RoundToInt(v.Y*10.f), FMath::RoundToInt(v.Z*10.f)); };
    auto Less = [](const FIntVector& a, const FIntVector& b){ if (a.X!=b.X) return a.X<b.X; if (a.Y!=b.Y) return a.Y<b.Y; return a.Z<b.Z; };
    for (int32 tr = 0; tr < NumTris; ++tr)
    {
        const FVector P[3] = {
            (FVector)PVB.VertexPosition(Idx[tr*3+0]),
            (FVector)PVB.VertexPosition(Idx[tr*3+1]),
            (FVector)PVB.VertexPosition(Idx[tr*3+2]) };
        const FVector N = ((P[1]-P[0]) ^ (P[2]-P[0])).GetSafeNormal();
        for (int32 e = 0; e < 3; ++e)
        {
            const FVector a = P[e], b = P[(e+1)%3];
            FIntVector qa = Q(a), qb = Q(b);
            TPair<FIntVector,FIntVector> key = Less(qa,qb) ? TPair<FIntVector,FIntVector>(qa,qb) : TPair<FIntVector,FIntVector>(qb,qa);
            if (FE* Ex = Map.Find(key)) { Ex->Count++; if (FVector::DotProduct(Ex->N, N) < 0.819f) Ex->bHard = true; }
            else { FE fe; fe.N=N; fe.A=a; fe.B=b; fe.Count=1; fe.bHard=false; Map.Add(key, fe); }
        }
    }
    for (const auto& KV : Map) { const FE& fe = KV.Value; if (fe.Count == 1 || fe.bHard) { Out.Add(fe.A); Out.Add(fe.B); } }
    return Out;
}

void FNMSViewportClient::DrawSelection(FCanvas* Canvas, const FSceneView* View)
{
    (void)View;
    if (!Canvas) return;
    TArray<AActor*> Sel; GatherSelection(Sel);
    if (Sel.Num() == 0) return;
    const FLinearColor Col(0.16f, 1.0f, 0.42f, 1.0f); // зелёный контур, как выделение в Blender
    int32 Budget = 12000; // защита от перегрузки при больших выделениях
    for (AActor* A : Sel)
    {
        AStaticMeshActor* SM = Cast<AStaticMeshActor>(A); if (!SM) continue;
        UStaticMeshComponent* Comp = SM->GetStaticMeshComponent(); if (!Comp) continue;
        UStaticMesh* Mesh = Comp->GetStaticMesh(); if (!Mesh) continue;
        const TArray<FVector>& E = NMS_GetHardEdges(Mesh);
        const FTransform X = Comp->GetComponentTransform();
        for (int32 i = 0; i + 1 < E.Num(); i += 2)
        {
            if (--Budget < 0) return;
            FVector2D s, e2;
            if (WorldToPixel(X.TransformPosition(E[i]), s) && WorldToPixel(X.TransformPosition(E[i+1]), e2))
            {
                FCanvasLineItem L(s, e2); L.SetColor(Col); L.LineThickness = 1.4f; Canvas->DrawItem(L);
            }
        }
    }
}

// Рамка выделения (2D-оверлей).
void FNMSViewportClient::DrawMarquee(FCanvas* Canvas, const FIntPoint& Size)
{
    if (!bMarquee || !Canvas) return;
    const float x0=FMath::Min(MarqStart.X,MarqCur.X), x1=FMath::Max(MarqStart.X,MarqCur.X);
    const float y0=FMath::Min(MarqStart.Y,MarqCur.Y), y1=FMath::Max(MarqStart.Y,MarqCur.Y);
    FCanvasTileItem Fill(FVector2D(x0,y0), FVector2D(x1-x0,y1-y0), FLinearColor(0.2f,0.6f,1.f,0.12f));
    Fill.BlendMode = SE_BLEND_Translucent; Canvas->DrawItem(Fill);
    const FLinearColor Col(0.25f,0.7f,1.f,1.f);
    auto Ln=[&](FVector2D A,FVector2D B){ FCanvasLineItem L(A,B); L.SetColor(Col); L.LineThickness=1.5f; Canvas->DrawItem(L); };
    Ln(FVector2D(x0,y0),FVector2D(x1,y0)); Ln(FVector2D(x1,y0),FVector2D(x1,y1)); Ln(FVector2D(x1,y1),FVector2D(x0,y1)); Ln(FVector2D(x0,y1),FVector2D(x0,y0));
}

// Выбрать все детали (NMS_) внутри рамки.
void FNMSViewportClient::SelectMarquee(FViewport* Viewport)
{
    SelectedActors.Reset();
    UWorld* World = GetRenderWorld(); if (!World) return;
    const float x0=FMath::Min(MarqStart.X,MarqCur.X), x1=FMath::Max(MarqStart.X,MarqCur.X);
    const float y0=FMath::Min(MarqStart.Y,MarqCur.Y), y1=FMath::Max(MarqStart.Y,MarqCur.Y);
    if (FMath::Abs(x1-x0) < 4.f && FMath::Abs(y1-y0) < 4.f) { SelectedActor = nullptr; return; }
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        AActor* A = *It; if (!A || !A->GetActorNameOrLabel().StartsWith(TEXT("NMS_"))) continue;
        FVector2D Px;
        if (WorldToPixel(A->GetActorLocation(), Px) && Px.X>=x0 && Px.X<=x1 && Px.Y>=y0 && Px.Y<=y1)
            SelectedActors.Add(A);
    }
    SelectedActor = (SelectedActors.Num()>0) ? SelectedActors[0].Get() : nullptr;
    ApplyHighlight();
}

// Подсветка выделенных деталей: overlay-материал поверх меша (без рамки-коробки).
void FNMSViewportClient::ApplyHighlight()
{
    // Подсветка теперь — зелёный КОНТУР по рёбрам (DrawSelection). Материал НЕ подменяем,
    // чтобы текстура/цвет оставались видны и покраска не ломалась. Здесь только снимаем
    // прежние подмены, если они где-то остались.
    for (auto It = HiSaved.CreateIterator(); It; ++It)
    {
        UStaticMeshComponent* C = It->Key.Get();
        if (C) { const TArray<UMaterialInterface*>& Orig = It->Value; for (int32 i = 0; i < Orig.Num(); ++i) C->SetMaterial(i, Orig[i]); }
        It.RemoveCurrent();
    }
}

// ===========================================================================
//  Навигация: зум (колесо), орбита (СКМ), пан (Alt+СКМ)
// ===========================================================================
void FNMSViewportClient::DollyZoom(float Dir)
{
    // приближение к точке фокуса; шаг растёт с дистанцией (плавно у далёких видов)
    const FVector Forward = CameraRotation.Vector();
    const float Dist = FMath::Max(FVector::Dist(CameraLocation, FocusPoint), 50.f);
    const float Step = FMath::Clamp(Dist * 0.15f, ZoomStep * 0.4f, ZoomStep * 4.f);
    CameraLocation += Forward * Step * Dir;
}

void FNMSViewportClient::OrbitCamera(float DX, float DY)
{
    float Dist = FVector::Dist(CameraLocation, FocusPoint);
    if (Dist < 1.f) Dist = 300.f;
    CameraRotation.Yaw   += DX * MouseSensitivity;
    CameraRotation.Pitch = FMath::Clamp(CameraRotation.Pitch + DY * MouseSensitivity, -89.f, 89.f);
    const FVector Dir = CameraRotation.Vector();
    CameraLocation = FocusPoint - Dir * Dist;   // камера смотрит на FocusPoint
}

void FNMSViewportClient::PanCamera(float DX, float DY)
{
    const FVector Right = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y);
    const FVector Up    = FVector::UpVector;
    const FVector Delta = (-Right * DX + Up * DY) * PanSpeed;
    CameraLocation += Delta;
    FocusPoint     += Delta;
}

// ===========================================================================
//  Выбор и манипуляция деталей
// ===========================================================================
UWorld* FNMSViewportClient::GetRenderWorld() const
{
    // Рендерим и трассируем нашу preview-сцену (детали теперь спавнятся в неё).
    return GetWorld();
}

bool FNMSViewportClient::PickPartUnderCursor(FViewport* Viewport)
{
    if (!Viewport || !bViewCached) return false;
    UWorld* World = GetRenderWorld();
    if (!World) return false;

    const FVector2D Mouse((float)Viewport->GetMouseX(), (float)Viewport->GetMouseY());
    FVector RayOrigin, RayDir;
    FSceneView::DeprojectScreenToWorld(Mouse, CachedViewRect, CachedInvView, CachedInvProj, RayOrigin, RayDir);

    const FVector Start = RayOrigin;
    const FVector End   = RayOrigin + RayDir * 1.0e6f;

    FHitResult Hit;
    FCollisionQueryParams Params(FName(TEXT("NMSPick")), /*bTraceComplex=*/true); // по мешу: простой коллизии нет
    if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
    {
        AActor* A = Hit.GetActor();
        if (A && A->GetActorNameOrLabel().StartsWith(TEXT("NMS_")))
        {
            SelectedActor = A;
            FocusPoint = A->GetActorLocation(); // фокус орбиты на выбранной детали
            return true;
        }
    }
    SelectedActor = nullptr; // клик мимо -> снять выделение
    return false;
}

void FNMSViewportClient::SetSelectedActor(AActor* InActor)
{
    SelectedActor = InActor;
    if (InActor) FocusPoint = InActor->GetActorLocation(); // орбита/фокус на новой детали
}

AActor* FNMSViewportClient::GetSelectedActor() const
{
    return SelectedActor.Get();
}

void FNMSViewportClient::ApplyPartMaterial(AActor* Actor)
{
    if (!Actor) return;
    UStaticMeshComponent* C = Actor->FindComponentByClass<UStaticMeshComponent>();
    if (!C) return;

    // Текстурный материал (вид как в игре), если UI передал атлас; иначе плоский цвет.
    UMaterialInterface* BaseM = nullptr;
    UTexture2D* Tex = nullptr;
    if (!PendingBaseTex.IsEmpty())
    {
        Tex = LoadObject<UTexture2D>(nullptr, *PendingBaseTex);
        if (Tex)
        {
            const TCHAR* MatPath = bUnlitParts
                ? TEXT("/Game/NMSBaseBuilder/Materials/M_PartUnlit.M_PartUnlit")
                : (PendingMasked ? TEXT("/Game/NMSBaseBuilder/Materials/M_PartFoliage.M_PartFoliage")
                                 : TEXT("/Game/NMSBaseBuilder/Materials/M_PartLit.M_PartLit"));
            BaseM = LoadObject<UMaterialInterface>(nullptr, MatPath);
            if (!BaseM)  // материала листвы ещё нет -> откат на обычный lit
                BaseM = LoadObject<UMaterialInterface>(nullptr,
                    TEXT("/Game/NMSBaseBuilder/Materials/M_PartLit.M_PartLit"));
        }
    }
    if (!BaseM) BaseM = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/NMSBaseBuilder/Materials/M_BasePart.M_BasePart"));
    if (!BaseM) return;

    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseM, C);
    if (Tex)
    {
        MID->SetTextureParameterValue(TEXT("BaseTex"), Tex);
        if (UTexture2D* Mask = LoadObject<UTexture2D>(nullptr, *PendingPaintMask))
            MID->SetTextureParameterValue(TEXT("PaintMask"), Mask);
        if (UTexture2D* Norm = LoadObject<UTexture2D>(nullptr, *PendingNormalTex))
            MID->SetTextureParameterValue(TEXT("NormalTex"), Norm);
        if (UTexture2D* Mk = LoadObject<UTexture2D>(nullptr, *PendingMasksTex))
        {
            MID->SetTextureParameterValue(TEXT("MasksTex"), Mk);
            MID->SetScalarParameterValue(TEXT("UseMasks"), 1.f);
        }
        if (UTexture2D* Oc = LoadObject<UTexture2D>(nullptr, *PendingOccTex))
            MID->SetTextureParameterValue(TEXT("OcclusionTex"), Oc);
        MID->SetVectorParameterValue(TEXT("Color"), PendingPartColor);
        MID->SetVectorParameterValue(TEXT("Color2"), PendingPartColor2);
    }
    else
    {
        MID->SetVectorParameterValue(TEXT("Color"), PendingPartColor);
    }
    const int32 N = C->GetNumMaterials();
    for (int32 i = 0; i < N; ++i) C->SetMaterial(i, MID);
}

void FNMSViewportClient::SelectSpawnedActor(AActor* Actor)
{
    if (!Actor) return;
    ClearSelection();                 // снять прошлый выбор (восстановит материалы)
    SetSelectedActor(Actor);          // показать гизмо, фокус орбиты на детали
    SelectedActors.Reset();
    SelectedActors.Add(Actor);
    ApplyHighlight();                 // подсветка выбранной (как при обычном выборе)
    TransformMode = ENMSTransformMode::Move; // сразу режим перемещения (гизмо/сетка/снап)
}

void FNMSViewportClient::StartPlacing(AActor* GhostActor)
{
    SetSelectedActor(GhostActor);
    bPlacingGhost = (GhostActor != nullptr);
}

void FNMSViewportClient::ClearSelection()
{
    ApplyHighlight();        // восстановить материалы, очистить HiSaved (снять подсветку)
    SelectedActors.Reset();
    SelectedActor = nullptr;
    GizmoAxis = -1;
    bDragging = false;
}

void FNMSViewportClient::CancelPlacing()
{
    if (bPlacingGhost)
    {
        bPlacingGhost = false;
        if (SelectedActor.IsValid()) { SelectedActor->Destroy(); SelectedActor = nullptr; }
    }
}

void FNMSViewportClient::MouseMove(FViewport* Viewport, int32 X, int32 Y)
{
    if (bMarquee && Viewport) { MarqCur = FVector2D((float)X, (float)Y); Viewport->Invalidate(); return; }
    // Деталь следует за курсором (снап к деталям / сетка), как при постройке.
    if (bPlacingGhost && SelectedActor.IsValid())
    {
        FVector G;
        if (DeprojectMouseToGround(Viewport, G))
        {
            const bool bSnapped = bSnapToParts && TrySnapGhost(G);
            if (!bSnapped)
            {
                G.X = FMath::GridSnap(G.X, MoveSnap);
                G.Y = FMath::GridSnap(G.Y, MoveSnap);
                G.Z = 0.f;
                SelectedActor->SetActorLocation(G);
            }
            if (Viewport) Viewport->Invalidate();
        }
    }
}

// ID детали (ObjectID) из тега актёра. Пусто — не наша деталь.
static FString NMS_ActorPartId(const AActor* A)
{
    if (!A) return FString();
    for (const FName& Tg : A->Tags)
    {
        FString S = Tg.ToString();
        if (S.StartsWith(TEXT("UD:"))) continue;        // тег цвета/материала
        if (S == TEXT("IMPORT") || S == TEXT("NMS_CABLE")) return FString();
        S.RemoveFromStart(TEXT("^"));
        return S;
    }
    return FString();
}

// Пытается прилепить призрак к ближайшей совместимой точке стыковки соседней детали.
bool FNMSViewportClient::TrySnapGhost(const FVector& Cursor)
{
    AActor* Ghost = SelectedActor.Get();
    if (!Ghost) return false;

    FNMSSnapData& SD = FNMSSnapData::Get();
    SD.EnsureLoaded();

    const FString GhostId = NMS_ActorPartId(Ghost);
    const FString* GhostGroupName = SD.GroupForPart(GhostId);
    if (!GhostGroupName) return false;
    const FNMSSnapGroup* GhostGroup = SD.GetGroup(*GhostGroupName);
    if (!GhostGroup) return false;

    UWorld* World = GetRenderWorld();
    if (!World) return false;

    const FMatrix Flip = FRotationMatrix(FRotator(0.f, 180.f, 0.f)); // разворот «лицом к лицу» вокруг Z

    float   BestDist = SnapRadius;
    bool    bFound   = false;
    FMatrix BestGhost = FMatrix::Identity;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Other = *It;
        if (!Other || Other == Ghost) continue;
        if (!Other->GetActorNameOrLabel().StartsWith(TEXT("NMS_"))) continue;

        const FString OtherId = NMS_ActorPartId(Other);
        const FString* OtherGroupName = SD.GroupForPart(OtherId);
        if (!OtherGroupName) continue;
        const FNMSSnapGroup* OtherGroup = SD.GetGroup(*OtherGroupName);
        if (!OtherGroup) continue;

        TArray<FString> OldNames, NewNames;
        if (!SD.GetPair(*OtherGroupName, *GhostGroupName, OldNames, NewNames)) continue;
        if (NewNames.Num() == 0) continue;

        const FMatrix Told = Other->GetActorTransform().ToMatrixWithScale();

        for (const FString& OldName : OldNames)
        {
            const FNMSSnapPoint* OldSp = OtherGroup->FindPoint(OldName);
            if (!OldSp) continue;

            // мир точки стыковки старой детали: локаль * актёр (row-vector)
            const FMatrix OldLocal = OldSp->LocalXform.ToMatrixWithScale();
            const FMatrix WOld = OldLocal * Told;
            const FVector WPos = WOld.GetOrigin();

            // Дистанция по плоскости земли: курсор спроецирован на Z=0, поэтому 3D-метрика
            // ложно тянула бы к нижним точкам. По XY выбор края детали стабилен.
            const float D = FVector::Dist2D(WPos, Cursor);
            if (D >= BestDist) continue;

            // ответная точка на призраке: сначала по имени opposite, иначе первая валидная
            const FNMSSnapPoint* NewSp = nullptr;
            if (!OldSp->Opposite.IsEmpty() && NewNames.Contains(OldSp->Opposite))
                NewSp = GhostGroup->FindPoint(OldSp->Opposite);
            if (!NewSp)
            {
                for (const FString& NewName : NewNames)
                    if ((NewSp = GhostGroup->FindPoint(NewName)) != nullptr) break;
            }
            if (!NewSp) continue;

            // Хотим: NewLocal * Tghost = Flip * WOld
            //  => Tghost = NewLocal^-1 * Flip * WOld
            const FMatrix NewLocal = NewSp->LocalXform.ToMatrixWithScale();
            const FMatrix Tghost = NewLocal.Inverse() * Flip * WOld;

            BestDist  = D;
            BestGhost = Tghost;
            bFound    = true;
        }
    }

    if (bFound)
    {
        FTransform GT;
        GT.SetFromMatrix(BestGhost);
        GT.SetScale3D(Ghost->GetActorScale3D()); // сохранить масштаб детали
        Ghost->SetActorTransform(GT);
        return true;
    }
    return false;
}

// Рисует кадры точек стыковки (RGB-оси) у ближайших деталей и у призрака.
// Помогает на глаз проверить корректность матриц снапа.
void FNMSViewportClient::DrawSnapDebug()
{
    if (!bShowSnapPoints || !PreviewScene.IsValid()) return;
    UWorld* World = PreviewScene->GetWorld();
    if (!World) return;
    ULineBatchComponent* LB = World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent);
    if (!LB) return;

    FNMSSnapData& SD = FNMSSnapData::Get();
    SD.EnsureLoaded();

    const float Ax = 25.f; // длина оси, см
    auto DrawFrame = [&](const FMatrix& W)
    {
        const FVector O = W.GetOrigin();
        LB->DrawLine(O, O + W.GetScaledAxis(EAxis::X) * Ax, FLinearColor::Red,   SDPG_Foreground, 2.f, 0.f);
        LB->DrawLine(O, O + W.GetScaledAxis(EAxis::Y) * Ax, FLinearColor::Green, SDPG_Foreground, 2.f, 0.f);
        LB->DrawLine(O, O + W.GetScaledAxis(EAxis::Z) * Ax, FLinearColor::Blue,  SDPG_Foreground, 2.f, 0.f);
    };

    const FVector Around = SelectedActor.IsValid() ? SelectedActor->GetActorLocation() : CameraLocation;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* A = *It;
        if (!A || !A->GetActorNameOrLabel().StartsWith(TEXT("NMS_"))) continue;
        if (FVector::Dist(A->GetActorLocation(), Around) > 2000.f) continue;

        const FString Id = NMS_ActorPartId(A);
        const FString* GName = SD.GroupForPart(Id);
        if (!GName) continue;
        const FNMSSnapGroup* Grp = SD.GetGroup(*GName);
        if (!Grp) continue;

        const FMatrix T = A->GetActorTransform().ToMatrixWithScale();
        for (const FNMSSnapPoint& P : Grp->Points)
            DrawFrame(P.LocalXform.ToMatrixWithScale() * T);
    }
}

FVector FNMSViewportClient::GetGroundFocusPoint() const
{
    const FVector Dir = CameraRotation.Vector();
    // пересечение луча камеры с плоскостью Z=0
    if (FMath::Abs(Dir.Z) > 1e-3f)
    {
        const float t = -CameraLocation.Z / Dir.Z;
        if (t > 0.f)
            return CameraLocation + Dir * t;
    }
    // камера смотрит почти горизонтально — ставим на фикс. дистанции перед ней
    return FVector(CameraLocation.X + Dir.X * 1000.f,
                   CameraLocation.Y + Dir.Y * 1000.f, 0.f);
}

bool FNMSViewportClient::DeprojectMouseRay(FViewport* Viewport, FVector& O, FVector& D) const
{
    if (!Viewport || !bViewCached) return false;
    const FVector2D M((float)Viewport->GetMouseX(), (float)Viewport->GetMouseY());
    FSceneView::DeprojectScreenToWorld(M, CachedViewRect, CachedInvView, CachedInvProj, O, D);
    return true;
}

bool FNMSViewportClient::DeprojectMouseToGround(FViewport* Viewport, FVector& OutGround) const
{
    FVector O, D;
    if (!DeprojectMouseRay(Viewport, O, D)) return false;
    if (FMath::Abs(D.Z) < 1e-4f) return false;
    const float t = -O.Z / D.Z;            // пересечение с плоскостью Z=0
    if (t <= 0.f) return false;
    OutGround = O + D * t;
    return true;
}

bool FNMSViewportClient::WorldToPixel(const FVector& P, FVector2D& Out) const
{
    const FVector4 Clip = CachedViewProj.TransformFVector4(FVector4(P, 1.f));
    if (Clip.W <= 0.001f) return false;
    const float NdcX = Clip.X / Clip.W;
    const float NdcY = Clip.Y / Clip.W;
    Out.X = (NdcX * 0.5f + 0.5f) * (float)CachedViewRect.Width();
    Out.Y = (1.f - (NdcY * 0.5f + 0.5f)) * (float)CachedViewRect.Height();
    return true;
}

// ось гизмо как единичный вектор
static FVector NMS_AxisDir(int32 Axis)
{
    return FVector(Axis == 0 ? 1.f : 0.f, Axis == 1 ? 1.f : 0.f, Axis == 2 ? 1.f : 0.f);
}
// ближайший параметр t на прямой (P0 + t*A) к лучу (O + s*D)
static float NMS_ClosestAxisT(const FVector& P0, const FVector& A, const FVector& O, const FVector& D)
{
    const FVector w0 = P0 - O;
    const float a = FVector::DotProduct(A, A);
    const float b = FVector::DotProduct(A, D);
    const float c = FVector::DotProduct(D, D);
    const float d = FVector::DotProduct(A, w0);
    const float e = FVector::DotProduct(D, w0);
    const float denom = a * c - b * b;
    if (FMath::Abs(denom) < 1e-6f) return 0.f;
    return (b * e - c * d) / denom;
}

// плоскость кольца вращения вокруг оси Axis
static void NMS_RingBasis(int32 Axis, FVector& U, FVector& V)
{
    if (Axis == 0)      { U = FVector(0, 1, 0); V = FVector(0, 0, 1); } // вокруг X
    else if (Axis == 1) { U = FVector(1, 0, 0); V = FVector(0, 0, 1); } // вокруг Y
    else                { U = FVector(1, 0, 0); V = FVector(0, 1, 0); } // вокруг Z
}

void FNMSViewportClient::DrawGizmo()
{
    AActor* Sel = SelectedActor.Get();
    if (!Sel || !PreviewScene.IsValid()) return;
    if (TransformMode != ENMSTransformMode::Move && TransformMode != ENMSTransformMode::Rotate) return;
    UWorld* World = PreviewScene->GetWorld();
    if (!World) return;
    ULineBatchComponent* LB = World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent);
    if (!LB) return; // НЕ флашим — добавляем поверх грида (грид флашит в начале кадра)

    const FVector O = Sel->GetActorLocation();
    const float L = FMath::Max(FVector::Dist(CameraLocation, O) * 0.18f, 30.f);
    const FLinearColor Cols[3] = {
        FLinearColor(1.f, 0.12f, 0.12f, 1.f),   // X красный
        FLinearColor(0.12f, 1.f, 0.12f, 1.f),   // Y зелёный
        FLinearColor(0.2f, 0.45f, 1.f, 1.f) };  // Z синий

    if (TransformMode == ENMSTransformMode::Move)
    {
        for (int32 a = 0; a < 3; ++a)
        {
            const FLinearColor C = (GizmoAxis == a) ? FLinearColor(1.f, 1.f, 0.2f, 1.f) : Cols[a];
            const FVector D   = NMS_AxisDir(a);
            const FVector Tip = O + D * L;
            LB->DrawLine(O, Tip, C, SDPG_Foreground, 4.f, 0.f);   // стержень
            // наконечник-конус (как стрелка перемещения в Unity/Unreal)
            FVector U, V; NMS_RingBasis(a, U, V);
            const float HL = L * 0.24f;   // длина наконечника
            const float HR = L * 0.09f;   // радиус основания
            const FVector Base = Tip - D * HL;
            const int32 NS = 10;
            FVector Prev = Base + U * HR;
            for (int32 s = 1; s <= NS; ++s)
            {
                const float Tt = 2.f * PI * (float)s / (float)NS;
                const FVector P = Base + (U * FMath::Cos(Tt) + V * FMath::Sin(Tt)) * HR;
                LB->DrawLine(P, Tip,  C, SDPG_Foreground, 3.f, 0.f);   // боковина конуса
                LB->DrawLine(Prev, P, C, SDPG_Foreground, 2.5f, 0.f);  // основание
                Prev = P;
            }
        }
    }
    else // Rotate: три кольца по ЛОКАЛЬНЫМ осям детали (наклоняются вместе с ней)
    {
        const FQuat Q = Sel->GetActorQuat();
        const int32 N = 48;
        for (int32 a = 0; a < 3; ++a)
        {
            const FLinearColor C = (GizmoAxis == a) ? FLinearColor(1.f, 1.f, 0.2f, 1.f) : Cols[a];
            FVector U, V; NMS_RingBasis(a, U, V);
            U = Q.RotateVector(U); V = Q.RotateVector(V);
            FVector Prev = O + U * L;
            for (int32 s = 1; s <= N; ++s)
            {
                const float T = 2.f * PI * (float)s / (float)N;
                const FVector P = O + (U * FMath::Cos(T) + V * FMath::Sin(T)) * L;
                LB->DrawLine(Prev, P, C, SDPG_Foreground, 2.5f, 0.f);
                Prev = P;
            }
        }
    }
}

int32 FNMSViewportClient::PickGizmoAxis(FViewport* Viewport) const
{
    AActor* Sel = SelectedActor.Get();
    if (!Sel || !bViewCached || !Viewport) return -1;
    if (TransformMode != ENMSTransformMode::Move && TransformMode != ENMSTransformMode::Rotate) return -1;
    const FVector O = Sel->GetActorLocation();
    const float L = FMath::Max(FVector::Dist(CameraLocation, O) * 0.18f, 30.f);
    FVector2D OScreen;
    if (!WorldToPixel(O, OScreen)) return -1;
    const FVector2D Mouse((float)Viewport->GetMouseX(), (float)Viewport->GetMouseY());
    int32 Best = -1; float BestD = 14.f; // порог, пиксели

    if (TransformMode == ENMSTransformMode::Move)
    {
        for (int32 a = 0; a < 3; ++a)
        {
            FVector2D Tip;
            if (!WorldToPixel(O + NMS_AxisDir(a) * L, Tip)) continue;
            const float Dist = FMath::PointDistToSegment(
                FVector(Mouse, 0.f), FVector(OScreen, 0.f), FVector(Tip, 0.f));
            if (Dist < BestD) { BestD = Dist; Best = a; }
        }
    }
    else // Rotate: ближайшее кольцо (кольца по локальным осям детали)
    {
        const FQuat Q = Sel->GetActorQuat();
        const int32 N = 24;
        for (int32 a = 0; a < 3; ++a)
        {
            FVector U, V; NMS_RingBasis(a, U, V);
            U = Q.RotateVector(U); V = Q.RotateVector(V);
            FVector2D PrevPx;
            bool bPrev = WorldToPixel(O + U * L, PrevPx);
            for (int32 s = 1; s <= N; ++s)
            {
                const float T = 2.f * PI * (float)s / (float)N;
                FVector2D Px;
                const bool bOk = WorldToPixel(O + (U * FMath::Cos(T) + V * FMath::Sin(T)) * L, Px);
                if (bPrev && bOk)
                {
                    const float Dist = FMath::PointDistToSegment(
                        FVector(Mouse, 0.f), FVector(PrevPx, 0.f), FVector(Px, 0.f));
                    if (Dist < BestD) { BestD = Dist; Best = a; }
                }
                PrevPx = Px; bPrev = bOk;
            }
        }
    }
    return Best;
}

void FNMSViewportClient::GatherSelection(TArray<AActor*>& Out) const
{
    Out.Reset();
    for (const TWeakObjectPtr<AActor>& W : SelectedActors) if (W.IsValid()) Out.Add(W.Get());
    if (Out.Num() == 0 && SelectedActor.IsValid()) Out.Add(SelectedActor.Get());
}

void FNMSViewportClient::MoveSelectionTo(AActor* Lead, const FVector& NewLoc)
{
    if (!Lead) return;
    const FVector Delta = NewLoc - Lead->GetActorLocation();
    if (Delta.IsNearlyZero()) return;
    TArray<AActor*> Sel; GatherSelection(Sel);
    if (!Sel.Contains(Lead)) Sel.Add(Lead);
    for (AActor* A : Sel) if (A) A->AddActorWorldOffset(Delta);
}

void FNMSViewportClient::RotateSelection(const FQuat& Q, const FVector& Pivot)
{
    if (Q.IsIdentity()) return;
    TArray<AActor*> Sel; GatherSelection(Sel);
    for (AActor* A : Sel) if (A)
    {
        const FVector Rel = A->GetActorLocation() - Pivot;
        A->SetActorLocation(Pivot + Q.RotateVector(Rel));
        A->AddActorWorldRotation(Q);
    }
}

void FNMSViewportClient::TransformSelected(const FRotator& DRot, const FVector& DMove, float ScaleMul)
{
    TArray<AActor*> Sel; GatherSelection(Sel);
    if (Sel.Num() == 0) return;
    FVector Center = FVector::ZeroVector;
    for (AActor* A : Sel) Center += A->GetActorLocation();
    Center /= Sel.Num();
    for (AActor* A : Sel)
    {
        if (!DMove.IsNearlyZero()) A->AddActorWorldOffset(DMove);
        if (!DRot.IsNearlyZero())
        {
            const FQuat Q = DRot.Quaternion();
            const FVector Rel = A->GetActorLocation() - Center;
            A->SetActorLocation(Center + Q.RotateVector(Rel));
            A->AddActorWorldRotation(DRot);
        }
        if (!FMath::IsNearlyEqual(ScaleMul, 1.f))
            A->SetActorScale3D(A->GetActorScale3D() * ScaleMul);
    }
}

// ===========================================================================
//  Управление камерой (FreeCam)
// ===========================================================================
void FNMSViewportClient::ResetCamera()
{
    CameraLocation = FVector(-600.f, 0.f, 400.f);
    CameraRotation = FRotator(-25.f, 0.f, 0.f);
    FocusPoint = FVector::ZeroVector;
}

void FNMSViewportClient::UpdateCamera(float Dt)
{
    if (Dt <= 0.f) return;

    // направления из текущего поворота
    const FVector Fwd   = CameraRotation.Vector();
    const FVector Right = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Y);
    const FVector Up    = FVector::UpVector;

    FVector Move = FVector::ZeroVector;
    if (bFwd)   Move += Fwd;
    if (bBack)  Move -= Fwd;
    if (bRight) Move += Right;
    if (bLeft)  Move -= Right;
    if (bUp)    Move += Up;
    if (bDown)  Move -= Up;

    if (!Move.IsNearlyZero())
    {
        Move.Normalize();
        CameraLocation += Move * CameraSpeed * Dt;
    }
}

bool FNMSViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
    const FKey Key = EventArgs.Key;
    const bool bPressed = (EventArgs.Event == IE_Pressed);
    const bool bReleased = (EventArgs.Event == IE_Released);

    // Любой клик мышью по сцене -> забрать клавиатурный фокус на вьюпорт,
    // иначе WASD/QE-полёт не доходит до InputKey (как в Unreal: кликнул в
    // окно -> можно летать). Особенно важно для ПКМ-полёта.
    if (bPressed && (Key == EKeys::LeftMouseButton || Key == EKeys::RightMouseButton || Key == EKeys::MiddleMouseButton))
        if (OnRequestFocus) OnRequestFocus();

    // ПКМ -> режим полёта + захват мыши
    if (Key == EKeys::RightMouseButton)
    {
        if (bPressed)  { bRMBDown = true;  bMarquee = false;  if (EventArgs.Viewport) EventArgs.Viewport->LockMouseToViewport(true); }
        if (bReleased) { bRMBDown = false; if (EventArgs.Viewport) EventArgs.Viewport->LockMouseToViewport(false); }
        return true;
    }

    // Esc -> УНИВЕРСАЛЬНАЯ ОТМЕНА: одно нажатие отменяет ВСЁ текущее —
    // выходит из любого режима (проводка/кривая), убирает призрак установки,
    // снимает рамку выделения и тягу гизмо, сбрасывает выделение деталей.
    if (Key == EKeys::Escape && bPressed)
    {
        if (bWiringMode)   { bWiringMode = false; bHaveWireA = false; }
        if (bPlacingGhost) { CancelPlacing(); }                 // убрать голограмму
        if (bCurveMode)    { CurvePoints.Reset(); bCurveMode = false; if (OnCurveChanged) OnCurveChanged(); }
        bMarquee = false;                                       // отменить рамку выбора
        ClearSelection();                                       // снять выделение + подсветку, сбросить тягу/ось
        if (EventArgs.Viewport) EventArgs.Viewport->Invalidate();
        return true;
    }
    if (Key == EKeys::Enter && bPressed && bCurveMode)
    {
        if (OnCurveApply) OnCurveApply();
        return true;
    }

    // G -> вкл/выкл грид-сетку
    if (Key == EKeys::G && bPressed)
    {
        bShowGrid = !bShowGrid;
        if (EventArgs.Viewport) EventArgs.Viewport->Invalidate();
        return true;
    }

    // V -> вкл/выкл снап к деталям (как кнопка снапа в игре)
    if (Key == EKeys::V && bPressed)
    {
        bSnapToParts = !bSnapToParts;
        UE_LOG(LogTemp, Log, TEXT("NMS Snap: снап к деталям %s"), bSnapToParts ? TEXT("ВКЛ") : TEXT("ВЫКЛ"));
        if (EventArgs.Viewport) EventArgs.Viewport->Invalidate();
        return true;
    }
    // B -> показать/скрыть точки стыковки (отладка)
    if (Key == EKeys::B && bPressed)
    {
        bShowSnapPoints = !bShowSnapPoints;
        if (EventArgs.Viewport) EventArgs.Viewport->Invalidate();
        return true;
    }

    // Alt -> режим пана при зажатой СКМ
    if (Key == EKeys::LeftAlt || Key == EKeys::RightAlt)
    {
        if (bPressed)  bAltDown = true;
        if (bReleased) bAltDown = false;
        return true;
    }

    // Ctrl -> модификатор (Undo/Redo)
    if (Key == EKeys::LeftControl || Key == EKeys::RightControl)
    {
        if (bPressed)  bCtrlDown = true;
        if (bReleased) bCtrlDown = false;
        return true;
    }

    // СКМ -> орбита / пан вокруг точки фокуса
    if (Key == EKeys::MiddleMouseButton)
    {
        if (bPressed)  { bMMBDown = true;  if (EventArgs.Viewport) EventArgs.Viewport->LockMouseToViewport(true); }
        if (bReleased) { bMMBDown = false; if (EventArgs.Viewport) EventArgs.Viewport->LockMouseToViewport(false); }
        return true;
    }

    // Колесо -> зум (доли камеры)
    if (Key == EKeys::MouseScrollUp && bPressed)   { DollyZoom(+1.f); return true; }
    if (Key == EKeys::MouseScrollDown && bPressed) { DollyZoom(-1.f); return true; }

    // Переключение режима манипуляции (как кнопки тулбара): 1 двигать, 2 крутить, 3 масштаб
    if (bPressed && Key == EKeys::One)   { TransformMode = ENMSTransformMode::Move;   return true; }
    if (bPressed && Key == EKeys::Two)   { TransformMode = ENMSTransformMode::Rotate; return true; }
    if (bPressed && Key == EKeys::Three) { TransformMode = ENMSTransformMode::Scale;  return true; }
    if (bPressed && bCtrlDown && Key == EKeys::Z) { if (OnUndo) OnUndo(); return true; }
    if (bPressed && bCtrlDown && Key == EKeys::Y) { if (OnRedo) OnRedo(); return true; }
    if (bPressed && !bCtrlDown && Key == EKeys::M) { MirrorSelection(0); return true; }

    // ЛКМ -> сначала проверяем попадание в ось гизмо, иначе выбор детали
    if (Key == EKeys::LeftMouseButton && bPressed)
    {
        if (bWiringMode)
        {
            FVector Pt, RO, RD;
            bool bGot = false;
            if (bViewCached && DeprojectMouseRay(EventArgs.Viewport, RO, RD))
            {
                FHitResult Hit; FCollisionQueryParams QP(FName(TEXT("WirePick")), true);
                if (GetRenderWorld() && GetRenderWorld()->LineTraceSingleByChannel(Hit, RO, RO + RD * 1.0e6f, ECC_Visibility, QP))
                { Pt = Hit.Location; bGot = true; }
                else if (DeprojectMouseToGround(EventArgs.Viewport, Pt)) bGot = true;
            }
            if (bGot)
            {
                if (!bHaveWireA) { WireA = Pt; bHaveWireA = true; }
                else { SpawnCable(WireA, Pt); bHaveWireA = false; }
                if (EventArgs.Viewport) EventArgs.Viewport->Invalidate();
            }
            return true;
        }
        if (bCurveMode)
        {
            FVector G;
            if (DeprojectMouseToGround(EventArgs.Viewport, G)) { G.Z = 0.f; CurvePoints.Add(G); if (OnCurveChanged) OnCurveChanged(); if (EventArgs.Viewport) EventArgs.Viewport->Invalidate(); }
            return true;
        }
        // Постановка детали, следующей за курсором: ЛКМ фиксирует и ставит следующую.
        if (bPlacingGhost && SelectedActor.IsValid())
        {
            bPlacingGhost = false;            // материал уже настоящий (применён при спавне)
            if (OnEdited) OnEdited();
            if (bContinuousPlace && OnPlaceContinue) OnPlaceContinue(); // следующая деталь
            if (EventArgs.Viewport) EventArgs.Viewport->Invalidate();
            return true;
        }

        bDragging = false;
        GizmoAxis = -1;
        // если уже есть выбранная деталь и кликнули по её оси — тянем по оси
        if (SelectedActor.IsValid())
        {
            const int32 Axis = PickGizmoAxis(EventArgs.Viewport);
            if (Axis >= 0)
            {
                GizmoAxis = Axis;
                GizmoBaseLoc = SelectedActor->GetActorLocation();
                if (TransformMode == ENMSTransformMode::Rotate)
                {
                    // ось вращения = ЛОКАЛЬНАЯ ось детали, зафиксированная на момент захвата
                    GizmoAxisWorld = SelectedActor->GetActorQuat().RotateVector(NMS_AxisDir(Axis));
                    // стартовый угол курсора вокруг центра детали на экране
                    FVector2D CPx;
                    if (EventArgs.Viewport && WorldToPixel(GizmoBaseLoc, CPx))
                    {
                        const FVector2D M((float)EventArgs.Viewport->GetMouseX(),
                                          (float)EventArgs.Viewport->GetMouseY());
                        GizmoPrevAngle = FMath::Atan2(M.Y - CPx.Y, M.X - CPx.X);
                    }
                }
                else
                {
                    FVector RO, RD;
                    if (DeprojectMouseRay(EventArgs.Viewport, RO, RD))
                        GizmoStartT = NMS_ClosestAxisT(GizmoBaseLoc, NMS_AxisDir(Axis), RO, RD);
                }
                bDragging = true;
                if (EventArgs.Viewport) EventArgs.Viewport->Invalidate();
                return true;
            }
        }
        // иначе — выбор детали под курсором; по пустому месту — рамка выделения
        const bool bHitPart = PickPartUnderCursor(EventArgs.Viewport);
        if (bHitPart && SelectedActor.IsValid())
        {
            const TWeakObjectPtr<AActor> Hit = SelectedActor;
            if (bCtrlDown)
            {
                // Ctrl+клик — добавить/снять деталь в мультивыборе (без перетаскивания)
                const int32 Idx = SelectedActors.IndexOfByKey(Hit);
                if (Idx != INDEX_NONE) SelectedActors.RemoveAt(Idx);
                else SelectedActors.AddUnique(Hit);
                ApplyHighlight();
            }
            else
            {
                // обычный клик: если деталь не в группе — выбрать только её; иначе тянем всю группу
                if (!SelectedActors.Contains(Hit)) { SelectedActors.Reset(); SelectedActors.Add(Hit); }
                ApplyHighlight();
                FVector G;
                if (DeprojectMouseToGround(EventArgs.Viewport, G))
                {
                    const FVector L = SelectedActor->GetActorLocation();
                    DragOffset = FVector(L.X - G.X, L.Y - G.Y, 0.f);
                    bDragging = true;
                }
            }
        }
        else if (EventArgs.Viewport)
        {
            bMarquee = true;
            MarqStart = FVector2D((float)EventArgs.Viewport->GetMouseX(), (float)EventArgs.Viewport->GetMouseY());
            MarqCur = MarqStart;
            SelectedActors.Reset(); ApplyHighlight();
        }
        if (EventArgs.Viewport) EventArgs.Viewport->Invalidate();
        return true;
    }
    if (Key == EKeys::LeftMouseButton && bReleased)
    {
        if (bMarquee) { SelectMarquee(EventArgs.Viewport); bMarquee = false; if (EventArgs.Viewport) EventArgs.Viewport->Invalidate(); }
        const bool bWasEdit = (bDragging || GizmoAxis >= 0) && !bMarquee;
        bDragging = false; GizmoAxis = -1;
        if (bWasEdit && OnEdited) OnEdited();
        return true;
    }

    // Манипуляции выбранной деталью (горячие клавиши)
    if (bPressed && SelectedActor.IsValid())
    {
        const float R = RotStepDeg;
        const float Cell = FMath::Max(GridCellSize, 1.f);
        bool bHandled = true;
        if      (Key == EKeys::RightBracket) TransformSelected(FRotator(0, +R, 0), FVector::ZeroVector, 1.f); // поворот Z+
        else if (Key == EKeys::LeftBracket)  TransformSelected(FRotator(0, -R, 0), FVector::ZeroVector, 1.f); // поворот Z-
        else if (Key == EKeys::Apostrophe)   TransformSelected(FRotator(+R, 0, 0), FVector::ZeroVector, 1.f); // наклон (pitch)
        else if (Key == EKeys::Semicolon)    TransformSelected(FRotator(-R, 0, 0), FVector::ZeroVector, 1.f);
        else if (Key == EKeys::Period)       TransformSelected(FRotator(0, 0, +R), FVector::ZeroVector, 1.f); // крен (roll)
        else if (Key == EKeys::Comma)        TransformSelected(FRotator(0, 0, -R), FVector::ZeroVector, 1.f);
        else if (Key == EKeys::Equals)       TransformSelected(FRotator::ZeroRotator, FVector::ZeroVector, 1.f + ScaleStep); // масштаб+
        else if (Key == EKeys::Hyphen)       TransformSelected(FRotator::ZeroRotator, FVector::ZeroVector, 1.f - ScaleStep); // масштаб-
        else if (Key == EKeys::Right)        TransformSelected(FRotator::ZeroRotator, FVector(+Cell, 0, 0), 1.f);
        else if (Key == EKeys::Left)         TransformSelected(FRotator::ZeroRotator, FVector(-Cell, 0, 0), 1.f);
        else if (Key == EKeys::Up)           TransformSelected(FRotator::ZeroRotator, FVector(0, +Cell, 0), 1.f);
        else if (Key == EKeys::Down)         TransformSelected(FRotator::ZeroRotator, FVector(0, -Cell, 0), 1.f);
        else if (Key == EKeys::PageUp)       TransformSelected(FRotator::ZeroRotator, FVector(0, 0, +Cell), 1.f);
        else if (Key == EKeys::PageDown)     TransformSelected(FRotator::ZeroRotator, FVector(0, 0, -Cell), 1.f);
        else if (Key == EKeys::F)            { FocusPoint = SelectedActor->GetActorLocation(); }
        else if (Key == EKeys::Delete)       { TArray<AActor*> S; GatherSelection(S); for (AActor* A : S) if (A) A->Destroy(); SelectedActor = nullptr; SelectedActors.Reset(); HiSaved.Reset(); }
        else bHandled = false;
        if (bHandled) { if (OnEdited) OnEdited(); if (EventArgs.Viewport) EventArgs.Viewport->Invalidate(); return true; }
    }

    // WASD/QE работают только при зажатой ПКМ (как в UE)
    if (bPressed || bReleased)
    {
        const bool v = bPressed;
        if (Key == EKeys::W) { bFwd = v;  return true; }
        if (Key == EKeys::S) { bBack = v; return true; }
        if (Key == EKeys::A) { bLeft = v; return true; }
        if (Key == EKeys::D) { bRight = v; return true; }
        if (Key == EKeys::E) { bUp = v;   return true; }
        if (Key == EKeys::Q) { bDown = v; return true; }
    }
    return false;
}

bool FNMSViewportClient::InputAxis(const FInputKeyEventArgs& EventArgs)
{
    const FKey AxisKey = EventArgs.Key;
    const float AxisDelta = EventArgs.AmountDepressed; // величина движения оси

    // рамка выделения тянется ЛКМ (движение приходит как ось мыши)
    if (bMarquee && !bRMBDown && !bMMBDown && EventArgs.Viewport && (AxisKey == EKeys::MouseX || AxisKey == EKeys::MouseY))
    {
        MarqCur = FVector2D((float)EventArgs.Viewport->GetMouseX(), (float)EventArgs.Viewport->GetMouseY());
        EventArgs.Viewport->Invalidate();
        return true;
    }

    // СКМ -> орбита (или пан при Alt) вокруг точки фокуса
    if (bMMBDown)
    {
        if (AxisKey == EKeys::MouseX) { bAltDown ? PanCamera(AxisDelta, 0.f) : OrbitCamera(AxisDelta, 0.f); return true; }
        if (AxisKey == EKeys::MouseY) { bAltDown ? PanCamera(0.f, AxisDelta) : OrbitCamera(0.f, AxisDelta); return true; }
        return false;
    }

    // ЛКМ зажата на детали -> манипуляция по текущему режиму
    if (bDragging && SelectedActor.IsValid())
    {
        AActor* Sel = SelectedActor.Get();
        // тяга по конкретной оси гизмо (X/Y/Z)
        if (GizmoAxis >= 0)
        {
            // кольцо вращения: угол курсора вокруг центра детали на экране
            if (TransformMode == ENMSTransformMode::Rotate)
            {
                FVector2D CPx;
                if (EventArgs.Viewport && WorldToPixel(Sel->GetActorLocation(), CPx))
                {
                    const FVector2D M((float)EventArgs.Viewport->GetMouseX(),
                                      (float)EventArgs.Viewport->GetMouseY());
                    const float Ang = FMath::Atan2(M.Y - CPx.Y, M.X - CPx.X);
                    float Delta = Ang - GizmoPrevAngle;
                    while (Delta >  PI) Delta -= 2.f * PI;
                    while (Delta < -PI) Delta += 2.f * PI;
                    GizmoPrevAngle = Ang;
                    const FVector A = GizmoAxisWorld; // локальная ось, зафиксированная при захвате
                    // знак — чтобы деталь крутилась «за курсором» с любой стороны
                    const float Sign = (FVector::DotProduct(A,
                        (CameraLocation - Sel->GetActorLocation()).GetSafeNormal()) >= 0.f) ? 1.f : -1.f;
                    RotateSelection(FQuat(A, Delta * Sign), GizmoBaseLoc);
                }
                return true;
            }
            // ось перемещения
            FVector RO, RD;
            if (DeprojectMouseRay(EventArgs.Viewport, RO, RD))
            {
                const FVector A = NMS_AxisDir(GizmoAxis);
                const float t = NMS_ClosestAxisT(GizmoBaseLoc, A, RO, RD);
                FVector NewLoc = GizmoBaseLoc + A * (t - GizmoStartT);
                if (GizmoAxis == 0)      NewLoc.X = FMath::GridSnap(NewLoc.X, MoveSnap);
                else if (GizmoAxis == 1) NewLoc.Y = FMath::GridSnap(NewLoc.Y, MoveSnap);
                else                     NewLoc.Z = FMath::GridSnap(NewLoc.Z, MoveSnap);
                MoveSelectionTo(Sel, NewLoc);
            }
            return true;
        }
        if (TransformMode == ENMSTransformMode::Move)
        {
            FVector G;
            if (DeprojectMouseToGround(EventArgs.Viewport, G))
            {
                // снап к соседней детали работает и для уже поставленной (одиночный выбор)
                const bool bSingle = (SelectedActors.Num() <= 1);
                const FVector Target(G.X + DragOffset.X, G.Y + DragOffset.Y, 0.f);
                if (bSnapToParts && bSingle && TrySnapGhost(Target))
                {
                    // TrySnapGhost задал полный трансформ выбранной детали
                }
                else
                {
                    const float NX = FMath::GridSnap(G.X + DragOffset.X, MoveSnap);
                    const float NY = FMath::GridSnap(G.Y + DragOffset.Y, MoveSnap);
                    const FVector L = Sel->GetActorLocation();
                    MoveSelectionTo(Sel, FVector(NX, NY, L.Z));
                }
            }
            return true;
        }
        if (TransformMode == ENMSTransformMode::Rotate && AxisKey == EKeys::MouseX)
        {
            Sel->AddActorWorldRotation(FRotator(0.f, AxisDelta * RotDragSpeed, 0.f));
            return true;
        }
        if (TransformMode == ENMSTransformMode::Scale && AxisKey == EKeys::MouseY)
        {
            const float F = FMath::Clamp(1.f + AxisDelta * 0.01f, 0.5f, 2.f);
            Sel->SetActorScale3D(Sel->GetActorScale3D() * F);
            return true;
        }
    }

    if (!bRMBDown) return false; // вращаем только с зажатой ПКМ

    const FKey Key = EventArgs.Key;
    const float Delta = EventArgs.AmountDepressed; // величина движения оси

    if (Key == EKeys::MouseX)
    {
        CameraRotation.Yaw += Delta * MouseSensitivity;
        return true;
    }
    if (Key == EKeys::MouseY)
    {
        CameraRotation.Pitch = FMath::Clamp(
            CameraRotation.Pitch + Delta * MouseSensitivity, -89.f, 89.f); // мышь вверх -> взгляд вверх
        return true;
    }
    return false;
}


// Компас осей в левом-нижнем углу: X(красн)/Y(зел)/Z(син) поворачиваются вместе
// с камерой — видно, куда повёрнута сцена/деталь (аналог nav-гизмо Blender).
void FNMSViewportClient::DrawCompass(FCanvas* Canvas, const FIntPoint& Size)
{
    if (!Canvas) return;
    const FVector2D O(96.f, (float)Size.Y - 96.f);
    const float R = 52.f;
    const FRotationMatrix RM(CameraRotation);
    const FVector Fwd   = RM.GetUnitAxis(EAxis::X);
    const FVector Right = RM.GetUnitAxis(EAxis::Y);
    const FVector Up    = RM.GetUnitAxis(EAxis::Z);
    auto Proj = [&](const FVector& D){ return FVector2D(FVector::DotProduct(D, Right), -FVector::DotProduct(D, Up)); };
    auto Line = [&](FVector2D A, FVector2D B, FLinearColor C, float W){ FCanvasLineItem L(A, B); L.SetColor(C); L.LineThickness = W; Canvas->DrawItem(L); };

    struct FAx { FVector Dir; FLinearColor Col; const TCHAR* L; };
    FAx Axes[3] = {
        { FVector(1,0,0), FLinearColor(0.92f, 0.20f, 0.22f, 1.f), TEXT("X") },
        { FVector(0,1,0), FLinearColor(0.45f, 0.80f, 0.22f, 1.f), TEXT("Y") },
        { FVector(0,0,1), FLinearColor(0.22f, 0.52f, 0.96f, 1.f), TEXT("Z") },
    };
    // порядок по глубине: дальние оси рисуем первыми (ближние перекрывают)
    int32 Ord[3] = { 0, 1, 2 };
    auto Dep = [&](int32 i){ return FVector::DotProduct(Axes[i].Dir, Fwd); };
    for (int32 a = 0; a < 3; ++a) for (int32 b = a + 1; b < 3; ++b) if (Dep(Ord[a]) > Dep(Ord[b])) { int32 tmp = Ord[a]; Ord[a] = Ord[b]; Ord[b] = tmp; }

    UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
    for (int32 k = 0; k < 3; ++k)
    {
        const FAx& A = Axes[Ord[k]];
        const FVector2D dir = Proj(A.Dir);
        const FVector2D LineEnd = O + dir * (R - 20.f);   // линия НЕ доходит до буквы
        const FVector2D Lab     = O + dir * (R + 6.f);    // буква дальше — с зазором
        Line(O, LineEnd, A.Col, 2.5f);
        if (Font)
        {
            FCanvasTextItem T(Lab - FVector2D(8.f, 14.f), FText::FromString(A.L), Font, A.Col);
            T.Scale = FVector2D(2.f, 2.f);
            Canvas->DrawItem(T);
        }
    }
}

// Превью кривой: жёлтые узлы + голубой Catmull-сплайн сквозь них (на гриде Z=0).
void FNMSViewportClient::DrawCurvePreview()
{
    if (!bCurveMode || CurvePoints.Num() == 0 || !PreviewScene.IsValid()) return;
    UWorld* World = PreviewScene->GetWorld(); if (!World) return;
    ULineBatchComponent* LB = World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent);
    if (!LB) return;
    const FLinearColor PtCol(1.f,0.85f,0.1f,1.f), LnCol(0.2f,0.9f,1.f,1.f);
    auto L2 = [&](const FVector& A, const FVector& B, const FLinearColor& C, float W){ LB->DrawLine(A, B, C, SDPG_Foreground, W, 0.f); };
    for (const FVector& P : CurvePoints)
    { L2(P-FVector(22,0,0),P+FVector(22,0,0),PtCol,3.f); L2(P-FVector(0,22,0),P+FVector(0,22,0),PtCol,3.f); L2(P,P+FVector(0,0,70),PtCol,2.f); }

    // плотная линия по типу кривой
    TArray<FVector> D;
    auto CM = [](float a1,float a2,float a3,float a4,float t){ return (a1*((-t+2)*t-1)*t + a2*(((3*t-5)*t)*t+2) + a3*((-3*t+4)*t+1)*t + a4*((t-1)*t*t))*0.5f; };
    if (CurveType == ENMSCurveType::Polyline) { D = CurvePoints; }
    else if (CurveType == ENMSCurveType::Rect)
    {
        if (CurvePoints.Num() >= 2){ const FVector A=CurvePoints[0],C=CurvePoints[1]; D = { A, FVector(C.X,A.Y,0.f), C, FVector(A.X,C.Y,0.f), A }; }
    }
    else if (CurveType == ENMSCurveType::Circle)
    {
        if (CurvePoints.Num() >= 2){ const FVector C=CurvePoints[0]; const float R=(CurveRadius>1.f)?CurveRadius:FVector::Dist2D(C,CurvePoints[1]); const int32 N=64; for(int32 i=0;i<=N;++i){const float a=2.f*PI*i/N; D.Add(C+FVector(R*FMath::Cos(a),R*FMath::Sin(a),0.f));} }
    }
    else
    {
        if (CurvePoints.Num() >= 2){ TArray<FVector> P; P.Reserve(CurvePoints.Num()+2); P.Add(CurvePoints[0]); P.Append(CurvePoints); P.Add(CurvePoints.Last()); const int32 Seg=18; for(int32 i=1;i+2<P.Num();++i)for(int32 s=0;s<Seg;++s){const float t=(float)s/Seg; D.Add(FVector(CM(P[i-1].X,P[i].X,P[i+1].X,P[i+2].X,t),CM(P[i-1].Y,P[i].Y,P[i+1].Y,P[i+2].Y,t),0.f));} D.Add(CurvePoints.Last()); }
    }
    if (D.Num() < 2) return;
    for (int32 i=0;i+1<D.Num();++i) L2(D[i],D[i+1],LnCol,2.5f);
}


// ===========================================================================
//  Зеркалирование выделения по оси (0=X, 1=Y), через центр выделения.
// ===========================================================================
void FNMSViewportClient::MirrorSelection(int32 Axis)
{
    TArray<AActor*> S; GatherSelection(S);
    if (S.Num() == 0) return;
    FVector Pivot = FVector::ZeroVector;
    for (AActor* A : S) Pivot += A->GetActorLocation();
    Pivot /= (float)S.Num();
    for (AActor* A : S)
    {
        if (!A) continue;
        FVector L = A->GetActorLocation();
        FRotator R = A->GetActorRotation();
        if (Axis == 0) { L.X = 2.f * Pivot.X - L.X; R.Yaw = 180.f - R.Yaw; R.Roll = -R.Roll; }
        else           { L.Y = 2.f * Pivot.Y - L.Y; R.Yaw = -R.Yaw;        R.Roll = -R.Roll; }
        A->SetActorLocation(L);
        A->SetActorRotation(R);
    }
    if (OnEdited) OnEdited();
}


// ===========================================================================
//  Кабель/труба между двумя точками (наш аналог BP_Cable/BP_PipeVisual)
// ===========================================================================
void FNMSViewportClient::SpawnCable(const FVector& A, const FVector& B)
{
    UWorld* W = GetRenderWorld();
    if (!W) return;
    UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!Cyl) return;
    FVector Dir = B - A; const float Len = Dir.Size();
    if (Len < 1.f) return; Dir /= Len;
    FActorSpawnParameters Pm; Pm.ObjectFlags = RF_Transient;
    AStaticMeshActor* Act = W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform((A + B) * 0.5f), Pm);
    if (!Act) return;
#if WITH_EDITOR
    Act->SetActorLabel(TEXT("NMS_CABLE"));
#endif
    Act->Tags.Add(FName(TEXT("NMS_CABLE")));
    if (UStaticMeshComponent* C = Act->GetStaticMeshComponent())
    {
        C->SetMobility(EComponentMobility::Movable);
        C->SetStaticMesh(Cyl);
        Act->SetActorRotation(FRotationMatrix::MakeFromZ(Dir).Rotator());
        const float Rad = 3.f; // толщина кабеля, см
        Act->SetActorScale3D(FVector(Rad / 50.f, Rad / 50.f, Len / 100.f));
        C->SetCastShadow(false);
        C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        C->SetCollisionResponseToAllChannels(ECR_Ignore);
        C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NMSBaseBuilder/Materials/M_BasePart.M_BasePart")))
        {
            UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, C);
            MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.04f, 0.04f, 0.05f));
            const int32 N = C->GetNumMaterials();
            for (int32 i = 0; i < N; ++i) C->SetMaterial(i, MID);
        }
    }
}
