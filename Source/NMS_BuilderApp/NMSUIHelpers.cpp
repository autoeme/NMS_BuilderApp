#include "NMSUIHelpers.h"
#include "NMSStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> MakeSectionHeader(const FText& Title)
{
    // строка-секция как в меню игры: светлая полупрозрачная полоса, текст слева
    return SNew(SBorder)
        .BorderImage(NMS::Box())
        .BorderBackgroundColor(NMS::PanelHeader)
        .Padding(FMargin(10.f, 6.f))
        .HAlign(HAlign_Left)
        [
            SNew(STextBlock)
            .Text(Title)
            .Font(NMS::Font(10, true))
            .ColorAndOpacity(NMS::TextPrimary)
        ];
}

TSharedRef<SWidget> MakeField(const FText& Value)
{
    return SNew(SBox).WidthOverride(34.f)
    [
        SNew(SBorder)
        .BorderImage(NMS::Box())
        .BorderBackgroundColor(NMS::InsetBg)
        .Padding(FMargin(6.f, 4.f))
        [
            SNew(STextBlock).Text(Value)
            .Font(NMS::Font(9)).ColorAndOpacity(NMS::TextSecondary)
        ]
    ];
}

TSharedRef<SWidget> MakeTransformRow(const FText& Label, bool bSingle)
{
    TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 4, 0))
        [
            SNew(SBox).WidthOverride(46.f)
            [
                SNew(STextBlock).Text(Label)
                .Font(NMS::Font(8)).ColorAndOpacity(NMS::TextSecondary)
            ]
        ];

    const int32 Count = bSingle ? 1 : 3;
    for (int32 i = 0; i < Count; ++i)
    {
        Row->AddSlot().AutoWidth().Padding(FMargin(0, 0, 4, 0))
        [
            MakeField(bSingle ? FText::FromString(TEXT("1.0")) : FText::FromString(TEXT("0.0")))
        ];
    }
    return Row;
}

TSharedRef<SWidget> MakePropertyRow(const FText& Label, const FText& Value)
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
        [
            SNew(STextBlock).Text(Label)
            .Font(NMS::Font(9)).ColorAndOpacity(NMS::TextSecondary)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock).Text(Value)
            .Font(NMS::Font(9)).ColorAndOpacity(NMS::TextPrimary)
        ];
}
