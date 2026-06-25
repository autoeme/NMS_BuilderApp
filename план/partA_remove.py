#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Part A: вырезать из SNMSBuilderUI.cpp вынесенный stateless-код
(i18n + namespace NMS + part visuals + UI helpers + NMS_ColourFromIndex
+ NMS_DefaultPartColor). Тела уже перенесены в отдельные файлы.
Работает по строковым маркерам, CRLF сохраняется."""
import io

SRC = r"C:\Users\User\Documents\Unreal Projects\NMS_BuilderApp\Source\NMS_BuilderApp\SNMSBuilderUI.cpp"

with io.open(SRC, "r", encoding="utf-8", newline="") as f:
    text = f.read()

def cut(text, start_marker, end_marker, keep_end=True, expect=1):
    """Удаляет от start_marker до end_marker. keep_end=True -> end_marker остаётся."""
    i = text.find(start_marker)
    assert i != -1, f"start not found: {start_marker[:50]!r}"
    j = text.find(end_marker, i + len(start_marker))
    assert j != -1, f"end not found: {end_marker[:50]!r}"
    if keep_end:
        return text[:i] + text[j:]
    else:
        return text[:i] + text[j + len(end_marker):]

before = len(text)

# 1) Большой непрерывный блок: i18n + namespace NMS + part visuals + UI helpers.
#    Удаляем от начала локализации до комментария перед LoadPalettes (он остаётся).
text = cut(text,
           "// ===================== ЛОКАЛИЗАЦИЯ (17 языков) =====================",
           "// Читает настоящие игровые палитры")

# 2) NMS_ColourFromIndex -> до SpawnFromManager (он остаётся).
text = cut(text,
           "static bool NMS_ColourFromIndex(int32 Index, FLinearColor& OutP, FLinearColor& OutS)",
           "void SNMSBuilderUI::SpawnFromManager")

# 3) NMS_DefaultPartColor (определение) -> до OnPartSelected (он остаётся).
text = cut(text,
           "static FLinearColor NMS_DefaultPartColor(const FString& Category, const FString& ObjectID)\r\n{",
           "void SNMSBuilderUI::OnPartSelected")

after = len(text)
print(f"removed chars: {before - after}")

# sanity: ОПРЕДЕЛЕНИЯ вынесенного кода не должны остаться (вызовы — можно).
for m in ["GNmsI18nLoaded", "static void NMS_LoadI18n", "namespace NMS\r\n{",
          "static bool NMS_GamePartColors", "struct FNMSTexSet {",
          "static const TArray<FNMSTexSet>* NMS_PartSlots",
          "static TSharedRef<SWidget> MakeSectionHeader",
          "static bool NMS_ColourFromIndex(int32",
          "static FLinearColor NMS_DefaultPartColor(const FString& Category, const FString& ObjectID)\r\n{"]:
    assert m not in text, f"STILL PRESENT: {m}"

# нужные обращения должны остаться
for m in ["void SNMSBuilderUI::LoadPalettes()", "void SNMSBuilderUI::SpawnFromManager",
          "void SNMSBuilderUI::OnPartSelected"]:
    assert m in text, f"MISSING: {m}"

with io.open(SRC, "w", encoding="utf-8", newline="") as f:
    f.write(text)
print("ok")
