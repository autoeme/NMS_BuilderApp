#include "NMSModelImporter.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "Materials/MaterialInterface.h"

// ============================ Загрузка с диска ============================
bool FNMSModelImporter::LoadFromFile(const FString& Path, FNMSImportedMesh& Out, FString& OutError)
{
    const FString Ext = FPaths::GetExtension(Path).ToLower();
    if (Ext == TEXT("obj"))
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = TEXT("не удалось прочитать OBJ-файл"); return false; }
        return ParseOBJ(Text, Out, OutError);
    }
    if (Ext == TEXT("stl"))
    {
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *Path)) { OutError = TEXT("не удалось прочитать STL-файл"); return false; }
        return ParseSTL(Bytes, Out, OutError);
    }
    OutError = FString::Printf(TEXT("неподдерживаемый формат: .%s (нужен .obj или .stl)"), *Ext);
    return false;
}

// ============================ OBJ ============================
// Поддержка: v, vt, vn, f (v / v/vt / v/vt/vn / v//vn), отрицательные индексы,
// полигоны >3 вершин (веер-триангуляция). Группы/материалы .mtl игнорируются
// (геометрия в один слот).
bool FNMSModelImporter::ParseOBJ(const FString& Text, FNMSImportedMesh& Out, FString& OutError)
{
    TArray<FVector3f> P, N;
    TArray<FVector2f> T;

    TArray<FString> Lines;
    Text.ParseIntoArrayLines(Lines, /*bCullEmpty=*/false);

    TMap<FString, uint32> Unique; // "vi/ti/ni" -> выходной индекс
    Unique.Reserve(Lines.Num());

    auto ResolveIdx = [](int32 Raw, int32 Count) -> int32
    {
        // OBJ: 1-based; отрицательное = с конца
        if (Raw > 0) return Raw - 1;
        if (Raw < 0) return Count + Raw;
        return -1;
    };

    for (const FString& RawLine : Lines)
    {
        FString Line = RawLine.TrimStartAndEnd();
        if (Line.IsEmpty() || Line[0] == '#') continue;

        TArray<FString> Tok;
        Line.ParseIntoArray(Tok, TEXT(" "), /*bCullEmpty=*/true);
        if (Tok.Num() == 0) continue;
        const FString& Key = Tok[0];

        if (Key == TEXT("v") && Tok.Num() >= 4)
        {
            P.Add(FVector3f(FCString::Atof(*Tok[1]), FCString::Atof(*Tok[2]), FCString::Atof(*Tok[3])));
        }
        else if (Key == TEXT("vt") && Tok.Num() >= 3)
        {
            T.Add(FVector2f(FCString::Atof(*Tok[1]), 1.f - FCString::Atof(*Tok[2]))); // V инвертируем под UE
        }
        else if (Key == TEXT("vn") && Tok.Num() >= 4)
        {
            N.Add(FVector3f(FCString::Atof(*Tok[1]), FCString::Atof(*Tok[2]), FCString::Atof(*Tok[3])));
        }
        else if (Key == TEXT("f") && Tok.Num() >= 4)
        {
            // собираем индексы углов грани
            TArray<uint32> Face;
            for (int32 c = 1; c < Tok.Num(); ++c)
            {
                TArray<FString> Parts;
                Tok[c].ParseIntoArray(Parts, TEXT("/"), /*bCullEmpty=*/false);
                if (Parts.Num() == 0 || Parts[0].IsEmpty()) continue;

                const int32 vi = ResolveIdx(FCString::Atoi(*Parts[0]), P.Num());
                const int32 ti = (Parts.Num() >= 2 && !Parts[1].IsEmpty()) ? ResolveIdx(FCString::Atoi(*Parts[1]), T.Num()) : -1;
                const int32 ni = (Parts.Num() >= 3 && !Parts[2].IsEmpty()) ? ResolveIdx(FCString::Atoi(*Parts[2]), N.Num()) : -1;
                if (vi < 0 || vi >= P.Num()) continue;

                const FString UKey = FString::Printf(TEXT("%d/%d/%d"), vi, ti, ni);
                uint32 OutIdx;
                if (uint32* Found = Unique.Find(UKey)) { OutIdx = *Found; }
                else
                {
                    OutIdx = (uint32)Out.Positions.Num();
                    Out.Positions.Add(P[vi]);
                    Out.Normals.Add((ni >= 0 && ni < N.Num()) ? N[ni] : FVector3f::ZeroVector);
                    Out.UVs.Add((ti >= 0 && ti < T.Num()) ? T[ti] : FVector2f::ZeroVector);
                    Unique.Add(UKey, OutIdx);
                }
                Face.Add(OutIdx);
            }
            // веер-триангуляция многоугольника
            for (int32 k = 1; k + 1 < Face.Num(); ++k)
            {
                Out.Indices.Add(Face[0]);
                Out.Indices.Add(Face[k]);
                Out.Indices.Add(Face[k + 1]);
            }
        }
    }

    if (!Out.IsValid()) { OutError = TEXT("в OBJ нет валидной геометрии"); return false; }
    return true;
}

