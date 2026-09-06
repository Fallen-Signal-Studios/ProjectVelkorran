// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovRuntimeObjectTestFixtures.h"
#include "Accessibility/SovAccessibleNarrationSubsystem.h"
#include "UI/Dialogue/SovDialogueChoiceWidget.h"
#include "UI/Dialogue/SovDialoguePresentationComponent.h"
#include "UI/Dialogue/SovDialoguePresentationState.h"
#include "Tests/SovDialogueRuntimeTestFixtures.h"
#include "Tests/SovFrontendRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "UI/SovNativeGameplayHUD.h"
#include "NarrativeGameplayTags.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "ICommonInputModule.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
struct FSovNarrationTestAccess
{
	static void SetFactory(USovAccessibleNarrationSubsystem* S, TFunction<TSharedPtr<ISovAccessibleSpeech>()> Factory)
	{ S->TestBackendFactory = MoveTemp(Factory); }
};

struct FSovDialogueTestAccess
{
	static int32 ButtonCount(USovDialogueChoiceWidget* W) { return W->Buttons.Num(); }
	static void ConfigureWithoutHUD(USovDialoguePresentationComponent* C, UTalesComponent* Tales, UDialogue* Dialogue)
	{ C->Tales = Tales; C->OnReplies(Dialogue, Dialogue->AvailableResponses); }
	static bool HasLivePressure(USovDialoguePresentationComponent* C) { return C->State->Pressure.Active; }
	static USovDialogueChoiceWidget* Widget(USovDialoguePresentationComponent* C) { return C->ChoiceWidget; }
	static void Tick(USovDialoguePresentationComponent* C) { C->TickComponent(.1f, LEVELTICK_All, nullptr); }
};

