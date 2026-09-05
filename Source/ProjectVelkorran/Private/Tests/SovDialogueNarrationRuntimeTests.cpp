// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Accessibility/SovAccessibleNarrationSubsystem.h"
#include "UI/Dialogue/SovDialogueChoiceWidget.h"
#include "UI/Dialogue/SovDialoguePresentationComponent.h"
#include "UI/Dialogue/SovDialoguePresentationState.h"
#include "Tests/SovDialogueRuntimeTestFixtures.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
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
		UWorld* World;
		APlayerController* PC;
		UTalesComponent* Tales;
		FDialogueWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
				.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
			PC = World->SpawnActor<APlayerController>();
			Tales = NewObject<UTalesComponent>(PC); Tales->RegisterComponent();
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
	ULocalPlayer* Player = NewObject<ULocalPlayer>();
	USovAccessibleNarrationSubsystem* Narration = NewObject<USovAccessibleNarrationSubsystem>(Player);
	UObject* OwnerA = NewObject<UObject>(); UObject* OwnerB = NewObject<UObject>();
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
	ULocalPlayer* Player = NewObject<ULocalPlayer>();
	USovAccessibleNarrationSubsystem* Narration = NewObject<USovAccessibleNarrationSubsystem>(Player);
	UObject* OldOwner = NewObject<UObject>(); UObject* NewOwner = NewObject<UObject>();
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
#endif
