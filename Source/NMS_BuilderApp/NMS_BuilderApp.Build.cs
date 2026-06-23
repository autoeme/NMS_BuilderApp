// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NMS_BuilderApp : ModuleRules
{
	public NMS_BuilderApp(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"Json", "JsonUtilities"
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

		// zstd (только распаковка, исходники v1.4.9 в ThirdParty/zstdlib) —
		// чтение .blend напрямую, без установленного Blender.
		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "ThirdParty/zstdlib"));
		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "ThirdParty/zstdlib/common"));
		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "ThirdParty/zstdlib/decompress"));
		PrivateDefinitions.Add("XXH_NAMESPACE=NMSZ_"); // не конфликтуем с xxhash движка
		PrivateDefinitions.Add("ZSTD_DISABLE_ASM=1");  // без .S-файла (MSVC он и не нужен)
	}
}
