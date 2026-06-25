#pragma once

#include "CoreMinimal.h"
#include "UnrealClient.h"
#include "InputCoreTypes.h"
#include "PreviewScene.h"

class AActor;
class FSceneView;
class FViewport;
class FCanvas;

// Режим манипуляции выбранной деталью мышью (как тулбар в приложении).
enum class ENMSTransformMode : uint8 { Move, Rotate, Scale };
enum class ENMSCurveType : uint8 { Catmull, Polyline, Circle, Bezier, Rect };

// Материал детали, который UI задаёт ПЕРЕД постановкой (StartPlacing).
// Пустые текстуры = плоский цвет. Передаётся через SetPendingMaterial().
struct FNMSPendingMaterial
{
    FLinearColor Color  = FLinearColor(0.78f, 0.78f, 0.74f); // основной
    FLinearColor Color2 = FLinearColor::White;               // вторичный (металл/канты)
    FString BaseTex;     // диффуз
    FString PaintMask;   // маска покраски
    FString NormalTex;   // карта рельефа
    FString MasksTex;    // карта масок (AO/шероховатость/металл)
    FString OccTex;      // карта затенения швов
};

/**
 * Вьюпорт-клиент встроенного 3D окна редактора баз.
 * Своя сцена (FPreviewScene) + рендер через FSceneView + FreeCam-камера.
 * Управление: ПКМ зажата -> мышь вращает, WASD/QE двигают (как в UE/Blender).
 */
class FNMSViewportClient : public FViewportClient
{
public:
    FNMSViewportClient();
    virtual ~FNMSViewportClient();

    // --- FViewportClient ---
    virtual void Draw(FViewport* Viewport, FCanvas* Canvas) override;
    virtual UWorld* GetWorld() const override;

    // ввод
    virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
    virtual bool InputAxis(const FInputKeyEventArgs& EventArgs) override;

    FPreviewScene* GetPreviewScene() { return PreviewScene.Get(); }

    // Сбросить камеру в стартовый ракурс (кнопка «КАМЕРА» в тулбаре).
    void ResetCamera();

    // Назначить выбранную деталь извне (из UI при клике по детали).
    void SetSelectedActor(AActor* InActor);
    // Текущая выбранная деталь (для покраски из UI).
    AActor* GetSelectedActor() const;
    const TArray<TWeakObjectPtr<AActor>>& GetSelectedActorsList() const { return SelectedActors; }

    // Применить настоящий (текстурный/цветной) материал к детали из Pending*-полей.
    void ApplyPartMaterial(AActor* Actor);
    // Выбрать только что заспавненную деталь для правки (гизмо + сетка + снап).
    void SelectSpawnedActor(AActor* Actor);
    // Режим размещения «как в игре»: зелёная голограмма следует за курсором, ЛКМ ставит.
    void StartPlacing(AActor* GhostActor);
    // Отменить текущую установку (убрать незавершённый призрак).
    void CancelPlacing();
    // Завершить предыдущее: снять выделение со всех деталей.
    void ClearSelection();
    // Режим прокладки кабеля/трубы: клик А, клик Б -> кабель между ними.
    void ToggleWiringMode() { CancelPlacing(); bWiringMode = !bWiringMode; } // с отменой установки
    bool IsWiringMode() const { return bWiringMode; }
    void SpawnCable(const FVector& A, const FVector& B);
    // Зеркалирование выделения по оси (0=X, 1=Y).
    void MirrorSelection(int32 Axis);
    // Колбэки в UI: правка сцены (для Undo), запросы Undo/Redo (Ctrl+Z/Y).
    TFunction<void()> OnEdited;
    // Запрос фокуса клавиатуры на вьюпорт (UI ставит фокус на SViewport).
    // Без него WASD-полёт не работает при зажатой ПКМ: мышь захвачена вьюпортом,
    // а клавиатурный фокус остаётся на другом виджете.
    TFunction<void()> OnRequestFocus;
    // Непрерывная установка: после постановки сразу новый призрак (Esc — выход).
    TFunction<void()> OnPlaceContinue;
    TFunction<void()> OnUndo;
    TFunction<void()> OnRedo;
    // Материал детали для следующей постановки (задаёт UI перед StartPlacing).
    void SetPendingMaterial(const FNMSPendingMaterial& M)
    {
        PendingPartColor = M.Color; PendingPartColor2 = M.Color2;
        PendingBaseTex = M.BaseTex; PendingPaintMask = M.PaintMask;
        PendingNormalTex = M.NormalTex; PendingMasksTex = M.MasksTex; PendingOccTex = M.OccTex;
    }
    // Режим «без света»: детали ставятся с безсветовым материалом (ровный вид).
    static bool bUnlitParts;