// ============================ STL ============================
bool FNMSModelImporter::ParseSTL(const TArray<uint8>& Bytes, FNMSImportedMesh& Out, FString& OutError)
{
    // Определяем бинарный/ASCII: у бинарного размер == 84 + 50*N (N на смещении 80).
    bool bBinary = false;
    if (Bytes.Num() >= 84)
    {
        const uint32 Tri = *reinterpret_cast<const uint32*>(Bytes.GetData() + 80);
        if ((int64)Bytes.Num() == 84 + 50LL * (int64)Tri && Tri > 0) bBinary = true;
    }

    if (bBinary)
    {
        const uint8* D = Bytes.GetData();
        const uint32 Tri = *reinterpret_cast<const uint32*>(D + 80);
        Out.Positions.Reserve(Tri * 3);
        Out.Normals.Reserve(Tri * 3);
        Out.Indices.Reserve(Tri * 3);
        const uint8* Ptr = D + 84;
        auto F = [&](const uint8* p) { return *reinterpret_cast<const float*>(p); };
        for (uint32 i = 0; i < Tri; ++i, Ptr += 50)
        {
            const FVector3f Nrm(F(Ptr), F(Ptr + 4), F(Ptr + 8));
            for (int32 v = 0; v < 3; ++v)
            {
                const uint8* vp = Ptr + 12 + v * 12;
                const uint32 idx = (uint32)Out.Positions.Num();
                Out.Positions.Add(FVector3f(F(vp), F(vp + 4), F(vp + 8)));
                Out.Normals.Add(Nrm);
                Out.UVs.Add(FVector2f::ZeroVector);
                Out.Indices.Add(idx);
            }
        }
    }
    else
    {
        // ASCII STL
        FString Text;
        FFileHelper::BufferToString(Text, Bytes.GetData(), Bytes.Num());
        TArray<FString> Lines;
        Text.ParseIntoArrayLines(Lines, /*bCullEmpty=*/true);
        FVector3f CurN = FVector3f::ZeroVector;
        for (const FString& RawLine : Lines)
        {
            FString L = RawLine.TrimStartAndEnd();
            if (L.StartsWith(TEXT("facet normal")))
            {
                TArray<FString> Tok; L.ParseIntoArray(Tok, TEXT(" "), true);
                if (Tok.Num() >= 5) CurN = FVector3f(FCString::Atof(*Tok[2]), FCString::Atof(*Tok[3]), FCString::Atof(*Tok[4]));
            }
            else if (L.StartsWith(TEXT("vertex")))
            {
                TArray<FString> Tok; L.ParseIntoArray(Tok, TEXT(" "), true);
                if (Tok.Num() >= 4)
                {
                    const uint32 idx = (uint32)Out.Positions.Num();
                    Out.Positions.Add(FVector3f(FCString::Atof(*Tok[1]), FCString::Atof(*Tok[2]), FCString::Atof(*Tok[3])));
                    Out.Normals.Add(CurN);
                    Out.UVs.Add(FVector2f::ZeroVector);
                    Out.Indices.Add(idx);
                }
            }
        }
    }

    if (!Out.IsValid()) { OutError = TEXT("в STL нет валидной геометрии"); return false; }
    return true;
}