namespace
{
	struct FTestSpeech final : ISovAccessibleSpeech
	{
		FSimpleDelegate Finished;
		int32 Stops = 0;
		bool bAccept = true;
		TFunction<void()> DuringSpeak;
		bool Speak(const FString&, FSimpleDelegate InFinished) override
		{ Finished = MoveTemp(InFinished); if (DuringSpeak) { DuringSpeak(); } return bAccept; }
		// Keep the old delegate deliberately, modelling a callback already queued by the platform.
		void Stop() override { ++Stops; }
	};
	struct FDialogueWorld
	{
		TStrongObjectPtr<ULocalPlayer> LocalPlayer{NewObject<ULocalPlayer>(GEngine)};
		UWorld* World;
		APlayerController* PC;
		UTalesComponent* Tales;
		FDialogueWorld()
		{
			ICommonInputModule::GetSettings().LoadData();
			const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
				.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			PC = World->SpawnActor<APlayerController>();
			MakeLocal(PC);
			Tales = NewObject<UTalesComponent>(PC); Tales->RegisterComponent();
		}
		void MakeLocal(APlayerController* Controller)
		{
			// UPlayer resolves its controller through the world's registered controllers.
			// Transfer the previous association instead of leaving two owners for one player.
			if (APlayerController* Previous = LocalPlayer->PlayerController)
			{ if (Previous != Controller && Previous->Player == LocalPlayer.Get()) { Previous->Player = nullptr; } }
			Controller->Player = LocalPlayer.Get(); LocalPlayer->PlayerController = Controller;
			Controller->SetAsLocalPlayerController(); World->AddController(Controller);
		}
		~FDialogueWorld()
		{
			Tales->CurrentDialogue = nullptr;
			World->DestroyWorld(false);
			if (GEngine) { GEngine->DestroyWorldContext(World); }
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNarrationCompletionTest, "ProjectVelkorran.UI.Dialogue.NarrationCompletionOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNarrationCompletionTest::RunTest(const FString&)
{
	ULocalPlayer* Player = NewObject<ULocalPlayer>(GEngine);
	USovAccessibleNarrationSubsystem* Narration = NewObject<USovAccessibleNarrationSubsystem>(Player);
	UObject* OwnerA = NewObject<USovRuntimeTestIdentity>(); UObject* OwnerB = NewObject<USovRuntimeTestIdentity>();
	TArray<TSharedPtr<FTestSpeech>> Backends;
	FSovNarrationTestAccess::SetFactory(Narration, [&Backends]()
	{
		TSharedPtr<FTestSpeech> Backend = MakeShared<FTestSpeech>(); Backends.Add(Backend);
		return StaticCastSharedPtr<ISovAccessibleSpeech>(Backend);
	});
	int32 Completed = 0, Interrupted = 0;
	FSovNarrationCompletion Completion = FSovNarrationCompletion::CreateLambda([&](FGuid, bool bCompleted)
	{ bCompleted ? ++Completed : ++Interrupted; });
	FGuid A, B;
	TestTrue(TEXT("First real subsystem request accepted by deterministic backend"), Narration->Announce(OwnerA, FText::FromString(TEXT("A")), A, Completion));
	TestTrue(TEXT("Replacement accepted"), Narration->Announce(OwnerB, FText::FromString(TEXT("B")), B, Completion));
	TestEqual(TEXT("Replacement reports cancellation, never reading completion"), Interrupted, 1);
	Backends[0]->Finished.ExecuteIfBound();
	TestEqual(TEXT("Queued stale platform completion cannot complete current request"), Completed, 0);
	Narration->Cancel(OwnerA);
	TestEqual(TEXT("Unrelated owner cancellation cannot stop active request"), Backends[1]->Stops, 0);
	Backends[1]->Finished.ExecuteIfBound(); Backends[1]->Finished.ExecuteIfBound();
	TestEqual(TEXT("Successful request completes exactly once"), Completed, 1);
	FGuid Reentered;
	bool bReentryAccepted = false;
	Narration->Announce(OwnerA, FText::FromString(TEXT("Old")), A,
		FSovNarrationCompletion::CreateLambda([&](FGuid, bool bCompleted)
		{
			if (!bCompleted) { bReentryAccepted = Narration->Announce(OwnerA, FText::FromString(TEXT("Newer")), Reentered, Completion); }
		}));
	TestFalse(TEXT("A cancellation callback's newer request supersedes outer request"),
		Narration->Announce(OwnerB, FText::FromString(TEXT("Obsolete")), B, Completion));
	TestTrue(TEXT("Reentrant new request survives"), bReentryAccepted);
	Narration->Deinitialize();
	Backends.Last()->Finished.ExecuteIfBound();
	TestEqual(TEXT("Teardown cannot generate successful completion"), Completed, 1);
	FGuid Rejected;
	TestFalse(TEXT("Subsystem teardown rejects further speech"), Narration->Announce(OwnerA, FText::FromString(TEXT("Late")), Rejected));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNarrationFactoryReentryTest, "ProjectVelkorran.UI.Dialogue.NarrationFactoryAndSpeakReentry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNarrationFactoryReentryTest::RunTest(const FString&)
{
	ULocalPlayer* Player = NewObject<ULocalPlayer>(GEngine);
	USovAccessibleNarrationSubsystem* Narration = NewObject<USovAccessibleNarrationSubsystem>(Player);
	UObject* OldOwner = NewObject<USovRuntimeTestIdentity>(); UObject* NewOwner = NewObject<USovRuntimeTestIdentity>();
	FGuid SharedReceipt;
	int32 Creates = 0, Successful = 0;
	TArray<TSharedPtr<FTestSpeech>> Backends;
	const FSovNarrationCompletion Completed = FSovNarrationCompletion::CreateLambda([&](FGuid, bool bSuccess) { if (bSuccess) { ++Successful; } });
	FSovNarrationTestAccess::SetFactory(Narration, [&]()
	{
		const int32 Index = Creates++;
		TSharedPtr<FTestSpeech> B = MakeShared<FTestSpeech>(); Backends.Add(B);
		if (Index == 0) { Narration->Announce(NewOwner, FText::FromString(TEXT("Factory successor")), SharedReceipt, Completed); }
		return StaticCastSharedPtr<ISovAccessibleSpeech>(B);
	});
	TestFalse(TEXT("Factory callback's successor invalidates the outer start"),
		Narration->Announce(OldOwner, FText::FromString(TEXT("Outer factory")), SharedReceipt));
	TestTrue(TEXT("Outer start does not invalidate the successor receipt"), SharedReceipt.IsValid());
	Backends[1]->Finished.ExecuteIfBound();
	TestEqual(TEXT("Factory successor completes through the real subsystem"), Successful, 1);
	Creates = 0; Backends.Reset();
	FSovNarrationTestAccess::SetFactory(Narration, [&]()
	{
		const int32 Index = Creates++;
		TSharedPtr<FTestSpeech> B = MakeShared<FTestSpeech>(); Backends.Add(B);
		if (Index == 0)
		{
			B->bAccept = false;
			B->DuringSpeak = [&]() { Narration->Announce(NewOwner, FText::FromString(TEXT("Speech successor")), SharedReceipt, Completed); };
		}
		return StaticCastSharedPtr<ISovAccessibleSpeech>(B);
	});
	TestFalse(TEXT("Failed old Speak returns without claiming successor success"),
		Narration->Announce(OldOwner, FText::FromString(TEXT("Outer speak")), SharedReceipt));
	TestTrue(TEXT("Failed old Speak cannot erase a successor receipt in the same output reference"), SharedReceipt.IsValid());
	Backends[0]->Finished.ExecuteIfBound();
	TestEqual(TEXT("Old Speak completion remains stale"), Successful, 1);
	Backends[1]->Finished.ExecuteIfBound();
	TestEqual(TEXT("Speech successor finishes once"), Successful, 2);
	Narration->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNarrationApplicationSuspendTest, "ProjectVelkorran.Platform.Lifecycle.NarrationSuspension",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNarrationApplicationSuspendTest::RunTest(const FString&)
{
	ULocalPlayer* Player = NewObject<ULocalPlayer>(GEngine);
	auto* Narration = NewObject<USovAccessibleNarrationSubsystem>(Player);
	auto* Owner = NewObject<USovRuntimeTestIdentity>(); TSharedPtr<FTestSpeech> Backend = MakeShared<FTestSpeech>();
	FSovNarrationTestAccess::SetFactory(Narration, [Backend]() { return StaticCastSharedPtr<ISovAccessibleSpeech>(Backend); });
	int32 Completed = 0, Cancelled = 0; bool bRestartAccepted = false; FGuid Request;
	Narration->Announce(Owner, FText::FromString(TEXT("Before suspend")), Request,
		FSovNarrationCompletion::CreateLambda([&](FGuid, bool bCompleted)
		{
			if (bCompleted) { ++Completed; } else
			{
				++Cancelled; FGuid NewRequest;
				bRestartAccepted = Narration->Announce(Owner, FText::FromString(TEXT("Reentrant speech")), NewRequest);
			}
		}));
	Narration->SetApplicationSuspended(true); Narration->SetApplicationSuspended(true);
	TestEqual(TEXT("Suspend cancels narration once"), Cancelled, 1);
	TestFalse(TEXT("Cancellation callback cannot speak over platform UI"), bRestartAccepted);
	Backend->Finished.ExecuteIfBound();
	TestEqual(TEXT("Queued platform completion does not report a finished reading after suspend"), Completed, 0);
	TestFalse(TEXT("Suspended subsystem rejects further requests"), Narration->Announce(Owner, FText::FromString(TEXT("Hidden")), Request));
	Narration->SetApplicationSuspended(false);
	TestTrue(TEXT("Foreground can narrate the explicit resume screen"), Narration->Announce(Owner, FText::FromString(TEXT("Resume game")), Request));
	Narration->Deinitialize(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDialogueRevisionTest, "ProjectVelkorran.UI.Dialogue.TalesPresentationRevision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDialogueRevisionTest::RunTest(const FString&)
{
	FDialogueWorld W;
	USovDialogueRuntimeFixture* Dialogue = NewObject<USovDialogueRuntimeFixture>(W.PC); Dialogue->Stage(W.Tales);
	USovDialogueRuntimeProbe* Probe = NewObject<USovDialogueRuntimeProbe>(W.PC);
	W.Tales->OnDialogueRepliesAvailable.AddDynamic(Probe, &USovDialogueRuntimeProbe::Replies);
	TestFalse(TEXT("Available graph responses are not yet presented choices"), Dialogue->AreRepliesPresented());
	TestFalse(TEXT("Early UI selection rejected"), W.Tales->TrySelectPresentedDialogueOption(Dialogue, 0, Dialogue->Choice(0)));
	Dialogue->NPCFinishedTalking();
	TestEqual(TEXT("Actual Tales replies-available event fires"), Probe->Notifications, 1);
	TestTrue(TEXT("Event carries current reply revision"), Dialogue->IsCurrentReplyPresentation(Probe->Revision));
	const int64 PriorRevision = Probe->Revision;
	Dialogue->NPCFinishedTalking();
	TestFalse(TEXT("Same node and replies do not reuse the old revision"), Dialogue->IsCurrentReplyPresentation(PriorRevision));
	TestFalse(TEXT("Stale timeout selection is rejected"), W.Tales->TrySelectPresentedDialogueOption(Dialogue, PriorRevision, Dialogue->Choice(1)));
	USovDialoguePresentationComponent* Presentation = NewObject<USovDialoguePresentationComponent>(W.PC);
	FSovDialogueTestAccess::ConfigureWithoutHUD(Presentation, W.Tales, Dialogue);
	TestFalse(TEXT("No native HUD/text presentation means no live pressure"), FSovDialogueTestAccess::HasLivePressure(Presentation));
	USovDialogueRuntimeFixture* Replacement = NewObject<USovDialogueRuntimeFixture>(W.PC); Replacement->Stage(W.Tales);
	TestFalse(TEXT("Replaced dialogue cannot accept old callback even with matching node IDs"),
		W.Tales->TrySelectPresentedDialogueOption(Dialogue, Probe->Revision, Dialogue->Choice(1)));
	Dialogue->RetireRevision();
	TestFalse(TEXT("Retired presentation cannot select"), Dialogue->AreRepliesPresented());
	Replacement->NPCFinishedTalking();
	const int64 CurrentRevision = Replacement->GetReplyPresentationRevision();
	TestTrue(TEXT("Live silence selection uses the real Tales authority path"),
		W.Tales->TrySelectPresentedDialogueOption(Replacement, CurrentRevision, Replacement->Choice(1)));
	TestTrue(TEXT("Existing graph owns the selected silence node"), Replacement->GetCurrentNode() == Replacement->Choice(1));
	TestTrue(TEXT("Selection consumes the existing response set"), Replacement->AvailableResponses.IsEmpty());
	TestFalse(TEXT("Duplicate silence cannot commit the same presentation twice"),
		W.Tales->TrySelectPresentedDialogueOption(Replacement, CurrentRevision, Replacement->Choice(1)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNativeDialogueWidgetTest, "ProjectVelkorran.UI.Dialogue.NativeWidgetTree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNativeDialogueWidgetTest::RunTest(const FString&)
{
	USovDialogueChoiceWidget* Widget = NewObject<USovDialogueChoiceWidget>();
	Widget->Initialize();
	Widget->TakeWidget();
	Widget->Present(FText::FromString(TEXT("Selene")), {FText::FromString(TEXT("Examine the evidence.")), FText::FromString(TEXT("Remain silent."))}, 2.f, true);
	TestTrue(TEXT("Native widget creates its tree without an authored widget asset"), Widget->WidgetTree && Widget->WidgetTree->RootWidget);
	TestEqual(TEXT("Real CommonUI native buttons exist for all choices"), FSovDialogueTestAccess::ButtonCount(Widget), 2);
	TestTrue(TEXT("Native menu retains inherited wrap policy"), Widget->IsMenuNavigationWrapEnabled());
	TestFalse(TEXT("An unlaid-out native tree is not fabricated reading readiness"), Widget->IsTextPresented());
	Widget->Retire();
	TestFalse(TEXT("Removed presentation cannot become ready"), Widget->IsTextPresented());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPausedDialogueCompletionTest, "ProjectVelkorran.UI.Dialogue.QueuedMediaCompletionAcrossWorldPause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPausedDialogueCompletionTest::RunTest(const FString&)
{
	FDialogueWorld W; auto* Dialogue = NewObject<USovDialogueRuntimeFixture>(W.PC); Dialogue->Stage(W.Tales);
	UDialogueLineCompletionToken* Token = Dialogue->CompletionToken();
	APlayerState* Pauser = W.World->SpawnActor<APlayerState>();
	W.World->GetWorldSettings()->SetPauserPlayerState(Pauser);
	TestTrue(TEXT("Fixture uses actual world pause independent of dialogue suspension"), W.World->IsPaused());
	Token->Complete(); Token->Complete(); Dialogue->TickDialogue(.5f);
	TestTrue(TEXT("Queued media completion remains owned by its line while paused"), Dialogue->HasDeferredCompletion());
	TestEqual(TEXT("Pause cannot execute graph completion"), Dialogue->FinishedLines, 0);
	TestFalse(TEXT("Pause cannot expose the next choices"), Dialogue->AreRepliesPresented());
	W.World->GetWorldSettings()->SetPauserPlayerState(nullptr);
	Dialogue->TickDialogue(.1f);
	TestEqual(TEXT("Foreground drains exactly one owned completion"), Dialogue->FinishedLines, 1);
	TestTrue(TEXT("Native graph progresses to its actual reply presentation"), Dialogue->AreRepliesPresented());
	Token->Complete(); Dialogue->TickDialogue(.1f);
	TestEqual(TEXT("Duplicate queued callbacks cannot finish a successor"), Dialogue->FinishedLines, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRetiredDialogueCompletionTest, "ProjectVelkorran.UI.Dialogue.RetiredMediaCannotFinishReusedNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovRetiredDialogueCompletionTest::RunTest(const FString&)
{
	FDialogueWorld W; auto* Dialogue = NewObject<USovDialogueRuntimeFixture>(W.PC); Dialogue->Stage(W.Tales);
	UDialogueLineCompletionToken* Retired = Dialogue->CompletionToken();
	TestTrue(TEXT("Authored walk-and-talk suspension accepted"), Dialogue->SetPlaybackSuspended(true));
	Retired->Complete(); TestTrue(TEXT("Explicit suspension retains completion too"), Dialogue->HasDeferredCompletion());
	Dialogue->RestartPrompt();
	TestFalse(TEXT("Restart retires the deferred predecessor before reusing its node"), Dialogue->HasDeferredCompletion());
	Dialogue->SetPlaybackSuspended(false); Retired->Complete(); Dialogue->TickDialogue(.1f);
	TestEqual(TEXT("Same node identity cannot rescue a retired line revision"), Dialogue->FinishedLines, 0);
	Dialogue->CompletionToken()->Complete();
	TestEqual(TEXT("Current line still completes through the production graph"), Dialogue->FinishedLines, 1);
	auto* Replacement = NewObject<USovDialogueRuntimeFixture>(W.PC); Replacement->Stage(W.Tales);
	Retired->Complete();
	TestEqual(TEXT("Old dialogue cannot complete replacement owner"), Replacement->FinishedLines, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDialogueWidgetRecoveryTest, "ProjectVelkorran.UI.Dialogue.RemovedWidgetRebuildsCurrentChoices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDialogueWidgetRecoveryTest::RunTest(const FString&)
{
	FDialogueWorld W;
	auto* PC = W.World->SpawnActor<ASovFrontendRuntimeController>(); W.MakeLocal(PC);
	TestTrue(TEXT("Local player resolves the current controller for the actual HUD"), W.LocalPlayer->GetPlayerController(W.World) == PC);
	auto* Player = W.World->SpawnActor<ASovHandoffRuntimeTestPawn>();
	if (!TestNotNull(TEXT("Dialogue owner has an actual Narrative player"), Player)) { return false; }
	PC->StagePawn(Player); PC->SetOwnedCharacter(Player);
	TestTrue(TEXT("Native menu tag checks use the associated player"), PC->GetOwnedCharacter() == Player);
	TestFalse(TEXT("Associated player removes the controller's unavailable-owner tag guard"),
		PC->HasAnyMatchingGameplayTags(FGameplayTagContainer()));
	auto* HUD = NewObject<USovNativeGameplayHUD>(PC); HUD->SetOwningPlayer(PC); HUD->Initialize();
	// The viewport normally owns this Slate tree. Keep that ownership in the headless fixture.
	const TSharedRef<SWidget> HUDSlate = HUD->TakeWidget(); HUD->NativeConstruct(); PC->StageHUD(HUD);
	auto* GameLayer = HUD->GetLayerContainer(FNarrativeGameplayTags::Get().UI_Layer_Game);
	if (!TestNotNull(TEXT("Native HUD registered its actual game layer"), GameLayer)) { return false; }
	// This synchronous headless fixture does not tick Slate's cosmetic animation.
	// Use the public instant transition so native activation/removal completes normally.
	GameLayer->SetTransitionDuration(0.f);
	W.Tales = PC->GetTalesComponent();
	auto* Dialogue = NewObject<USovDialogueRuntimeFixture>(PC); Dialogue->Stage(W.Tales); Dialogue->NPCFinishedTalking();
	auto* Presentation = NewObject<USovDialoguePresentationComponent>(PC);
	PC->AddInstanceComponent(Presentation); Presentation->RegisterComponent();
	TestTrue(TEXT("Recovery ticks a registered presentation component"), Presentation->IsRegistered());
	FSovDialogueTestAccess::ConfigureWithoutHUD(Presentation, W.Tales, Dialogue);
	auto* Initial = FSovDialogueTestAccess::Widget(Presentation);
	if (!TestNotNull(TEXT("Real native game layer accepts the choice widget"), Initial)) { return false; }
	TestTrue(TEXT("Native stack activates the initial choice widget"), Initial->IsActivated());
	const int64 Revision = Dialogue->GetReplyPresentationRevision();
	Initial->DeactivateWidget();
	TestNull(TEXT("Removed widget retires its owned presentation"), FSovDialogueTestAccess::Widget(Presentation));
	TestNull(TEXT("Native stack completes the outgoing widget transition"), GameLayer->GetActiveWidget());
	FSovDialogueTestAccess::Tick(Presentation);
	auto* Restored = FSovDialogueTestAccess::Widget(Presentation);
	TestNotNull(TEXT("Unchanged graph revision reconstructs a removed widget"), Restored);
	TestEqual(TEXT("Presentation recovery cannot advance the narrative graph"), Dialogue->GetReplyPresentationRevision(), Revision);
	if (Restored) { TestEqual(TEXT("Recreated native widget retains all choices"), FSovDialogueTestAccess::ButtonCount(Restored), 2); Restored->DeactivateWidget(); }
	Dialogue->RetireRevision(); FSovDialogueTestAccess::Tick(Presentation);
	TestNull(TEXT("Retired graph choices cannot be resurrected"), FSovDialogueTestAccess::Widget(Presentation));
	PC->StageHUD(nullptr);
	return true;
}
#endif
