// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovHUDStyle.h"
#include "Internationalization/StringTableRegistry.h"
#include "Sovereign/SovGameplayTags.h"

FText SovHUDStyle::Text(const TCHAR* Key)
{
    static const bool bRegistered = []()
    {
        LOCTABLE_NEW("SovHUD", "Sovereign.HUD");
        LOCTABLE_SETSTRING("SovHUD", "Identity.Tarrik", "TARRIK / DOMINION");
        LOCTABLE_SETSTRING("SovHUD", "Identity.Selene", "SELENE / REFORMATION");
        LOCTABLE_SETSTRING("SovHUD", "Identity.Unknown", "VITALS");
        LOCTABLE_SETSTRING("SovHUD", "Echo", "ECHO / RESONANCE");
        LOCTABLE_SETSTRING("SovHUD", "Waypoint.Interaction", "Objective / {0} m");
        LOCTABLE_SETSTRING("SovHUD", "Waypoint.Retry", "Encounter failed / Retry / {0} m");
        LOCTABLE_SETSTRING("SovHUD", "Waypoint.Encounter", "Objective area / {0} m");
        LOCTABLE_SETSTRING("SovHUD", "Waypoint.Receiver", "Receiver / {0} m");
        return true;
    }();
    (void)bRegistered;
    return FText::FromStringTable(TEXT("SovHUD"), Key);
}

SovHUDStyle::FTheme SovHUDStyle::ForProtagonist(FGameplayTag Identity, bool bHighContrast)
{
    FTheme Theme;
    const auto& Tags = FSovGameplayTags::Get();
    if (Identity == Tags.Character_Player_Tarrik)
    {
        Theme.Accent = FLinearColor(1.f, .62f, .22f);
        Theme.Frame = EFrame::Shield;
        Theme.Identity = Text(TEXT("Identity.Tarrik"));
    }
    else if (Identity == Tags.Character_Player_Selene)
    {
        Theme.Accent = FLinearColor(.62f, .87f, 1.f);
        Theme.Frame = EFrame::Split;
        Theme.Identity = Text(TEXT("Identity.Selene"));
    }
    else { Theme.Identity = Text(TEXT("Identity.Unknown")); }
    if (bHighContrast) { Theme.Accent = FLinearColor::White; Theme.Background = FLinearColor::Black; }
    return Theme;
}
