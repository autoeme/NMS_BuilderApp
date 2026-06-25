// Часть класса SNMSBuilderUI (разнесение по TU, Часть C плана).
// Включения общие для всех TU класса — см. SNMSBuilderUI_Internal.h.
#include "SNMSBuilderUI_Internal.h"

#define LOCTEXT_NAMESPACE "NMSBuilder"

void SNMSBuilderUI::UpdateCurvePreview()
{
    if (!ViewportClient.IsValid()) return;
    UWorld* World = ViewportClient->GetWorld(); if (!World) return;
    if (!CurvePreviewISM)
    {
        AActor* Host = World->SpawnActor<AActor>(); if (!Host) return;
        Host->SetFlags(RF_Transient);
        USceneComponent* Root = NewObject<USceneComponent>(Host);
        Host->SetRootComponent(Root); Root->RegisterComponent();
        CurvePreviewISM = NewObject<UInstancedStaticMeshComponent>(Host);
        CurvePreviewISM->SetupAttachment(Root);
        CurvePreviewISM->RegisterComponent();
        CurvePreviewISM->SetCastShadow(false);
        CurvePreviewISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    CurvePreviewISM->ClearInstances();
    if (!ViewportClient->IsCurveMode() || !CurveBuildPart.IsValid()) return;
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *CurveBuildPart->ModelPath); if (!Mesh) return;
    CurvePreviewISM->SetStaticMesh(Mesh);

    FString TexP,MaskP,NormP,MasksP,OccP;
    NMS_PartTexture(CurveBuildPart->Category, CurveBuildPart->ObjectID, TexP,MaskP,NormP,MasksP,OccP);
    UMaterialInterface* LitM  = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NMSBaseBuilder/Materials/M_PartLit.M_PartLit"));
    UMaterialInterface* BaseM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NMSBaseBuilder/Materials/M_BasePart.M_BasePart"));
    UTexture2D* BodyTex = TexP.IsEmpty()  ? nullptr : LoadObject<UTexture2D>(nullptr, *TexP);
    UTexture2D* MaskTex = MaskP.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *MaskP);
    UTexture2D* NormTex = NormP.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *NormP);
    UTexture2D* MaskaTex= MasksP.IsEmpty()? nullptr : LoadObject<UTexture2D>(nullptr, *MasksP);
    const FLinearColor Col1 = NMS_DefaultPartColor(CurveBuildPart->Category, CurveBuildPart->ObjectID);
    const FLinearColor Col2 = NMS_DefaultPartColor2(CurveBuildPart->Category, CurveBuildPart->ObjectID);
    UMaterialInterface* M = (BodyTex && LitM) ? LitM : BaseM;
    if (M)
    {
        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, CurvePreviewISM);
        if (BodyTex && LitM)
        {
            MID->SetTextureParameterValue(TEXT("BaseTex"), BodyTex);
            if (MaskTex)  MID->SetTextureParameterValue(TEXT("PaintMask"), MaskTex);
            if (NormTex)  MID->SetTextureParameterValue(TEXT("NormalTex"), NormTex);
            if (MaskaTex) { MID->SetTextureParameterValue(TEXT("MasksTex"), MaskaTex); MID->SetScalarParameterValue(TEXT("UseMasks"), 1.f); }
            MID->SetVectorParameterValue(TEXT("Color"), Col1);
            MID->SetVectorParameterValue(TEXT("Color2"), Col2);
        }
        else MID->SetVectorParameterValue(TEXT("Color"), Col1);
        MID->SetVectorParameterValue(TEXT("ZoneCol0"), Col1);
        MID->SetVectorParameterValue(TEXT("ZoneCol1"), Col1);
        MID->SetVectorParameterValue(TEXT("ZoneCol2"), Col1);
        MID->SetVectorParameterValue(TEXT("ZoneCol3"), Col1);
        const int32 NM = CurvePreviewISM->GetNumMaterials();
        for (int32 i=0;i<NM;++i) CurvePreviewISM->SetMaterial(i, MID);
    }

    TArray<FVector> Curve = TessellateCurve(); if (Curve.Num() < 2) return;
    const FVector Sz = Mesh->GetBoundingBox().GetSize();
    float PartLen = FMath::Max(FMath::Max(Sz.X, Sz.Y), 1.f);
    const float Step = FMath::Max(PartLen * (1.f - ViewportClient->GetCurveOverlap()), 1.f);
    TArray<float> Seg; float Total=0.f;
    for (int32 i=0;i+1<Curve.Num();++i){ const float dd=FVector::Dist2D(Curve[i],Curve[i+1]); Seg.Add(dd); Total+=dd; }
    const int32 NSteps = FMath::Min(FMath::Max(1, FMath::RoundToInt(Total / Step)), 5000);
    const float AStep = Total / NSteps;
    for (int32 ki=0; ki<=NSteps; ++ki)
    {
        const float d = ki * AStep;
        float acc=0.f; int32 si=0; while(si+1<Seg.Num() && acc+Seg[si] < d){ acc+=Seg[si]; ++si; }
        const float f = (Seg.IsValidIndex(si) && Seg[si]>1e-3f) ? (d-acc)/Seg[si] : 0.f;
        const int32 ni = FMath::Min(si+1, Curve.Num()-1);
        const FVector Pos = FMath::Lerp(Curve[si], Curve[ni], f);
        FVector T = Curve[ni]-Curve[si]; T.Z=0.f; if (T.IsNearlyZero()) T=FVector(1,0,0); T.Normalize();
        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X));
        CurvePreviewISM->AddInstance(FTransform(FRotator(ViewportClient->GetCurveTilt(), Yaw, ViewportClient->GetCurveRoll()), FVector(Pos.X, Pos.Y, 0.f)), true);
    }
}

