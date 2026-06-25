// Тема и стилевые билдеры для UI билдера NMS (header-only + FlatButton в .cpp).
// Вынесено из SNMSBuilderUI.cpp без изменения поведения.
#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SBorder.h"

// ===========================================================================
//  Цветовая палитра — как меню игры NMS: дымчатый тёмный фон без синевы,
//  полупрозрачные белёсые строки, белый текст.
// ===========================================================================
namespace NMS
{
    // inline const — единое определение на всё приложение (C++17, без ODR-проблем).
    inline const FLinearColor WindowBg     = FLinearColor(FColor(0x10, 0x14, 0x18));
    inline const FLinearColor PanelBg       = FLinearColor(0.07f, 0.08f, 0.09f, 0.80f);
    inline const FLinearColor PanelHeader   = FLinearColor(1.f, 1.f, 1.f, 0.14f);  // светлая строка, как в меню игры
    inline const FLinearColor InsetBg       = FLinearColor(0.f, 0.f, 0.f, 0.35f);
    inline const FLinearColor ItemBg        = FLinearColor(1.f, 1.f, 1.f, 0.08f);
    inline const FLinearColor ItemHover     = FLinearColor(1.f, 1.f, 1.f, 0.22f);
    inline const FLinearColor CardBg        = FLinearColor(1.f, 1.f, 1.f, 0.10f);
    inline const FLinearColor TabActive     = FLinearColor(1.f, 1.f, 1.f, 0.25f);
    inline const FLinearColor TabInactive   = FLinearColor(1.f, 1.f, 1.f, 0.06f);
    inline const FLinearColor Accent        = FLinearColor(FColor(0xF0, 0xF2, 0xF4)); // белый акцент, как в игре
    inline const FLinearColor AccentDim     = FLinearColor(1.f, 1.f, 1.f, 0.30f);
    inline const FLinearColor ViewportBlue  = FLinearColor(FColor(0x3A, 0x40, 0x46));
    inline const FLinearColor Border        = FLinearColor(1.f, 1.f, 1.f, 0.18f);
    inline const FLinearColor TextPrimary   = FLinearColor(FColor(0xF2, 0xF4, 0xF6));
    inline const FLinearColor TextSecondary = FLinearColor(FColor(0xA8, 0xAE, 0xB4));
    inline const FLinearColor TextCategory  = FLinearColor(FColor(0xD2, 0xD6, 0xDA));
    inline const FLinearColor Gold          = FLinearColor(FColor(0xE8, 0xC7, 0x5A));

    inline const FSlateBrush* Box()
    {
        return FCoreStyle::Get().GetBrush("GenericWhiteBox");
    }

    inline FSlateFontInfo Font(int32 Size, bool bBold = false)
    {
        return FCoreStyle::GetDefaultFontStyle("Bold", Size); // весь текст жирнее для чёткости
    }

    // Плоский стиль кнопки с заданными цветами (без штатной рамки движка).
    // Тело в NMSStyle.cpp — внутренний static Cache должен быть один на всё
    // приложение (а не по копии на каждый TU).
    const FButtonStyle& FlatButton(const FLinearColor& Normal,
                                   const FLinearColor& Hover,
                                   const FLinearColor& Pressed);

    inline TSharedRef<SWidget> Fill(const FLinearColor& Color)
    {
        return SNew(SBorder).BorderImage(Box()).BorderBackgroundColor(Color);
    }
}
