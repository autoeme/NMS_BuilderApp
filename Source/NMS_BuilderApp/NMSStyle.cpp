#include "NMSStyle.h"

namespace NMS
{
    const FButtonStyle& FlatButton(const FLinearColor& Normal,
                                   const FLinearColor& Hover,
                                   const FLinearColor& Pressed)
    {
        static TArray<TSharedPtr<FButtonStyle>> Cache;
        TSharedPtr<FButtonStyle> S = MakeShared<FButtonStyle>();
        S->SetNormal(FSlateColorBrush(Normal));
        S->SetHovered(FSlateColorBrush(Hover));
        S->SetPressed(FSlateColorBrush(Pressed));
        S->SetNormalPadding(FMargin(0));
        S->SetPressedPadding(FMargin(0));
        Cache.Add(S);
        return *Cache.Last();
    }
}
