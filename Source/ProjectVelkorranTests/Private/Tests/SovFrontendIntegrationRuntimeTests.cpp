// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovFrontendRuntimeTestFixtures.h"
#include "Tests/SovDialogueRuntimeTestFixtures.h"
#include "UI/SovFrontendComponent.h"
#include "UI/SovNativeGameplayHUD.h"
#include "UI/Dialogue/SovDialogueChoiceWidget.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Narrative/SovNarrativeCue.h"
#include "Narrative/SovNarrativeCueComponent.h"
#include "NarrativeGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "ICommonInputModule.h"
#include "UObject/StrongObjectPtr.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Tales/TalesComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
struct FSovFrontendTestAccess
{
	static void Bind(USovFrontendComponent* C, USovAccessibilityPresentation* Presentation,
		UTalesComponent* Tales, USovNarrativeCueComponent* Cues, UNarrativeAbilitySystemComponent* ASC)
	{
		Presentation->Settings = FSovUserSettingsSnapshot();
		C->Presentation = Presentation; C->BindProducers(Tales, Cues, ASC);
	}
	static void Begin(USovFrontendComponent* C) { C->RegisterAllComponentTickFunctions(true); C->BeginPlay(); }
	static void End(USovFrontendComponent* C) { if (C->HasBegunPlay()) { C->EndPlay(EEndPlayReason::RemovedFromWorld); } }
};