// Построить плотную линию пути по типу кривой (Catmull/Ломаная/Круг/Прямоугольник).
TArray<FVector> SNMSBuilderUI::TessellateCurve()
{
    TArray<FVector> Curve;
    if (!ViewportClient.IsValid()) return Curve;
    const TArray<FVector> Pts = ViewportClient->GetCurvePoints();
    if (Pts.Num() < 2) return Curve;
    const ENMSCurveType Type = ViewportClient->GetCurveType();
    auto CM = [](float a1,float a2,float a3,float a4,float t){ return (a1*((-t+2)*t-1)*t + a2*(((3*t-5)*t)*t+2) + a3*((-3*t+4)*t+1)*t + a4*((t-1)*t*t))*0.5f; };
    if (Type == ENMSCurveType::Polyline) { Curve = Pts; }
    else if (Type == ENMSCurveType::Rect)
    {
        const FVector A = Pts[0], C = Pts[1];
        Curve = { A, FVector(C.X,A.Y,0.f), C, FVector(A.X,C.Y,0.f), A };
    }
    else if (Type == ENMSCurveType::Circle)
    {
        const FVector C = Pts[0]; const float R = (ViewportClient->GetCurveRadius() > 1.f) ? ViewportClient->GetCurveRadius() : FVector::Dist2D(C, Pts[1]); const int32 N = 64;
        for (int32 i=0;i<=N;++i){ const float a=2.f*PI*i/N; Curve.Add(C+FVector(R*FMath::Cos(a),R*FMath::Sin(a),0.f)); }
    }
    else
    {
        TArray<FVector> P; P.Reserve(Pts.Num()+2); P.Add(Pts[0]); P.Append(Pts); P.Add(Pts.Last());
        const int32 Seg=20;
        for (int32 i=1;i+2<P.Num();++i) for (int32 s=0;s<Seg;++s){ const float t=(float)s/Seg; Curve.Add(FVector(CM(P[i-1].X,P[i].X,P[i+1].X,P[i+2].X,t),CM(P[i-1].Y,P[i].Y,P[i+1].Y,P[i+2].Y,t),0.f)); }
        Curve.Add(Pts.Last());
    }
    return Curve;
}

// Сколько деталей встанет при текущем нахлёсте (для счётчика).
int32 SNMSBuilderUI::CurvePartCount()
{
    if (!ViewportClient.IsValid() || !CurveBuildPart.IsValid()) return 0;
    TArray<FVector> Curve = TessellateCurve();
    if (Curve.Num() < 2) return 0;
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *CurveBuildPart->ModelPath);
    if (!Mesh) return 0;
    const FVector Sz = Mesh->GetBoundingBox().GetSize();
    float PartLen = FMath::Max(Sz.X, Sz.Y); if (PartLen < 1.f) PartLen = 100.f;
    const float Step = FMath::Max(PartLen * (1.f - ViewportClient->GetCurveOverlap()), 1.f);
    float Total = 0.f;
    for (int32 i=0;i+1<Curve.Num();++i) Total += FVector::Dist2D(Curve[i], Curve[i+1]);
    return FMath::Max(1, FMath::RoundToInt(Total / Step)) + 1;
}

