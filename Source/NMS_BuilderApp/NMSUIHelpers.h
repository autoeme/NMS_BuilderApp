// Мелкие UI-фабрики без состояния (зависят от темы NMSStyle.h).
// Вынесено из SNMSBuilderUI.cpp без изменения поведения.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

// Строка-секция как в меню игры (светлая полупрозрачная полоса, текст слева).
TSharedRef<SWidget> MakeSectionHeader(const FText& Title);

// Маленькое поле со значением (числовой инсет).
TSharedRef<SWidget> MakeField(const FText& Value);

// Строка трансформа: метка + 1 или 3 поля.
TSharedRef<SWidget> MakeTransformRow(const FText& Label, bool bSingle);

// Строка «свойство: значение».
TSharedRef<SWidget> MakePropertyRow(const FText& Label, const FText& Value);
