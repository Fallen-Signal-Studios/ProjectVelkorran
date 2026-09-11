// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovThreatCueLayout.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovThreatCueDirectionTest,
    "ProjectVelkorran.Campaign.Presentation.ThreatCueCameraRelativeDirection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovThreatCueDirectionTest::RunTest(const FString& Parameters)
{
    ESovThreatCueSide Side;
    TestTrue(TEXT("Behind resolves"), SovThreatCueLayout::Classify(FVector::ForwardVector, FVector::RightVector, FVector(-100, 0, 0), Side));
    TestTrue(TEXT("Behind stays behind, never forced to a lateral side"), Side == ESovThreatCueSide::Behind);
    TestTrue(TEXT("Camera rotation resolves"), SovThreatCueLayout::Classify(FVector::RightVector, -FVector::ForwardVector, FVector(100, 0, 0), Side));
    TestTrue(TEXT("World east appears left after camera turns north"), Side == ESovThreatCueSide::Left);
    TestTrue(TEXT("Elevated right source retains bearing"), SovThreatCueLayout::Classify(FVector::ForwardVector, FVector::RightVector, FVector(10, 100, 900), Side));
    TestTrue(TEXT("Height does not flip direction"), Side == ESovThreatCueSide::Right);
    TestFalse(TEXT("Coincident source cannot invent a bearing"), SovThreatCueLayout::Classify(FVector::ForwardVector, FVector::RightVector, FVector::ZeroVector, Side));
    TestFalse(TEXT("Vertical-only source cannot invent a bearing"), SovThreatCueLayout::Classify(FVector::ForwardVector, FVector::RightVector, FVector::UpVector, Side));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovThreatCueVisibilityTest,
    "ProjectVelkorran.Campaign.Presentation.ThreatCueSafeAreaAndClipping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovThreatCueVisibilityTest::RunTest(const FString& Parameters)
{
    for (const FVector2D View : { FVector2D(640, 360), FVector2D(1280, 360), FVector2D(1280, 720), FVector2D(3440, 1440) })
    {
        for (const float RequestedScale : { .75f, 1.f, 2.f })
        {
            FVector2D Positions[4], Sizes[4];
            for (uint8 Index = 0; Index < 4; ++Index)
            {
                FVector2D Position, Size; float Scale;
                TestTrue(TEXT("Supported viewport gets a cue"), SovThreatCueLayout::Place(View, RequestedScale, static_cast<ESovThreatCueSide>(Index), Position, Size, Scale));
                TestTrue(TEXT("Cue remains within safe area"), SovThreatCueLayout::FullyInside(Position, Position + Size, View * .1 - FVector2D(.01), View * .9 + FVector2D(.01)));
                TestFalse(TEXT("Clipped right edge cannot qualify as visibly presented"), SovThreatCueLayout::FullyInside(Position, Position + Size, FVector2D::ZeroVector, Position + Size - FVector2D(1, 0)));
                Positions[Index] = Position; Sizes[Index] = Size;
            }
            for (uint8 First = 0; First < 4; ++First)
            {
                for (uint8 Second = First + 1; Second < 4; ++Second)
                {
                    const bool bOverlaps = Positions[First].X < Positions[Second].X + Sizes[Second].X
                        && Positions[First].X + Sizes[First].X > Positions[Second].X
                        && Positions[First].Y < Positions[Second].Y + Sizes[Second].Y
                        && Positions[First].Y + Sizes[First].Y > Positions[Second].Y;
                    TestFalse(TEXT("No sector can cover another warning that will be acknowledged"), bOverlaps);
                }
            }
        }
    }
    FVector2D Position, Size; float Scale;
    TestFalse(TEXT("Tiny widget cannot acknowledge unreadable cue"), SovThreatCueLayout::Place(FVector2D(80, 50), 1.f, ESovThreatCueSide::Ahead, Position, Size, Scale));
    TestFalse(TEXT("Zero area is not a painted cue"), SovThreatCueLayout::FullyInside(FVector2D(10), FVector2D(10), FVector2D::ZeroVector, FVector2D(100)));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovThreatCueHUDExclusionTest,
    "ProjectVelkorran.Campaign.Presentation.ThreatCueAvoidsScaledHUD",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovThreatCueHUDExclusionTest::RunTest(const FString& Parameters)
{
    // Representative short editor viewport with 2x panels, reproducing the observed
    // right warning over the ability names. These are explicit geometry fixtures,
    // not a replacement for inspecting the next actual rendered cached bounds.
    const FVector2D View(1696,862);
    TArray<FBox2D> Panels{
        FBox2D(FVector2D(13,570),FVector2D(528,829)),
        FBox2D(FVector2D(805,277),FVector2D(1670,601)),
        FBox2D(FVector2D(30,11),FVector2D(616,318))};
    FVector2D RightPosition, RightSize; float RightScale;
    TestTrue(TEXT("Original right geometry exists"), SovThreatCueLayout::Place(View,2.f,ESovThreatCueSide::Right,RightPosition,RightSize,RightScale));
    TestTrue(TEXT("Fixture reproduces old warning covering Echo"), SovThreatCueLayout::OverlapsPanel(RightPosition,RightSize,Panels[1],0.));
    for (const auto Side : {ESovThreatCueSide::Ahead, ESovThreatCueSide::Right})
    {
        FVector2D Position, Size; float Scale;
        TestTrue(TEXT("Original directional warning retains its sizing"), SovThreatCueLayout::Place(View,2.f,Side,Position,Size,Scale));
        const auto OriginalSize = Size;
        TestTrue(TEXT("The displayed panels leave a usable warning position"), SovThreatCueLayout::AvoidPanels(View,Side,Panels,Size,Position));
        TestTrue(TEXT("No font or card shrinking"),Size == OriginalSize);
        TestTrue(TEXT("Repositioned warning stays in the actual safe area"),SovThreatCueLayout::FullyInside(Position,Position+Size,View*.1,View*.9));
        for (const auto& Panel : Panels)
        { TestFalse(TEXT("Warning avoids every displayed HUD panel and earlier warning"),SovThreatCueLayout::OverlapsPanel(Position,Size,Panel)); }
        if (Side==ESovThreatCueSide::Right)
        { TestTrue(TEXT("Right warning remains in the right half"),(Position+Size*.5).X>View.X*.5); }
        Panels.Emplace(Position,Position+Size);
    }
    FVector2D Position, Size; float Scale;
    SovThreatCueLayout::Place(View,1.f,ESovThreatCueSide::Left,Position,Size,Scale);
    const auto Original = Position;
    TestTrue(TEXT("No visible panel does not disturb the existing location"),SovThreatCueLayout::AvoidPanels(View,ESovThreatCueSide::Left,{},Size,Position));
    TestTrue(TEXT("Retired/hidden panel geometry cannot leave a layout memory"),Position==Original);
    const TArray<FBox2D> FullScreen{FBox2D(FVector2D::ZeroVector,View)};
    TestFalse(TEXT("No free space is reported honestly"),SovThreatCueLayout::AvoidPanels(View,ESovThreatCueSide::Left,FullScreen,Size,Position));
    TestTrue(TEXT("No-fit preserves the original essential warning for the caller"),Position==Original);
    return true;
}
#endif
