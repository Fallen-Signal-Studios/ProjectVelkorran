// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

namespace SovWorldLabelLayout
{
    /** Nearest complete label placement outside displayed text panels. Coordinates share paint space.
     *  No fit omits the secondary label; the caller retains the world/bearing glyph. */
    inline bool Place(const FVector2D Desired, const FVector2D Size, const FBox2D& Safe,
        const TArray<FBox2D>& Panels, FVector2D& Out, const double Gap = 8.)
    {
        if (!Safe.bIsValid || Desired.ContainsNaN() || Size.ContainsNaN()
            || Size.X <= 0 || Size.Y <= 0 || Safe.GetSize().X < Size.X || Safe.GetSize().Y < Size.Y)
        { return false; }
        const auto Clamp = [&](FVector2D P)
        {
            return FVector2D(FMath::Clamp(P.X, Safe.Min.X, Safe.Max.X - Size.X),
                FMath::Clamp(P.Y, Safe.Min.Y, Safe.Max.Y - Size.Y));
        };
        const FVector2D Start = Clamp(Desired);
        TArray<double> Xs = {Start.X, Safe.Min.X, Safe.Max.X - Size.X};
        TArray<double> Ys = {Start.Y, Safe.Min.Y, Safe.Max.Y - Size.Y};
        for (const FBox2D& Panel : Panels)
        {
            if (!Panel.bIsValid) { continue; }
            Xs.Add(Panel.Min.X - Gap - Size.X); Xs.Add(Panel.Max.X + Gap);
            Ys.Add(Panel.Min.Y - Gap - Size.Y); Ys.Add(Panel.Max.Y + Gap);
        }
        bool Found = false;
        double Best = TNumericLimits<double>::Max();
        for (const double X : Xs)
        {
            for (const double Y : Ys)
            {
                const FVector2D P = Clamp(FVector2D(X, Y));
                bool Clear = true;
                for (const FBox2D& Panel : Panels)
                {
                    if (Panel.bIsValid && P.X < Panel.Max.X + Gap && P.X + Size.X > Panel.Min.X - Gap
                        && P.Y < Panel.Max.Y + Gap && P.Y + Size.Y > Panel.Min.Y - Gap)
                    { Clear = false; break; }
                }
                const double Distance = FVector2D::DistSquared(P, Start);
                if (Clear && Distance < Best) { Found = true; Best = Distance; Out = P; }
            }
        }
        return Found;
    }
}
