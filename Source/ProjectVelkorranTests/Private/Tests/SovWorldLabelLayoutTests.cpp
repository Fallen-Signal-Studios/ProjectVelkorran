// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovWorldLabelLayout.h"
#include "UI/SovThreatCueLayout.h"
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
#endif
