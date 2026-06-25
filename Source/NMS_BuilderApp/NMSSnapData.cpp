#include "NMSSnapData.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ---------------------------------------------------------------------------
//  Конвертация матрицы точки снапа из системы NMS в UE-пространство меша.
// ---------------------------------------------------------------------------
// Меши деталей экспортированы со сменой осей NMS->UE: (x,y,z) -> (x, -z, y) и x100
// (тот же базис, что и геометрия — см. заметки проекта). Точки снапа лежат в той же
// локальной системе, поэтому к ним применяем ту же замену базиса.
//
// JSON-матрица row-major, конвенция «столбец»: p' = R*p + t, перенос в столбце 3.
// Это полноценный трансформ кадра снапа (родитель=NMS), поэтому смену системы координат
// делаем сопряжением: R_ue = B * R_nms * B^T (не просто B*R — иначе кадр переворачивается
// вверх ногами). Перенос: t_ue = B * t_nms * 100 (метры -> см). B(x,y,z) = (x, -z, y).
// Проверено симуляцией: при такой конверсии правильная стыковка = флип ровно 180° вокруг Z.
static FTransform NMS_SnapMatrixToUE(const TArray<TSharedPtr<FJsonValue>>& Rows)
{
    if (Rows.Num() < 3) return FTransform::Identity;

    double M[3][4] = {};
    for (int32 r = 0; r < 3; ++r)
    {
        const TArray<TSharedPtr<FJsonValue>>* Row = nullptr;
        if (!Rows[r].IsValid() || !Rows[r]->TryGetArray(Row) || !Row || Row->Num() < 4)
            return FTransform::Identity;
        for (int32 c = 0; c < 4; ++c)
            M[r][c] = (*Row)[c].IsValid() ? (*Row)[c]->AsNumber() : 0.0;
    }

    auto Mul3 = [](const double A[3][3], const double Bm[3][3], double Out[3][3])
    {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
            {
                double s = 0.0;
                for (int k = 0; k < 3; ++k) s += A[i][k] * Bm[k][j];
                Out[i][j] = s;
            }
    };

    const double R[3][3]  = { {M[0][0],M[0][1],M[0][2]}, {M[1][0],M[1][1],M[1][2]}, {M[2][0],M[2][1],M[2][2]} };
    const double B[3][3]  = { {1,0,0}, {0,0,-1}, {0,1,0} };   // NMS->UE: ue=(x,-z,y)
    const double Bt[3][3] = { {1,0,0}, {0,0,1},  {0,-1,0} };  // B^T
    double BR[3][3], Rue[3][3];
    Mul3(B, R, BR);
    Mul3(BR, Bt, Rue);

    // t_ue = B * t_nms * 100
    const FVector T(M[0][3] * 100.0, -M[2][3] * 100.0, M[1][3] * 100.0);

    // UE row-vector: строки FMatrix = столбцы R_ue (образы осей).
    const FVector C0(Rue[0][0], Rue[1][0], Rue[2][0]);
    const FVector C1(Rue[0][1], Rue[1][1], Rue[2][1]);
    const FVector C2(Rue[0][2], Rue[1][2], Rue[2][2]);
    const FMatrix Mtx(
        FPlane(C0.X, C0.Y, C0.Z, 0.f),
        FPlane(C1.X, C1.Y, C1.Z, 0.f),
        FPlane(C2.X, C2.Y, C2.Z, 0.f),
        FPlane(T.X,  T.Y,  T.Z,  1.f));

    FTransform Out;
    Out.SetFromMatrix(Mtx);
    return Out;
}

// "A, B,  C" -> ["A","B","C"] (без пустых, с обрезкой пробелов)
static void NMS_SplitNames(const FString& In, TArray<FString>& Out)
{
    Out.Reset();
    TArray<FString> Raw;
    In.ParseIntoArray(Raw, TEXT(","), true);
    for (FString S : Raw)
    {
        S.TrimStartAndEndInline();
        if (!S.IsEmpty()) Out.Add(S);
    }
}

static FString NMS_CleanId(const FString& Id)
{
    FString S = Id;
    S.RemoveFromStart(TEXT("^"));
    return S;
}

// ---------------------------------------------------------------------------

const FNMSSnapPoint* FNMSSnapGroup::FindPoint(const FString& InName) const
{
    for (const FNMSSnapPoint& P : Points)
        if (P.Name == InName) return &P;
    return nullptr;
}

FNMSSnapData& FNMSSnapData::Get()
{
    static FNMSSnapData Inst;
    return Inst;
}

void FNMSSnapData::EnsureLoaded()
{
    if (bLoaded) return;
    bLoaded = true; // даже при ошибке не пытаемся повторно каждый кадр

    const FString Dir = FPaths::ProjectContentDir() / TEXT("NMSData");
    LoadInfo(Dir / TEXT("snapping_info.json"));
    LoadPairs(Dir / TEXT("snapping_pairs.json"));

    UE_LOG(LogTemp, Log, TEXT("NMS Snap: загружено групп=%d, деталей=%d, пар-источников=%d"),
        Groups.Num(), PartToGroup.Num(), Pairs.Num());
}