// ============================ Сборка UStaticMesh ============================
UStaticMesh* FNMSModelImporter::BuildStaticMesh(const FNMSImportedMesh& In, UMaterialInterface* Material, float AutoFitTargetSize)
{
    if (!In.IsValid()) return nullptr;

    // Рабочая копия позиций (масштаб/центрирование)
    TArray<FVector3f> Pos = In.Positions;

    // Авто-подгон размера и установка на землю в начале координат
    if (AutoFitTargetSize > 0.f)
    {
        FVector3f Min(FLT_MAX), Max(-FLT_MAX);
        for (const FVector3f& V : Pos) { Min = Min.ComponentMin(V); Max = Max.ComponentMax(V); }
        const FVector3f Size = Max - Min;
        const float MaxDim = FMath::Max3(Size.X, Size.Y, Size.Z);
        const float Scale = (MaxDim > KINDA_SMALL_NUMBER) ? (AutoFitTargetSize / MaxDim) : 1.f;
        const FVector3f CenterXY((Min.X + Max.X) * 0.5f, (Min.Y + Max.Y) * 0.5f, Min.Z);
        for (FVector3f& V : Pos) V = (V - CenterXY) * Scale;
    }

    // Нормали: если нет — считаем по граням
    TArray<FVector3f> Nrm = In.Normals;
    bool bNeedNormals = (Nrm.Num() != Pos.Num());
    if (!bNeedNormals)
        for (const FVector3f& N : Nrm) if (N.IsNearlyZero()) { bNeedNormals = true; break; }
    if (bNeedNormals)
    {
        Nrm.Init(FVector3f::ZeroVector, Pos.Num());
        for (int32 i = 0; i + 2 < In.Indices.Num(); i += 3)
        {
            const uint32 a = In.Indices[i], b = In.Indices[i + 1], c = In.Indices[i + 2];
            if (a >= (uint32)Pos.Num() || b >= (uint32)Pos.Num() || c >= (uint32)Pos.Num()) continue;
            const FVector3f FN = FVector3f::CrossProduct(Pos[b] - Pos[a], Pos[c] - Pos[a]);
            Nrm[a] += FN; Nrm[b] += FN; Nrm[c] += FN;
        }
        for (FVector3f& N : Nrm) N = N.IsNearlyZero() ? FVector3f::UpVector : N.GetSafeNormal();
    }

    // Строим FMeshDescription
    FMeshDescription MeshDesc;
    FStaticMeshAttributes Attr(MeshDesc);
    Attr.Register();

    MeshDesc.ReserveNewVertices(Pos.Num());
    MeshDesc.ReserveNewVertexInstances(In.Indices.Num());
    MeshDesc.ReserveNewPolygons(In.Indices.Num() / 3);
    MeshDesc.ReserveNewEdges(In.Indices.Num());

    TVertexAttributesRef<FVector3f> VPos = Attr.GetVertexPositions();
    TVertexInstanceAttributesRef<FVector3f> VNrm = Attr.GetVertexInstanceNormals();
    TVertexInstanceAttributesRef<FVector2f> VUV = Attr.GetVertexInstanceUVs();
    VUV.SetNumChannels(1);

    TArray<FVertexID> VID; VID.SetNum(Pos.Num());
    for (int32 i = 0; i < Pos.Num(); ++i) { VID[i] = MeshDesc.CreateVertex(); VPos[VID[i]] = Pos[i]; }

    const FPolygonGroupID PG = MeshDesc.CreatePolygonGroup();
    Attr.GetPolygonGroupMaterialSlotNames()[PG] = FName(TEXT("Mat0"));

    const bool bHasUV = (In.UVs.Num() == Pos.Num());
    for (int32 i = 0; i + 2 < In.Indices.Num(); i += 3)
    {
        const uint32 Idx[3] = { In.Indices[i], In.Indices[i + 1], In.Indices[i + 2] };
        if (Idx[0] >= (uint32)Pos.Num() || Idx[1] >= (uint32)Pos.Num() || Idx[2] >= (uint32)Pos.Num()) continue;
        if (Idx[0] == Idx[1] || Idx[1] == Idx[2] || Idx[0] == Idx[2]) continue; // вырожденный

        TArray<FVertexInstanceID, TInlineAllocator<3>> Corners;
        for (int32 k = 0; k < 3; ++k)
        {
            const FVertexInstanceID VI = MeshDesc.CreateVertexInstance(VID[Idx[k]]);
            VNrm[VI] = Nrm[Idx[k]];
            VUV.Set(VI, 0, bHasUV ? In.UVs[Idx[k]] : FVector2f::ZeroVector);
            Corners.Add(VI);
        }
        MeshDesc.CreatePolygon(PG, Corners);
    }

    UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage());
    Mesh->NeverStream = true;
    Mesh->bAllowCPUAccess = true; // для сложного (по мешу) трассирования при клике
    Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, FName(TEXT("Mat0"))));

    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bBuildSimpleCollision = false;
    Params.bFastBuild = true;
    Params.bCommitMeshDescription = false;

    if (!Mesh->BuildFromMeshDescriptions({ &MeshDesc }, Params)) return nullptr;

    if (UBodySetup* BS = Mesh->GetBodySetup())
        BS->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;

    return Mesh;
}
