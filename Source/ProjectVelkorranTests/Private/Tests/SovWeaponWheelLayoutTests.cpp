// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovWeaponWheelLayout.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWeaponWheelClearanceTest,
    "ProjectVelkorran.UI.WeaponWheel.KeepsControlsClearAndStableWhileSpeechChanges",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWeaponWheelClearanceTest::RunTest(const FString& Parameters)
{
    const FBox2D Safe(FVector2D(20,20),FVector2D(1648,1060));
    const TArray<FBox2D> HUD={
        FBox2D(FVector2D(484,20),FVector2D(1184,130)),
        FBox2D(FVector2D(60,700),FVector2D(460,1060)),
        FBox2D(FVector2D(470,920),FVector2D(1600,1030)),
        FBox2D(FVector2D(20,20),FVector2D(440,180))};
    FBox2D Default,WithSpeech,Retained,WithCaption;
    TestTrue(TEXT("Authored size fits the ordinary HUD"),SovWeaponWheelLayout::Place(Safe,HUD,600.,FBox2D(ForceInit),Default));
    TestEqual(TEXT("No unnecessary shrinking"),Default.GetSize().X,600.);
    TArray<FBox2D> Panels=HUD;
    Panels.Emplace(FVector2D(510,650),FVector2D(1210,850));
    TestTrue(TEXT("The complete wheel fits around live speech"),SovWeaponWheelLayout::Place(Safe,Panels,600.,Default,WithSpeech));
    TestTrue(TEXT("All occupied panels have clearance"),SovWeaponWheelLayout::IsClear(WithSpeech,Safe,Panels));
    TestTrue(TEXT("The compact fixture retains usable controls"),WithSpeech.GetSize().X>480.);
    TestTrue(TEXT("Closing speech does not move controls during selection"),SovWeaponWheelLayout::Place(Safe,HUD,600.,WithSpeech,Retained));
    TestEqual(TEXT("Stable position"),Retained.Min,WithSpeech.Min);
    TestEqual(TEXT("Stable scale"),Retained.GetSize(),WithSpeech.GetSize());
    Panels.Emplace(FVector2D(540,150),FVector2D(1130,280));
    TestTrue(TEXT("A new caption can request another clear placement"),SovWeaponWheelLayout::Place(Safe,Panels,600.,WithSpeech,WithCaption));
    TestTrue(TEXT("No caption, subtitle or HUD collision"),SovWeaponWheelLayout::IsClear(WithCaption,Safe,Panels));
    FBox2D Reopened;
    TestTrue(TEXT("A new activation can use the original size again"),SovWeaponWheelLayout::Place(Safe,HUD,600.,FBox2D(ForceInit),Reopened));
    TestEqual(TEXT("Reset returns normal size"),Reopened.GetSize().X,600.);
    FBox2D Unused;
    TestFalse(TEXT("A fully blocked screen is explicitly rejected"),SovWeaponWheelLayout::Place(Safe,{Safe},600.,FBox2D(ForceInit),Unused));
    TestFalse(TEXT("Invalid preferred sizes are rejected"),SovWeaponWheelLayout::Place(Safe,{},-1.,FBox2D(ForceInit),Unused));
    return true;
}
#endif
