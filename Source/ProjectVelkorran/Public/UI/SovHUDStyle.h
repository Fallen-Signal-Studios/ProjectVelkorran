// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/** Shared presentation only. Identity comes from the current pawn, never its mesh or faction tint. */
namespace SovHUDStyle
{
    enum class EFrame : uint8 { Neutral, Shield, Split };
    struct FTheme
    {
        FLinearColor Accent = FLinearColor(.65f, .8f, .9f);
        // A light optical veil keeps the scene visible behind projected information.
        // High-contrast mode supplies its own opaque backing in ForProtagonist.
        FLinearColor Background = FLinearColor(.008f, .018f, .028f, .30f);
        EFrame Frame = EFrame::Neutral;
        FText Identity;
    };
    PROJECTVELKORRAN_API FTheme ForProtagonist(FGameplayTag Identity, bool bHighContrast);
    /** Stable string-table IDs, shared by the resource and objective surfaces. */
    PROJECTVELKORRAN_API FText Text(const TCHAR* Key);
}
