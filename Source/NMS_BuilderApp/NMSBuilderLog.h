// Категории логирования редакторного модуля NMS_BuilderApp.
// Используем их вместо LogTemp (требование CLAUDE.md). Определения — в NMS_BuilderApp.cpp.
#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNMSBuilderEditor, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogNMSBuilderScene,  Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogNMSBuilderSave,   Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogNMSBuilderImport, Log, All);
