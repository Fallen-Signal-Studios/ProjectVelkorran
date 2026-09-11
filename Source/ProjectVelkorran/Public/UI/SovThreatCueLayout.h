// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

enum class ESovThreatCueSide : uint8 { Ahead, Right, Behind, Left };

/** Pure presentation geometry. This never authorizes an attack or queries a hidden target. */
namespace SovThreatCueLayout
{
    inline bool Classify(const FVector& CameraForward, const FVector& CameraRight,
        const FVector& ToAttacker, ESovThreatCueSide& Out)
    {
        if (CameraForward.ContainsNaN() || CameraRight.ContainsNaN() || ToAttacker.ContainsNaN()
            || ToAttacker.IsNearlyZero()) { return false; }
        const FVector Direction = ToAttacker.GetSafeNormal();
        const double Front = FVector::DotProduct(CameraForward, Direction);
        const double Right = FVector::DotProduct(CameraRight, Direction);
        if (!FMath::IsFinite(Front) || !FMath::IsFinite(Right)
            || (FMath::Abs(Front) < UE_SMALL_NUMBER && FMath::Abs(Right) < UE_SMALL_NUMBER)) { return false; }
        Out = FMath::Abs(Front) >= FMath::Abs(Right)
            ? (Front >= 0 ? ESovThreatCueSide::Ahead : ESovThreatCueSide::Behind)
            : (Right >= 0 ? ESovThreatCueSide::Right : ESovThreatCueSide::Left);
        return true;
    }

    inline bool Place(const FVector2D ViewSize, const float RequestedScale, const ESovThreatCueSide Side,
        FVector2D& OutPosition, FVector2D& OutSize, float& OutScale)
    {
        OutPosition = OutSize = FVector2D::ZeroVector; OutScale = 1.f;
        if (ViewSize.ContainsNaN() || ViewSize.X < 480 || ViewSize.Y < 320
            || static_cast<uint8>(Side) > static_cast<uint8>(ESovThreatCueSide::Left)) { return false; }
        // Four reserved regions inside an 80% safe area; no scrolling stack can cover the reticle.
        const float UserScale = FMath::IsFinite(RequestedScale) ? FMath::Clamp(RequestedScale, .75f, 2.f) : 1.f;
        const float WidthScale = static_cast<float>(ViewSize.X * .36 / 280.);
        // Side panels and the rear panel must retain a 12px gap even on short, wide windows.
        const float HeightScale = static_cast<float>((ViewSize.Y * .24 - 12.) / 99.);
        OutScale = FMath::Min3(UserScale, WidthScale, HeightScale);
        OutSize = FVector2D(280.f * OutScale, 66.f * OutScale);
        const FVector2D Inset = ViewSize * .1;
        switch (Side)
        {
        case ESovThreatCueSide::Ahead: OutPosition = FVector2D((ViewSize.X - OutSize.X) * .5, Inset.Y); break;
        case ESovThreatCueSide::Behind: OutPosition = FVector2D((ViewSize.X - OutSize.X) * .5, ViewSize.Y * .74 - OutSize.Y); break;
        case ESovThreatCueSide::Right: OutPosition = FVector2D(ViewSize.X - Inset.X - OutSize.X, (ViewSize.Y - OutSize.Y) * .5); break;
        case ESovThreatCueSide::Left: OutPosition = FVector2D(Inset.X, (ViewSize.Y - OutSize.Y) * .5); break;
        }
        return true;
    }

    inline bool FullyInside(const FVector2D Min, const FVector2D Max, const FVector2D ClipMin, const FVector2D ClipMax)
    {
        return !Min.ContainsNaN() && !Max.ContainsNaN() && !ClipMin.ContainsNaN() && !ClipMax.ContainsNaN()
            && Min.X < Max.X && Min.Y < Max.Y && ClipMin.X <= Min.X && ClipMin.Y <= Min.Y
            && ClipMax.X >= Max.X && ClipMax.Y >= Max.Y;
    }

