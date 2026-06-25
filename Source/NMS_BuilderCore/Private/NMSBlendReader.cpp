// Чтение .blend без Blender: zstd-распаковка + разбор блоков/DNA/IDProperty.
// Алгоритм выверен на реальных файлах (DoubleGrill.blend против эталонного
// экспорта плагина: 63/63 деталей, макс. ошибка координат 1.5e-4).

#include "NMSBlendReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"

namespace
{
// ---------- маленькие читалки little-endian ----------
uint32 RdU32(const uint8* D, int64 P) { uint32 V; FMemory::Memcpy(&V, D + P, 4); return V; }
uint64 RdU64(const uint8* D, int64 P) { uint64 V; FMemory::Memcpy(&V, D + P, 8); return V; }
uint16 RdU16(const uint8* D, int64 P) { uint16 V; FMemory::Memcpy(&V, D + P, 2); return V; }
int32  RdI32(const uint8* D, int64 P) { int32 V; FMemory::Memcpy(&V, D + P, 4); return V; }
int16  RdI16(const uint8* D, int64 P) { int16 V; FMemory::Memcpy(&V, D + P, 2); return V; }
float  RdF32(const uint8* D, int64 P) { float V; FMemory::Memcpy(&V, D + P, 4); return V; }
double RdF64(const uint8* D, int64 P) { double V; FMemory::Memcpy(&V, D + P, 8); return V; }

FString RdCStr(const uint8* D, int64 P, int32 MaxLen)
{
    int32 L = 0;
    while (L < MaxLen && D[P + L] != 0) ++L;
    return FString(L, (const ANSICHAR*)(D + P));
}

// ---------- матрицы 4x4 (double, колоночные векторы: M @ v) ----------
struct FM44 { double M[4][4]; };

FM44 M44_Identity()
{
    FM44 R{};
    for (int i = 0; i < 4; ++i) R.M[i][i] = 1.0;
    return R;
}

FM44 M44_Mul(const FM44& A, const FM44& B)
{
    FM44 R{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
        {
            double S = 0.0;
            for (int k = 0; k < 4; ++k) S += A.M[i][k] * B.M[k][j];
            R.M[i][j] = S;
        }
    return R;
}

FM44 M44_EulerXYZ(double X, double Y, double Z)
{
    const double cx = FMath::Cos(X), sx = FMath::Sin(X);
    const double cy = FMath::Cos(Y), sy = FMath::Sin(Y);
    const double cz = FMath::Cos(Z), sz = FMath::Sin(Z);
    FM44 Rx = M44_Identity(), Ry = M44_Identity(), Rz = M44_Identity();
    Rx.M[1][1] = cx; Rx.M[1][2] = -sx; Rx.M[2][1] = sx; Rx.M[2][2] = cx;
    Ry.M[0][0] = cy; Ry.M[0][2] = sy;  Ry.M[2][0] = -sy; Ry.M[2][2] = cy;
    Rz.M[0][0] = cz; Rz.M[0][1] = -sz; Rz.M[1][0] = sz; Rz.M[1][1] = cz;
    return M44_Mul(Rz, M44_Mul(Ry, Rx)); // порядок Blender Euler XYZ
}

FM44 M44_Quat(double W, double X, double Y, double Z)
{
    const double N = FMath::Sqrt(W*W + X*X + Y*Y + Z*Z);
    if (N > 1e-12) { W /= N; X /= N; Y /= N; Z /= N; }
    FM44 R = M44_Identity();
    R.M[0][0] = 1 - 2*(Y*Y + Z*Z); R.M[0][1] = 2*(X*Y - W*Z); R.M[0][2] = 2*(X*Z + W*Y);
    R.M[1][0] = 2*(X*Y + W*Z); R.M[1][1] = 1 - 2*(X*X + Z*Z); R.M[1][2] = 2*(Y*Z - W*X);
    R.M[2][0] = 2*(X*Z - W*Y); R.M[2][1] = 2*(Y*Z + W*X); R.M[2][2] = 1 - 2*(X*X + Y*Y);
    return R;
}

// ---------- структуры разбора ----------
struct FBlock
{
    char   Code[5];
    uint32 Sdna = 0;
    uint32 Count = 0;
    int64  Data = 0;
    int64  Len = 0;
};

struct FFieldInfo { int32 Offset = -1; int32 Type = 0; FString FullName; };
using FOffsets = TMap<FString, FFieldInfo>;

struct FParseCtx
{
    const uint8* D = nullptr;
    int64 Size = 0;
    TArray<FBlock> Blocks;
    TMap<uint64, int32> ByOld;
    // SDNA
    TArray<FString> Names;
    TArray<FString> Types;
    TArray<uint16>  TLens;
    struct FStruct { uint16 Type; TArray<TPair<uint16,uint16>> Fields; };
    TArray<FStruct> Structs;
    TMap<FString, int32> StructByName;

    int32 PtrSize = 8;

    int32 FieldSize(uint16 FType, uint16 FName) const
    {
        const FString& Nm = Names[FName];
        int32 Arr = 1;
        // имя вида "loc[3]" / "mat[4][4]"
        int32 I = 0;
        FString Base = Nm;
        while (true)
        {
            int32 LB, RB;
            if (!Base.FindChar(TEXT('['), LB)) break;
            if (!Base.FindChar(TEXT(']'), RB)) break;
            Arr *= FCString::Atoi(*Base.Mid(LB + 1, RB - LB - 1));
            Base = Base.Left(LB) + Base.Mid(RB + 1);
        }
        if (Nm.StartsWith(TEXT("*")) || Nm.StartsWith(TEXT("(*")))
            return PtrSize * Arr;
        return TLens[FType] * Arr;
    }

    FOffsets MakeOffsets(int32 StructIdx) const
    {
        FOffsets Out;
        int32 O = 0;
        for (const TPair<uint16,uint16>& F : Structs[StructIdx].Fields)
        {
            const FString& Nm = Names[F.Value];
            FString Key = Nm;
            while (Key.StartsWith(TEXT("*")) || Key.StartsWith(TEXT("("))) Key = Key.Mid(1);
            int32 Br;
            if (Key.FindChar(TEXT('['), Br)) Key = Key.Left(Br);
            int32 Par;
            if (Key.FindChar(TEXT(')'), Par)) Key = Key.Left(Par);
            FFieldInfo Info; Info.Offset = O; Info.Type = F.Key; Info.FullName = Nm;
            Out.Add(Key, Info);
            O += FieldSize(F.Key, F.Value);
        }
        return Out;
    }
};

// поиск ASCII-сигнатуры
int64 FindSig(const uint8* D, int64 Size, int64 From, const char* Sig)
{
    const int32 L = FCStringAnsi::Strlen(Sig);
    for (int64 P = From; P + L <= Size; ++P)
        if (FMemory::Memcmp(D + P, Sig, L) == 0) return P;
    return -1;
}
} // namespace

bool NMSBlend::ReadParts(const FString& FilePath, TArray<FNMSBlendPart>& Out, FString& OutError)
{
    Out.Reset();

    TArray<uint8> Raw;
    if (!FFileHelper::LoadFileToArray(Raw, *FilePath))
    {
        OutError = TEXT("не удалось прочитать файл");
        return false;
    }

    // --- zstd? (Blender 3.0+ сжимает по кадрам; декодируем потоково) ---
    TArray<uint8> Plain;
    if (Raw.Num() > 4 && Raw[0] == 0x28 && Raw[1] == 0xB5 && Raw[2] == 0x2F && Raw[3] == 0xFD)
    {
        ZSTD_DStream* DS = ZSTD_createDStream();
        ZSTD_initDStream(DS);
        ZSTD_inBuffer In{ Raw.GetData(), (size_t)Raw.Num(), 0 };
        TArray<uint8> Chunk;
        Chunk.SetNumUninitialized(1 << 20);
        while (In.pos < In.size)
        {
            ZSTD_outBuffer OutB{ Chunk.GetData(), (size_t)Chunk.Num(), 0 };
            const size_t R = ZSTD_decompressStream(DS, &OutB, &In);
            if (ZSTD_isError(R))
            {
                // пропуск skippable-кадров zstd сам делает; настоящая ошибка:
                ZSTD_freeDStream(DS);
                OutError = FString::Printf(TEXT("zstd: %hs"), ZSTD_getErrorName(R));
                return false;
            }
            Plain.Append(Chunk.GetData(), OutB.pos);
            if (R == 0) ZSTD_initDStream(DS); // кадр кончился — возможен следующий
        }
        ZSTD_freeDStream(DS);
    }
    else
    {
        Plain = MoveTemp(Raw);
    }

    if (Plain.Num() < 32 || FMemory::Memcmp(Plain.GetData(), "BLENDER", 7) != 0)
    {
        OutError = TEXT("это не .blend (нет сигнатуры BLENDER)");
        return false;
    }

    FParseCtx C;
    C.D = Plain.GetData();
    C.Size = Plain.Num();

    // --- заголовок: новый 17-байтовый ("BLENDER17-01vXXXX") или старый 12 ---
    const bool bNew = (C.D[7] == '1' && C.D[8] == '7');
    int64 Pos = bNew ? 17 : 12;
    if (!bNew && C.D[7] == '_')
    {
        OutError = TEXT("32-битный .blend (очень старый) не поддерживается");
        return false;
    }

    // --- блоки ---
    int64 DnaOff = -1;
    while (Pos + 32 <= C.Size)
    {
        FBlock B;
        FMemory::Memcpy(B.Code, C.D + Pos, 4); B.Code[4] = 0;
        // убрать хвостовые нули в коде
        for (int i = 3; i >= 0 && B.Code[i] == 0; --i) {}
        uint64 Old;
        if (bNew)
        {
            Old     = RdU64(C.D, Pos + 8);
            B.Len   = (int64)RdU64(C.D, Pos + 16);
            B.Sdna  = RdU32(C.D, Pos + 24);
            B.Count = RdU32(C.D, Pos + 28);
            B.Data  = Pos + 32;
        }
        else
        {
            B.Len   = RdU32(C.D, Pos + 4);
            Old     = RdU64(C.D, Pos + 8);
            B.Sdna  = RdU32(C.D, Pos + 16);
            B.Count = RdU32(C.D, Pos + 20);
            B.Data  = Pos + 24;
        }
        if (FMemory::Memcmp(B.Code, "ENDB", 4) == 0) break;
        if (B.Len < 0 || B.Data + B.Len > C.Size) { OutError = TEXT("повреждённый блок"); return false; }
        C.Blocks.Add(B);
        C.ByOld.Add(Old, C.Blocks.Num() - 1);
        if (FMemory::Memcmp(B.Code, "DNA1", 4) == 0) DnaOff = B.Data;
        Pos = B.Data + B.Len;
    }
    if (DnaOff < 0) { OutError = TEXT("нет DNA1"); return false; }

    // --- SDNA (секции ищем по сигнатурам — выравнивание в 5.x нестандартное) ---
    {
        int64 P = FindSig(C.D, C.Size, DnaOff, "NAME");
        if (P < 0) { OutError = TEXT("SDNA: нет NAME"); return false; }
        P += 4;
        const uint32 NName = RdU32(C.D, P); P += 4;
        C.Names.Reserve(NName);
        for (uint32 i = 0; i < NName; ++i)
        {
            const FString S = RdCStr(C.D, P, 256);
            P += S.Len() + 1;
            C.Names.Add(S);
        }
        P = FindSig(C.D, C.Size, P, "TYPE"); P += 4;
        const uint32 NType = RdU32(C.D, P); P += 4;
        C.Types.Reserve(NType);
        for (uint32 i = 0; i < NType; ++i)
        {
            const FString S = RdCStr(C.D, P, 256);
            P += S.Len() + 1;
            C.Types.Add(S);
        }
        P = FindSig(C.D, C.Size, P, "TLEN"); P += 4;
        C.TLens.Reserve(NType);
        for (uint32 i = 0; i < NType; ++i) { C.TLens.Add(RdU16(C.D, P)); P += 2; }
        P = FindSig(C.D, C.Size, P, "STRC"); P += 4;
        const uint32 NStrc = RdU32(C.D, P); P += 4;
        for (uint32 i = 0; i < NStrc; ++i)
        {
            FParseCtx::FStruct S;
            S.Type = RdU16(C.D, P); P += 2;
            const uint16 NF = RdU16(C.D, P); P += 2;
            S.Fields.Reserve(NF);
            for (uint16 f = 0; f < NF; ++f)
            {
                const uint16 FT = RdU16(C.D, P); P += 2;
                const uint16 FN = RdU16(C.D, P); P += 2;
                S.Fields.Emplace(FT, FN);
            }
            C.StructByName.Add(C.Types[S.Type], C.Structs.Num());
            C.Structs.Add(MoveTemp(S));
        }
    }

    const int32* IdIdx   = C.StructByName.Find(TEXT("ID"));
    const int32* ObjIdx  = C.StructByName.Find(TEXT("Object"));
    const int32* IdpIdx  = C.StructByName.Find(TEXT("IDProperty"));
    const int32* IdpdIdx = C.StructByName.Find(TEXT("IDPropertyData"));
    if (!IdIdx || !ObjIdx || !IdpIdx || !IdpdIdx)
    {
        OutError = TEXT("SDNA: нет нужных структур");
        return false;
    }
    const FOffsets IdOff   = C.MakeOffsets(*IdIdx);
    const FOffsets ObjOff  = C.MakeOffsets(*ObjIdx);
    const FOffsets IdpOff  = C.MakeOffsets(*IdpIdx);
    const FOffsets IdpdOff = C.MakeOffsets(*IdpdIdx);

    auto Need = [&OutError](const FOffsets& O, const TCHAR* K) -> int32
    {
        const FFieldInfo* F = O.Find(K);
        return F ? F->Offset : -1;
    };
    const int32 OfIdProps  = Need(IdOff,  TEXT("properties"));
    const int32 OfIdpNext  = Need(IdpOff, TEXT("next"));
    const int32 OfIdpType  = Need(IdpOff, TEXT("type"));
    const int32 OfIdpName  = Need(IdpOff, TEXT("name"));
    const int32 OfIdpData  = Need(IdpOff, TEXT("data"));
    const int32 OfPdPtr    = Need(IdpdOff, TEXT("pointer"));
    const int32 OfPdGroup  = Need(IdpdOff, TEXT("group"));
    const int32 OfPdVal    = Need(IdpdOff, TEXT("val"));
    const int32 OfLoc      = Need(ObjOff, TEXT("loc"));
    const int32 OfSizeF    = Need(ObjOff, TEXT("size"));
    const int32 OfRot      = Need(ObjOff, TEXT("rot"));
    const int32 OfQuat     = Need(ObjOff, TEXT("quat"));
    const int32 OfRotMode  = Need(ObjOff, TEXT("rotmode"));
    const int32 OfParent   = Need(ObjOff, TEXT("parent"));
    const int32 OfParInv   = Need(ObjOff, TEXT("parentinv"));
    if (OfIdProps < 0 || OfIdpNext < 0 || OfLoc < 0 || OfParInv < 0)
    {
        OutError = TEXT("SDNA: неожиданная раскладка Object/IDProperty");
        return false;
    }

    // --- IDProperty: группа -> плоский словарь строк/чисел ---
    TMap<FString, FString> Props; // переиспользуем на каждый объект
    TFunction<void(uint64)> ReadGroup = [&](uint64 Ptr)
    {
        int32 Guard = 0;
        while (Ptr && Guard++ < 512)
        {
            const int32* Bi = C.ByOld.Find(Ptr);
            if (!Bi) break;
            const FBlock& B = C.Blocks[*Bi];
            const FString Name = RdCStr(C.D, B.Data + OfIdpName, 64);
            const uint8 Typ = C.D[B.Data + OfIdpType];
            const int64 DO = B.Data + OfIdpData;
            if (Typ == 0) // STRING
            {
                const uint64 SP = RdU64(C.D, DO + OfPdPtr);
                if (const int32* Sb = C.ByOld.Find(SP))
                {
                    const FBlock& SB = C.Blocks[*Sb];
                    Props.Add(Name, RdCStr(C.D, SB.Data, (int32)FMath::Min<int64>(SB.Len, 4096)));
                }
            }
            else if (Typ == 1) // INT
            {
                Props.Add(Name, FString::FromInt(RdI32(C.D, DO + OfPdVal)));
            }
            else if (Typ == 8) // DOUBLE
            {
                Props.Add(Name, FString::SanitizeFloat(RdF64(C.D, DO + OfPdVal)));
            }
            else if (Typ == 6) // GROUP — углубляемся (user_properties и т.п.)
            {
                ReadGroup(RdU64(C.D, DO + OfPdGroup));
            }
            Ptr = RdU64(C.D, B.Data + OfIdpNext);
        }
    };

    // --- объекты OB ---
    TMap<uint64, int64> ObData; // old ptr -> data offset
    for (int32 i = 0; i < C.Blocks.Num(); ++i)
    {
        if (FMemory::Memcmp(C.Blocks[i].Code, "OB", 2) == 0 &&
            C.Blocks[i].Code[2] == 0)
        {
            for (const TPair<uint64,int32>& KV : C.ByOld)
                if (KV.Value == i) { ObData.Add(KV.Key, C.Blocks[i].Data); break; }
        }
    }

    TMap<uint64, FM44> WorldCache;
    TFunction<FM44(uint64)> WorldOf = [&](uint64 Old) -> FM44
    {
        if (const FM44* Hit = WorldCache.Find(Old)) return *Hit;
        const int64 Data = ObData[Old];

        // локальная матрица: T * R * S
        const double Lx = RdF32(C.D, Data + OfLoc),     Ly = RdF32(C.D, Data + OfLoc + 4),  Lz = RdF32(C.D, Data + OfLoc + 8);
        const double Sx = RdF32(C.D, Data + OfSizeF),   Sy = RdF32(C.D, Data + OfSizeF + 4), Sz = RdF32(C.D, Data + OfSizeF + 8);
        const int16 RotMode = (OfRotMode >= 0) ? RdI16(C.D, Data + OfRotMode) : 1;
        FM44 R;
        if (RotMode == 0 && OfQuat >= 0)
            R = M44_Quat(RdF32(C.D, Data + OfQuat), RdF32(C.D, Data + OfQuat + 4),
                         RdF32(C.D, Data + OfQuat + 8), RdF32(C.D, Data + OfQuat + 12));
        else
            R = M44_EulerXYZ(RdF32(C.D, Data + OfRot), RdF32(C.D, Data + OfRot + 4),
                             RdF32(C.D, Data + OfRot + 8));
        FM44 M = R;
        for (int i = 0; i < 3; ++i) { M.M[i][0] *= Sx; M.M[i][1] *= Sy; M.M[i][2] *= Sz; }
        M.M[0][3] = Lx; M.M[1][3] = Ly; M.M[2][3] = Lz;

        const uint64 Par = (OfParent >= 0) ? RdU64(C.D, Data + OfParent) : 0;
        if (Par && ObData.Contains(Par))
        {
            // parentinv хранится колонками float[4][4] -> в строки
            FM44 ParInv;
            FMemory::Memzero(&ParInv, sizeof(ParInv));
            for (int col = 0; col < 4; ++col)
                for (int row = 0; row < 4; ++row)
                    ParInv.M[row][col] = RdF32(C.D, Data + OfParInv + 16 * col + 4 * row);
            M = M44_Mul(WorldOf(Par), M44_Mul(ParInv, M));
        }
        WorldCache.Add(Old, M);
        return M;
    };

    // --- извлечение деталей (математика serialise() плагина) ---
    const double c = 0.0, s = -1.0; // cos(-90°)=0, sin(-90°)=-1
    for (const TPair<uint64,int64>& KV : ObData)
    {
        Props.Reset();
        const uint64 PropsPtr = RdU64(C.D, KV.Value + OfIdProps);
        if (!PropsPtr) continue;
        if (const int32* Bi = C.ByOld.Find(PropsPtr))
        {
            const FBlock& B = C.Blocks[*Bi];
            if (C.D[B.Data + OfIdpType] == 6)
                ReadGroup(RdU64(C.D, B.Data + OfIdpData + OfPdGroup));
        }
        const FString* Oid = Props.Find(TEXT("ObjectID"));
        if (!Oid || Oid->IsEmpty()) continue;

        const FM44 W = WorldOf(KV.Key);
        // m2 = Rx(-90) @ W
        FM44 RX = M44_Identity();
        RX.M[1][1] = c; RX.M[1][2] = -s; RX.M[2][1] = s; RX.M[2][2] = c;
        const FM44 M2 = M44_Mul(RX, W);

        FNMSBlendPart P;
        FString Clean = *Oid; Clean.RemoveFromStart(TEXT("^"));
        P.ObjectID = TEXT("^") + Clean;
        P.Position = FVector(M2.M[0][3], M2.M[1][3], M2.M[2][3]);
        P.Up       = FVector(M2.M[0][1], M2.M[1][1], M2.M[2][1]);
        FVector At = FVector(M2.M[0][2], M2.M[1][2], M2.M[2][2]);
        P.At = At.GetSafeNormal();
        if (const FString* UD = Props.Find(TEXT("UserData"))) P.UserData = FCString::Atoi(**UD);
        if (const FString* TS = Props.Find(TEXT("Timestamp"))) P.Timestamp = FCString::Atoi64(**TS);
        Out.Add(MoveTemp(P));
    }

    if (Out.Num() == 0)
    {
        OutError = TEXT("в файле нет деталей NMS (объектов с ObjectID)");
        return false;
    }
    return true;
}
