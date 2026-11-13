#pragma once
#include "CoreMinimal.h"
/** Session-local presentation state. Never changes the saved model or generation eligibility. */
struct FCEBiomePreviewState
{
    TSet<FGuid> Hidden,BeforeSolo;
    FGuid Solo;
    void SetEye(FGuid Id,bool Visible){if(Visible)Hidden.Remove(Id);else Hidden.Add(Id);}
    void SetSolo(FGuid Id)
    {
        if(!Solo.IsValid()&&Id.IsValid())BeforeSolo=Hidden;
        if(Solo.IsValid()&&!Id.IsValid()){Hidden=BeforeSolo;BeforeSolo.Reset();}
        Solo=Id;
    }
    bool Visible(FGuid Id)const{return !Hidden.Contains(Id)&&(!Solo.IsValid()||Solo==Id);}
};
