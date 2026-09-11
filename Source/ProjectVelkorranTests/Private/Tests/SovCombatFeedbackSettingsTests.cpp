// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Presentation/SovCombatFeedbackComponent.h"
#include "Tests/SovSettingsTestFixtures.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatFeedbackComfortPreference,
    "ProjectVelkorran.Presentation.CombatFeedback.LiveComfortPreference",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCombatFeedbackComfortPreference::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine settings owner"), GEngine)) { return false; }
    TStrongObjectPtr<USovSettingsTestSettings> Preferences(NewObject<USovSettingsTestSettings>());
    TStrongObjectPtr<USovCombatFeedbackComponent> Presenter(NewObject<USovCombatFeedbackComponent>());
    TGuardValue<TObjectPtr<UGameUserSettings>> RestoreSettings(GEngine->GameUserSettings, Preferences.Get());
    Preferences->SetVisualEffectQuality(3);
    FString Error;
    FSovUserSettingsSnapshot Value;
    TestTrue(TEXT("Standard local preferences accepted"), Preferences->ApplySettingsSnapshot(Value, Error));
    TestFalse(TEXT("High graphics quality uses standard presentation initially"), Presenter->IsReducedCombatEffects());
    Value.bReduceCombatEffects = true;
    TestTrue(TEXT("Comfort preference accepted during the same avatar life"), Preferences->ApplySettingsSnapshot(Value, Error));
    TestTrue(TEXT("Existing presenter immediately selects reduced effects"), Presenter->IsReducedCombatEffects());
    TestEqual(TEXT("Comfort does not lower graphics quality"), Preferences->GetVisualEffectQuality(), 3);
    Value.bReduceCombatEffects = false;
    TestTrue(TEXT("Player can return to standard effects"), Preferences->ApplySettingsSnapshot(Value, Error));
    TestFalse(TEXT("Existing presenter does not retain a stale reduced flag"), Presenter->IsReducedCombatEffects());
    Preferences->SetVisualEffectQuality(0);
    TestTrue(TEXT("Low effects quality still selects reduced presentation"), Presenter->IsReducedCombatEffects());
    Preferences->SetVisualEffectQuality(3);
    Presenter->SetReducedCombatEffects(true);
    TestTrue(TEXT("Authored per-presenter reduction remains respected"), Presenter->IsReducedCombatEffects());
    TestEqual(TEXT("Preference reads spawn no cosmetic components"), Presenter->GetLiveBurstCount(), 0);
    return true;
}
#endif
