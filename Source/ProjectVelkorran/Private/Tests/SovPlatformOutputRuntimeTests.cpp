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
};
#if WITH_DEV_AUTOMATION_TESTS
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