namespace
{
	struct FFrontendWorld
	{
		TStrongObjectPtr<ULocalPlayer> LocalPlayer{NewObject<ULocalPlayer>(GEngine)};
		UWorld* World;
		ASovFrontendRuntimeController* PC;
		APawn* Pawn;
		UTalesComponent* Tales;
		USovNarrativeCueComponent* Cues;
		UNarrativeAbilitySystemComponent* ASC;
		USovFrontendComponent* Frontend;
		USovFrontendRuntimePresentation* Presentation;
		FFrontendWorld()
		{
			ICommonInputModule::GetSettings().LoadData();
			const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
				.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			PC = World->SpawnActor<ASovFrontendRuntimeController>();
			Pawn = World->SpawnActor<APawn>(); PC->StagePawn(Pawn);
			Tales = PC->GetTalesComponent(); Cues = PC->GetNarrativeCues();
			ASC = NewObject<UNarrativeAbilitySystemComponent>(PC); PC->AddInstanceComponent(ASC); ASC->RegisterComponent(); ASC->InitAbilityActorInfo(PC, Pawn);
			Frontend = NewObject<USovFrontendComponent>(PC);
			PC->AddInstanceComponent(Frontend); Frontend->RegisterComponent();
			// Begin without a local viewport, then bind the same producers explicitly.
			FSovFrontendTestAccess::Begin(Frontend);
			PC->Player = LocalPlayer.Get(); LocalPlayer->PlayerController = PC; PC->SetAsLocalPlayerController(); World->AddController(PC);
			Presentation = NewObject<USovFrontendRuntimePresentation>(PC);
			Presentation->SetOwningPlayer(PC); Presentation->Initialize(); Presentation->TakeWidget();
			// Only viewport admission is bypassed. The exact production binding helper subscribes to actual component delegates.
			FSovFrontendTestAccess::Bind(Frontend, Presentation, Tales, Cues, ASC);
		}
		~FFrontendWorld()
		{
			FSovFrontendTestAccess::End(Frontend);
			Tales->CurrentDialogue = nullptr; PC->StagePawn(nullptr);
			World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); }
		}
		USovDialogueRuntimeFixture* Dialogue()
		{
			auto* Result = NewObject<USovDialogueRuntimeFixture>(PC); Result->Stage(Tales); return Result;
		}
		void Start(USovDialogueRuntimeFixture* Dialogue)
		{
			Tales->OnNPCDialogueLineStarted.Broadcast(Dialogue, Dialogue->RootDialogue, Dialogue->RootDialogue->Line, FSpeakerInfo());
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFrontendTalesIntegrationTest, "ProjectVelkorran.UI.Frontend.TalesTextOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovFrontendTalesIntegrationTest::RunTest(const FString&)
{
	FFrontendWorld W;
	TestFalse(TEXT("Headless native fixtures do not acquire a first-boot viewport prerequisite"),
		USovFrontendComponent::IsInitialAccessibilitySetupPending(W.PC));
	USovDialogueRuntimeFixture* First = W.Dialogue();
	First->RootDialogue->Line.Text = FText::FromString(TEXT("First scene text.")); W.Start(First);
	TestEqual(TEXT("Actual Tales producer reaches native presentation"), W.Presentation->GetCurrentSpeechText().ToString(), FString(TEXT("First scene text.")));
	USovDialogueRuntimeFixture* Second = W.Dialogue();
	Second->RootDialogue->Line.Text = FText::FromString(TEXT("Replacement scene text.")); W.Start(Second);
	TestEqual(TEXT("Replacement scene cannot remain queued behind an unfinished old scene"), W.Presentation->GetCurrentSpeechText().ToString(), FString(TEXT("Replacement scene text.")));
	TestEqual(TEXT("Previous scene remains available in recent dialogue review"), W.Presentation->GetSceneHistory().Num(), 2);
	W.Tales->OnNPCDialogueLineFinished.Broadcast(First, First->RootDialogue, First->RootDialogue->Line, FSpeakerInfo());
	W.Tales->OnDialogueFinished.Broadcast(First, false, EExitDialogueReason::EDR_NoLines);
	TestEqual(TEXT("Old line/scene completion cannot clear its successor"), W.Presentation->GetCurrentSpeechText().ToString(), FString(TEXT("Replacement scene text.")));
	W.Start(First);
	TestEqual(TEXT("A stale producer cannot append history to the current scene"), W.Presentation->GetSceneHistory().Num(), 2);
	W.Tales->OnNPCDialogueLineFinished.Broadcast(Second, Second->RootDialogue, Second->RootDialogue->Line, FSpeakerInfo());
	TestFalse(TEXT("Line finish preserves its minimum readable interval"), W.Presentation->GetCurrentSpeechText().IsEmpty());
	W.Presentation->Advance(3.f);
	TestTrue(TEXT("Finished native subtitle retires after readable time"), W.Presentation->GetCurrentSpeechText().IsEmpty());
	W.Tales->OnDialogueFinished.Broadcast(Second, false, EExitDialogueReason::EDR_NoLines);
	TestEqual(TEXT("Current scene exit retains final information for review"), W.Presentation->GetSceneHistory().Num(), 2);
	FSovFrontendTestAccess::End(W.Frontend); W.Start(Second);
	TestTrue(TEXT("Teardown unbinds the actual Tales producer"), W.Presentation->GetSceneHistory().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFrontendCueDamageIntegrationTest, "ProjectVelkorran.UI.Frontend.CueDamageAndRebinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovFrontendCueDamageIntegrationTest::RunTest(const FString&)
{
	FFrontendWorld W;
	auto* Cue = NewObject<USovNarrativeCue>(W.PC); Cue->SpeakerId = TEXT("Tarrik");
	W.Cues->OnCueStarted.Broadcast(Cue, W.Pawn, FText::FromString(TEXT("A native bark.")), 3.f);
	TestEqual(TEXT("Actual cue component delegate produces a subtitle"), W.Presentation->GetCurrentSpeechText().ToString(), FString(TEXT("A native bark.")));
	auto* Unrelated = NewObject<USovNarrativeCue>(W.PC);
	W.Cues->OnCueEnded.Broadcast(Unrelated, true); W.Presentation->Advance(1.f);
	TestFalse(TEXT("Unrelated cue completion cannot erase live speech"), W.Presentation->GetCurrentSpeechText().IsEmpty());
	FSovDamageResult Damage; Damage.TargetActor = W.Pawn; Damage.bShieldBroken = true;
	W.ASC->OnDamageResolvedAsTarget.Broadcast(Damage);
	TestEqual(TEXT("Actual combat damage delegate produces non-color caption"), W.Presentation->GetCurrentCaptionText().ToString(), FString(TEXT("Shield broken")));
	FSovDamageResult Chip; Chip.TargetActor = W.Pawn; Chip.AppliedHealthDamage = 1.f;
	for (int32 Hit = 0; Hit < 20; ++Hit) { W.ASC->OnDamageResolvedAsTarget.Broadcast(Chip); }
	TestEqual(TEXT("Repeated chip damage cannot replace the critical break warning"), W.Presentation->GetCurrentCaptionText().ToString(), FString(TEXT("Shield broken")));
	TestEqual(TEXT("Repeated chip damage coalesces to one pending history entry"), W.Presentation->GetSceneHistory().Num(), 3);
	Damage.TargetActor = W.PC; Damage.bShieldBroken = false; Damage.bPerfectDefense = true;
	W.ASC->OnDamageResolvedAsTarget.Broadcast(Damage);
	TestEqual(TEXT("Another avatar's damage cannot overwrite local feedback"), W.Presentation->GetCurrentCaptionText().ToString(), FString(TEXT("Shield broken")));
	auto* NewTales = NewObject<UTalesComponent>(W.PC);
	auto* NewCues = NewObject<USovNarrativeCueComponent>(W.PC);
	auto* NewASC = NewObject<UNarrativeAbilitySystemComponent>(W.PC); W.PC->AddInstanceComponent(NewASC); NewASC->RegisterComponent(); NewASC->InitAbilityActorInfo(W.PC, W.Pawn);
	FSovFrontendTestAccess::Bind(W.Frontend, W.Presentation, NewTales, NewCues, NewASC);
	W.Cues->OnCueStarted.Broadcast(Cue, W.Pawn, FText::FromString(TEXT("Retired producer.")), 3.f);
	Damage.TargetActor = W.Pawn; W.ASC->OnDamageResolvedAsTarget.Broadcast(Damage);
	TestTrue(TEXT("Replacing producers clears their old text and ignores old delegates"), W.Presentation->GetSceneHistory().IsEmpty());
	NewCues->OnCueStarted.Broadcast(Cue, W.Pawn, FText::FromString(TEXT("New producer.")), 3.f);
	TestEqual(TEXT("Replacement producer is bound exactly once"), W.Presentation->GetSceneHistory().Num(), 1);
	FSovFrontendTestAccess::Bind(W.Frontend, W.Presentation, NewTales, NewCues, NewASC);
	NewASC->OnDamageResolvedAsTarget.Broadcast(Damage);
	TestEqual(TEXT("Repeated refresh does not duplicate damage subscription"), W.Presentation->GetSceneHistory().Num(), 2);
	TestEqual(TEXT("Replacement ASC reaches caption consumer"), W.Presentation->GetCurrentCaptionText().ToString(), FString(TEXT("Perfect defense")));
	FSovFrontendTestAccess::End(W.Frontend);
	NewCues->OnCueStarted.Broadcast(Cue, W.Pawn, FText::FromString(TEXT("Late producer.")), 3.f);
	NewASC->OnDamageResolvedAsTarget.Broadcast(Damage);
	TestTrue(TEXT("Teardown removes all cue/combat bindings"), W.Presentation->GetSceneHistory().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNativeHUDIntegrationTest, "ProjectVelkorran.UI.Frontend.NativeCommonUIHost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNativeHUDIntegrationTest::RunTest(const FString&)
{
	ICommonInputModule::GetSettings().LoadData();
	USovNativeGameplayHUD* HUD = NewObject<USovNativeGameplayHUD>(); HUD->Initialize(); HUD->TakeWidget(); HUD->NativeConstruct();
	const auto& Tags = FNarrativeGameplayTags::Get();
	TestNotNull(TEXT("Native host registers existing game layer"), HUD->GetLayerContainer(Tags.UI_Layer_Game));
	TestNotNull(TEXT("Native host registers existing menu layer"), HUD->GetLayerContainer(Tags.UI_Layer_Menu));
	TestNotNull(TEXT("Native host registers existing modal layer"), HUD->GetLayerContainer(Tags.UI_Layer_Modal));
	TestTrue(TEXT("Menu/modal have independent stacks in the same CommonUI host"), HUD->GetLayerContainer(Tags.UI_Layer_Menu) != HUD->GetLayerContainer(Tags.UI_Layer_Modal));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovBarkDuringSuspendedDialogueTest, "ProjectVelkorran.UI.Frontend.BarkDuringSuspendedDialogueIsSubtitled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovBarkDuringSuspendedDialogueTest::RunTest(const FString&)
{
	FFrontendWorld W; auto* Dialogue = W.Dialogue();
	Dialogue->RootDialogue->Line.Text = FText::FromString(TEXT("Walk-and-talk line.")); W.Start(Dialogue);
	auto* Cue = NewObject<USovNarrativeCue>(W.PC); Cue->SpeakerId = TEXT("Tarrik");
	W.Cues->OnCueStarted.Broadcast(Cue, W.Pawn, FText::FromString(TEXT("Talking over the scene.")), 3.f);
	TestEqual(TEXT("A playing conversation keeps the speech surface"), W.Presentation->GetCurrentSpeechText().ToString(), FString(TEXT("Walk-and-talk line.")));
	W.Cues->OnCueEnded.Broadcast(Cue, false);

	// The cue arbiter suspends the conversation so a bark can be heard; the bark must be readable too.
	if (!TestTrue(TEXT("The authored walk-and-talk suspends"), Dialogue->SetPlaybackSuspended(true))) { return false; }
	W.Cues->OnCueStarted.Broadcast(Cue, W.Pawn, FText::FromString(TEXT("Contact, left side!")), 3.f);
	TestEqual(TEXT("A bark during a suspended conversation is subtitled"), W.Presentation->GetCurrentSpeechText().ToString(), FString(TEXT("Contact, left side!")));
	W.Cues->OnCueEnded.Broadcast(Cue, false); W.Presentation->Advance(3.f);
	TestTrue(TEXT("The finished bark retires after its readable time"), W.Presentation->GetCurrentSpeechText().IsEmpty());

	if (!TestTrue(TEXT("The conversation resumes"), Dialogue->SetPlaybackSuspended(false))) { return false; }
	TestEqual(TEXT("The displaced line is shown again under its own speaker"), W.Presentation->GetCurrentSpeechText().ToString(), FString(TEXT("Walk-and-talk line.")));
	TestTrue(TEXT("The bark stays in recent dialogue review"), W.Presentation->GetSceneHistory().ContainsByPredicate(
		[](const FSovSceneSubtitleEntry& Entry) { return Entry.Text.ToString() == TEXT("Contact, left side!"); }));
	const int32 HistoryAfterResume = W.Presentation->GetSceneHistory().Num();
	TestTrue(TEXT("Suspending again without a bark is accepted"), Dialogue->SetPlaybackSuspended(true));
	TestTrue(TEXT("Resuming again is accepted"), Dialogue->SetPlaybackSuspended(false));
	TestEqual(TEXT("A resume with no displaced line presents nothing new"), W.Presentation->GetSceneHistory().Num(), HistoryAfterResume);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFinalDialogueRetentionTest, "ProjectVelkorran.UI.Frontend.FinalLineSurvivesDialogueEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovFinalDialogueRetentionTest::RunTest(const FString&)
{
	FFrontendWorld W; auto* Dialogue = W.Dialogue(); W.Start(Dialogue);
	W.Tales->OnNPCDialogueLineFinished.Broadcast(Dialogue, Dialogue->RootDialogue, Dialogue->RootDialogue->Line, FSpeakerInfo());
	W.Tales->OnDialogueFinished.Broadcast(Dialogue, false, EExitDialogueReason::EDR_NoLines);
	TestFalse(TEXT("Final line remains readable after graph completion"), W.Presentation->GetCurrentSpeechText().IsEmpty());
	W.Presentation->Advance(1.f);
	TestFalse(TEXT("Scene completion cannot bypass the minimum readable interval"), W.Presentation->GetCurrentSpeechText().IsEmpty());
	W.Presentation->Advance(2.f);
	TestTrue(TEXT("Finished final page eventually retires"), W.Presentation->GetCurrentSpeechText().IsEmpty());
	TestEqual(TEXT("Final line remains accessible in recent dialogue review"), W.Presentation->GetSceneHistory().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCaptionPriorityLifetimeTest, "ProjectVelkorran.UI.Frontend.CaptionPriorityAndReadableLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCaptionPriorityLifetimeTest::RunTest(const FString&)
{
	FFrontendWorld W;
	W.Presentation->PresentCaption(FText::FromString(TEXT("Critical warning")), 3.f, FVector::ZeroVector, ESovCaptionPriority::Critical);
	W.Presentation->Advance(2.f);
	for (int32 Count = 0; Count < 50; ++Count)
	{
		W.Presentation->PresentCaption(FText::FromString(TEXT("Critical warning")), 3.f, FVector::ZeroVector, ESovCaptionPriority::Critical);
		W.Presentation->PresentCaption(FText::FromString(TEXT("Chip damage")), 3.f, FVector::ZeroVector, ESovCaptionPriority::Routine);
	}
	TestEqual(TEXT("Duplicates never replace the live critical page"), W.Presentation->GetCurrentCaptionText().ToString(), FString(TEXT("Critical warning")));
	TestEqual(TEXT("Burst duplicates retain only distinct records"), W.Presentation->GetSceneHistory().Num(), 2);
	W.Presentation->Advance(1.1f);
	TestEqual(TEXT("Coalescing does not restart the critical lifetime or starve its queued successor"), W.Presentation->GetCurrentCaptionText().ToString(), FString(TEXT("Chip damage")));
	W.Presentation->PresentCaption(FText::FromString(TEXT("Guard broken")), 3.f, FVector::ZeroVector, ESovCaptionPriority::Critical);
	TestEqual(TEXT("New critical warning immediately preempts routine feedback"), W.Presentation->GetCurrentCaptionText().ToString(), FString(TEXT("Guard broken")));
	return true;
}
#endif
