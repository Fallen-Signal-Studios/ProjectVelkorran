// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovHolographicHUDLayout.h"
#include "UI/SovThreatCueLayout.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHolographicHUDTextClearanceTest,
    "ProjectVelkorran.UI.HolographicHUD.TextAndWarningsStayClearOfTheHUD",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovHolographicHUDTextClearanceTest::RunTest(const FString& Parameters)
{
    using namespace SovHolographicHUDLayout;
    // Safe areas: full 1080p, an 80% title-safe television, a short ultrawide and 720p.
    const FVector2D SafeSizes[] = { FVector2D(1920, 1080), FVector2D(1536, 864), FVector2D(2560, 1080), FVector2D(1280, 720) };
    const float UIScales[] = { 1.f, 1.5f, 2.f };
    const float TextScales[] = { 1.f, 2.5f };
    for (const FVector2D& Size : SafeSizes)
    {
        for (const float UIScale : UIScales)
        {
            const FHUDGeometry Layout = Compute(Size, UIScale);
            const FString Case = FString::Printf(TEXT("%.0fx%.0f at UI scale %.1f"), Size.X, Size.Y, UIScale);
            const FBox2D Safe(FVector2D::ZeroVector, Size);
            for (const FBox2D& Region : Layout.Regions())
            {
                TestTrue(*(Case + TEXT(": every HUD readout lies inside the safe area")), Safe.IsInsideOrOn(Region.Min) && Safe.IsInsideOrOn(Region.Max));
            }
            for (const float TextScale : TextScales)
            {
                const FString TextCase = FString::Printf(TEXT("%s, text scale %.1f"), *Case, TextScale);
                const float Budget = TextHeightBudget(3, TextScale);

                // Subtitles, from both default anchors the presentation uses.
                for (const float Anchor : { .9f, .8f })
                {
                    const FSubtitlePlacement Placement = PlaceSubtitle(Layout, float(Size.Y) * Anchor, float(Size.X) * .84f, Budget);
                    const FBox2D Box(FVector2D((Size.X - Placement.MaximumWidth) * .5, FMath::Max(0., Placement.Bottom - Budget)),
                        FVector2D((Size.X + Placement.MaximumWidth) * .5, Placement.Bottom));
                    TestTrue(*(TextCase + TEXT(": the subtitle column keeps at least half the safe width")), Placement.MaximumWidth >= Size.X * .5 - 1.);
                    // A worst-case box can be taller than the space between the plate and the arc (720p, UI scale 2,
                    // text scale 2.5 leaves under 350 px for a 384 px box). It still never covers the lower readouts.
                    const bool bFits = Layout.Arc.Min.Y - 12. - Budget >= Layout.Plate.Max.Y + 12.;
                    for (const FBox2D& Region : Layout.Regions())
                    {
                        if (!bFits && (Region == Layout.Plate || Region == Layout.Ammo)) { continue; }
                        TestFalse(*(TextCase + TEXT(": no subtitle is drawn through a HUD readout")), Overlaps(Box, Region));
                    }
                    if (!bFits) { AddInfo(TextCase + TEXT(": a worst-case subtitle cannot fit between the plate and the arc; it stays clear of the arc and radar")); }
                }

                // Captions, with and without the ammo readout.
                for (const bool bAmmo : { true, false })
                {
                    const float Width = float(Size.X) * .8f;
                    const float Top = PlaceCaptionTop(Layout, float(Size.Y) * .13f, Width, bAmmo);
                    const FBox2D Box(FVector2D((Size.X - Width) * .5, Top), FVector2D((Size.X + Width) * .5, Top + Budget));
                    TestFalse(*(TextCase + TEXT(": a caption never sits under the identity plate")), Overlaps(Box, Layout.Plate));
                    if (bAmmo) { TestFalse(*(TextCase + TEXT(": a caption never sits under the ammo readout")), Overlaps(Box, Layout.Ammo)); }
                }
            }

            // The objective panel stops short of the plate.
            const float ObjectiveRight = 12.f + TopLeftPanelMaximumWidth(Layout, 12.f);
            TestTrue(*(Case + TEXT(": the objective panel stops short of the identity plate")), ObjectiveRight < Layout.Plate.Min.X);

            // Off-screen warnings avoid the HUD whenever the viewport has room for them.
            TArray<FBox2D> Panels = Layout.Regions();
            for (uint8 Side = 0; Side < 4; ++Side)
            {
                FVector2D Position, CardSize; float CardScale;
                if (!SovThreatCueLayout::Place(Size, UIScale, static_cast<ESovThreatCueSide>(Side), Position, CardSize, CardScale)) { continue; }
                if (!SovThreatCueLayout::AvoidPanels(Size, static_cast<ESovThreatCueSide>(Side), Panels, CardSize, Position)) { continue; }
                const FBox2D Card(Position, Position + CardSize);
                for (const FBox2D& Region : Layout.Regions())
                {
                    TestFalse(*FString::Printf(TEXT("%s: warning %d is not drawn over a HUD readout"), *Case, Side), Overlaps(Card, Region));
                }
            }
        }
    }
    return true;
}
#endif