    inline bool OverlapsPanel(const FVector2D Position, const FVector2D Size, const FBox2D& Panel, const double Gap = 12.)
    {
        return Panel.bIsValid && !Panel.Min.ContainsNaN() && !Panel.Max.ContainsNaN()
            && Position.X < Panel.Max.X + Gap && Position.X + Size.X > Panel.Min.X - Gap
            && Position.Y < Panel.Max.Y + Gap && Position.Y + Size.Y > Panel.Min.Y - Gap;
    }

    /** A finite placement search in the same directional half of the local viewport.
     *  Only displayed panel rectangles and already placed warnings are inputs. Size, font,
     *  direction and native cue lifetime never change. No fit leaves the original position intact.
     */
    inline bool AvoidPanels(const FVector2D ViewSize, const ESovThreatCueSide Side,
        const TArray<FBox2D>& Panels, const FVector2D Size, FVector2D& Position)
    {
        if (ViewSize.ContainsNaN() || Size.ContainsNaN() || Position.ContainsNaN()
            || Size.X <= 0 || Size.Y <= 0 || Panels.Num() > 9
            || static_cast<uint8>(Side) > static_cast<uint8>(ESovThreatCueSide::Left)) { return false; }
        const FVector2D SafeMin = ViewSize * .1;
        const FVector2D SafeMax = ViewSize * .9 - Size;
        if (SafeMax.X < SafeMin.X || SafeMax.Y < SafeMin.Y) { return false; }
        const auto Admitted = [&](const FVector2D Candidate)
        {
            if (!FullyInside(Candidate, Candidate + Size, SafeMin, ViewSize * .9)) { return false; }
            const FVector2D Center = Candidate + Size * .5;
            if ((Side == ESovThreatCueSide::Right && Center.X <= ViewSize.X * .5)
                || (Side == ESovThreatCueSide::Left && Center.X >= ViewSize.X * .5)
                || (Side == ESovThreatCueSide::Ahead && Center.Y >= ViewSize.Y * .5)
                || (Side == ESovThreatCueSide::Behind && Center.Y <= ViewSize.Y * .5)) { return false; }
            // Keep the central aim area clear even when a large panel displaces a cue.
            if (OverlapsPanel(Candidate, Size, FBox2D(ViewSize * FVector2D(.46,.43), ViewSize * FVector2D(.54,.57)), 0.)) { return false; }
            for (const auto& Panel : Panels) { if (OverlapsPanel(Candidate, Size, Panel)) { return false; } }
            return true;
        };
        if (Admitted(Position)) { return true; }
        TArray<double> Xs{Position.X, SafeMin.X, SafeMax.X};
        TArray<double> Ys{Position.Y, SafeMin.Y, SafeMax.Y};
        for (const auto& Panel : Panels)
        {
            if (!Panel.bIsValid || Panel.Min.ContainsNaN() || Panel.Max.ContainsNaN()) { continue; }
            Xs.AddUnique(FMath::Clamp(Panel.Min.X - Size.X - 12., SafeMin.X, SafeMax.X));
            Xs.AddUnique(FMath::Clamp(Panel.Max.X + 12., SafeMin.X, SafeMax.X));
            Ys.AddUnique(FMath::Clamp(Panel.Min.Y - Size.Y - 12., SafeMin.Y, SafeMax.Y));
            Ys.AddUnique(FMath::Clamp(Panel.Max.Y + 12., SafeMin.Y, SafeMax.Y));
        }
        bool bFound = false;
        double BestDistance = TNumericLimits<double>::Max();
        FVector2D Best = Position;
        // At most 21x21 candidates against nine current rectangles, with no world queries.
        for (const double X : Xs) { for (const double Y : Ys)
        {
            const FVector2D Candidate(X,Y);
            const double Distance = FVector2D::DistSquared(Position, Candidate);
            if (Distance < BestDistance && Admitted(Candidate))
            { Best = Candidate; BestDistance = Distance; bFound = true; }
        } }
        if (bFound) { Position = Best; }
        return bFound;
    }
}