    // Сменить фон сцены: PNG с диска -> текстура -> материал неба (M_SkyBG).
    void SetBackgroundImage(const FString& PngPath);
    virtual void MouseMove(FViewport* Viewport, int32 X, int32 Y) override;

    // Точка на плоскости Z=0 прямо перед камерой (куда смотрит вид) — место постановки.
    FVector GetGroundFocusPoint() const;

    // --- манипуляция мышью (тулбар задаёт через команды) ---
    void SetTransformMode(ENMSTransformMode M) { TransformMode = M; }
    ENMSTransformMode GetTransformMode() const { return TransformMode; }
    void  SetMoveSnap(float Snap) { MoveSnap = Snap; }
    float GetMoveSnap() const { return MoveSnap; }

    bool bCurveMode = false;            // режим рисования кривой
    ENMSCurveType CurveType = ENMSCurveType::Catmull; // тип кривой
    float CurveOverlap = 0.5f;          // нахлёст 0..0.9 (плотность)
    float CurvePartLen = 100.f;         // длина детали (для предпросмотра раскладки)
    float CurveTilt = 0.f;              // наклон деталей (pitch, градусы)
    float CurveRoll = 0.f;              // крен деталей (roll, градусы)
    float CurveRadius = 0.f;            // радиус круга (0 = по клику)
    TFunction<void()> OnCurveApply;     // колбэк постройки (UI выставляет)
    TFunction<void()> OnCurveChanged;   // колбэк: точки/режим изменились (для меш-предпросмотра)
    TArray<FVector> CurvePoints;        // узлы кривой на гриде (Z=0)

private:
    // --- внутренние настройки/состояние (UI к ним напрямую не обращается) ---
    float    CameraFOV = 80.f;
    // грид-сетка режима постройки (стиль NMS); тогглится клавишами в InputKey
    bool   bShowGrid      = true;
    float  GridCellSize   = 100.f;    // размер ячейки, юниты (100 ≈ 1 м)
    int32  GridHalfCount  = 400;      // линий в каждую сторону
    int32  GridMajorEvery = 10;       // каждая N-я линия — основная
    float  GridFadeStart  = 2500.f;   // ярко только вблизи (~25 м)
    float  GridFadeEnd    = 14000.f;  // плавно гаснет к ~140 м
    int32  GridMode       = 1;        // 0 выкл, 1 обычная, 2 чёткая
    FLinearColor GridMinorColor = FLinearColor(1.00f, 1.00f, 1.00f, 0.22f);
    FLinearColor GridMajorColor = FLinearColor(1.00f, 1.00f, 1.00f, 0.50f);
    bool bContinuousPlace = true;     // после постановки сразу новый призрак
    bool bSunFollowsCamera = false;   // солнце вдоль взгляда (равномерный свет)
    TWeakObjectPtr<class UDirectionalLightComponent> SunComp;
    float ZoomStep = 250.f;           // шаг доли на «щелчок» колеса
    float PanSpeed = 2.0f;            // скорость пана (юнит / пиксель)
    float RotStepDeg = 15.f;          // шаг поворота, градусы
    float ScaleStep  = 0.10f;         // шаг масштаба
    bool  bSnapToParts = true;        // тоггл V: прилипать к соседним деталям
    float SnapRadius   = 350.f;       // макс. дистанция до точки стыковки, см
    bool  bShowSnapPoints = false;    // отладка точек стыковки (клавиша B)
    float RotDragSpeed = 1.0f;        // скорость поворота при перетаскивании
    // управляется публичными командами выше (Set*/Toggle*):
    ENMSTransformMode TransformMode = ENMSTransformMode::Move; // 1=двигать 2=крутить 3=масштаб
    float MoveSnap = 50.f;            // шаг привязки перемещения, юниты
    bool  bWiringMode = false;        // режим прокладки кабеля
    // камера/фокус: UI меняет через ResetCamera(); фокус по клавише F
    FVector  CameraLocation = FVector(-600.f, 0.f, 400.f);
    FRotator CameraRotation = FRotator(-25.f, 0.f, 0.f);
    FVector  FocusPoint = FVector::ZeroVector;
    // материал следующей постановки (задаётся через SetPendingMaterial)
    FLinearColor PendingPartColor = FLinearColor(0.78f, 0.78f, 0.74f);
    FLinearColor PendingPartColor2 = FLinearColor::White;
    FString PendingBaseTex, PendingPaintMask, PendingNormalTex, PendingMasksTex, PendingOccTex;

    TUniquePtr<FPreviewScene> PreviewScene;
    FLinearColor SkyColor = FLinearColor(0.32f, 0.55f, 0.82f, 1.f);
    TWeakObjectPtr<class UStaticMeshComponent> SkySphereComp; // купол неба (для смены фона)

