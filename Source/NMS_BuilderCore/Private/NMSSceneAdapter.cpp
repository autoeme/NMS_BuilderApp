#include "NMSSceneAdapter.h"
#include "NMSPartVisuals.h"   // NMS_DefaultPartColor / NMS_DefaultPartColor2 / NMS_ColourFromIndex
#include "NMSLocalization.h"  // NMS_PalTQ

// Порт логики из SpawnFromManager (резолв цвета детали), но без сцены.
FNMSPartRenderInfo NMS_ResolvePartRender(const FNMSPlacedPart& Part, const FString& Category)
{
    FNMSPartRenderInfo R;
    R.ObjectID  = Part.ObjectID;
    R.Transform = Part.Transform;

    FLinearColor Col1 = NMS_DefaultPartColor(Category, Part.ObjectID);
    FLinearColor Col2 = NMS_DefaultPartColor2(Category, Part.ObjectID);
    // 4 зоны стартуют с дефолтного основного цвета (как в оригинале SpawnFromManager)
    FLinearColor ZA = Col1, ZB = Col1, ZC = Col1, ZD = Col1;

    // цвет из базы: декодируем UserData (NMS пакует индекс палитры в биты)
    const int32 ColourIndex = NMS_ColourIndexFromUserData(Part.UserData);
    FLinearColor PP, SS;
    if (ColourIndex != 0 && NMS_ColourFromIndex(ColourIndex, PP, SS))
    {
        Col1 = PP;
        Col2 = SS;
        ZA = PP;
        ZB = SS;
        FLinearColor TT = Col1, QQ = Col1;
        NMS_PalTQ(ColourIndex, TT, QQ);
        ZC = TT;
        ZD = QQ;
        R.bPaletteColor = true;
    }

    R.Color1 = Col1;
    R.Color2 = Col2;
    R.ZoneA = ZA;
    R.ZoneB = ZB;
    R.ZoneC = ZC;
    R.ZoneD = ZD;
    return R;
}
