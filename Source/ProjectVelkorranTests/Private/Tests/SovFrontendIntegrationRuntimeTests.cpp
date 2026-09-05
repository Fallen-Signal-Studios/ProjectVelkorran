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
	static void Paging(USovAccessibilityPresentation* P, int32 Characters) { P->Settings.SubtitleCharactersPerLine=Characters; P->Settings.SubtitleMaximumLines=1; }
	static void Subtitles(USovAccessibilityPresentation* P,bool Enabled) { auto Settings=P->Settings; Settings.bSubtitles=Enabled; P->SettingsChanged(Settings); }
	static void End(USovFrontendComponent* C) { C->EndPlay(EEndPlayReason::RemovedFromWorld); }
};

namespace
{
	struct FFrontendWorld
	{
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
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
				.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
			PC = World->SpawnActor<ASovFrontendRuntimeController>();
			Pawn = World->SpawnActor<APawn>(); PC->StagePawn(Pawn);
			Tales = PC->GetTalesComponent(); Cues = PC->GetNarrativeCues();
			ASC = NewObject<UNarrativeAbilitySystemComponent>(PC); ASC->InitAbilityActorInfo(PC, Pawn);
			Frontend = NewObject<USovFrontendComponent>(PC);
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
	TestEqual(TEXT("Scene history belongs only to the replacement"), W.Presentation->GetSceneHistory().Num(), 1);
	W.Tales->OnNPCDialogueLineFinished.Broadcast(First, First->RootDialogue, First->RootDialogue->Line, FSpeakerInfo());
	W.Tales->OnDialogueFinished.Broadcast(First, false, EExitDialogueReason::EDR_NoLines);
	TestEqual(TEXT("Old line/scene completion cannot clear its successor"), W.Presentation->GetCurrentSpeechText().ToString(), FString(TEXT("Replacement scene text.")));
	W.Start(First);
	TestEqual(TEXT("A stale producer cannot append history to the current scene"), W.Presentation->GetSceneHistory().Num(), 1);
	W.Tales->OnNPCDialogueLineFinished.Broadcast(Second, Second->RootDialogue, Second->RootDialogue->Line, FSpeakerInfo());
	TestFalse(TEXT("Line finish preserves its minimum readable interval"), W.Presentation->GetCurrentSpeechText().IsEmpty());
	W.Presentation->Advance(3.f);
	TestTrue(TEXT("Finished native subtitle retires after readable time"), W.Presentation->GetCurrentSpeechText().IsEmpty());
	W.Tales->OnDialogueFinished.Broadcast(Second, false, EExitDialogueReason::EDR_NoLines);
	TestEqual(TEXT("Natural scene completion preserves reviewable history until replacement"), W.Presentation->GetSceneHistory().Num(), 1);
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
	Damage.TargetActor = W.PC; Damage.bShieldBroken = false; Damage.bPerfectDefense = true;
	W.ASC->OnDamageResolvedAsTarget.Broadcast(Damage);
	TestEqual(TEXT("Another avatar's damage cannot overwrite local feedback"), W.Presentation->GetCurrentCaptionText().ToString(), FString(TEXT("Shield broken")));
	auto* NewTales = NewObject<UTalesComponent>(W.PC);
	auto* NewCues = NewObject<USovNarrativeCueComponent>(W.PC);
	auto* NewASC = NewObject<UNarrativeAbilitySystemComponent>(W.PC); NewASC->InitAbilityActorInfo(W.PC, W.Pawn);
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
	USovNativeGameplayHUD* HUD = NewObject<USovNativeGameplayHUD>(); HUD->Initialize(); HUD->TakeWidget(); HUD->NativeConstruct();
	const auto& Tags = FNarrativeGameplayTags::Get();
	TestNotNull(TEXT("Native host registers existing game layer"), HUD->GetLayerContainer(Tags.UI_Layer_Game));
	TestNotNull(TEXT("Native host registers existing menu layer"), HUD->GetLayerContainer(Tags.UI_Layer_Menu));
	TestNotNull(TEXT("Native host registers existing modal layer"), HUD->GetLayerContainer(Tags.UI_Layer_Modal));
	TestTrue(TEXT("Menu/modal have independent stacks in the same CommonUI host"), HUD->GetLayerContainer(Tags.UI_Layer_Menu) != HUD->GetLayerContainer(Tags.UI_Layer_Modal));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFrontendInterruptedPageRepair,"ProjectVelkorran.UI.Frontend.InterruptedPagesAndFinalLineDrain",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovFrontendInterruptedPageRepair::RunTest(const FString&)
{
    FFrontendWorld W; FSovFrontendTestAccess::Paging(W.Presentation,20);
    auto* Dialogue=W.Dialogue(); Dialogue->RootDialogue->Line.Text=FText::FromString(TEXT("ABCDEFGHIJKLMNOPQRSTabcdefghijklmnopqrst01234567890123456789")); W.Start(Dialogue);
    const FString First=W.Presentation->GetCurrentSpeechText().ToString();
    auto* Cue=NewObject<USovNarrativeCue>(W.PC); Cue->SpeakerId=TEXT("Tarrik");
    for(int32 Cycle=0;Cycle<2;++Cycle)
    {
        TestTrue(TEXT("Real Tales graph suspends"),Dialogue->SetPlaybackSuspended(true));
        W.Cues->OnCueStarted.Broadcast(Cue,W.Pawn,FText::FromString(TEXT("Critical direction")),2.f);
        TestTrue(TEXT("Interrupting bark reaches the actual frontend despite live suspended dialogue"),W.Presentation->GetCurrentSpeechText().ToString().Contains(TEXT("Critical direction")));
        W.Cues->OnCueAudioReady.Broadcast(Cue,2.f);
        W.Presentation->Advance(2.1f); W.Cues->OnCueEnded.Broadcast(Cue,false);
        TestTrue(TEXT("Real Tales graph resumes"),Dialogue->SetPlaybackSuspended(false));
        TestEqual(TEXT("Suspended dialogue resumes at its retained page"),W.Presentation->GetCurrentSpeechText().ToString(),First);
    }
    W.Tales->OnNPCDialogueLineFinished.Broadcast(Dialogue,Dialogue->RootDialogue,Dialogue->RootDialogue->Line,FSpeakerInfo());
    W.Tales->OnDialogueFinished.Broadcast(Dialogue,false,EExitDialogueReason::EDR_NoLines);
    TestFalse(TEXT("Natural final-line exit does not clear unread pages"),W.Presentation->GetCurrentSpeechText().IsEmpty());
    W.Presentation->Advance(2.1f);
    TestTrue(TEXT("Unread second page survives final dialogue completion"),W.Presentation->GetCurrentSpeechText().ToString().Contains(TEXT("abcdefghijklmnopqrst")));
    W.Presentation->Advance(2.1f); W.Presentation->Advance(2.1f);
    TestTrue(TEXT("Final text eventually drains without a live dialogue"),W.Presentation->GetCurrentSpeechText().IsEmpty());
    TestEqual(TEXT("Audio readiness did not duplicate any history entries"),W.Presentation->GetSceneHistory().Num(),3);
    FSovFrontendTestAccess::Subtitles(W.Presentation,false);
    const FGuid Disabled=W.Presentation->PresentOwnedSpeech(FText(),FText::FromString(TEXT("Disabled subtitle")),2.f,FVector::ZeroVector,false,false);
    TestFalse(TEXT("Disabled subtitles never hold the cue arbiter"),W.Presentation->HasUnreadSpeech(Disabled));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFrontendCriticalCaptionRepair,"ProjectVelkorran.UI.Frontend.CriticalCaptionsSurviveDamageBurst",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovFrontendCriticalCaptionRepair::RunTest(const FString&)
{
    FFrontendWorld W; FSovDamageResult Damage; Damage.TargetActor=W.Pawn; Damage.bShieldBroken=true;
    W.ASC->OnDamageResolvedAsTarget.Broadcast(Damage);
    Damage.bShieldBroken=false; Damage.AppliedHealthDamage=1.f;
    for(int32 Hit=0;Hit<100;++Hit) { W.ASC->OnDamageResolvedAsTarget.Broadcast(Damage); }
    TestEqual(TEXT("Ordinary damage cannot replace active shield-break information"),W.Presentation->GetCurrentCaptionText().ToString(),FString(TEXT("Shield broken")));
    Damage.bGuardBroken=true; W.ASC->OnDamageResolvedAsTarget.Broadcast(Damage);
    W.Presentation->Advance(3.1f);
    TestEqual(TEXT("Second critical event is queued and read after first"),W.Presentation->GetCurrentCaptionText().ToString(),FString(TEXT("Guard broken")));
    W.Presentation->Advance(3.1f);
    TestTrue(TEXT("Damage burst cannot monopolize pending caption presentation"),W.Presentation->GetCurrentCaptionText().IsEmpty());
    TestEqual(TEXT("History is bounded during a burst"),W.Presentation->GetSceneHistory().Num(),64);
    return true;
}

#endif
