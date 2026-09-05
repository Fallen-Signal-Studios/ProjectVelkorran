// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovPlatformOutputTestFixtures.h"
#include "Tests/SovSettingsTestFixtures.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include <limits>

struct FSovPlatformOutputTestAccess
{
	static bool TickPreview(USovGameUserSettings* Settings) { return Settings->TickHDRPreview(0.f); }
	static void DisplayMetricsChanged(USovGameUserSettings* Settings) { Settings->bDisplayMetricsInvalidated = true; }
};
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSystemDisplayOwnershipRuntime, "ProjectVelkorran.Campaign.PlatformOutput.SystemDisplayOwnership", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovSystemDisplayOwnershipRuntime::RunTest(const FString& Parameters)
{
	auto* Settings = NewObject<USovPlatformOutputTestSettings>(); Settings->InitializeOutput(true, 2000);
	Settings->bSystemManaged = true;
	FSovHDROutputStatus Status = Settings->GetHDROutputStatus();
	TestTrue(TEXT("System-managed HDR remains observable"), Status.bSupported && Status.bEnabled && Status.bSystemManaged);
	TestEqual(TEXT("System-managed output keeps observed peak"), Status.PeakNits, 2000);
	TestFalse(TEXT("System output offers no desktop preview"), Status.bCanPreviewInGame);
	TestFalse(TEXT("System output offers no renderer calibration"), Status.bCanCalibrateInGame);
	FGuid Receipt = FGuid::NewGuid(); FString Error;
	TestFalse(TEXT("Desktop HDR request cannot take over system output"), Settings->PreviewHDRCalibration(true, 1000, Receipt, Error));
	TestFalse(TEXT("Rejected request has no receipt"), Receipt.IsValid());
	TestTrue(TEXT("Failure explains platform ownership"), Error.Contains(TEXT("platform display settings")));
	TestFalse(TEXT("SDR request also cannot take over system output"), Settings->PreviewHDRCalibration(false, 1000, Receipt, Error));
	TestFalse(TEXT("Full calibration request cannot take over system output"), Settings->PreviewHDRDisplay(true, 1000, FSovHDRCalibration(), Receipt, Error));
	TestEqual(TEXT("Rejected requests perform no output writes"), Settings->Writes, 0);
	TestEqual(TEXT("Rejected requests do not persist"), Settings->Saves, 0);
	auto* RendererSettings = NewObject<USovDisplayCVarTestSettings>();
	RendererSettings->InitializeVariables(); RendererSettings->bSystemManaged = true;
	FSovHDRCalibration Calibration; Calibration.PaperWhiteNits = 200.f;
	TestFalse(TEXT("Direct renderer adapter cannot bypass system display ownership"), RendererSettings->WriteProductionCalibration(Calibration));
	TestEqual(TEXT("Rejected system calibration preserves renderer gray"), RendererSettings->GetVariable(TEXT("r.HDR.Display.MidLuminance"))->GetFloat(), 15.f);
	RendererSettings->ResetVariables();
	Settings->bSystemManaged = false;
	Status = Settings->GetHDROutputStatus();
	TestTrue(TEXT("Desktop capabilities restored independently"), Status.bCanPreviewInGame && Status.bCanCalibrateInGame);
	Settings->bCalibrationAvailable = false;
	Status = Settings->GetHDROutputStatus();
	TestTrue(TEXT("Output-only preview does not require compositor"), Status.bCanPreviewInGame);
	TestFalse(TEXT("Missing compositor disables full calibration"), Status.bCanCalibrateInGame);
	Settings->DisplayIdentity.Reset();
	TestFalse(TEXT("Missing physical desktop identity disables preview"), Settings->GetHDROutputStatus().bCanPreviewInGame);
	Settings->DisplayIdentity = TEXT("display-a"); Settings->bCalibrationAvailable = true;
	TestTrue(TEXT("Desktop preview before suspension starts"), Settings->PreviewHDRCalibration(false, 1000, Receipt, Error));
	TestTrue(TEXT("Suspension cancels unconfirmed preview"), Settings->RevertUnconfirmedHDRPreview());
	TestTrue(TEXT("Suspension restores confirmed HDR"), Settings->GetHDROutputStatus().bEnabled);
	TestFalse(TEXT("Resume cannot confirm outgoing preview"), Settings->ConfirmHDRCalibration(Receipt, Error));
	const int32 WritesAfterRollback = Settings->Writes;
	TestFalse(TEXT("Repeated suspend with no preview is a no-op"), Settings->RevertUnconfirmedHDRPreview());
	TestEqual(TEXT("Repeated suspension does not touch output"), Settings->Writes, WritesAfterRollback);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFullHDRCalibrationRuntime, "ProjectVelkorran.Campaign.PlatformOutput.FullCalibrationAndDisplayIdentity", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovFullHDRCalibrationRuntime::RunTest(const FString& Parameters)
{
	auto* Settings = NewObject<USovPlatformOutputTestSettings>(); Settings->InitializeOutput(false, 1000);
	const FSovHDRCalibration Original = Settings->PhysicalCalibration;
	FSovHDRCalibration Request; Request.BlackFloorNits = .005f; Request.PaperWhiteNits = 200.f; Request.UIWhiteNits = 150.f;
	FGuid Receipt; FString Error;
	TestTrue(TEXT("Full HDR calibration applies real adapter fields"), Settings->PreviewHDRDisplay(true, 2000, Request, Receipt, Error));
	TestTrue(TEXT("Renderer receives all requested fields"), Settings->PhysicalCalibration.Equals(Request));
	Settings->SaveSettings();
	TestTrue(TEXT("Unrelated saves retain confirmed calibration"), Settings->SavedCalibration.Equals(Original));
	TestTrue(TEXT("Preview display persists only matching receipt"), Settings->ConfirmHDRCalibration(Receipt, Error));
	TestTrue(TEXT("Confirmed calibration stored"), Settings->SavedCalibration.Equals(Request));
	FSovHDRCalibration Second = Request; Second.PaperWhiteNits = 300.f; Second.UIWhiteNits = 350.f;
	TestTrue(TEXT("Second calibrated preview"), Settings->PreviewHDRDisplay(true, 2000, Second, Receipt, Error));
	Settings->DisplayIdentity = TEXT("display-b"); // Deliberately identical HDR support and nits.
	TestFalse(TEXT("Move to identically capable display cannot confirm"), Settings->ConfirmHDRCalibration(Receipt, Error));
	TestTrue(TEXT("Display move restores previous renderer calibration"), Settings->PhysicalCalibration.Equals(Request));
	Settings->SaveSettings(); TestTrue(TEXT("Display move preserves confirmed config"), Settings->SavedCalibration.Equals(Request));
	TestTrue(TEXT("Restart on the new display"), Settings->PreviewHDRDisplay(true, 2000, Second, Receipt, Error));
	FSovPlatformOutputTestAccess::DisplayMetricsChanged(Settings);
	TestFalse(TEXT("Hotplug event invalidates even unchanged monitor descriptor"), FSovPlatformOutputTestAccess::TickPreview(Settings));
	TestTrue(TEXT("Hotplug reverted values"), Settings->PhysicalCalibration.Equals(Request));
	TestTrue(TEXT("External calibration conflict starts"), Settings->PreviewHDRDisplay(true, 2000, Second, Receipt, Error));
	Settings->PhysicalCalibration.UIWhiteNits = 225.f;
	TestFalse(TEXT("External renderer adjustment refuses confirmation"), Settings->ConfirmHDRCalibration(Receipt, Error));
	TestEqual(TEXT("Rollback preserves external UI adjustment"), Settings->PhysicalCalibration.UIWhiteNits, 225.f);
	TestEqual(TEXT("Rollback restores still-owned scene white"), Settings->PhysicalCalibration.PaperWhiteNits, Request.PaperWhiteNits);
	TestTrue(TEXT("Unavailable rollback case starts"), Settings->PreviewHDRDisplay(true, 2000, Second, Receipt, Error));
	Settings->bAvailable = false; Settings->Now += 15.;
	TestFalse(TEXT("Unavailable timeout retires preview"), FSovPlatformOutputTestAccess::TickPreview(Settings));
	Settings->SaveSettings(); TestTrue(TEXT("Device loss never persists unconfirmed calibration"), Settings->SavedCalibration.Equals(Request));
	Settings->bAvailable = true; Settings->DisplayIdentity.Reset();
	TestFalse(TEXT("Unknown physical display refuses HDR receipt"), Settings->PreviewHDRCalibration(true, 1000, Receipt, Error));
	Settings->DisplayIdentity = TEXT("display-b"); Settings->bCalibrationAvailable = false;
	TestFalse(TEXT("Missing renderer controls explicitly reject full calibration"), Settings->PreviewHDRDisplay(true, 2000, Request, Receipt, Error));
	Settings->bCalibrationAvailable = true; Request.PaperWhiteNits = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Malformed calibration cannot write"), Settings->PreviewHDRDisplay(true, 2000, Request, Receipt, Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHDRPreviewRuntime, "ProjectVelkorran.Campaign.PlatformOutput.HDRPreviewTransaction", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovHDRPreviewRuntime::RunTest(const FString& Parameters)
{
	auto* Settings = NewObject<USovPlatformOutputTestSettings>(); Settings->InitializeOutput(false, 1000);
	FGuid Receipt; FString Error;
	TestTrue(TEXT("Preview applies output"), Settings->PreviewHDRCalibration(true, 1800, Receipt, Error));
	TestEqual(TEXT("Actual engine-selected nits, not requested nits"), Settings->GetHDROutputStatus().PeakNits, 2000);
	TestEqual(TEXT("Preview is not persisted"), Settings->Saves, 0);
	Settings->bTryReentryOnPersist = true; Settings->ReentryReceipt = Receipt;
	Settings->SaveSettings();
	TestFalse(TEXT("Unrelated Narrative widget save persists confirmed SDR"), Settings->bSavedEnabled);
	TestFalse(TEXT("Save callback cannot confirm while persisted values temporarily restored"), Settings->bReentryAccepted);
	TestTrue(TEXT("Preview output remains active"), Settings->GetHDROutputStatus().bEnabled);
	Settings->bTryReentryOnPersist = false;
	TestTrue(TEXT("Matching receipt confirms actual output"), Settings->ConfirmHDRCalibration(Receipt, Error));
	TestTrue(TEXT("Confirmation persists HDR"), Settings->bSavedEnabled);
	TestEqual(TEXT("Confirmation persists normalized nits"), Settings->SavedNits, 2000);
	TestFalse(TEXT("Consumed receipt cannot revert"), Settings->RevertHDRCalibration(Receipt));
	FGuid Next; TestTrue(TEXT("New SDR preview accepted"), Settings->PreviewHDRCalibration(false, 1000, Next, Error));
	TestFalse(TEXT("Old receipt cannot cancel new preview"), Settings->RevertHDRCalibration(Receipt));
	TestTrue(TEXT("Cancel restores confirmed HDR"), Settings->RevertHDRCalibration(Next));
	TestEqual(TEXT("Rollback restores exact prior peak"), Settings->GetHDROutputStatus().PeakNits, 2000);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHDRFailureRuntime, "ProjectVelkorran.Campaign.PlatformOutput.HDRFailureAndExpiry", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovHDRFailureRuntime::RunTest(const FString& Parameters)
{
	auto* Settings = NewObject<USovPlatformOutputTestSettings>(); Settings->InitializeOutput(false, 1000);
	FGuid Receipt; FString Error;
	Settings->bSupported = false;
	TestFalse(TEXT("Unsupported HDR cannot mutate output"), Settings->PreviewHDRCalibration(true, 1000, Receipt, Error));
	TestEqual(TEXT("No hardware request on unsupported device"), Settings->Writes, 0);
	Settings->bSupported = true; Settings->bRejectEnable = true;
	TestFalse(TEXT("Engine rejection fails preview"), Settings->PreviewHDRCalibration(true, 1000, Receipt, Error));
	TestFalse(TEXT("No false enabled report"), Settings->GetHDROutputStatus().bEnabled);
	Settings->bRejectEnable = false;
	TestFalse(TEXT("Out of range calibration rejected"), Settings->PreviewHDRCalibration(true, 100000, Receipt, Error));
	TestTrue(TEXT("Valid retry"), Settings->PreviewHDRCalibration(true, 1000, Receipt, Error));
	Settings->Now += 15.;
	TestFalse(TEXT("Real-time expiry refuses confirmation"), Settings->ConfirmHDRCalibration(Receipt, Error));
	TestFalse(TEXT("Expired preview restores SDR"), Settings->GetHDROutputStatus().bEnabled);
	TestTrue(TEXT("Device change case starts"), Settings->PreviewHDRCalibration(true, 1000, Receipt, Error));
	Settings->bSupported = false;
	TestFalse(TEXT("Device loss terminates core ticker preview"), FSovPlatformOutputTestAccess::TickPreview(Settings));
	TestFalse(TEXT("Device loss falls back to SDR"), Settings->GetHDROutputStatus().bEnabled);
	TestFalse(TEXT("Device loss retires receipt"), Settings->ConfirmHDRCalibration(Receipt, Error));
	Settings->bSupported = true; Settings->InitializeOutput(false, 1000);
	TestTrue(TEXT("Render-unavailable rollback case starts"), Settings->PreviewHDRCalibration(true, 2000, Receipt, Error));
	Settings->bAvailable = false;
	TestTrue(TEXT("Manual rollback succeeds even without a rendering output"), Settings->RevertHDRCalibration(Receipt));
	Settings->SaveSettings();
	TestFalse(TEXT("Unavailable output cannot leave unconfirmed enable in persisted config"), Settings->bSavedEnabled);
	TestEqual(TEXT("Unavailable output rollback restores confirmed calibration in config"), Settings->SavedNits, 1000);
	Settings->bAvailable = true; Settings->InitializeOutput(false, 1000);
	TestTrue(TEXT("Render-unavailable timeout case starts"), Settings->PreviewHDRCalibration(true, 2000, Receipt, Error));
	Settings->bAvailable = false; Settings->Now += 15.;
	TestFalse(TEXT("Timeout ends preview without a render device"), FSovPlatformOutputTestAccess::TickPreview(Settings));
	Settings->SaveSettings();
	TestFalse(TEXT("Later save after unavailable timeout still persists confirmed SDR"), Settings->bSavedEnabled);
	TestEqual(TEXT("Later save after unavailable timeout still persists confirmed nits"), Settings->SavedNits, 1000);
	Settings->bAvailable = true; Settings->InitializeOutput(false, 1000); Settings->bLoseOutputAfterWrite = true;
	TestFalse(TEXT("Output disappearing during preview application fails transaction"), Settings->PreviewHDRCalibration(true, 2000, Receipt, Error));
	Settings->SaveSettings();
	TestFalse(TEXT("Failed preview with unavailable rollback preserves confirmed enable"), Settings->bSavedEnabled);
	TestEqual(TEXT("Failed preview with unavailable rollback preserves confirmed nits"), Settings->SavedNits, 1000);
	Settings->bAvailable = true; Settings->InitializeOutput(true, 2000);
	TestTrue(TEXT("SDR preview starts from confirmed HDR"), Settings->PreviewHDRCalibration(false, 1000, Receipt, Error));
	Settings->bAvailable = false;
	TestFalse(TEXT("Unavailable output cannot confirm even when default SDR values match"), Settings->ConfirmHDRCalibration(Receipt, Error));
	Settings->SaveSettings();
	TestTrue(TEXT("Rejected SDR confirmation preserves confirmed HDR preference"), Settings->bSavedEnabled);
	TestEqual(TEXT("Rejected SDR confirmation preserves confirmed peak"), Settings->SavedNits, 2000);
	Settings->bAvailable = true; Settings->InitializeOutput(true, 2000); Settings->bLoseOutputAfterWrite = true;
	TestFalse(TEXT("Output disappearing during SDR apply cannot admit a preview"), Settings->PreviewHDRCalibration(false, 1000, Receipt, Error));
	Settings->SaveSettings();
	TestTrue(TEXT("Failed SDR apply preserves confirmed HDR preference"), Settings->bSavedEnabled);
	TestEqual(TEXT("Failed SDR apply preserves confirmed peak"), Settings->SavedNits, 2000);
	Settings->bAvailable = true; Settings->InitializeOutput(true, 2000);
	TestTrue(TEXT("SDR capability-change case starts"), Settings->PreviewHDRCalibration(false, 1000, Receipt, Error));
	Settings->bSupported = false;
	TestFalse(TEXT("Capability change invalidates SDR preview despite unchanged false/zero output"), Settings->ConfirmHDRCalibration(Receipt, Error));
	Settings->SaveSettings();
	TestTrue(TEXT("Capability fallback does not overwrite confirmed preference"), Settings->bSavedEnabled);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHapticOutputRuntime, "ProjectVelkorran.Campaign.PlatformOutput.HapticOwnershipAndAccessibility", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovHapticOutputRuntime::RunTest(const FString& Parameters)
{
	auto* Feedback = NewObject<USovHapticOutputTestComponent>();
	Feedback->Value.Combat = .5f;
	const int64 First = Feedback->PlayFeedback(ESovHapticChannel::Combat, 1.f, 1.f, 10);
	TestTrue(TEXT("Valid owned receipt"), First > 0);
	TestEqual(TEXT("Native output applies channel setting"), Feedback->Outputs[0], .5f);
	Feedback->Value.Master = 0.f;
	Feedback->CancelFeedback(First);
	TestEqual(TEXT("Muted output immediately stops"), Feedback->Outputs[0], 0.f);
	TestEqual(TEXT("Disabled channel cannot queue deferred rumble"), Feedback->PlayFeedback(ESovHapticChannel::Combat, 1.f, 1.f), int64(0));
	Feedback->Value.Master = 1.f;
	const int64 Second = Feedback->PlayFeedback(ESovHapticChannel::Combat, 1.f, 1.f, 10);
	Feedback->CancelAllFeedback();
	const int64 Third = Feedback->PlayFeedback(ESovHapticChannel::Combat, .4f, 1.f, 10);
	TestTrue(TEXT("Receipts never reused after reset"), Third > Second);
	TestFalse(TEXT("Outgoing owner cannot cancel incoming pulse"), Feedback->CancelFeedback(Second));
	TestEqual(TEXT("Incoming pulse survives stale cancellation"), Feedback->Outputs[0], .2f);
	Feedback->bCanOutput = false;
	Feedback->CancelFeedback(Third);
	TestEqual(TEXT("Suppression stops owned hardware channels"), Feedback->Outputs[0], 0.f);
	TestEqual(TEXT("Suppression rejects requests"), Feedback->PlayFeedback(ESovHapticChannel::UI, 1.f, 1.f), int64(0));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHapticSettingsRuntime, "ProjectVelkorran.Campaign.PlatformOutput.LocalSettingsPrivacy", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovHapticSettingsRuntime::RunTest(const FString& Parameters)
{
	auto* Settings = NewObject<USovPlatformOutputTestSettings>(); FString Error;
	FSovHapticSettings Value; Value.Combat = 0.f; Value.Master = .5f;
	TestTrue(TEXT("Channel choice saved"), Settings->ApplyHapticSettings(Value, Error));
	TArray<uint8> Bytes; TestTrue(TEXT("Gameplay snapshot still available"), Settings->CapturePortableSettings(Bytes));
	TestEqual(TEXT("Existing portable byte schema unchanged"), Bytes.Num(), 11);
	TestTrue(TEXT("Gameplay import accepted"), Settings->RestorePortableSettings(Bytes, Error));
	TestEqual(TEXT("Import preserves local muted combat channel"), Settings->GetHapticSettings().Combat, 0.f);
	Value.UI = std::numeric_limits<float>::quiet_NaN();
	const int32 SavesBefore = Settings->Saves;
	TestFalse(TEXT("Nonfinite channel rejects atomic transaction"), Settings->ApplyHapticSettings(Value, Error));
	TestEqual(TEXT("Invalid settings not persisted"), Settings->Saves, SavesBefore);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHapticEventRuntime, "ProjectVelkorran.Campaign.PlatformOutput.CommittedEventProducers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovHapticEventRuntime::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World) { return false; }
	if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
	World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false));
	auto* PC = World->SpawnActor<ASovInputRoutingTestController>();
	auto* Pawn = World->SpawnActor<APawn>();
	if (!PC || !Pawn) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } return false; }
	PC->TestASC = NewObject<UNarrativeAbilitySystemComponent>(PC); PC->TestASC->RegisterComponent();
	PC->Possess(Pawn); PC->TestASC->InitAbilityActorInfo(PC, Pawn); PC->TestASC->SetCharacterReadyEpoch(1);
	auto* Feedback = NewObject<USovHapticOutputTestComponent>(PC); Feedback->RegisterComponent(); Feedback->RefreshSources();
	FSovDamageResult Damage; Damage.TargetActor = Pawn; Damage.AppliedHealthDamage = 5.f;
	PC->TestASC->OnDamageResolvedAsTarget.Broadcast(Damage);
	TestTrue(TEXT("Committed target damage generates combat feedback"), Feedback->Outputs[0] > 0.f);
	Feedback->CancelAllFeedback();
	Damage.TargetActor = PC;
	PC->TestASC->OnDamageResolvedAsTarget.Broadcast(Damage);
	TestEqual(TEXT("Wrong avatar event is rejected"), Feedback->Outputs[0], 0.f);
	auto* InteractableActor = World->SpawnActor<AActor>();
	auto* Interactable = NewObject<UNarrativeInteractableComponent>(InteractableActor); Interactable->RegisterComponent();
	PC->GetInteractionComponent()->GetOnBeginUseInteractable().Broadcast(InteractableActor, Interactable);
	TestTrue(TEXT("Admitted interaction event generates feedback"), Feedback->Outputs[1] > 0.f);
	auto* Sequence = World->SpawnActor<ANarrativeLevelSequenceActor>(); FNarrativeSequencePlaybackSettings Playback;
	PC->LevelSequencePlayed(Sequence, Playback);
	TestEqual(TEXT("Cinematic takeover cancels gameplay channels"), Feedback->Outputs[1], 0.f);
	TestTrue(TEXT("Actual cinematic lifecycle generates brief transition feedback"), Feedback->Outputs[2] > 0.f);
	PC->LevelSequenceStopped(Sequence, Playback);
	TestEqual(TEXT("Matching sequence stop cancels its receipt"), Feedback->Outputs[2], 0.f);
	Damage.TargetActor = Pawn; PC->TestASC->OnDamageResolvedAsTarget.Broadcast(Damage);
	PC->TestASC->OnDeathStateChanged.Broadcast(Pawn, PC->TestASC, true);
	TestEqual(TEXT("Death stops all owned output"), Feedback->Outputs[0], 0.f);
	PC->TestASC->OnDamageResolvedAsTarget.Broadcast(Damage);
	PC->TestASC->SetCharacterReadyEpoch(2);
	TestEqual(TEXT("Same-ASC restore readiness invalidates outgoing requests"), Feedback->Outputs[0], 0.f);
	PC->UnPossess(); Feedback->RefreshSources();
	PC->TestASC->OnDamageResolvedAsTarget.Broadcast(Damage);
	TestEqual(TEXT("Unpossessed avatar cannot produce feedback"), Feedback->Outputs[0], 0.f);
	Feedback->Deactivate();
	World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); }
	return true;
}
#endif
