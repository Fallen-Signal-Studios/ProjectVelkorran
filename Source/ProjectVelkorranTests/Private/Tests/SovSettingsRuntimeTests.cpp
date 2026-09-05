// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovSettingsTestFixtures.h"
#include "Diagnostics/SovDiagnosticsSubsystem.h"
#include "Misc/AutomationTest.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSettingsAtomic, "ProjectVelkorran.Campaign.Settings.AtomicApplication", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovSettingsAtomic::RunTest(const FString& Parameters)
{
	USovSettingsTestSettings* Settings = NewObject<USovSettingsTestSettings>();
	USovSettingsReentryProbe* Probe = NewObject<USovSettingsReentryProbe>(); Probe->Settings = Settings;
	Settings->OnUserSettingsChanged.AddDynamic(Probe, &USovSettingsReentryProbe::OnChanged);
	FString Error;
	FSovUserSettingsSnapshot Value; Value.Preset = ESovDifficultyPreset::Custom; Value.DefenseWindowScale = 1.7f;
	TestTrue(TEXT("Valid complete transaction accepted"), Settings->ApplySettingsSnapshot(Value, Error));
	TestEqual(TEXT("Persisted immediately once"), Settings->Saves, 1);
	TestEqual(TEXT("One coherent change event"), Probe->Calls, 1);
	TestFalse(TEXT("Reentrant mutation rejected"), Probe->bReentryAccepted);
	Value.IncomingDamageScale = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite transaction rejected"), Settings->ApplySettingsSnapshot(Value, Error));
	TestEqual(TEXT("No invalid persistence"), Settings->Saves, 1);
	TestEqual(TEXT("Previous valid assist remains"), Settings->GetDefenseWindowScale(), 1.7f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSettingsPreset, "ProjectVelkorran.Campaign.Settings.PresetsPreserveAssistance", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovSettingsPreset::RunTest(const FString& Parameters)
{
	USovSettingsTestSettings* Settings = NewObject<USovSettingsTestSettings>(); FString Error;
	TestFalse(TEXT("Sovereign locked before campaign completion"), Settings->ApplyDifficultyPreset(ESovDifficultyPreset::Sovereign, Error));
	TestTrue(TEXT("Story available before gameplay"), Settings->ApplyDifficultyPreset(ESovDifficultyPreset::Story, Error));
	TestTrue(TEXT("Story lowers incoming damage"), Settings->GetIncomingDamageScale() < 1.f);
	const float Assist = Settings->GetDefenseWindowScale();
	TestTrue(TEXT("Veteran available"), Settings->ApplyDifficultyPreset(ESovDifficultyPreset::Veteran, Error));
	TestEqual(TEXT("Preset does not withdraw selected timing accessibility"), Settings->GetDefenseWindowScale(), Assist);
	TestFalse(TEXT("Veteran rescue disallowed"), Settings->IsCompanionRescueAllowed());
	Settings->SetGameplayDifficulty(ENarrativeGameplayDifficulty::Easy);
	TestEqual(TEXT("Legacy Narrative setter routes to project consumer"), Settings->GetDifficultyId(), FName(TEXT("Story")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSettingsPortable, "ProjectVelkorran.Campaign.Settings.PortableSnapshotPrivacy", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovSettingsPortable::RunTest(const FString& Parameters)
{
	USovSettingsTestSettings* Settings = NewObject<USovSettingsTestSettings>(); FString Error;
	Settings->ApplyDifficultyPreset(ESovDifficultyPreset::Story, Error);
	TArray<uint8> Bytes; TestTrue(TEXT("Capture fixed schema"), Settings->CapturePortableSettings(Bytes));
	TestEqual(TEXT("No unbounded strings or identity fields"), Bytes.Num(), 11);
	FSovUserSettingsSnapshot Custom = Settings->GetSettingsSnapshot();
	Custom.DefenseWindowScale = 2.f; Custom.ExertionCostScale = .25f; Custom.bReduceCorruptionEffects = true;
	Custom.bAutomaticSprint = true; Custom.bAimSnap = true; Custom.bProjectileLead = true;
	Custom.IncomingDamageScale = 1.5f; Custom.Preset = ESovDifficultyPreset::Custom;
	Settings->ApplySettingsSnapshot(Custom, Error);
	TestTrue(TEXT("Explicit restore succeeds"), Settings->RestorePortableSettings(Bytes, Error));
	TestEqual(TEXT("Difficulty restored"), Settings->GetDifficultyId(), FName(TEXT("Story")));
	TestEqual(TEXT("Current accessibility preserved"), Settings->GetDefenseWindowScale(), 2.f);
	TestEqual(TEXT("Current exertion assist preserved"), Settings->GetExertionCostScale(), .25f);
	TestTrue(TEXT("Current comfort preserved"), Settings->IsReducedCorruptionEffectsEnabled());
	TestTrue(TEXT("Automatic sprint accessibility survives portable gameplay import"), Settings->UseAutomaticSprint());
	TestTrue(TEXT("Aim snap accessibility survives portable gameplay import"), Settings->UseAimSnap());
	TestTrue(TEXT("Projectile lead accessibility survives portable gameplay import"), Settings->UseProjectileLead());
	Bytes[0] = 99;
	TestFalse(TEXT("Unknown future schema rejected before mutation"), USovGameUserSettings::ValidatePortableSettings(Bytes, Error));
	TestFalse(TEXT("Corrupted import rejected"), Settings->RestorePortableSettings(Bytes, Error));
	Bytes.Add(0);
	TestFalse(TEXT("Trailing bytes rejected"), USovGameUserSettings::ValidatePortableSettings(Bytes, Error));
	TestTrue(TEXT("Stable semantic ID exportable"), USovDiagnosticsSubsystem::IsSafeDebugId(TEXT("M01.Courtyard")));
	TestFalse(TEXT("Free text not exportable"), USovDiagnosticsSubsystem::IsSafeDebugId(TEXT("A player's message")));
	TestFalse(TEXT("Path not exportable"), USovDiagnosticsSubsystem::IsSafeDebugId(TEXT("/Users/Name/Save")));
	return true;
}
#endif
