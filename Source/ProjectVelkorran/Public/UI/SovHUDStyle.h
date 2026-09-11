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
        // Neutral graphite keeps the scene readable; faction identity stays in the edge and label.
        FLinearColor Background = FLinearColor(.008f, .011f, .016f, .86f);
        EFrame Frame = EFrame::Neutral;
        FText Identity;
    };
    PROJECTVELKORRAN_API FTheme ForProtagonist(FGameplayTag Identity, bool bHighContrast);
    /** Stable string-table IDs, shared by the resource and objective surfaces. */
    PROJECTVELKORRAN_API FText Text(const TCHAR* Key);
}
