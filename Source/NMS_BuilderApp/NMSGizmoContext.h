#pragma once

// Реализация ToolsContext API (минимум) для движкового гизмо UCombinedTransformGizmo (ITF)
// внутри нашего кастомного вьюпорта. Этап 1: появление/рендер гизмо.

#include "CoreMinimal.h"
#include "ToolContextInterfaces.h"
#include "InteractiveToolsContext.h"   // ToolManager/GizmoManager для builder-state

// --- Запросы контекста: камера/мир/вид ---
class FNMSToolsContextQueries : public IToolsContextQueriesAPI
{
public:
    FNMSToolsContextQueries(UWorld* InWorld, FViewport* InViewport)
        : World(InWorld), Viewport(InViewport) {}

    virtual UWorld* GetCurrentEditingWorld() const override { return World; }

    virtual void GetCurrentSelectionState(FToolBuilderState& StateOut) const override
    {
        // ВАЖНО: CreateGizmo строит builder-state ЦЕЛИКОМ отсюда — обязаны отдать менеджеры,
        // иначе BuildGizmo делает NewObject(nullptr) -> краш «Object is not packaged».
        StateOut.World = World;
        if (Context)
        {
            StateOut.ToolManager  = Context->ToolManager;
            StateOut.GizmoManager = Context->GizmoManager;
        }
    }

    virtual void GetCurrentViewState(FViewCameraState& StateOut) const override
    {
        StateOut = CamState;      // обновляется вьюпортом каждый кадр
    }

    virtual EToolContextCoordinateSystem GetCurrentCoordinateSystem() const override
    {
        return EToolContextCoordinateSystem::World;
    }

    virtual UMaterialInterface* GetStandardMaterial(EStandardToolContextMaterials /*MaterialType*/) const override
    {
        return nullptr; // гизмо использует свои материалы
    }

    virtual FViewport* GetHoveredViewport() const override { return Viewport; }
    virtual FViewport* GetFocusedViewport() const override { return Viewport; }

    UWorld* World = nullptr;
    FViewport* Viewport = nullptr;
    class UInteractiveToolsContext* Context = nullptr; // для ToolManager/GizmoManager
    FViewCameraState CamState; // выставляет FNMSViewportClient::Draw каждый кадр
};

// --- Транзакции/undo: для гизмо достаточно заглушек ---
class FNMSToolsContextTransactions : public IToolsContextTransactionsAPI
{
public:
    virtual void DisplayMessage(const FText& /*Message*/, EToolMessageLevel /*Level*/) override {}
    virtual void PostInvalidation() override {}
    virtual void BeginUndoTransaction(const FText& /*Description*/) override {}
    virtual void EndUndoTransaction() override {}
    virtual void CancelUndoTransaction() override {}
    virtual void AppendChange(UObject* /*TargetObject*/, TUniquePtr<FToolCommandChange> /*Change*/, const FText& /*Description*/) override {}
    virtual bool RequestSelectionChange(const FSelectedObjectsChangeList& /*SelectionChange*/) override { return false; }
};
