// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovFrontendRuntimeTestFixtures.h"
#include "Tests/SovCampaignRuntimeTestFixtures.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Character/PlayerDefinition.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Sovereign/SovGameplayTags.h"
#include "UI/SovFrontendComponent.h"
#include "UI/SovAccessibleRecordMenu.h"
#include "Save/SovSaveSubsystem.h"
#include "Platform/SovPlatformServicesAdapter.h"

#if WITH_AUTOMATION_TESTS
#define LOCTEXT_NAMESPACE "SovObjectivePresentationTests"
namespace
{
	class FObjectiveHUDTestStorage final : public ISovSaveStorage
	{
	public:
		TMap<FString, TArray<uint8>> Files;
		bool Read(const FString& Slot, int32 User, TArray<uint8>& Bytes) override
		{ if (const auto* Found = Files.Find(FString::FromInt(User) + Slot)) { Bytes = *Found; return true; } return false; }
		bool Write(const FString& Slot, int32 User, const TArray<uint8>& Bytes) override
		{ Files.Add(FString::FromInt(User) + Slot, Bytes); return true; }
		bool Exists(const FString& Slot, int32 User) override { return Files.Contains(FString::FromInt(User) + Slot); }
	};
}
struct FSovObjectivePresentationTestAccess
{
	static void Bind(USovFrontendComponent& Frontend, USovAccessibilityPresentation* Presentation,
		ASovPlayerController* Controller, USovCampaignStateComponent* Campaign)
	{
		Frontend.Presentation = Presentation; Frontend.Activate(true);
		Frontend.BindObjectives(Controller, Campaign); Frontend.RefreshObjectives();
	}
	static void Refresh(USovFrontendComponent& Frontend) { Frontend.RefreshObjectives(false); }
	static void End(USovFrontendComponent& Frontend) { Frontend.EndPlay(EEndPlayReason::RemovedFromWorld); }
	static void Settings(USovAccessibilityPresentation& Presentation, const FSovUserSettingsSnapshot& Value)
	{ Presentation.SettingsChanged(Value); }
	static bool Visible(const USovAccessibilityPresentation& Presentation)
	{ return Presentation.ObjectiveBackground && Presentation.ObjectiveBackground->GetVisibility() != ESlateVisibility::Collapsed; }
	static int32 FontSize(const USovAccessibilityPresentation& Presentation) { return Presentation.ObjectiveText->GetFont().Size; }
	static FText Text(const USovAccessibilityPresentation& Presentation) { return Presentation.ObjectiveText->GetText(); }
	static void InitializeNativeAccount(USovSaveSubsystem& Saves, const FSovObservedPlatformAccount& Account)
	{ Saves.Storage = MakeUnique<FObjectiveHUDTestStorage>(); Saves.ObserveNativePlatformAccount(Account); }
	static void ObserveNativeAccount(USovSaveSubsystem& Saves, const FSovObservedPlatformAccount& Account)
	{ Saves.ObserveNativePlatformAccount(Account); }
	static void BindNativeSave(USovFrontendComponent& Frontend, USovSaveSubsystem* Saves, ASovPlayerController* PC)
	{ Frontend.UnbindObjectives(); Frontend.BoundSave = Saves; Frontend.BindObjectives(PC, PC->GetCampaignState()); Frontend.RefreshObjectives(); }
	static void AccountChanged(USovFrontendComponent& Frontend) { Frontend.OnObjectiveAccountChanged(false, false); }
	static void Layout(USovAccessibilityPresentation& Presentation, float Width, float Height)
	{ Presentation.LayoutObjectives(Width, Height); }
	static int32 VisibleRows(const USovAccessibilityPresentation& Presentation) { return Presentation.VisibleObjectiveRows; }
	static float HeightBudget(const USovAccessibilityPresentation& Presentation) { return Presentation.ObjectiveSize->GetMaxDesiredHeight(); }
	static float FirstRowHeight(const USovAccessibilityPresentation& Presentation) { return float(Presentation.ObjectiveRows[0]->GetDesiredSize().Y) + 8.f; }
	static void Reconstruct(USovAccessibilityPresentation& Presentation)
	{ Presentation.NativeDestruct(); Presentation.NativeConstruct(); }
	static void ConstructReview(USovAccessibleRecordMenu& Menu) { Menu.NativeConstruct(); }
	static void DestructReview(USovAccessibleRecordMenu& Menu) { Menu.NativeDestruct(); }
	static int32 ReviewCount(const USovAccessibleRecordMenu& Menu) { return Menu.Records.Num(); }
	static FText ReviewBody(const USovAccessibleRecordMenu& Menu) { return Menu.Body->GetText(); }
};

