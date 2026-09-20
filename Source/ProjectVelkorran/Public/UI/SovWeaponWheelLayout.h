// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UI/SovWorldLabelLayout.h"

namespace SovWeaponWheelLayout
{
    inline bool IsClear(const FBox2D& Wheel, const FBox2D& Safe, const TArray<FBox2D>& Panels, double Gap = 12.)
    {
        if (!Wheel.bIsValid || !Safe.bIsValid || !Safe.IsInsideOrOn(Wheel.Min) || !Safe.IsInsideOrOn(Wheel.Max)) { return false; }
        for (const FBox2D& Panel : Panels)
        {
            if (Panel.bIsValid && Wheel.Min.X < Panel.Max.X+Gap && Wheel.Max.X > Panel.Min.X-Gap
                && Wheel.Min.Y < Panel.Max.Y+Gap && Wheel.Max.Y > Panel.Min.Y-Gap) { return false; }
        }
        return true;
    }

    /** Prefer the authored size, then the largest available square. Never move an already clear wheel
     * during selection merely because a subtitle disappears. Reset Previous on menu activation. */
    inline bool Place(const FBox2D& Safe, const TArray<FBox2D>& Panels, double PreferredSide,
        const FBox2D& Previous, FBox2D& Out, double Gap = 12.)
    {
        if (!Safe.bIsValid || PreferredSide <= 0. || !FMath::IsFinite(PreferredSide)
            || Safe.Min.ContainsNaN() || Safe.Max.ContainsNaN()) { return false; }
        if (IsClear(Previous, Safe, Panels, Gap)) { Out = Previous; return true; }
        const double Maximum = FMath::Min3(PreferredSide, Safe.GetSize().X, Safe.GetSize().Y);
        if (Maximum < 1.) { return false; }
        auto Fit = [&](double Side, FBox2D& Result)
        {
            FVector2D Position;
            const FVector2D Extent(Side,Side);
            if (!SovWorldLabelLayout::Place(Safe.GetCenter()-Extent*.5, Extent, Safe, Panels, Position, Gap)) { return false; }
            Result = FBox2D(Position,Position+Extent); return true;
        };
        if (Fit(Maximum, Out)) { return true; }
        FBox2D Best;
        if (!Fit(1., Best)) { return false; }
        double Low=1., High=Maximum;
        for (int32 Iteration=0; Iteration<14; ++Iteration)
        {
            const double Mid=(Low+High)*.5;
            FBox2D Candidate;
            if (Fit(Mid,Candidate)) { Low=Mid; Best=Candidate; }
            else { High=Mid; }
        }
        Out=Best; return true;
    }
}