// Разложить выбранную деталь вдоль нарисованной кривой (встык/с нахлёстом, поворот по касательной).
void SNMSBuilderUI::BuildAlongCurve()
{
    if (!ViewportClient.IsValid() || !CurveBuildPart.IsValid()) return;
    UWorld* World = ViewportClient->GetWorld(); if (!World) return;
    TArray<FVector> Curve = TessellateCurve();
    if (Curve.Num() < 2) return;

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *CurveBuildPart->ModelPath);
    if (!Mesh) return;
    const FVector Sz = Mesh->GetBoundingBox().GetSize();
    float PartLen = FMath::Max(Sz.X, Sz.Y); if (PartLen < 1.f) PartLen = 100.f;
    const float Step = FMath::Max(PartLen * (1.f - ViewportClient->GetCurveOverlap()), 1.f);

    FString TexP, MaskP, NormP, MasksP, OccP;
    NMS_PartTexture(CurveBuildPart->Category, CurveBuildPart->ObjectID, TexP, MaskP, NormP, MasksP, OccP);
    UMaterialInterface* LitM  = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NMSBaseBuilder/Materials/M_PartLit.M_PartLit"));
    UMaterialInterface* BaseM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NMSBaseBuilder/Materials/M_BasePart.M_BasePart"));
    UTexture2D* BodyTex = TexP.IsEmpty()  ? nullptr : LoadObject<UTexture2D>(nullptr, *TexP);
    UTexture2D* MaskTex = MaskP.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *MaskP);
    UTexture2D* NormTex = NormP.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *NormP);
    UTexture2D* MaskaTex= MasksP.IsEmpty()? nullptr : LoadObject<UTexture2D>(nullptr, *MasksP);
    const FLinearColor Col1 = NMS_DefaultPartColor(CurveBuildPart->Category, CurveBuildPart->ObjectID);
    const FLinearColor Col2 = NMS_DefaultPartColor2(CurveBuildPart->Category, CurveBuildPart->ObjectID);

    TArray<float> Seg; float Total=0.f;
    for (int32 i=0;i+1<Curve.Num();++i){ const float dd=FVector::Dist2D(Curve[i],Curve[i+1]); Seg.Add(dd); Total+=dd; }
    int32 Count=0;
    const int32 NSteps = FMath::Max(1, FMath::RoundToInt(Total / Step));
    const float AStep = Total / NSteps;
    for (int32 ki=0; ki<=NSteps; ++ki)
    {
        const float d = ki * AStep;
        float acc=0.f; int32 si=0;
        while (si+1<Seg.Num() && acc+Seg[si] < d) { acc+=Seg[si]; ++si; }
        const float f = (Seg.IsValidIndex(si) && Seg[si]>1e-3f) ? (d-acc)/Seg[si] : 0.f;
        const int32 ni = FMath::Min(si+1, Curve.Num()-1);
        const FVector Pos = FMath::Lerp(Curve[si], Curve[ni], f);
        FVector Tang = Curve[ni] - Curve[si]; Tang.Z = 0.f;
        if (Tang.IsNearlyZero()) Tang = FVector(1,0,0); Tang.Normalize();
        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Tang.Y, Tang.X));

        FActorSpawnParameters SP; SP.ObjectFlags = RF_Transient;
        AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
            FTransform(FRotator(ViewportClient->GetCurveTilt(), Yaw, ViewportClient->GetCurveRoll()), FVector(Pos.X, Pos.Y, 0.f)), SP);
        if (A)
        {
#if WITH_EDITOR
            A->SetActorLabel(FString::Printf(TEXT("NMS_%s"), *CurveBuildPart->ObjectID));
#endif
            A->Tags.Add(FName(*CurveBuildPart->ObjectID));
            if (UStaticMeshComponent* Comp = A->GetStaticMeshComponent())
            {
                Comp->SetMobility(EComponentMobility::Movable);
                Comp->SetStaticMesh(Mesh);
                Comp->SetCastShadow(false);
                Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                Comp->SetCollisionObjectType(ECC_WorldStatic);
                Comp->SetCollisionResponseToAllChannels(ECR_Ignore);
                Comp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
                UMaterialInterface* M = (BodyTex && LitM) ? LitM : BaseM;
                if (M)
                {
                    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, Comp);
                    if (BodyTex && LitM)
                    {
                        MID->SetTextureParameterValue(TEXT("BaseTex"), BodyTex);
                        if (MaskTex)  MID->SetTextureParameterValue(TEXT("PaintMask"), MaskTex);
                        if (NormTex)  MID->SetTextureParameterValue(TEXT("NormalTex"), NormTex);
                        if (MaskaTex) { MID->SetTextureParameterValue(TEXT("MasksTex"), MaskaTex); MID->SetScalarParameterValue(TEXT("UseMasks"), 1.f); }
                        MID->SetVectorParameterValue(TEXT("Color"), Col1);
                        MID->SetVectorParameterValue(TEXT("Color2"), Col2);
                    }
                    else MID->SetVectorParameterValue(TEXT("Color"), Col1);
                    MID->SetVectorParameterValue(TEXT("ZoneCol0"), Col1);
                    MID->SetVectorParameterValue(TEXT("ZoneCol1"), Col1);
                    MID->SetVectorParameterValue(TEXT("ZoneCol2"), Col1);
                    MID->SetVectorParameterValue(TEXT("ZoneCol3"), Col1);
                    const int32 NM = Comp->GetNumMaterials();
                    for (int32 i=0;i<NM;++i) Comp->SetMaterial(i, MID);
                }
            }
            ++Count;
        }
    }
    UE_LOG(LogTemp, Log, TEXT("NMS: curve built %d parts (%s)"), Count, *CurveBuildPart->ObjectID);
    ViewportClient->ExitCurve();
    UpdateCurvePreview();
}

// ===========================================================================
//  Загрузка данных (без изменений)
// ===========================================================================
#undef LOCTEXT_NAMESPACE
