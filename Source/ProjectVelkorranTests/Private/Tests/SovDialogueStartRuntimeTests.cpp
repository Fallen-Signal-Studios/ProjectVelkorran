// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovDialogueStartRuntimeTestFixtures.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"

float USovDialogueStartRuntimeFixture::Remaining(bool bNPC) const
{
	// Inspect the actual world's timer even after Deinitialize has cleared OwningComp/GetWorld.
	return GetOuter()->GetWorld()->GetTimerManager().GetTimerRemaining(bNPC ? TimerHandle_NPCReplyFinished : TimerHandle_PlayerReplyFinished);
}

bool USovDialogueStartRuntimeFixture::IsTimerPaused(bool bNPC) const
{
	return GetOuter()->GetWorld()->GetTimerManager().IsTimerPaused(bNPC ? TimerHandle_NPCReplyFinished : TimerHandle_PlayerReplyFinished);
}

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	struct FStartWorld
	{
		UWorld* World = nullptr;
		APlayerController* PC = nullptr;
		UTalesComponent* Tales = nullptr;
		USovDialogueStartRuntimeFixture* Dialogue = nullptr;
		FStartWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
				.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
			PC = World->SpawnActor<APlayerController>();
			Tales = NewObject<UTalesComponent>(PC); Tales->RegisterComponent();
			Dialogue = NewObject<USovDialogueStartRuntimeFixture>(PC); Dialogue->Stage(Tales);
		}
		~FStartWorld()
		{
			Tales->CurrentDialogue = nullptr;
			World->DestroyWorld(false);
			if (GEngine) { GEngine->DestroyWorldContext(World); }
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDialogueStartExitTest, "ProjectVelkorran.UI.Dialogue.StartCallbackExitRetiresContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDialogueStartExitTest::RunTest(const FString&)
{
	{
		FStartWorld W;
		auto* Probe = NewObject<USovDialogueStartRuntimeProbe>(W.PC);
		W.Tales->OnPlayerDialogueLineStarted.AddDynamic(Probe, &USovDialogueStartRuntimeProbe::PlayerStarted);
		W.Dialogue->StartPlayer(W.Dialogue->Choice(0));
		TestNull(TEXT("Actual Tales exit retires the dialogue"), W.Tales->GetCurrentDialogue());
		TestEqual(TEXT("Exit listener prevents subsequent Blueprint start hook"), W.Dialogue->PlayerHooks, 0);
		TestEqual(TEXT("Exited player line never starts its media"), W.Dialogue->PlayerMedia, 0);
		TestEqual(TEXT("Exited player line never queries duration"), W.Dialogue->DurationCalls, 0);
		TestTrue(TEXT("Exited player line cannot install a completion timer"), W.Dialogue->Remaining(false) < 0.f);
	}
	{
		FStartWorld W;
		auto* Probe = NewObject<USovDialogueStartRuntimeProbe>(W.PC); Probe->bExitNPC = true;
		W.Tales->OnNPCDialogueLineStarted.AddDynamic(Probe, &USovDialogueStartRuntimeProbe::NPCStarted);
		W.Dialogue->StartNPC();
		TestNull(TEXT("NPC start listener can exit through the real owner"), W.Tales->GetCurrentDialogue());
		TestEqual(TEXT("Exited NPC line never queries duration"), W.Dialogue->DurationCalls, 0);
		TestTrue(TEXT("Exited NPC line cannot install a completion timer"), W.Dialogue->Remaining(true) < 0.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDialogueSameNodeRestartTest, "ProjectVelkorran.UI.Dialogue.StartCallbackSameNodeRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDialogueSameNodeRestartTest::RunTest(const FString&)
{
	{
		FStartWorld W;
		UDialogueNode_Player* SameNode = W.Dialogue->Choice(0);
		W.Dialogue->DuringPlayerHook = [&]() { W.Dialogue->StartPlayer(SameNode); };
		W.Dialogue->StartPlayer(SameNode);
		TestEqual(TEXT("Same node was intentionally started twice"), W.Dialogue->PlayerHooks, 2);
		TestEqual(TEXT("Only the successor starts player media"), W.Dialogue->PlayerMedia, 1);
		TestEqual(TEXT("Only the successor queries duration"), W.Dialogue->DurationCalls, 1);
		TestTrue(TEXT("Successor retains its timer"), W.Dialogue->Remaining(false) > 9.f);
	}
	{
		FStartWorld W;
		auto* Probe = NewObject<USovDialogueStartRuntimeProbe>(W.PC);
		W.Tales->OnNPCDialogueLineStarted.AddDynamic(Probe, &USovDialogueStartRuntimeProbe::NPCStarted);
		W.Dialogue->DuringNPCMedia = [&]() { W.Dialogue->StartNPC(); };
		W.Dialogue->StartNPC();
		TestEqual(TEXT("Only current NPC start is published after same-node media reentry"), Probe->NPCStarts, 1);
		TestEqual(TEXT("Obsolete NPC start never queries duration"), W.Dialogue->DurationCalls, 1);
		TestTrue(TEXT("Current NPC timer survives"), W.Dialogue->Remaining(true) > 9.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDialogueDurationRestartTest, "ProjectVelkorran.UI.Dialogue.DurationCallbackCannotOverwriteSuccessorTimer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDialogueDurationRestartTest::RunTest(const FString&)
{
	for (bool bNPC : { false, true })
	{
		FStartWorld W;
		UDialogueNode_Player* SameNode = W.Dialogue->Choice(0);
		W.Dialogue->Duration = 0.1f;
		W.Dialogue->DuringDuration = [&]()
		{
			W.Dialogue->Duration = 20.f;
			if (bNPC) { W.Dialogue->StartNPC(); } else { W.Dialogue->StartPlayer(SameNode); }
		};
		if (bNPC) { W.Dialogue->StartNPC(); } else { W.Dialogue->StartPlayer(SameNode); }
		TestEqual(TEXT("Both actual duration callbacks ran"), W.Dialogue->DurationCalls, 2);
		TestTrue(TEXT("Obsolete short duration does not replace the successor's full timer"), W.Dialogue->Remaining(bNPC) > 19.f);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDialogueVariableStartTest, "ProjectVelkorran.UI.Dialogue.VariableFormattingOwnershipAndDistinctKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDialogueVariableStartTest::RunTest(const FString&)
{
	{
		FStartWorld W;
		W.Dialogue->Choice(0)->Line.Text = FText::FromString(TEXT("{Hero} faces {Enemy}; {Hero} persists."));
		W.Dialogue->StartPlayer(W.Dialogue->Choice(0));
		TestEqual(TEXT("All distinct variables are substituted"), W.Dialogue->CurrentText(), FString(TEXT("Selene faces Dominion; Selene persists.")));
		TestEqual(TEXT("Repeated key invokes provider exactly once"), W.Dialogue->VariableCalls.FindRef(TEXT("Hero")), 1);
		TestEqual(TEXT("Later key invokes provider exactly once"), W.Dialogue->VariableCalls.FindRef(TEXT("Enemy")), 1);
	}
	{
		FStartWorld W;
		W.Dialogue->Choice(0)->Line.Text = FText::FromString(TEXT("{Hero} faces {Enemy}"));
		UDialogueNode_Player* Successor = W.Dialogue->Choice(1);
		W.Dialogue->DuringVariable = [&]() { W.Dialogue->StartPlayer(Successor); };
		W.Dialogue->StartPlayer(W.Dialogue->Choice(0));
		TestEqual(TEXT("Replacement retains its own text"), W.Dialogue->CurrentText(), Successor->Line.Text.ToString());
		TestEqual(TEXT("Old variable provider is not called repeatedly after replacement"), W.Dialogue->VariableCalls.FindRef(TEXT("Hero")), 1);
		TestEqual(TEXT("Retired formatting cannot call later providers"), W.Dialogue->VariableCalls.FindRef(TEXT("Enemy")), 0);
		TestEqual(TEXT("Only successor starts media"), W.Dialogue->PlayerMedia, 1);
		TestTrue(TEXT("Only successor owns its completion timer"), W.Dialogue->Remaining(false) > 9.f);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDialogueMidStartSuspendTest, "ProjectVelkorran.UI.Dialogue.MidStartSuspensionDefersMediaOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDialogueMidStartSuspendTest::RunTest(const FString&)
{
	FStartWorld W;
	bool bSuspendAccepted = false;
	W.Dialogue->DuringPlayerHook = [&]() { bSuspendAccepted = W.Dialogue->SetPlaybackSuspended(true); };
	W.Dialogue->StartPlayer(W.Dialogue->Choice(0));
	TestTrue(TEXT("Real suspension is accepted within the start callback"), bSuspendAccepted);
	TestEqual(TEXT("Suspended start never creates player media"), W.Dialogue->PlayerMedia, 0);
	TestTrue(TEXT("Completion timer is born paused"), W.Dialogue->IsTimerPaused(false));
	TestTrue(TEXT("Resume succeeds through the real owner"), W.Dialogue->SetPlaybackSuspended(false));
	TestEqual(TEXT("Resume creates media exactly once"), W.Dialogue->PlayerMedia, 1);
	TestEqual(TEXT("Resume never replays line-start hooks"), W.Dialogue->PlayerHooks, 1);
	TestEqual(TEXT("Resume never recalculates/replaces duration"), W.Dialogue->DurationCalls, 1);
	TestFalse(TEXT("Original timer resumes"), W.Dialogue->IsTimerPaused(false));
	W.Dialogue->SetPlaybackSuspended(false);
	TestEqual(TEXT("Duplicate resume cannot replay media"), W.Dialogue->PlayerMedia, 1);
	{
		FStartWorld Reentry;
		UDialogueNode_Player* SameNode = Reentry.Dialogue->Choice(0);
		Reentry.Dialogue->DuringPlayerHook = [&]() { Reentry.Dialogue->SetPlaybackSuspended(true); };
		Reentry.Dialogue->StartPlayer(SameNode);
		auto* Probe = NewObject<USovDialogueStartRuntimeProbe>(Reentry.PC);
		Reentry.Tales->OnDialogueSuspensionChanged.AddDynamic(Probe, &USovDialogueStartRuntimeProbe::SuspensionChanged);
		Reentry.Dialogue->DuringPlayerMedia = [&]() { Reentry.Dialogue->StartPlayer(SameNode); };
		TestFalse(TEXT("Resumed media restarting the same node retires the predecessor resume"), Reentry.Dialogue->SetPlaybackSuspended(false));
		TestEqual(TEXT("Retired resume never broadcasts a predecessor suspension receipt for the successor"), Probe->Resumes, 0);
		TestEqual(TEXT("Successor duration is retained"), Reentry.Dialogue->DurationCalls, 2);
		TestTrue(TEXT("Successor keeps its timer"), Reentry.Dialogue->Remaining(false) > 9.f);
	}
	return true;
}
#endif