namespace
{
	struct FObjectivePresentationWorld
	{
		UWorld* World = nullptr;
		ASovFrontendRuntimeController* PC = nullptr;
		ASovCampaignRuntimeTestPawn* Pawn = nullptr;
		USovCampaignStateComponent* State = nullptr;
		USovFrontendComponent* Frontend = nullptr;
		USovAccessibilityPresentation* Presentation = nullptr;
		FObjectivePresentationWorld()
		{
			const auto IVS = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			PC = World->SpawnActor<ASovFrontendRuntimeController>(); Pawn = World->SpawnActor<ASovCampaignRuntimeTestPawn>();
			if (!PC || !Pawn) { return; }
			PC->StagePawn(Pawn); State = PC->GetCampaignState(); Frontend = PC->GetFrontend();
			Presentation = NewObject<USovAccessibilityPresentation>(PC);
			Presentation->SetOwningPlayer(PC); Presentation->Initialize(); Presentation->TakeWidget();
			FSovObjectivePresentationTestAccess::Settings(*Presentation, FSovUserSettingsSnapshot());
			FSovObjectivePresentationTestAccess::Bind(*Frontend, Presentation, PC, State);
		}
		~FObjectivePresentationWorld()
		{
			if (Frontend) { FSovObjectivePresentationTestAccess::End(*Frontend); }
			if (PC) { PC->StagePawn(nullptr); }
			if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		}
		USovCampaignDefinition* Mission()
		{
			auto* Definition = NewObject<USovCampaignDefinition>(PC);
			Definition->MissionId = TEXT("M12_ObjectivePresentation"); Definition->Protagonist = Pawn->TestHero;
			Definition->PawnClass = ASovCampaignRuntimeTestPawn::StaticClass(); Definition->PlayerDefinition = NewObject<UPlayerDefinition>(PC);
			FSovCampaignBeatDefinition Start; Start.BeatId = TEXT("ReachSurvivors"); Start.ObjectiveText = LOCTEXT("Reach", "Reach the survivors"); Definition->Beats.Add(Start);
			FSovCampaignBeatDefinition Rescue; Rescue.BeatId = TEXT("OptionalRescue"); Rescue.bOptional = true;
			Rescue.PrerequisiteBeats = { Start.BeatId }; Rescue.ObjectiveText = LOCTEXT("Rescue", "Protect the wounded crew");
			Rescue.FailureReasonId = TEXT("RescueExpired"); Rescue.FailureRuleText = LOCTEXT("RescueRule", "Reach them before the corridor collapses."); Definition->Beats.Add(Rescue);
			FSovCampaignBeatDefinition Escape; Escape.BeatId = TEXT("EscapeTogether"); Escape.PrerequisiteBeats = { Start.BeatId };
			Escape.bCanonGate = true; Escape.ObjectiveText = LOCTEXT("Escape", "Find a route out"); Definition->Beats.Add(Escape);
			return Definition;
		}
		TArray<uint8> Save()
		{
			TArray<uint8> Bytes; FMemoryWriter Writer(Bytes); FObjectAndNameAsStringProxyArchive Archive(Writer, false);
			Archive.ArIsSaveGame = true; Archive.ArNoDelta = true; State->Serialize(Archive); return Bytes;
		}
		void Restore(const TArray<uint8>& Bytes)
		{
			FMemoryReader Reader(Bytes); FObjectAndNameAsStringProxyArchive Archive(Reader, true); Archive.ArIsSaveGame = true;
			State->Serialize(Archive); State->Load_Implementation();
		}
		bool Has(FName Id) const
		{ return Presentation->GetPresentedObjectives().ContainsByPredicate([Id](const auto& Row) { return Row.BeatId == Id; }); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectivePresentationLifecycleTest, "ProjectVelkorran.UI.Objectives.ActionableLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectivePresentationLifecycleTest::RunTest(const FString&)
{
	FObjectivePresentationWorld F; if (!F.Frontend || !F.Presentation) { AddError(TEXT("Fixture failed")); return false; }
	auto* Mission = F.Mission();
	TestEqual(TEXT("Mission begins"), F.State->BeginMission(Mission), ESovCampaignResult::Applied);
	if (!TestEqual(TEXT("Only immediate goal shown"), F.Presentation->GetPresentedObjectives().Num(), 1)) { return false; }
	TestTrue(TEXT("Localized authored text retained"), F.Presentation->GetPresentedObjectives()[0].Text.EqualTo(Mission->Beats[0].ObjectiveText));
	TestFalse(TEXT("Future rescue text is absent"), FSovObjectivePresentationTestAccess::Text(*F.Presentation).ToString().Contains(TEXT("wounded")));
	F.State->CompleteBeat(TEXT("ReachSurvivors"));
	TestFalse(TEXT("Completion retires immediately without ticking"), F.Has(TEXT("ReachSurvivors")));
	TestTrue(TEXT("New optional goal shown on beat receipt"), F.Has(TEXT("OptionalRescue")));
	if (F.Presentation->GetPresentedObjectives().Num() != 2) { return false; }
	TestEqual(TEXT("Required route precedes optional available goal"), F.Presentation->GetPresentedObjectives()[0].BeatId, FName(TEXT("EscapeTogether")));
	F.State->TransitionObjective(TEXT("OptionalRescue"), ESovObjectiveState::Active);
	if (!TestTrue(TEXT("Activated goal remains available to presentation"), F.Has(TEXT("OptionalRescue")))) { return false; }
	TestEqual(TEXT("Active goal receives priority"), F.Presentation->GetPresentedObjectives()[0].BeatId, FName(TEXT("OptionalRescue")));
	TestTrue(TEXT("Failure rule shown before failure"), F.Presentation->GetPresentedObjectives()[0].FailureRule.EqualTo(Mission->Beats[1].FailureRuleText));
	TestTrue(TEXT("Optional is explicit"), F.Presentation->GetPresentedObjectives()[0].bOptional);
	F.State->TransitionObjective(TEXT("OptionalRescue"), ESovObjectiveState::Failed);
	TestFalse(TEXT("Failure retires immediately"), F.Has(TEXT("OptionalRescue")));
	TestTrue(TEXT("Canon route remains presented"), F.Has(TEXT("EscapeTogether")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectivePresentationRestoreTest, "ProjectVelkorran.UI.Objectives.RestoreAndInvalidState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectivePresentationRestoreTest::RunTest(const FString&)
{
	FObjectivePresentationWorld F; if (!F.Frontend || !F.Presentation) { AddError(TEXT("Fixture failed")); return false; }
	auto* Mission = F.Mission(); F.State->BeginMission(Mission); F.State->CompleteBeat(TEXT("ReachSurvivors"));
	const auto Bytes = F.Save();
	F.State->TransitionObjective(TEXT("OptionalRescue"), ESovObjectiveState::Skipped);
	TestFalse(TEXT("Skipped goal retires immediately"), F.Has(TEXT("OptionalRescue")));
	F.Restore(Bytes);
	TestTrue(TEXT("Valid restored state republishes available goal"), F.Has(TEXT("OptionalRescue")));
	Mission->Beats.RemoveAt(0); F.State->Load_Implementation();
	TestFalse(TEXT("Invalid state rejected by real restore validator"), F.State->IsStateValid());
	TestTrue(TEXT("Invalid restore clears all displayed goals"), F.Presentation->GetPresentedObjectives().IsEmpty());
	TestTrue(TEXT("Invalid restore also clears overflow review"), F.Presentation->GetObjectiveReviewEntries().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectivePresentationOwnerTest, "ProjectVelkorran.UI.Objectives.ProtagonistAndTeardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectivePresentationOwnerTest::RunTest(const FString&)
{
	FObjectivePresentationWorld F; if (!F.Frontend || !F.Presentation) { AddError(TEXT("Fixture failed")); return false; }
	auto* Mission = F.Mission(); F.State->BeginMission(Mission);
	F.Pawn->TestHero = FSovGameplayTags::Get().Character_Player_Selene;
	FSovObjectivePresentationTestAccess::Refresh(*F.Frontend);
	TestTrue(TEXT("Outgoing protagonist goal removed before handoff commit"), F.Presentation->GetPresentedObjectives().IsEmpty());
	F.Pawn->TestHero = Mission->Protagonist; FSovObjectivePresentationTestAccess::Refresh(*F.Frontend);
	TestTrue(TEXT("Matching protagonist restores its own goal"), F.Has(TEXT("ReachSurvivors")));
	FSovObjectivePresentationTestAccess::Reconstruct(*F.Presentation);
	TestTrue(TEXT("Removed surface erases previous view"), F.Presentation->GetPresentedObjectives().IsEmpty());
	FSovObjectivePresentationTestAccess::Refresh(*F.Frontend);
	TestTrue(TEXT("Reconstructed surface refreshes without campaign mutation"), F.Has(TEXT("ReachSurvivors")));
	F.PC->StagePawn(nullptr); FSovObjectivePresentationTestAccess::Refresh(*F.Frontend);
	TestTrue(TEXT("Unpossession clears stale goal"), F.Presentation->GetPresentedObjectives().IsEmpty());
	F.PC->StagePawn(F.Pawn); FSovObjectivePresentationTestAccess::Refresh(*F.Frontend);
	FSovObjectivePresentationTestAccess::End(*F.Frontend);
	TestTrue(TEXT("Teardown clears content"), F.Presentation->GetPresentedObjectives().IsEmpty());
	F.State->CompleteBeat(TEXT("ReachSurvivors"));
	TestTrue(TEXT("Late producer cannot repopulate retired frontend"), F.Presentation->GetPresentedObjectives().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectivePresentationSettingsTest, "ProjectVelkorran.UI.Objectives.SettingsAndBoundedRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectivePresentationSettingsTest::RunTest(const FString&)
{
	FObjectivePresentationWorld F; if (!F.Frontend || !F.Presentation) { AddError(TEXT("Fixture failed")); return false; }
	auto* Mission = F.Mission();
	for (int32 Index = 0; Index < 7; ++Index)
	{
		FSovCampaignBeatDefinition Beat; Beat.BeatId = FName(*FString::Printf(TEXT("KnownOptional%d"), Index));
		Beat.bOptional = true; Beat.ObjectiveText = FText::Format(LOCTEXT("KnownOptional", "Protect group {0}"), FText::AsNumber(Index)); Mission->Beats.Add(Beat);
	}
	F.State->BeginMission(Mission);
	TestEqual(TEXT("At most three current goals"), F.Presentation->GetPresentedObjectives().Num(), 3);
	TestEqual(TEXT("Overflow counts actionable rows only, excluding two future beats"), F.Presentation->GetAdditionalObjectiveCount(), 5);
	TestEqual(TEXT("Full authorized cache preserves every goal for review"), F.Presentation->GetObjectiveReviewEntries().Num(), 8);
	FSovUserSettingsSnapshot Settings; Settings.UIScale = 1.5f; Settings.SubtitleScale = 2.f;
	FSovObjectivePresentationTestAccess::Settings(*F.Presentation, Settings);
	TestEqual(TEXT("Objective font follows UI scale independently of subtitle scale"), FSovObjectivePresentationTestAccess::FontSize(*F.Presentation), 30);
	Settings.bShowObjectiveText = false; FSovObjectivePresentationTestAccess::Settings(*F.Presentation, Settings);
	TestFalse(TEXT("Player can hide objective text"), FSovObjectivePresentationTestAccess::Visible(*F.Presentation));
	TestEqual(TEXT("Hiding text never changes campaign state"), F.State->GetObjectiveState(Mission->MissionId, TEXT("ReachSurvivors")), ESovObjectiveState::Available);
	Settings.bShowObjectiveText = true; FSovObjectivePresentationTestAccess::Settings(*F.Presentation, Settings);
	TestTrue(TEXT("Showing text restores current view"), FSovObjectivePresentationTestAccess::Visible(*F.Presentation));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectivePresentationChoiceTest, "ProjectVelkorran.UI.Objectives.ChoiceRetiresAlternatives",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectivePresentationChoiceTest::RunTest(const FString&)
{
	FObjectivePresentationWorld F; if (!F.Frontend || !F.Presentation) { AddError(TEXT("Fixture failed")); return false; }
	auto* Mission = F.Mission();
	FSovCampaignChoiceGroup Group; Group.GroupId = TEXT("ProtectionPriority"); Group.ReconciliationBeatId = TEXT("EscapeTogether");
	Group.ReconciliationNote = LOCTEXT("ChoiceRejoin", "Both crews use the same escape route."); Mission->ChoiceGroups.Add(Group);
	Mission->Beats.Last().RequiredChoiceGroups.Add(Group.GroupId);
	for (FName Id : { FName(TEXT("ProtectDominion")), FName(TEXT("ProtectReformation")) })
	{
		FSovCampaignBeatDefinition Beat; Beat.BeatId = Id; Beat.bOptional = true; Beat.bInteractiveChoice = true;
		Beat.ChoiceGroupId = Group.GroupId; Beat.PrerequisiteBeats = { TEXT("ReachSurvivors") };
		Beat.ObjectiveText = Id == TEXT("ProtectDominion") ? LOCTEXT("DominionCrew", "Protect the Dominion crew") : LOCTEXT("ReformationCrew", "Protect the Reformation crew");
		Mission->Beats.Add(Beat);
	}
	TestEqual(TEXT("Choice mission begins"), F.State->BeginMission(Mission), ESovCampaignResult::Applied);
	F.State->CompleteBeat(TEXT("ReachSurvivors"));
	TestTrue(TEXT("Both known choices visible before selection"), F.Has(TEXT("ProtectDominion")) && F.Has(TEXT("ProtectReformation")));
	TestEqual(TEXT("Native choice commits"), F.State->ResolveChoice(Group.GroupId, TEXT("ProtectDominion")), ESovCampaignResult::Applied);
	TestFalse(TEXT("Succeeded choice leaves HUD immediately"), F.Has(TEXT("ProtectDominion")));
	TestFalse(TEXT("Superseded alternative leaves HUD immediately"), F.Has(TEXT("ProtectReformation")));
	TestTrue(TEXT("Reconciled route becomes visible"), F.Has(TEXT("EscapeTogether")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectivePresentationAccountTest, "ProjectVelkorran.UI.Objectives.UnavailableAccountClearsView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectivePresentationAccountTest::RunTest(const FString&)
{
	FObjectivePresentationWorld F; if (!F.Frontend || !F.Presentation) { AddError(TEXT("Fixture failed")); return false; }
	auto* Saves = NewObject<USovSaveSubsystem>(NewObject<UGameInstance>());
	FSovObservedPlatformAccount Owner; Owner.StableId = TEXT("TestProvider|ObjectiveOwner"); Owner.LocalUser = 3;
	Owner.bIdentityKnown = true; Owner.bRequiresKnownStorageOwner = true; Owner.bStorageAccessAuthorized = true;
	FSovObjectivePresentationTestAccess::InitializeNativeAccount(*Saves, Owner); FString Error;
	if (!TestTrue(TEXT("Native observer admits the original local storage owner"), Saves->SelectPlatformUser(Owner.StableId, Owner.LocalUser, Error))) { return false; }
	const FString OriginalNamespace = Saves->GetAccountNamespace();
	FSovObjectivePresentationTestAccess::BindNativeSave(*F.Frontend, Saves, F.PC);
	F.State->BeginMission(F.Mission());
	TestTrue(TEXT("Authorized local profile needs no online sign-in to show its goal"), F.Has(TEXT("ReachSurvivors")));
	auto Unavailable = Owner; Unavailable.bIdentityKnown = false; Unavailable.bStorageAccessAuthorized = false;
	FSovObjectivePresentationTestAccess::ObserveNativeAccount(*Saves, Unavailable);
	TestFalse(TEXT("Native storage observation revokes access"), Saves->IsPlatformStorageOwnerAvailable());
	FSovObjectivePresentationTestAccess::AccountChanged(*F.Frontend);
	TestTrue(TEXT("Account observation removes private campaign view"), F.Presentation->GetPresentedObjectives().IsEmpty());
	FSovObjectivePresentationTestAccess::ObserveNativeAccount(*Saves, Owner);
	TestTrue(TEXT("Same original account restores native access"), Saves->IsPlatformStorageOwnerAvailable());
	FSovObjectivePresentationTestAccess::AccountChanged(*F.Frontend);
	TestTrue(TEXT("Same namespace and local user recover the HUD without loading"), F.Has(TEXT("ReachSurvivors")));
	TestEqual(TEXT("Ownership recovery does not mutate campaign"), F.State->GetJournal().Num(), 0);
	auto Other = Owner; Other.StableId = TEXT("TestProvider|OtherObjectiveOwner"); Other.LocalUser = 4;
	FSovObjectivePresentationTestAccess::ObserveNativeAccount(*Saves, Other);
	TestTrue(TEXT("Isolated native storage fixture can select another authorized profile"), Saves->SelectPlatformUser(Other.StableId, Other.LocalUser, Error));
	FSovObjectivePresentationTestAccess::AccountChanged(*F.Frontend);
	TestTrue(TEXT("A different owner never receives previous campaign goals"), F.Presentation->GetObjectiveReviewEntries().IsEmpty());
	FSovObjectivePresentationTestAccess::ObserveNativeAccount(*Saves, Owner);
	TestTrue(TEXT("Original profile can be selected again"), Saves->SelectPlatformUser(Owner.StableId, Owner.LocalUser, Error));
	TestEqual(TEXT("Original namespace restored"), Saves->GetAccountNamespace(), OriginalNamespace);
	FSovObjectivePresentationTestAccess::AccountChanged(*F.Frontend);
	TestTrue(TEXT("Real profile replacement retains the campaign-restore fence"), F.Presentation->GetObjectiveReviewEntries().IsEmpty());
	F.State->Load_Implementation();
	TestTrue(TEXT("Validated campaign restore releases replacement fence"), F.Has(TEXT("ReachSurvivors")));
	FSovObjectivePresentationTestAccess::ObserveNativeAccount(*Saves, Unavailable); FSovObjectivePresentationTestAccess::AccountChanged(*F.Frontend);
	F.State->CompleteBeat(TEXT("ReachSurvivors"));
	TestTrue(TEXT("Subsequent objective receipt cannot reopen unauthorized view"), F.Presentation->GetPresentedObjectives().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectivePresentationLayoutTest, "ProjectVelkorran.UI.Objectives.SafeHeightDefersWholeRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectivePresentationLayoutTest::RunTest(const FString&)
{
	FObjectivePresentationWorld F; if (!F.Frontend || !F.Presentation) { AddError(TEXT("Fixture failed")); return false; }
	auto* Mission = F.Mission(); F.State->BeginMission(Mission);
	FSovObjectivePresentationTestAccess::Layout(*F.Presentation, 640.f, 720.f);
	const float OneRowSafeHeight = (FSovObjectivePresentationTestAccess::FirstRowHeight(*F.Presentation) + 17.f) / .63f;
	FSovObjectivePresentationTestAccess::Layout(*F.Presentation, 640.f, OneRowSafeHeight);
	TestEqual(TEXT("One fitting goal requires no room for the hidden overflow link"), FSovObjectivePresentationTestAccess::VisibleRows(*F.Presentation), 1);
	TestTrue(TEXT("Hidden overflow label cannot hide a fitting single goal"), FSovObjectivePresentationTestAccess::Visible(*F.Presentation));
	F.State->CompleteBeat(TEXT("ReachSurvivors"));
	F.State->TransitionObjective(TEXT("OptionalRescue"), ESovObjectiveState::Active);
	if (!TestTrue(TEXT("Layout fixture has actionable rows"), F.Has(TEXT("OptionalRescue")))) { return false; }
	FSovUserSettingsSnapshot Settings; Settings.UIScale = 1.5f; Settings.SubtitleScale = 2.f;
	FSovObjectivePresentationTestAccess::Settings(*F.Presentation, Settings);
	FSovObjectivePresentationTestAccess::Layout(*F.Presentation, 1280.f, 720.f);
	TestEqual(TEXT("Both full rows fit before competing speech"), FSovObjectivePresentationTestAccess::VisibleRows(*F.Presentation), 2);
	const float FullBudget = FSovObjectivePresentationTestAccess::HeightBudget(*F.Presentation);
	F.Presentation->PresentSpeech(LOCTEXT("Speaker", "Selene"), LOCTEXT("Speech", "There is another route."), 5.f, FVector::ZeroVector, false);
	F.Presentation->PresentCaption(LOCTEXT("Warning", "Corridor collapsing"), 5.f, FVector::ZeroVector, ESovCaptionPriority::Critical);
	FSovObjectivePresentationTestAccess::Layout(*F.Presentation, 640.f, 360.f);
	TestTrue(TEXT("Critical text reduces the objective height budget"), FSovObjectivePresentationTestAccess::HeightBudget(*F.Presentation) < FullBudget);
	TestEqual(TEXT("Tiny safe area defers entire rows instead of cutting failure text"), FSovObjectivePresentationTestAccess::VisibleRows(*F.Presentation), 0);
	TestFalse(TEXT("No room cannot paint over critical captions or speech"), FSovObjectivePresentationTestAccess::Visible(*F.Presentation));
	TestEqual(TEXT("Deferred rows remain counted"), F.Presentation->GetAdditionalObjectiveCount(), 2);
	TestTrue(TEXT("Failure rule remains intact for later presentation"), F.Presentation->GetPresentedObjectives()[0].FailureRule.EqualTo(Mission->Beats[1].FailureRuleText));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectivePresentationReviewTest, "ProjectVelkorran.UI.Objectives.OverflowReviewAndImmediateClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectivePresentationReviewTest::RunTest(const FString&)
{
	FObjectivePresentationWorld F; if (!F.Frontend || !F.Presentation) { AddError(TEXT("Fixture failed")); return false; }
	auto* Mission = F.Mission();
	for (int32 Index = 0; Index < 5; ++Index)
	{
		FSovCampaignBeatDefinition Beat; Beat.BeatId = FName(*FString::Printf(TEXT("KnownGroup%d"), Index)); Beat.bOptional = true;
		Beat.ObjectiveText = FText::Format(LOCTEXT("KnownGroup", "Help survivor group {0}"), FText::AsNumber(Index)); Mission->Beats.Add(Beat);
	}
	FSovCampaignBeatDefinition Future; Future.BeatId = TEXT("FutureMeeting"); Future.bOptional = true;
	Future.PrerequisiteBeats = { TEXT("EscapeTogether") }; Future.ObjectiveText = LOCTEXT("FutureMeeting", "A future meeting must remain hidden"); Mission->Beats.Add(Future);
	F.State->BeginMission(Mission); F.State->CompleteBeat(TEXT("ReachSurvivors"));
	F.State->TransitionObjective(TEXT("OptionalRescue"), ESovObjectiveState::Active);
	auto* Review = NewObject<USovAccessibleRecordMenu>(F.PC); Review->SetOwningPlayer(F.PC); Review->Initialize(); Review->TakeWidget();
	FSovObjectivePresentationTestAccess::ConstructReview(*Review); Review->ActivateWidget(); Review->SetObjectiveReviewMode();
	TestTrue(TEXT("Real accessible menu is active"), Review->IsActivated());
	TestEqual(TEXT("Review includes all seven authorized goals, including HUD overflow"), FSovObjectivePresentationTestAccess::ReviewCount(*Review), 7);
	const FString Body = FSovObjectivePresentationTestAccess::ReviewBody(*Review).ToString();
	TestTrue(TEXT("Review preserves full active goal"), Body.Contains(Mission->Beats[1].ObjectiveText.ToString()));
	TestTrue(TEXT("Review preserves full authored failure rule"), Body.Contains(Mission->Beats[1].FailureRuleText.ToString()));
	TestFalse(TEXT("Future row is absent from the shared authorized cache"), F.Presentation->GetObjectiveReviewEntries().ContainsByPredicate([](const auto& Entry) { return Entry.BeatId == TEXT("FutureMeeting"); }));
	F.Presentation->ClearObjectives();
	TestEqual(TEXT("Open review immediately clears without a navigation event"), FSovObjectivePresentationTestAccess::ReviewCount(*Review), 0);
	TestFalse(TEXT("Previous private failure text is no longer on screen"), FSovObjectivePresentationTestAccess::ReviewBody(*Review).ToString().Contains(Mission->Beats[1].FailureRuleText.ToString()));
	Review->DeactivateWidget(); FSovObjectivePresentationTestAccess::DestructReview(*Review);
	return true;
}
#undef LOCTEXT_NAMESPACE
#endif
