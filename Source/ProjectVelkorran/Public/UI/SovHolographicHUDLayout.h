// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

/**
 * Where the holographic HUD draws, as one pure description shared by the painter and by every text surface
 * that must stay clear of it. All coordinates are local to the safe area both the HUD and the subtitle canvas
 * paint in, so a rectangle computed here is the rectangle on screen for either of them.
 */
namespace SovHolographicHUDLayout
{
    struct FHUDGeometry
    {
        FVector2D Size = FVector2D::ZeroVector;
        float Scale = 1.f;
        FBox2D Plate = FBox2D(ForceInit);
        FBox2D Ammo = FBox2D(ForceInit);
        FVector2D ArcStart = FVector2D::ZeroVector, ArcControl = FVector2D::ZeroVector, ArcEnd = FVector2D::ZeroVector;
        FBox2D Arc = FBox2D(ForceInit);
        FVector2D RadarCentre = FVector2D::ZeroVector;
        float RadarRadius = 0.f;
        FBox2D Radar = FBox2D(ForceInit);

        FVector2D ArcPoint(float T) const
        {
            const float U = 1.f - T;
            return ArcStart * (U * U) + ArcControl * (2.f * U * T) + ArcEnd * (T * T);
        }
        /** Everything the HUD occupies apart from its full-screen edging. The ammo plate is absent without a ranged weapon. */
        TArray<FBox2D> Regions(bool bIncludeAmmo = true) const
        {
            TArray<FBox2D> Out;
            Out.Add(Plate);
            if (bIncludeAmmo) { Out.Add(Ammo); }
            Out.Add(Arc); Out.Add(Radar);
            return Out;
        }
    };

    inline float ClampScale(float Scale) { return FMath::IsFinite(Scale) ? FMath::Clamp(Scale, .75f, 2.f) : 1.f; }

    inline FHUDGeometry Compute(const FVector2D& SafeSize, float RequestedScale)
    {
        FHUDGeometry G;
        G.Size = FVector2D(FMath::Max(SafeSize.X, 1.), FMath::Max(SafeSize.Y, 1.));
        G.Scale = ClampScale(RequestedScale);
        const double W = G.Size.X, H = G.Size.Y, S = G.Scale;

        // Identity plate: vitals, name and the ability pips inside it.
        const double PlateWidth = FMath::Min(W * .44, 900. * S);
        const FVector2D PlateAt((W - PlateWidth) * .5, H * .022);
        G.Plate = FBox2D(PlateAt, PlateAt + FVector2D(PlateWidth, 96. * S));

        const FVector2D AmmoSize(232. * S, 72. * S);
        const FVector2D AmmoAt(W - AmmoSize.X - W * .035, H * .048);
        G.Ammo = FBox2D(AmmoAt, AmmoAt + AmmoSize);

        // The Echo arc: a quadratic sweep, its thickest stroke 16 px and a label below the middle.
        G.ArcStart = FVector2D(W * .175, H * .862);
        G.ArcControl = FVector2D(W * .560, H * .942);
        G.ArcEnd = FVector2D(W * .965, H * .800);
        FBox2D Curve(ForceInit);
        for (int32 Step = 0; Step <= 32; ++Step) { Curve += G.ArcPoint(Step / 32.f); }
        const FVector2D Middle = G.ArcPoint(.5f);
        G.Arc = FBox2D(Curve.Min - FVector2D(8. * S, 8. * S),
            FVector2D(Curve.Max.X + 8. * S, FMath::Max(Curve.Max.Y + 8. * S, Middle.Y + 30. * S)));

        // The radar disc with its cardinal ticks, which reach 1.17 radii.
        G.RadarRadius = static_cast<float>(FMath::Min(H * .145, 175. * S));
        G.RadarCentre = FVector2D(W * .060 + G.RadarRadius, H * .735);
        const double Reach = G.RadarRadius * 1.17 + 2.;
        G.Radar = FBox2D(G.RadarCentre - FVector2D(Reach, Reach), G.RadarCentre + FVector2D(Reach, Reach));
        return G;
    }

    inline bool Overlaps(const FBox2D& A, const FBox2D& B, double Gap = 0.)
    {
        return A.bIsValid && B.bIsValid && A.Min.X < B.Max.X + Gap && A.Max.X > B.Min.X - Gap
            && A.Min.Y < B.Max.Y + Gap && A.Max.Y > B.Min.Y - Gap;
    }

    /** The tallest text box a line limit and text scale can produce: its lines, a speaker line and the border padding. */
    inline float TextHeightBudget(int32 MaximumLines, float TextScale)
    { return (FMath::Max(MaximumLines, 1) + 1) * 26.f * FMath::Max(TextScale, .5f) * 1.4f + 20.f; }

    /** A centred subtitle box: where its bottom edge sits and how wide it may be, clear of the HUD. */
    struct FSubtitlePlacement { float Bottom = 0.f; float MaximumWidth = 0.f; };

    /**
     * DefaultBottom and DefaultWidth are the placement without a HUD; HeightBudget is the tallest box the current
     * text settings can produce. The box stays full width above the arc unless the radar then crowds it to less
     * than half the safe width, in which case it rises clear of the radar instead of shrinking further.
     */
    inline FSubtitlePlacement PlaceSubtitle(const FHUDGeometry& G, float DefaultBottom, float DefaultWidth, float HeightBudget, float Gap = 12.f)
    {
        FSubtitlePlacement Out;
        const float W = static_cast<float>(G.Size.X);
        Out.Bottom = FMath::Min(DefaultBottom, static_cast<float>(G.Arc.Min.Y) - Gap);
        Out.MaximumWidth = DefaultWidth;
        const auto BoxAt = [&](float Bottom, float Width)
        { return FBox2D(FVector2D((W - Width) * .5f, Bottom - HeightBudget), FVector2D((W + Width) * .5f, Bottom)); };
        if (!Overlaps(BoxAt(Out.Bottom, Out.MaximumWidth), G.Radar, Gap)) { return Out; }
        const float Narrowed = 2.f * (W * .5f - static_cast<float>(G.Radar.Max.X) - Gap);
        if (Narrowed >= W * .5f) { Out.MaximumWidth = FMath::Min(DefaultWidth, Narrowed); return Out; }
        Out.Bottom = FMath::Min(Out.Bottom, static_cast<float>(G.Radar.Min.Y) - Gap);
        return Out;
    }

    /** A centred caption box's top edge, below the plate and the ammo readout it would otherwise sit under. */
    inline float PlaceCaptionTop(const FHUDGeometry& G, float DefaultTop, float Width, bool bAmmoShown, float Gap = 12.f)
    {
        const float W = static_cast<float>(G.Size.X);
        float Top = DefaultTop;
        for (int32 Index = 0; Index < (bAmmoShown ? 2 : 1); ++Index)
        {
            const FBox2D& Region = Index == 0 ? G.Plate : G.Ammo;
            const bool bColumn = Region.Min.X < (W + Width) * .5f + Gap && Region.Max.X > (W - Width) * .5f - Gap;
            if (bColumn) { Top = FMath::Max(Top, static_cast<float>(Region.Max.Y) + Gap); }
        }
        return Top;
    }

    /** The widest a top-left panel starting at Inset may be before it reaches the plate. */
    inline float TopLeftPanelMaximumWidth(const FHUDGeometry& G, float Inset, float Gap = 12.f)
    { return FMath::Max(1.f, static_cast<float>(G.Plate.Min.X) - Gap - Inset); }
}
