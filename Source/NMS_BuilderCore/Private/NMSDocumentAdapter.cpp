#include "NMSDocumentAdapter.h"
#include "NMSBaseManager.h"

namespace NMSDoc
{
    FNMSBaseDocument FromManager(const UNMSBaseManager& Mgr)
    {
        FNMSBaseDocument Doc;
        Doc.Name = Mgr.BaseName;
        Doc.GalacticAddress = Mgr.GalacticAddress;
        Doc.Parts.Reserve(Mgr.PlacedObjects.Num());
        for (const FNMSPlacedObject& O : Mgr.PlacedObjects)
        {
            FNMSPlacedPart P;
            P.ObjectID  = O.ObjectID;
            P.Transform = O.Transform;
            P.UserData  = O.UserData;
            P.Timestamp = O.Timestamp;
            Doc.Parts.Add(MoveTemp(P));
        }
        return Doc;
    }

    void ToManager(const FNMSBaseDocument& Doc, UNMSBaseManager& Mgr)
    {
        Mgr.BaseName = Doc.Name;
        Mgr.GalacticAddress = Doc.GalacticAddress;
        Mgr.PlacedObjects.Reset(Doc.Parts.Num());
        for (const FNMSPlacedPart& P : Doc.Parts)
        {
            FNMSPlacedObject O;
            O.ObjectID  = P.ObjectID;
            O.Transform = P.Transform;
            O.UserData  = P.UserData;
            O.Timestamp = P.Timestamp;
            Mgr.PlacedObjects.Add(MoveTemp(O));
        }
    }
}
