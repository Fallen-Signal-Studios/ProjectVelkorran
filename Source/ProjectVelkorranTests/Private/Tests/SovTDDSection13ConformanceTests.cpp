// Copyright Fallen Signal Studios. All Rights Reserved.
// Conformance checks for Sovereign Call: Origins TDD v2.0 section 13
// (User Interface, User Experience, and Accessibility). Each test names the
// clause it enforces so alignment is machine-checkable rather than estimated.
// These assert the settings surface a player reaches; authored widget layout,
// recruited accessibility testing and per-language subtitle collision remain
// outside native scope.
#include "Tests/SovSettingsTestFixtures.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
/** A snapshot every clause below can start from, so one clause cannot mask another. */
FSovUserSettingsSnapshot TDD13Baseline()
{
	FSovUserSettingsSnapshot Value;
	Value.Preset = ESovDifficultyPreset::Custom;
	return Value;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTDD134ScaleIndependence, "ProjectVelkorran.Campaign.Accessibility.TDD134ScaleIndependence", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovTDD134ScaleIndependence::RunTest(const FString& Parameters)
{
	// TDD 13.4: "UI scales independently from subtitle size."
	USovSettingsTestSettings* Settings = NewObject<USovSettingsTestSettings>();
	FString Error;
	FSovUserSettingsSnapshot Value = TDD13Baseline();
	Value.UIScale = 2.f; Value.SubtitleScale = 1.f;
	TestTrue(TEXT("Largest UI scale accepted"), Settings->ApplySettingsSnapshot(Value, Error));
	TestEqual(TEXT("UI scale applied"), Settings->GetSettingsSnapshot().UIScale, 2.f);
	TestEqual(TEXT("Raising UI scale does not move subtitle scale"), Settings->GetSettingsSnapshot().SubtitleScale, 1.f);
	Value = Settings->GetSettingsSnapshot();
	Value.SubtitleScale = 2.5f;
	TestTrue(TEXT("Extra-large subtitle target accepted"), Settings->ApplySettingsSnapshot(Value, Error));
	TestEqual(TEXT("Subtitle scale applied"), Settings->GetSettingsSnapshot().SubtitleScale, 2.5f);
	TestEqual(TEXT("Raising subtitle scale does not move UI scale"), Settings->GetSettingsSnapshot().UIScale, 2.f);
	Value = Settings->GetSettingsSnapshot(); Value.UIScale = 2.01f;
	TestFalse(TEXT("UI scale beyond the supported band is rejected"), Settings->ApplySettingsSnapshot(Value, Error));
	Value = Settings->GetSettingsSnapshot(); Value.SubtitleScale = 2.51f;
	TestFalse(TEXT("Subtitle scale beyond the supported band is rejected"), Settings->ApplySettingsSnapshot(Value, Error));
	TestEqual(TEXT("Rejected layout transactions leave both scales intact"), Settings->GetSettingsSnapshot().UIScale, 2.f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTDD138SubtitleSurface, "ProjectVelkorran.Campaign.Accessibility.TDD138SubtitleSurface", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovTDD138SubtitleSurface::RunTest(const FString& Parameters)
{
	// TDD 13.8: font scale with large and extra-large targets; opaque or adjustable
	// background; speaker name; directional indicator; maximum characters per line
	// and line-count constraints.
	USovSettingsTestSettings* Settings = NewObject<USovSettingsTestSettings>();
	FString Error;
	FSovUserSettingsSnapshot Value = TDD13Baseline();
	Value.bSubtitles = true; Value.bClosedCaptions = true;
	Value.SubtitleBackgroundOpacity = 1.f;
	Value.bSubtitleSpeakerNames = true; Value.bSubtitleDirections = true;
	Value.SubtitleCharactersPerLine = 20; Value.SubtitleMaximumLines = 1;
	TestTrue(TEXT("Fully opaque background and tightest line constraints accepted"), Settings->ApplySettingsSnapshot(Value, Error));
	TestTrue(TEXT("Speaker names reachable"), Settings->GetSettingsSnapshot().bSubtitleSpeakerNames);
	TestTrue(TEXT("Speaker direction indicator reachable"), Settings->GetSettingsSnapshot().bSubtitleDirections);
	TestTrue(TEXT("Closed captions for gameplay-critical sound reachable"), Settings->GetSettingsSnapshot().bClosedCaptions);
	Value = Settings->GetSettingsSnapshot();
	Value.SubtitleBackgroundOpacity = 0.f; Value.SubtitleCharactersPerLine = 64; Value.SubtitleMaximumLines = 4;
	TestTrue(TEXT("Transparent background and widest line constraints accepted"), Settings->ApplySettingsSnapshot(Value, Error));
	Value = Settings->GetSettingsSnapshot(); Value.SubtitleCharactersPerLine = 19;
	TestFalse(TEXT("Characters per line below the readable floor is rejected"), Settings->ApplySettingsSnapshot(Value, Error));
	Value = Settings->GetSettingsSnapshot(); Value.SubtitleCharactersPerLine = 65;
	TestFalse(TEXT("Characters per line beyond the validated ceiling is rejected"), Settings->ApplySettingsSnapshot(Value, Error));
	Value = Settings->GetSettingsSnapshot(); Value.SubtitleMaximumLines = 0;
	TestFalse(TEXT("A subtitle with no lines is rejected"), Settings->ApplySettingsSnapshot(Value, Error));
	Value = Settings->GetSettingsSnapshot(); Value.SubtitleMaximumLines = 5;
	TestFalse(TEXT("Line count beyond the validated ceiling is rejected"), Settings->ApplySettingsSnapshot(Value, Error));
	TestEqual(TEXT("Rejected subtitle transactions leave the accepted line count intact"), Settings->GetSettingsSnapshot().SubtitleMaximumLines, 4);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTDD138DialoguePressure, "ProjectVelkorran.Campaign.Accessibility.TDD138DialoguePressure", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovTDD138DialoguePressure::RunTest(const FString& Parameters)
{
	// TDD 13.8: "Choice text uses a minimum readable duration before pressure timers can expire."
	USovSettingsTestSettings* Settings = NewObject<USovSettingsTestSettings>();
	FString Error;
	FSovUserSettingsSnapshot Value = TDD13Baseline();
	Value.DialoguePressureMode = ESovDialoguePressureMode::Disabled;
	TestTrue(TEXT("Pressure timers can be disabled outright"), Settings->ApplySettingsSnapshot(Value, Error));
	Value = Settings->GetSettingsSnapshot();
	Value.DialoguePressureMode = ESovDialoguePressureMode::Extended;
	Value.DialogueMinimumReadSeconds = 30.f; Value.DialoguePressureExtension = 5.f;
	TestTrue(TEXT("Longest readable duration and extension accepted"), Settings->ApplySettingsSnapshot(Value, Error));
	Value = Settings->GetSettingsSnapshot(); Value.DialogueMinimumReadSeconds = 1.f;
	TestFalse(TEXT("A minimum read duration below the readable floor is rejected"), Settings->ApplySettingsSnapshot(Value, Error));
	TestEqual(TEXT("Rejected pressure transactions retain the accepted duration"), Settings->GetSettingsSnapshot().DialogueMinimumReadSeconds, 30.f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTDD139InputMotor, "ProjectVelkorran.Campaign.Accessibility.TDD139InputMotor", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovTDD139InputMotor::RunTest(const FString& Parameters)
{
	// TDD 13.9 input and motor: hold/toggle/tap alternatives; adjustable hold duration;
	// combat input buffering; aim assist, snap and projectile lead; defense-window
	// assistance; optional auto-sprint; menu navigation wrap; one-stick camera assistance.
	// Read back through the gameplay-facing getters, not the snapshot, so a control that
	// stores but never reaches gameplay fails here.
	USovSettingsTestSettings* Settings = NewObject<USovSettingsTestSettings>();
	FString Error;
	FSovUserSettingsSnapshot Value = TDD13Baseline();
	Value.bTapInteractions = true; Value.bToggleAim = true; Value.bToggleGuard = true;
	Value.bToggleSprint = true; Value.bAutomaticSprint = true; Value.bToggleAbilityModifier = true;
	Value.InteractionHoldScale = .1f;
	Value.InputBufferAssistanceSeconds = .2f;
	Value.MeleeAimAssistStrength = 1.f; Value.RangedAimAssistStrength = 1.f;
	Value.bAimSnap = true; Value.bProjectileLead = true;
	Value.DefenseWindowScale = 2.f;
	Value.AutoCameraStrength = 1.f;
	Value.bMenuNavigationWrap = true;
	TestTrue(TEXT("Full motor assistance transaction accepted"), Settings->ApplySettingsSnapshot(Value, Error));
	TestTrue(TEXT("Tap alternative reaches gameplay"), Settings->UseTapInteractions());
	TestTrue(TEXT("Aim toggle alternative reaches gameplay"), Settings->ShouldAimToggle());
	TestTrue(TEXT("Guard toggle alternative reaches gameplay"), Settings->ShouldGuardToggle());
	TestTrue(TEXT("Sprint toggle alternative reaches gameplay"), Settings->ShouldSprintToggle());
	TestTrue(TEXT("Optional auto-sprint reaches gameplay"), Settings->UseAutomaticSprint());
	TestTrue(TEXT("Ability modifier toggle reaches gameplay"), Settings->ShouldAbilityModifierToggle());
	TestEqual(TEXT("Shortest adjustable hold duration reaches gameplay"), Settings->GetInteractionHoldScale(), .1f);
	TestEqual(TEXT("Maximum combat input buffering reaches gameplay"), Settings->GetInputBufferAssistanceSeconds(), .2f);
	TestEqual(TEXT("Melee aim assistance reaches gameplay"), Settings->GetMeleeAimAssistStrength(), 1.f);
	TestEqual(TEXT("Ranged aim assistance reaches gameplay"), Settings->GetRangedAimAssistStrength(), 1.f);
	TestTrue(TEXT("Aim snap reaches gameplay"), Settings->UseAimSnap());
	TestTrue(TEXT("Projectile lead reaches gameplay"), Settings->UseProjectileLead());
	TestEqual(TEXT("Maximum defense-window assistance reaches gameplay"), Settings->GetDefenseWindowScale(), 2.f);
	TestEqual(TEXT("One-stick camera assistance reaches gameplay"), Settings->GetAutoCameraStrength(), 1.f);
	TestTrue(TEXT("Menu navigation wrap reachable"), Settings->GetSettingsSnapshot().bMenuNavigationWrap);
	Value = Settings->GetSettingsSnapshot(); Value.InputBufferAssistanceSeconds = .3f;
	TestFalse(TEXT("Input buffering beyond the validated ceiling is rejected"), Settings->ApplySettingsSnapshot(Value, Error));
	TestEqual(TEXT("Rejected assistance transactions do not withdraw granted buffering"), Settings->GetInputBufferAssistanceSeconds(), .2f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTDD139Vision, "ProjectVelkorran.Campaign.Accessibility.TDD139Vision", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovTDD139Vision::RunTest(const FString& Parameters)
{
	// TDD 13.9 vision: scalable UI and text; high-contrast HUD; color-vision presets plus
	// independent team/threat colors; navigation contrast/pulse; interactable and
	// weak-point outline options. TDD 13.3: "Color is never the sole distinction."
	USovSettingsTestSettings* Settings = NewObject<USovSettingsTestSettings>();
	FString Error;
	const ESovColorVisionPreset Presets[] = { ESovColorVisionPreset::Default, ESovColorVisionPreset::Deuteranopia,
		ESovColorVisionPreset::Protanopia, ESovColorVisionPreset::Tritanopia };
	for (const ESovColorVisionPreset Preset : Presets)
	{
		FSovUserSettingsSnapshot Value = TDD13Baseline();
		Value.ColorVisionPreset = Preset;
		TestTrue(TEXT("Every color-vision preset is selectable"), Settings->ApplySettingsSnapshot(Value, Error));
		TestEqual(TEXT("Selected color-vision preset applied"),
			static_cast<int32>(Settings->GetSettingsSnapshot().ColorVisionPreset), static_cast<int32>(Preset));
	}
	FSovUserSettingsSnapshot Value = Settings->GetSettingsSnapshot();
	Value.bOverrideTeamColor = true; Value.TeamColor = FLinearColor(.2f, .8f, .3f);
	Value.bOverrideThreatColor = true; Value.ThreatColor = FLinearColor(.9f, .1f, .1f);
	Value.bHighContrastHUD = true;
	Value.bInteractableOutlines = true; Value.bWeakPointOutlines = true; Value.OutlineThickness = 6.f;
	Value.bNavigationContrast = true; Value.bNavigationPulse = true;
	TestTrue(TEXT("Independent team and threat colors accepted with high contrast"), Settings->ApplySettingsSnapshot(Value, Error));
	TestTrue(TEXT("Team color override is independent of the threat override"), Settings->GetSettingsSnapshot().bOverrideTeamColor);
	TestTrue(TEXT("Threat color override is independent of the team override"), Settings->GetSettingsSnapshot().bOverrideThreatColor);
	TestTrue(TEXT("High-contrast HUD reachable"), Settings->GetSettingsSnapshot().bHighContrastHUD);
	TestTrue(TEXT("Weak-point outlines give a non-color weak-point signal"), Settings->GetSettingsSnapshot().bWeakPointOutlines);
	TestTrue(TEXT("Navigation pulse gives a non-color navigation signal"), Settings->GetSettingsSnapshot().bNavigationPulse);
	TestEqual(TEXT("Thickest outline accepted"), Settings->GetSettingsSnapshot().OutlineThickness, 6.f);
	// A translucent accessibility color would let the HUD lose the distinction it exists to carry.
	Value = Settings->GetSettingsSnapshot(); Value.ThreatColor = FLinearColor(.9f, .1f, .1f, .5f);
	TestFalse(TEXT("A translucent threat color is rejected"), Settings->ApplySettingsSnapshot(Value, Error));
	Value = Settings->GetSettingsSnapshot(); Value.OutlineThickness = .5f;
	TestFalse(TEXT("An outline thinner than the legible floor is rejected"), Settings->ApplySettingsSnapshot(Value, Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTDD139DifficultyDecomposition, "ProjectVelkorran.Campaign.Accessibility.TDD139DifficultyDecomposition", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovTDD139DifficultyDecomposition::RunTest(const FString& Parameters)
{
	// TDD 13.9: "Players can alter incoming damage, enemy aggression, timing assistance,
	// aim assistance, navigation, and puzzle support separately."
	USovSettingsTestSettings* Settings = NewObject<USovSettingsTestSettings>();
	FString Error;
	FSovUserSettingsSnapshot Value = TDD13Baseline();
	TestTrue(TEXT("Decomposition baseline accepted"), Settings->ApplySettingsSnapshot(Value, Error));
	Value = Settings->GetSettingsSnapshot(); Value.IncomingDamageScale = .5f;
	TestTrue(TEXT("Incoming damage alterable"), Settings->ApplySettingsSnapshot(Value, Error));
	TestEqual(TEXT("Altering incoming damage leaves enemy aggression alone"), Settings->GetEnemyRecoveryScale(), 1.f);
	TestEqual(TEXT("Altering incoming damage leaves timing assistance alone"), Settings->GetDefenseWindowScale(), 1.f);
	TestEqual(TEXT("Altering incoming damage leaves aim assistance alone"), Settings->GetRangedAimAssistStrength(), 0.f);
	Value = Settings->GetSettingsSnapshot(); Value.EnemyRecoveryScale = 1.6f;
	TestTrue(TEXT("Enemy aggression alterable"), Settings->ApplySettingsSnapshot(Value, Error));
	TestEqual(TEXT("Altering enemy aggression leaves incoming damage alone"), Settings->GetIncomingDamageScale(), .5f);
	Value = Settings->GetSettingsSnapshot(); Value.DefenseWindowScale = 1.8f;
	TestTrue(TEXT("Timing assistance alterable"), Settings->ApplySettingsSnapshot(Value, Error));
	TestEqual(TEXT("Altering timing assistance leaves enemy aggression alone"), Settings->GetEnemyRecoveryScale(), 1.6f);
	Value = Settings->GetSettingsSnapshot(); Value.RangedAimAssistStrength = .75f;
	TestTrue(TEXT("Aim assistance alterable"), Settings->ApplySettingsSnapshot(Value, Error));
	TestEqual(TEXT("Altering aim assistance leaves timing assistance alone"), Settings->GetDefenseWindowScale(), 1.8f);
	Value = Settings->GetSettingsSnapshot(); Value.bShowObjectiveText = false;
	TestTrue(TEXT("Navigation support alterable"), Settings->ApplySettingsSnapshot(Value, Error));
	TestEqual(TEXT("Altering navigation support leaves aim assistance alone"), Settings->GetRangedAimAssistStrength(), .75f);
	TestEqual(TEXT("Every separate alteration keeps the player on a custom profile"), Settings->GetDifficultyId(), FName(TEXT("Custom")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTDD1310FirstBoot, "ProjectVelkorran.Campaign.Accessibility.TDD1310FirstBootAndImmediateSave", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovTDD1310FirstBoot::RunTest(const FString& Parameters)
{
	// TDD 13.10: "Accessibility settings are available before the opening cinematic and
	// are saved immediately."
	USovSettingsTestSettings* Settings = NewObject<USovSettingsTestSettings>();
	FString Error;
	// Config-backed instances inherit this PC's first-boot completion. Stage an
	// uncompleted instance without touching the real settings owner, and compare
	// persistence counts relatively so ambient config cannot decide the result.
	auto* Completed = FindFProperty<FBoolProperty>(Settings->GetClass(), TEXT("bAccessibilitySetupCompleted"));
	if (!TestNotNull(TEXT("Reflected first-boot fixture field"), Completed)) { return false; }
	Completed->SetPropertyValue_InContainer(Settings, false);
	TestFalse(TEXT("First boot has not completed accessibility setup"), Settings->HasCompletedAccessibilitySetup());
	const int32 BeforeConfigure = Settings->Saves;
	FSovUserSettingsSnapshot Value = TDD13Baseline();
	Value.UIScale = 1.75f; Value.bHighContrastHUD = true;
	TestTrue(TEXT("Accessibility is configurable before the opening cinematic"), Settings->ApplySettingsSnapshot(Value, Error));
	TestEqual(TEXT("An accepted accessibility choice persists immediately"), Settings->Saves, BeforeConfigure + 1);
	TestFalse(TEXT("Setup completion is never inferred from a settings edit"), Settings->HasCompletedAccessibilitySetup());
	const int32 AfterConfigure = Settings->Saves;
	FSovUserSettingsSnapshot Invalid = Settings->GetSettingsSnapshot();
	Invalid.UIScale = 9.f;
	TestFalse(TEXT("An unsupported accessibility choice is rejected"), Settings->ApplySettingsSnapshot(Invalid, Error));
	TestEqual(TEXT("A rejected choice persists nothing"), Settings->Saves, AfterConfigure);
	TestEqual(TEXT("A rejected choice does not disturb the accepted scale"), Settings->GetSettingsSnapshot().UIScale, 1.75f);
	TestTrue(TEXT("Explicit continue completes first-boot setup"), Settings->CompleteAccessibilitySetup());
	TestTrue(TEXT("Setup completion is recorded"), Settings->HasCompletedAccessibilitySetup());
	TestEqual(TEXT("Setup completion persists immediately"), Settings->Saves, AfterConfigure + 1);
	const int32 AfterComplete = Settings->Saves;
	TestTrue(TEXT("Repeating continue is accepted"), Settings->CompleteAccessibilitySetup());
	TestEqual(TEXT("Repeating continue does not persist again"), Settings->Saves, AfterComplete);
	TestTrue(TEXT("Accessibility remains configurable after setup completes"), Settings->ApplySettingsSnapshot(Value, Error));
	TestTrue(TEXT("Completion survives a later accessibility change"), Settings->HasCompletedAccessibilitySetup());
	return true;
}
#endif