void FNMSSnapData::LoadInfo(const FString& Path)
{
    FString Raw;
    if (!FFileHelper::LoadFileToString(Raw, *Path))
    {
        UE_LOG(LogTemp, Warning, TEXT("NMS Snap: не найден %s"), *Path);
        return;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("NMS Snap: не разобран %s"), *Path);
        return;
    }

    for (const auto& GroupPair : Root->Values)
    {
        FString GroupName; GroupName = GroupPair.Key;
        const TSharedPtr<FJsonObject>* GroupObj = nullptr;
        if (!GroupPair.Value.IsValid() || !GroupPair.Value->TryGetObject(GroupObj) || !GroupObj) continue;

        FNMSSnapGroup Group;

        // parts
        const TArray<TSharedPtr<FJsonValue>>* PartsArr = nullptr;
        if ((*GroupObj)->TryGetArrayField(TEXT("parts"), PartsArr) && PartsArr)
        {
            for (const TSharedPtr<FJsonValue>& V : *PartsArr)
            {
                if (!V.IsValid()) continue;
                const FString Id = NMS_CleanId(V->AsString());
                if (Id.IsEmpty()) continue;
                Group.Parts.Add(Id);
                PartToGroup.Add(Id, GroupName);
            }
        }

        // snap_points
        const TSharedPtr<FJsonObject>* SnapObj = nullptr;
        if ((*GroupObj)->TryGetObjectField(TEXT("snap_points"), SnapObj) && SnapObj)
        {
            for (const auto& SP : (*SnapObj)->Values)
            {
                const TSharedPtr<FJsonObject>* PtObj = nullptr;
                if (!SP.Value.IsValid() || !SP.Value->TryGetObject(PtObj) || !PtObj) continue;

                FNMSSnapPoint Point;
                Point.Name = SP.Key;
                (*PtObj)->TryGetStringField(TEXT("opposite"), Point.Opposite);

                const TArray<TSharedPtr<FJsonValue>>* MatArr = nullptr;
                if ((*PtObj)->TryGetArrayField(TEXT("matrix"), MatArr) && MatArr)
                    Point.LocalXform = NMS_SnapMatrixToUE(*MatArr);

                Group.Points.Add(MoveTemp(Point));
            }
        }

        Groups.Add(GroupName, MoveTemp(Group));
    }
}

void FNMSSnapData::LoadPairs(const FString& Path)
{
    FString Raw;
    if (!FFileHelper::LoadFileToString(Raw, *Path))
    {
        UE_LOG(LogTemp, Warning, TEXT("NMS Snap: не найден %s"), *Path);
        return;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("NMS Snap: не разобран %s"), *Path);
        return;
    }

    for (const auto& SrcPair : Root->Values)
    {
        FString SrcGroup; SrcGroup = SrcPair.Key;
        const TSharedPtr<FJsonObject>* TgtMap = nullptr;
        if (!SrcPair.Value.IsValid() || !SrcPair.Value->TryGetObject(TgtMap) || !TgtMap) continue;

        TMap<FString, TPair<TArray<FString>, TArray<FString>>>& Inner = Pairs.FindOrAdd(SrcGroup);

        for (const auto& TgtPair : (*TgtMap)->Values)
        {
            FString TgtGroup; TgtGroup = TgtPair.Key;
            const TArray<TSharedPtr<FJsonValue>>* Lists = nullptr;
            if (!TgtPair.Value.IsValid() || !TgtPair.Value->TryGetArray(Lists) || !Lists || Lists->Num() < 2)
                continue;

            TArray<FString> A, Bn;
            NMS_SplitNames((*Lists)[0]->AsString(), A);
            NMS_SplitNames((*Lists)[1]->AsString(), Bn);
            Inner.Add(TgtGroup, TPair<TArray<FString>, TArray<FString>>(MoveTemp(A), MoveTemp(Bn)));
        }
    }
}

const FString* FNMSSnapData::GroupForPart(const FString& PartId) const
{
    return PartToGroup.Find(NMS_CleanId(PartId));
}

const FNMSSnapGroup* FNMSSnapData::GetGroup(const FString& GroupName) const
{
    return Groups.Find(GroupName);
}

bool FNMSSnapData::GetPair(const FString& GroupOld, const FString& GroupNew,
                           TArray<FString>& OutOldNames, TArray<FString>& OutNewNames) const
{
    // Прямой порядок: Pairs[GroupOld][GroupNew] = (точки старой, точки новой)
    if (const TMap<FString, TPair<TArray<FString>, TArray<FString>>>* Inner = Pairs.Find(GroupOld))
    {
        if (const TPair<TArray<FString>, TArray<FString>>* P = Inner->Find(GroupNew))
        {
            OutOldNames = P->Key;
            OutNewNames = P->Value;
            return true;
        }
    }
    // Обратный порядок: Pairs[GroupNew][GroupOld] = (точки новой, точки старой)
    if (const TMap<FString, TPair<TArray<FString>, TArray<FString>>>* Inner = Pairs.Find(GroupNew))
    {
        if (const TPair<TArray<FString>, TArray<FString>>* P = Inner->Find(GroupOld))
        {
            OutOldNames = P->Value;
            OutNewNames = P->Key;
            return true;
        }
    }
    return false;
}
