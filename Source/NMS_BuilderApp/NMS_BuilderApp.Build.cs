// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NMS_BuilderApp : ModuleRules
{
	public NMS_BuilderApp(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"Json", "JsonUtilities",
			"NMS_BuilderCore"   // доменное/парсинг-ядро (модель, цвета, .hg/.blend, JSON)
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate", "SlateCore", "ApplicationCore",
			// для встроенного 3D-вьюпорта (свой FViewportClient + FSceneViewport)
			"RenderCore", "RHI", "Renderer",
			// рантайм-сборка UStaticMesh из OBJ/STL (FMeshDescription + StaticMeshAttributes)
			"MeshDescription", "StaticMeshDescription"
		});

		// Редакторная зависимость — ТОЛЬКО для editor-сборки.
		// Нужна для спавна боксов-превью в мир редактора (визуальная проверка
		// математики). При переходе на standalone этот путь будет заменён.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		// Файловый диалог выбора .json/.nmsbase. DesktopPlatform доступен и в
		// editor, и при необходимости в standalone (через target settings).
		PrivateDependencyModuleNames.Add("DesktopPlatform");

		// zstd и чтение .hg/.blend переехали в модуль NMS_BuilderCore.
	}
}
