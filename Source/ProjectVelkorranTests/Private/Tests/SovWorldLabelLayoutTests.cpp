// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovWorldLabelLayout.h"
#include "UI/SovThreatCueLayout.h"
#include "UI/SovHolographicHUDLayout.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWorldLabelClearanceTest,
    "ProjectVelkorran.UI.WorldLabels.ReserveVisibleSpeechAndKeepCompleteLabelsInsideSafeArea",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWorldLabelClearanceTest::RunTest(const FString& Parameters)
{
    for (const FVector2D View : {FVector2D(844,550), FVector2D(1280,720), FVector2D(1920,1080), FVector2D(2560,1080)})
    {
        // Nonzero origin represents a console safe-area inset in parent paint space.
        const FBox2D Safe(View * .05, View * .95);
        const TArray<FBox2D> Panels = {
            FBox2D(View * FVector2D(.15,.64), View * FVector2D(.85,.87)),
            FBox2D(View * FVector2D(.2,.12), View * FVector2D(.8,.28))};
        for (const double Scale : {1., 1.5, 2.})
        {
            for (const FVector2D Label : {FVector2D(160,26)*Scale, FVector2D(280,26)*Scale})
            {
                for (const FVector2D Bearing : {FVector2D(.5,.75), FVector2D(.5,.2), FVector2D(0,0),
                    FVector2D(1,0), FVector2D(0,1), FVector2D(1,1), FVector2D(.5,.5)})
                {
                    const FVector2D Desired = View * Bearing;
                    FVector2D Placed;
                    if (!TestTrue(TEXT("A complete label fits around simultaneous speech and caption panels"),
                        SovWorldLabelLayout::Place(Desired, Label, Safe, Panels, Placed))) { return false; }
                    TestTrue(TEXT("Label remains inside the safe rectangle"),
                        Safe.IsInsideOrOn(Placed) && Safe.IsInsideOrOn(Placed + Label));
                    for (const auto& Panel : Panels)
                    {
                        TestFalse(TEXT("No label or shadow crosses a priority text panel"),
                            SovThreatCueLayout::OverlapsPanel(Placed, Label, Panel, 7.99));
                    }
                    FVector2D Restored;
                    TestTrue(TEXT("Hiding panels restores the normal safe-area placement"),
                        SovWorldLabelLayout::Place(Desired, Label, Safe, {}, Restored));
                    TestEqual(TEXT("Horizontal bearing is unchanged when unobstructed"), Restored.X,
                        FMath::Clamp(Desired.X, Safe.Min.X, Safe.Max.X - Label.X));
                    TestEqual(TEXT("Vertical bearing is unchanged when unobstructed"), Restored.Y,
                        FMath::Clamp(Desired.Y, Safe.Min.Y, Safe.Max.Y - Label.Y));
                }
            }
        }
        FVector2D Unused;
        TestFalse(TEXT("A fully occupied screen omits only the secondary label instead of covering speech"),
            SovWorldLabelLayout::Place(View*.5, FVector2D(100,26), Safe, {Safe}, Unused));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWorldLabelHUDClearanceTest,
    "ProjectVelkorran.UI.WorldLabels.KeepBehindPlayerObjectiveClearOfEchoArcAndRadar",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWorldLabelHUDClearanceTest::RunTest(const FString& Parameters)
{
    for (const FVector2D View : {FVector2D(844,550), FVector2D(1280,720), FVector2D(1920,1080), FVector2D(2560,1080)})
    {
        const FBox2D Safe(View * .05, View * .95);
        for (const float Scale : {1.f, 1.5f, 2.f})
        {
            const auto HUD = SovHolographicHUDLayout::Compute(Safe.GetSize(), Scale);
            for (const bool bAmmoShown : {false, true})
            {
                TArray<FBox2D> Panels;
                for (const auto& Region : HUD.Regions(bAmmoShown))
                {
                    Panels.Emplace(Region.Min + Safe.Min, Region.Max + Safe.Min);
                }
                const FVector2D LabelSize = FVector2D(180,36) * Scale;
                // A rear objective projects to the lower screen edge, inside the Echo arc.
                const FVector2D Desired = Safe.Min + HUD.ArcPoint(.5f);
                FVector2D Placed;
                if (!TestTrue(TEXT("Rear objective fits around the rendered HUD"),
                    SovWorldLabelLayout::Place(Desired, LabelSize, Safe, Panels, Placed))) { return false; }
                TestTrue(TEXT("Complete objective label stays within the safe area"),
                    Safe.IsInsideOrOn(Placed) && Safe.IsInsideOrOn(Placed + LabelSize));
                for (const auto& Panel : Panels)
                {
                    TestFalse(TEXT("Objective label clears vitals, ammo, radar and Echo arc"),
                        SovThreatCueLayout::OverlapsPanel(Placed, LabelSize, Panel, 7.99));
                }
            }
        }
    }
    return true;
}
#endif
