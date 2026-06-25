// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

// Доменное/парсинг-ядро билдера NMS: модель базы, цвета/локализация, чтение
// .hg/.blend, JSON import/export. СОЗНАТЕЛЬНО без Slate / UnrealEd / DesktopPlatform —
// это правило архитектуры (см. docs/). Редакторный модуль зависит от этого.
public class NMS_BuilderCore : ModuleRules
{
	public NMS_BuilderCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine",
			"Json", "JsonUtilities"
		});

		// zstd (только распаковка) — чтение .blend напрямую, без Blender.
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty/zstdlib"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty/zstdlib/common"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty/zstdlib/decompress"));
		PrivateDefinitions.Add("XXH_NAMESPACE=NMSZ_"); // не конфликтуем с xxhash движка
		PrivateDefinitions.Add("ZSTD_DISABLE_ASM=1");  // без .S-файла (MSVC он и не нужен)
	}
}
