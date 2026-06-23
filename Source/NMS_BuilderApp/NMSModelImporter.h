// Импорт внешних 3D-моделей (.obj / .stl) во время выполнения.
// Парсит файл в геометрию и собирает рантайм UStaticMesh (через FMeshDescription),
// который затем спавнится как обычная деталь (AStaticMeshActor) и поддерживает
// выбор/трансформ/подсветку существующими инструментами вьюпорта.
#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMaterialInterface;

// Сырая геометрия после парсинга.
struct FNMSImportedMesh
{
    TArray<FVector3f>  Positions;   // вершины
    TArray<FVector3f>  Normals;     // по вершинам (может быть пусто -> посчитаем)
    TArray<FVector2f>  UVs;         // по вершинам (может быть пусто)
    TArray<uint32>     Indices;     // тройки -> треугольники
    bool IsValid() const { return Positions.Num() >= 3 && Indices.Num() >= 3; }
};

class FNMSModelImporter
{
public:
    // Загрузить .obj или .stl с диска. Расширение определяется по пути.
    static bool LoadFromFile(const FString& Path, FNMSImportedMesh& Out, FString& OutError);

    // Собрать рантайм UStaticMesh из геометрии. Material — материал слота (может быть null).
    // bAutoFitTargetSize > 0 -> отмасштабировать так, чтобы наибольшая сторона = это значение,
    // и центрировать по низу (модель стоит на земле в начале координат).
    static UStaticMesh* BuildStaticMesh(const FNMSImportedMesh& In, UMaterialInterface* Material,
        float AutoFitTargetSize = 300.f);

    // Парсеры (публичны для тестов).
    static bool ParseOBJ(const FString& Text, FNMSImportedMesh& Out, FString& OutError);
    static bool ParseSTL(const TArray<uint8>& Bytes, FNMSImportedMesh& Out, FString& OutError);
};
