// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovSettingsTestFixtures.h"
#include "Diagnostics/SovDiagnosticsSubsystem.h"
#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSettingsAtomic, "ProjectVelkorran.Campaign.Settings.AtomicApplication", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSettingsPreset, "ProjectVelkorran.Campaign.Settings.PresetsPreserveAssistance", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSettingsPortable, "ProjectVelkorran.Campaign.Settings.PortableSnapshotPrivacy", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovSettingsPortable::RunTest(const FString& Parameters)
{
	USovSettingsTestSettings* Settings = NewObject<USovSettingsTestSettings>(); FString Error;
	Settings->ApplyDifficultyPreset(ESovDifficultyPreset::Story, Error);
	TArray<uint8> Bytes; TestTrue(TEXT("Capture fixed schema"), Settings->CapturePortableSettings(Bytes));
	TestEqual(TEXT("No unbounded strings or identity fields"), Bytes.Num(), 11);
	FSovUserSettingsSnapshot Custom = Settings->GetSettingsSnapshot();
	Custom.DefenseWindowScale = 2.f; Custom.ExertionCostScale = .25f; Custom.bReduceCorruptionEffects = true;
	Custom.bAutomaticSprint = true; Custom.bAimSnap = true; Custom.bProjectileLead = true;
	Custom.bShowObjectiveText = false; Custom.bReduceCombatEffects = true;
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
	TestFalse(TEXT("Local objective visibility survives portable gameplay import"), Settings->GetSettingsSnapshot().bShowObjectiveText);
	TestTrue(TEXT("Local combat comfort survives portable gameplay import"), Settings->IsReducedCombatEffectsEnabled());
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectiveSettingsPersistence, "ProjectVelkorran.Campaign.Settings.ObjectiveVisibilityConfig", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovObjectiveSettingsPersistence::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Config cache available for disk round trip"), GConfig)) { return false; }
	TestTrue(TEXT("Existing installations default to visible objective text"), FSovUserSettingsSnapshot().bShowObjectiveText);
	const FSovUserSettingsSnapshot DefaultsBefore = GetDefault<USovSettingsTestSettings>()->GetSettingsSnapshot();
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/ObjectiveSettings"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString ConfigPath = Directory / (FGuid::NewGuid().ToString() + TEXT(".ini"));
	const FString LegacyPath = Directory / (FGuid::NewGuid().ToString() + TEXT(".ini"));
	ON_SCOPE_EXIT
	{
		// Remove the unique cache entries before deleting their files, so a later
		// global flush cannot recreate test artifacts or retain their config state.
		if (GConfig) { GConfig->UnloadFile(ConfigPath); GConfig->UnloadFile(LegacyPath); }
		IFileManager::Get().Delete(*ConfigPath); IFileManager::Get().Delete(*LegacyPath);
	};
	auto* Settings = NewObject<USovSettingsTestSettings>();
	FString Error;
	FSovUserSettingsSnapshot Value; Value.bShowObjectiveText = false; Value.UIScale = 1.5f; Value.bReduceCombatEffects = true;
	TestTrue(TEXT("Visibility transaction accepted"), Settings->ApplySettingsSnapshot(Value, Error));
	// Isolate both disk output and CDO state. SaveConfig's default allows copying
	// instance values into the class default, which would pollute later tests.
	Settings->SaveConfig(CPF_Config, *ConfigPath, GConfig, false);
	GConfig->Flush(false, ConfigPath);
	TestTrue(TEXT("Local config written to isolated test file"), IFileManager::Get().FileExists(*ConfigPath));
	GConfig->UnloadFile(ConfigPath);
	TestEqual(TEXT("Config save preserves fixture visibility default"), GetDefault<USovSettingsTestSettings>()->GetSettingsSnapshot().bShowObjectiveText, DefaultsBefore.bShowObjectiveText);
	TestEqual(TEXT("Config save preserves fixture UI scale default"), GetDefault<USovSettingsTestSettings>()->GetSettingsSnapshot().UIScale, DefaultsBefore.UIScale);
	auto* Reloaded = NewObject<USovSettingsTestSettings>();
	FSovUserSettingsSnapshot Opposite = Value; Opposite.bShowObjectiveText = true; Opposite.UIScale = 1.f; Opposite.bReduceCombatEffects = false;
	TestTrue(TEXT("Reload destination starts with different values"), Reloaded->ApplySettingsSnapshot(Opposite, Error));
	Reloaded->LoadConfig(Reloaded->GetClass(), *ConfigPath);
	TestFalse(TEXT("Hidden objectives survive a config round trip"), Reloaded->GetSettingsSnapshot().bShowObjectiveText);
	TestEqual(TEXT("Other local preferences survive the same round trip"), Reloaded->GetSettingsSnapshot().UIScale, 1.5f);
	TestTrue(TEXT("Combat comfort survives a config round trip"), Reloaded->IsReducedCombatEffectsEnabled());
	TestEqual(TEXT("Config save preserves fixture combat comfort default"), GetDefault<USovSettingsTestSettings>()->IsReducedCombatEffectsEnabled(), DefaultsBefore.bReduceCombatEffects);
	const FString LegacyConfig = FString::Printf(TEXT("[%s]\nSettingsSchemaVersion=1\nSettings=(UIScale=1.5)\n"), *Settings->GetClass()->GetPathName());
	TestTrue(TEXT("Pre-objective config fixture written"), FFileHelper::SaveStringToFile(LegacyConfig, *LegacyPath));
	auto* Legacy = NewObject<USovSettingsTestSettings>();
	// Start from native defaults exactly as an installation without this field would.
	Legacy->ApplySettingsSnapshot(FSovUserSettingsSnapshot(), Error);
	Legacy->LoadConfig(Legacy->GetClass(), *LegacyPath);
	TestTrue(TEXT("A legacy config without the new field keeps objective text visible"), Legacy->GetSettingsSnapshot().bShowObjectiveText);
	TestFalse(TEXT("Legacy config retains the standard combat effects default"), Legacy->IsReducedCombatEffectsEnabled());
	TestEqual(TEXT("Additive field does not reset legacy UI scale"), Legacy->GetSettingsSnapshot().UIScale, 1.5f);
	return true;
}
#endif