    // состояние управления
    bool bRMBDown = false;                 // ПКМ зажата -> режим полёта
    bool bMMBDown = false;                 // СКМ зажата -> орбита/пан
    bool bAltDown = false;
    bool bCtrlDown = false;
    FVector WireA = FVector::ZeroVector; bool bHaveWireA = false;                 // Alt -> пан вместо орбиты
    bool bDragging = false;                // ЛКМ зажата на детали -> манипуляция
    bool bPlacingGhost = false;            // идёт размещение голограммы
    FVector DragOffset = FVector::ZeroVector; // смещение точки захвата от центра детали
    bool bFwd=false, bBack=false, bLeft=false, bRight=false, bUp=false, bDown=false;
    float CameraSpeed = 1200.f;            // см/с (FreeCam)
    float MouseSensitivity = 0.2f;
    double LastDrawTime = 0.0;

    // выбранная деталь (актёр-превью с меткой "NMS_")
    TWeakObjectPtr<AActor> SelectedActor;

    // кэш для депроекции курсора (заполняется в Draw из активного вида)
    FMatrix  CachedInvView = FMatrix::Identity;
    FMatrix  CachedInvProj = FMatrix::Identity;
    FMatrix  CachedViewProj = FMatrix::Identity; // мир->экран (для гизмо)
    FIntRect CachedViewRect = FIntRect(0,0,1,1);
    bool     bViewCached = false;

    // --- 3D-гизмо перемещения (цветные оси X/Y/Z) ---
    int32   GizmoAxis = -1;                 // активная ось при тяге: 0=X 1=Y 2=Z, -1 нет
    FVector GizmoBaseLoc = FVector::ZeroVector;
    float   GizmoStartT = 0.f;
    float   GizmoPrevAngle = 0.f;           // угол курсора вокруг центра (кольца вращения)
    FVector GizmoAxisWorld = FVector::ZAxisVector; // мировая ось вращения, зафиксированная при захвате
    void  DrawGizmo();
	void  DrawCompass(class FCanvas* Canvas, const FIntPoint& Size);
	void  DrawMarquee(class FCanvas* Canvas, const FIntPoint& Size); // рамка выделения
	void  SelectMarquee(class FViewport* Viewport);                 // выбрать детали в рамке
	void  ApplyHighlight();                                         // подсветка выделенных (золотой материал)
	TMap<TWeakObjectPtr<class UStaticMeshComponent>, TArray<class UMaterialInterface*>> HiSaved; // оригиналы материалов
	TArray<TWeakObjectPtr<AActor>> SelectedActors;                  // мульти-выбор
	bool bMarquee = false; FVector2D MarqStart = FVector2D::ZeroVector, MarqCur = FVector2D::ZeroVector;
	void  DrawCurvePreview();           // превью кривой (узлы + Catmull-сплайн) // оси X/Y/Z в углу (как nav-гизмо Blender)                                  // рисует оси у выбранной детали
    int32 PickGizmoAxis(FViewport* Viewport) const;     // какая ось под курсором (или -1)
    bool  DeprojectMouseRay(FViewport* Viewport, FVector& O, FVector& D) const;
    bool  WorldToPixel(const FVector& P, FVector2D& Out) const; // мир->экран через CachedViewProj

    void UpdateCamera(float DeltaSeconds);

    // Рисует грид-сетку как 3D-линии в сцене (с глубиной — детали её перекрывают).
    void DrawGrid();
    // Рамка вокруг выбранной детали (overlay).
    void DrawSelection(FCanvas* Canvas, const FSceneView* View);

    // навигация
    void DollyZoom(float Dir);                 // колесо: +1 приблизить, -1 отдалить
    void OrbitCamera(float DX, float DY);      // СКМ-драг: орбита вокруг FocusPoint
    void PanCamera(float DX, float DY);        // Alt+СКМ-драг: пан

    // выбор/манипуляция деталей
    UWorld* GetRenderWorld() const;            // мир, который реально рендерится (editor)
    bool PickPartUnderCursor(FViewport* Viewport); // клик -> line trace -> SelectedActor
    bool DeprojectMouseToGround(FViewport* Viewport, FVector& OutGround) const; // курсор -> точка на Z=0
    void TransformSelected(const FRotator& DRot, const FVector& DMove, float ScaleMul);
    void GatherSelection(TArray<AActor*>& Out) const;

    // --- снап к деталям ---
    // Пытается прилепить призрак к ближайшей совместимой точке стыковки соседней детали.
    // Cursor — точка на земле под курсором. true — призрак установлен (трансформ задан).
    bool TrySnapGhost(const FVector& Cursor);
    // Отладочная отрисовка кадров точек стыковки соседних деталей и призрака.
    void DrawSnapDebug();
    void MoveSelectionTo(AActor* Lead, const FVector& NewLoc);
    void RotateSelection(const FQuat& Q, const FVector& Pivot);
};
