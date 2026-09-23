// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovFrontendRuntimeTestFixtures.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeGameplayTags.h"
#include "Campaign/SovCampaignInteractionTerminal.h"
#include "Campaign/SovCampaignEncounterObjective.h"
#include "Campaign/SovAurelionRequestActor.h"
#include "Campaign/SovEncounterDirector.h"
#include "Campaign/SovEncounterCoordinationComponent.h"
#include "World/SovWorldTransitActor.h"
#include "Components/BoxComponent.h"
#include "UI/SovObjectiveWaypoint.h"
#include "Tests/SovCampaignRuntimeTestFixtures.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Character/PlayerDefinition.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "ICommonInputModule.h"
#include "UObject/StrongObjectPtr.h"
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
    // Presentation fixture only; this does not qualify encounter gameplay.
    static void ActiveEncounter(ASovEncounterDirector& Director) { Director.State = ESovEncounterState::Active; }
    static void FailedEncounter(ASovEncounterDirector& Director) { Director.State = ESovEncounterState::Failed; }
	static void Bind(USovFrontendComponent& Frontend, USovAccessibilityPresentation* Presentation,
		ASovPlayerController* Controller, USovCampaignStateComponent* Campaign)
	{
		Frontend.Presentation = Presentation;
		Frontend.BindObjectives(Controller, Campaign); Frontend.RefreshObjectives();
	}
	static void Refresh(USovFrontendComponent& Frontend) { Frontend.RefreshObjectives(false); }
	static void Begin(USovFrontendComponent& Frontend) { Frontend.RegisterAllComponentTickFunctions(true); Frontend.BeginPlay(); }
	static void End(USovFrontendComponent& Frontend) { if (Frontend.HasBegunPlay()) { Frontend.EndPlay(EEndPlayReason::RemovedFromWorld); } }
	static void Settings(USovAccessibilityPresentation& Presentation, const FSovUserSettingsSnapshot& Value)
	{ Presentation.SettingsChanged(Value); }
	static bool Visible(const USovAccessibilityPresentation& Presentation)
	{ return Presentation.ObjectiveBackground && Presentation.ObjectiveBackground->GetVisibility() != ESlateVisibility::Collapsed; }
	static int32 FontSize(const USovAccessibilityPresentation& Presentation) { return Presentation.ObjectiveText->GetFont().Size; }
	static float OverflowHeight(const USovAccessibilityPresentation& Presentation) { return float(Presentation.ObjectiveOverflow->GetDesiredSize().Y); }
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
	static float FirstRowHeight(const USovAccessibilityPresentation& Presentation) { return float(Presentation.ObjectiveRowPanels[0]->GetDesiredSize().Y); }
	static float PanelHeight(const USovAccessibilityPresentation& Presentation) { return float(Presentation.ObjectiveBackground->GetDesiredSize().Y); }
	static float PanelPaddingHeight(const USovAccessibilityPresentation& Presentation)
	{ const auto Padding = Presentation.ObjectiveBackground->GetPadding(); return Padding.Top + Padding.Bottom; }
	static bool OverflowVisible(const USovAccessibilityPresentation& Presentation)
	{ return Presentation.ObjectiveOverflow->GetVisibility() != ESlateVisibility::Collapsed; }
	static const UCanvasPanelSlot* ObjectiveSlot(const USovAccessibilityPresentation& Presentation)
	{ return Cast<UCanvasPanelSlot>(Presentation.ObjectiveBackground->Slot); }
	static FBox2D PanelRect(const UBorder& Panel, const FVector2D& SafeSize)
	{
		const auto* Slot = CastChecked<UCanvasPanelSlot>(Panel.Slot);
		const FVector2D Size = Panel.GetDesiredSize();
		const FVector2D Min = Slot->GetAnchors().Minimum * SafeSize + Slot->GetPosition() - Slot->GetAlignment() * Size;
		return FBox2D(Min, Min + Size);
	}
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
		TStrongObjectPtr<ULocalPlayer> LocalPlayer{NewObject<ULocalPlayer>(GEngine)};
		UWorld* World = nullptr;
		ASovFrontendRuntimeController* PC = nullptr;
		ASovCampaignRuntimeTestPawn* Pawn = nullptr;
		USovCampaignStateComponent* State = nullptr;
		USovFrontendComponent* Frontend = nullptr;
		USovAccessibilityPresentation* Presentation = nullptr;
		TSharedPtr<SWidget> PresentationSlate;
		FObjectivePresentationWorld()
		{
			const auto IVS = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->InitializeActorsForPlay(FURL());
			PC = World->SpawnActor<ASovFrontendRuntimeController>(); Pawn = World->SpawnActor<ASovCampaignRuntimeTestPawn>();
			if (!PC || !Pawn) { return; }
			ICommonInputModule::GetSettings().LoadData();
			FSovObjectivePresentationTestAccess::Begin(*PC->GetFrontend());
			PC->Player = LocalPlayer.Get(); LocalPlayer->PlayerController = PC; PC->SetAsLocalPlayerController();
			PC->StagePawn(Pawn); State = PC->GetCampaignState(); Frontend = PC->GetFrontend();
			Presentation = NewObject<USovAccessibilityPresentation>(PC);
			Presentation->SetOwningPlayer(PC); Presentation->Initialize();
			// A real viewport retains Slate; releasing it here would make text measurement return zero.
			PresentationSlate = Presentation->TakeWidget();
			FSovObjectivePresentationTestAccess::Settings(*Presentation, FSovUserSettingsSnapshot());
			FSovObjectivePresentationTestAccess::Bind(*Frontend, Presentation, PC, State);
		}
		~FObjectivePresentationWorld()
		{
			if (Frontend) { FSovObjectivePresentationTestAccess::End(*Frontend); }
			PresentationSlate.Reset();
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
	if (!TestTrue(TEXT("Controller frontend activates through normal world initialization"), F.Frontend->IsActive())) { return false; }
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
	TestTrue(TEXT("Objective measurement retains the real Slate tree"), F.Presentation->GetCachedWidget().IsValid());
	TestTrue(TEXT("Overflow text has an actual measured height"), FSovObjectivePresentationTestAccess::OverflowHeight(*F.Presentation) > 0.f);
	TestEqual(TEXT("Compact objective font follows UI scale independently of subtitle scale"), FSovObjectivePresentationTestAccess::FontSize(*F.Presentation), 27);
	Settings.bHighContrastHUD = true; FSovObjectivePresentationTestAccess::Settings(*F.Presentation, Settings);
	TestEqual(TEXT("High-contrast objectives keep the larger readable type"), FSovObjectivePresentationTestAccess::FontSize(*F.Presentation), 30);
	Settings.bHighContrastHUD = false; FSovObjectivePresentationTestAccess::Settings(*F.Presentation, Settings);
	Settings.bShowObjectiveText = false; FSovObjectivePresentationTestAccess::Settings(*F.Presentation, Settings);
	TestFalse(TEXT("Player can hide objective text"), FSovObjectivePresentationTestAccess::Visible(*F.Presentation));
	TestEqual(TEXT("Hiding text never changes campaign state"), F.State->GetObjectiveState(Mission->MissionId, TEXT("ReachSurvivors")), ESovObjectiveState::Available);
	Settings.bShowObjectiveText = true; FSovObjectivePresentationTestAccess::Settings(*F.Presentation, Settings);
	TestTrue(TEXT("Showing text restores current view"), FSovObjectivePresentationTestAccess::Visible(*F.Presentation));
    F.Pawn->InitializePresentationTestASC();
    auto* ASC = F.Pawn->GetNarrativeAbilitySystemComponent();
    if (!TestNotNull(TEXT("Presentation fixture has a real ASC"), ASC)) { return false; }
    const auto CinematicTag = FNarrativeGameplayTags::Get().State_SequencerControlled;
    const int32 ReviewCount = F.Presentation->GetObjectiveReviewEntries().Num();
    ASC->AddLooseGameplayTag(CinematicTag);
    FSovObjectivePresentationTestAccess::Settings(*F.Presentation, Settings);
    TestFalse(TEXT("Cinematic ownership hides the objective overlay"), FSovObjectivePresentationTestAccess::Visible(*F.Presentation));
    TestEqual(TEXT("Cinematic presentation preserves authorized objective review"), F.Presentation->GetObjectiveReviewEntries().Num(), ReviewCount);
    ASC->RemoveLooseGameplayTag(CinematicTag);
    FSovObjectivePresentationTestAccess::Settings(*F.Presentation, Settings);
    TestTrue(TEXT("Releasing cinematic ownership restores current objectives"), FSovObjectivePresentationTestAccess::Visible(*F.Presentation));
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
	// Find the actual Slate row's fit boundary instead of copying the layout's height formula.
	float OneRowSafeHeight = 0.f;
	for (float Height = 80.f; Height <= 720.f; Height += 8.f)
	{
		FSovObjectivePresentationTestAccess::Layout(*F.Presentation, 640.f, Height);
		if (FSovObjectivePresentationTestAccess::VisibleRows(*F.Presentation) == 1) { OneRowSafeHeight = Height; break; }
	}
	if (!TestTrue(TEXT("A real measured goal fits within the supported safe height"), OneRowSafeHeight > 80.f)) { return false; }
	FSovObjectivePresentationTestAccess::Layout(*F.Presentation, 640.f, OneRowSafeHeight - 8.f);
	TestEqual(TEXT("Just below the measured fit boundary no partial goal is shown"), FSovObjectivePresentationTestAccess::VisibleRows(*F.Presentation), 0);
	FSovObjectivePresentationTestAccess::Layout(*F.Presentation, 640.f, OneRowSafeHeight);
	TestEqual(TEXT("One fitting goal requires no room for the hidden overflow link"), FSovObjectivePresentationTestAccess::VisibleRows(*F.Presentation), 1);
	TestTrue(TEXT("Hidden overflow label cannot hide a fitting single goal"), FSovObjectivePresentationTestAccess::Visible(*F.Presentation));
	TestFalse(TEXT("No hidden-overflow row consumes visible space"), FSovObjectivePresentationTestAccess::OverflowVisible(*F.Presentation));
	TestTrue(TEXT("The visible panel ends after the complete goal and its border padding"), FMath::IsNearlyEqual(
		FSovObjectivePresentationTestAccess::PanelHeight(*F.Presentation),
		FSovObjectivePresentationTestAccess::FirstRowHeight(*F.Presentation) + FSovObjectivePresentationTestAccess::PanelPaddingHeight(*F.Presentation), .1f));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectivePresentationCornerTest, "ProjectVelkorran.UI.Objectives.CornerStaysFixedThroughPriorityText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectivePresentationCornerTest::RunTest(const FString&)
{
	FObjectivePresentationWorld F; if (!F.Frontend || !F.Presentation) { AddError(TEXT("Fixture failed")); return false; }
	F.State->BeginMission(F.Mission()); const auto Before = F.Save();
	const auto CheckCorner = [&]()
	{
		const auto* Slot = FSovObjectivePresentationTestAccess::ObjectiveSlot(*F.Presentation);
		if (!TestNotNull(TEXT("Objective retains its safe-canvas slot"), Slot)) { return; }
		TestEqual(TEXT("Corner anchor minimum stays fixed"), Slot->GetAnchors().Minimum, FVector2D::ZeroVector);
		TestEqual(TEXT("Corner anchor maximum stays fixed"), Slot->GetAnchors().Maximum, FVector2D::ZeroVector);
		TestEqual(TEXT("Corner alignment stays fixed"), Slot->GetAlignment(), FVector2D::ZeroVector);
		TestEqual(TEXT("Corner uses a small pixel inset at every safe size"), Slot->GetPosition(), FVector2D(12.f, 12.f));
	};
	const FVector2D WideSize(1280.f, 720.f);
	FSovObjectivePresentationTestAccess::Layout(*F.Presentation, float(WideSize.X), float(WideSize.Y)); CheckCorner();
	const float UnobstructedBudget = FSovObjectivePresentationTestAccess::HeightBudget(*F.Presentation);
	F.Presentation->PresentCaption(LOCTEXT("ShortCornerWarning", "Ping"), 5.f, FVector::ZeroVector, ESovCaptionPriority::Critical);
	FSovObjectivePresentationTestAccess::Layout(*F.Presentation, float(WideSize.X), float(WideSize.Y)); CheckCorner();
	const auto ShortObjective = FSovObjectivePresentationTestAccess::PanelRect(*F.Presentation->GetObjectivePanel(), WideSize);
	const auto ShortCaption = FSovObjectivePresentationTestAccess::PanelRect(*F.Presentation->GetCaptionPanel(), WideSize);
	TestTrue(TEXT("Short centered caption is actually horizontally clear of the corner"), ShortObjective.Max.X + 12.f < ShortCaption.Min.X);
	TestTrue(TEXT("A horizontally clear critical caption does not remove the goal"), FSovObjectivePresentationTestAccess::Visible(*F.Presentation));
	TestEqual(TEXT("A horizontally clear caption does not reduce the goal budget"), FSovObjectivePresentationTestAccess::HeightBudget(*F.Presentation), UnobstructedBudget);
	F.Presentation->ClearSceneHistory();
	const FVector2D SmallSize(640.f, 360.f);
	FSovObjectivePresentationTestAccess::Layout(*F.Presentation, float(SmallSize.X), float(SmallSize.Y)); CheckCorner();
	const auto BeforeWarningRect = FSovObjectivePresentationTestAccess::PanelRect(*F.Presentation->GetObjectivePanel(), SmallSize);
	if (!TestTrue(TEXT("Goal fits in the small safe area before priority text"), FSovObjectivePresentationTestAccess::Visible(*F.Presentation))) { return false; }
	F.Presentation->PresentCaption(LOCTEXT("LongCornerWarning", "The corridor is collapsing. Move away from the broken support immediately."), 5.f, FVector::ZeroVector, ESovCaptionPriority::Critical);
	F.Presentation->PresentSpeech(LOCTEXT("CornerSpeaker", "Selene"), LOCTEXT("CornerSpeech", "Keep clear of the collapsing corridor."), 5.f, FVector::ZeroVector, false);
	FSovObjectivePresentationTestAccess::Layout(*F.Presentation, float(SmallSize.X), float(SmallSize.Y)); CheckCorner();
	const auto LongCaption = FSovObjectivePresentationTestAccess::PanelRect(*F.Presentation->GetCaptionPanel(), SmallSize);
	TestTrue(TEXT("Long caption really occupies the goal's horizontal range"), BeforeWarningRect.Max.X > LongCaption.Min.X && BeforeWarningRect.Min.X < LongCaption.Max.X);
	TestFalse(TEXT("Critical caption remains visible while the corner is deferred"), F.Presentation->GetCaptionPanel()->GetVisibility() == ESlateVisibility::Collapsed);
	TestFalse(TEXT("Speech remains visible while the corner is deferred"), F.Presentation->GetSubtitlePanel()->GetVisibility() == ESlateVisibility::Collapsed);
	TestEqual(TEXT("No complete goal can overlap the long critical caption"), FSovObjectivePresentationTestAccess::VisibleRows(*F.Presentation), 0);
	TestFalse(TEXT("Deferred panel cannot overlap priority text"), FSovObjectivePresentationTestAccess::Visible(*F.Presentation));
	TestEqual(TEXT("Deferred goal stays available for review"), F.Presentation->GetAdditionalObjectiveCount(), 1);
	F.Presentation->ClearSceneHistory();
	FSovObjectivePresentationTestAccess::Layout(*F.Presentation, float(SmallSize.X), float(SmallSize.Y)); CheckCorner();
	TestTrue(TEXT("Goal returns to the same corner when priority text retires"), FSovObjectivePresentationTestAccess::Visible(*F.Presentation));
	FSovObjectivePresentationTestAccess::Layout(*F.Presentation, 1920.f, 1080.f); CheckCorner();
	TestTrue(TEXT("Corner fitting never changes serialized campaign truth"), Before == F.Save());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectivePresentationCompactTest, "ProjectVelkorran.UI.Objectives.VisibleContentShrinksAfterOverflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectivePresentationCompactTest::RunTest(const FString&)
{
	FObjectivePresentationWorld F; if (!F.Frontend || !F.Presentation) { AddError(TEXT("Fixture failed")); return false; }
	F.State->BeginMission(F.Mission()); const auto Before = F.Save();
	const auto Entries = F.Presentation->GetPresentedObjectives();
	for (const float Scale : { 1.f, 2.f })
	{
		FSovUserSettingsSnapshot Settings; Settings.UIScale = Scale;
		FSovObjectivePresentationTestAccess::Settings(*F.Presentation, Settings);
		F.Presentation->PresentObjectives(Entries, 12);
		FSovObjectivePresentationTestAccess::Layout(*F.Presentation, 1920.f, 1080.f);
		if (!TestTrue(TEXT("Fixture measures a visible review overflow row"), FSovObjectivePresentationTestAccess::OverflowVisible(*F.Presentation))) { return false; }
		const float WithOverflow = FSovObjectivePresentationTestAccess::PanelHeight(*F.Presentation);
		F.Presentation->PresentObjectives(Entries, 0);
		FSovObjectivePresentationTestAccess::Layout(*F.Presentation, 1920.f, 1080.f);
		TestEqual(TEXT("The complete goal remains visible after overflow retires"), FSovObjectivePresentationTestAccess::VisibleRows(*F.Presentation), 1);
		TestFalse(TEXT("Retired overflow is not painted"), FSovObjectivePresentationTestAccess::OverflowVisible(*F.Presentation));
		const float CompactHeight = FSovObjectivePresentationTestAccess::PanelHeight(*F.Presentation);
		TestTrue(TEXT("Final prepass shrinks the panel after overflow removal"), CompactHeight < WithOverflow);
		TestTrue(TEXT("Final height contains only measured goal and border padding at both scales"), FMath::IsNearlyEqual(CompactHeight,
			FSovObjectivePresentationTestAccess::FirstRowHeight(*F.Presentation) + FSovObjectivePresentationTestAccess::PanelPaddingHeight(*F.Presentation), .1f));
		TestTrue(TEXT("Complete row remains within the available safe-height budget"),
			FSovObjectivePresentationTestAccess::FirstRowHeight(*F.Presentation) <= FSovObjectivePresentationTestAccess::HeightBudget(*F.Presentation));
	}
	TestTrue(TEXT("Overflow layout does not change serialized campaign truth"), Before == F.Save());
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectiveWaypointBindingTest, "ProjectVelkorran.UI.Objectives.WaypointUsesCurrentAuthoredBinding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectiveWaypointBindingTest::RunTest(const FString&)
{
    FObjectivePresentationWorld F;
    if (!F.State || !F.Presentation) { AddError(TEXT("Fixture failed")); return false; }
    // This existing state fixture stages only Controller.Pawn. Its override calls the real
    // APawn possession path while deliberately excluding Narrative content initialization.
    F.Pawn->PossessedBy(F.PC);
    if (!TestTrue(TEXT("Native pawn/controller ownership is reciprocal"), F.Pawn->GetController() == F.PC)) { return false; }
    auto* Mission = F.Mission();
    if (!TestEqual(TEXT("Native mission begins"), F.State->BeginMission(Mission), ESovCampaignResult::Applied)) { return false; }
    auto* Current = F.World->SpawnActor<ASovCampaignInteractionTerminal>();
    auto* Future = F.World->SpawnActor<ASovCampaignInteractionTerminal>();
    auto* Foreign = F.World->SpawnActor<ASovCampaignInteractionTerminal>();
    if (!Current || !Future || !Foreign) { AddError(TEXT("Native terminals failed to spawn")); return false; }
    Current->TerminalId = TEXT("CurrentTerminal"); Current->MissionId = Mission->MissionId; Current->CompletionBeat = TEXT("ReachSurvivors");
    Future->TerminalId = TEXT("FutureTerminal"); Future->MissionId = Mission->MissionId; Future->CompletionBeat = TEXT("EscapeTogether");
    Foreign->TerminalId = TEXT("ForeignTerminal"); Foreign->MissionId = TEXT("OtherMission"); Foreign->CompletionBeat = Current->CompletionBeat;
    TArray<TWeakObjectPtr<AActor>> Sources = {Future, Foreign, Current};
    FSovObjectiveWaypoint View;
    const auto Before = F.Save();
    TestTrue(TEXT("Existing authoritative terminal resolves even before interaction range"),
        SovObjectiveWaypoint::Resolve(F.PC, F.Presentation->GetPresentedObjectives(), Sources, View));
    TestTrue(TEXT("Future and foreign matching names cannot win"), View.Target.Get() == Current);
    TestTrue(TEXT("Waypoint pins the real native Body, not imported visual bounds"), View.Anchor.Get() == Current->Body.Get());
    TestTrue(TEXT("Resolved goal is still current before paint"), SovObjectiveWaypoint::IsCurrent(F.PC, View));
    TestTrue(TEXT("Navigation does not mutate the serialized campaign"), Before == F.Save());
    auto* Duplicate = F.World->SpawnActor<ASovCampaignInteractionTerminal>();
    if (!Duplicate) { return false; }
    Duplicate->TerminalId = Current->TerminalId; Duplicate->MissionId = Current->MissionId; Duplicate->CompletionBeat = Current->CompletionBeat;
    Sources.Add(Duplicate);
    TestFalse(TEXT("Duplicate native terminal identity refuses a guessed destination"),
        SovObjectiveWaypoint::Resolve(F.PC, F.Presentation->GetPresentedObjectives(), Sources, View));
    Duplicate->Destroy();
    TestTrue(TEXT("Retiring the duplicate restores the exact authored hint"),
        SovObjectiveWaypoint::Resolve(F.PC, F.Presentation->GetPresentedObjectives(), Sources, View));
    F.Pawn->TestHero = FSovGameplayTags::Get().Character_Player_Selene;
    TestFalse(TEXT("Outgoing protagonist hint retires before handoff commit"), SovObjectiveWaypoint::IsCurrent(F.PC, View));
    F.Pawn->TestHero = Mission->Protagonist;
    F.State->CompleteBeat(TEXT("ReachSurvivors"));
    TestFalse(TEXT("Native completion immediately invalidates the previous marker"), SovObjectiveWaypoint::IsCurrent(F.PC, View));
    TestTrue(TEXT("Newly actionable native terminal becomes the next marker"),
        SovObjectiveWaypoint::Resolve(F.PC, F.Presentation->GetPresentedObjectives(), Sources, View));
    TestTrue(TEXT("New marker is the future terminal only after its prerequisite commits"), View.Target.Get() == Future);
    Future->Destroy();
    TestFalse(TEXT("Destroyed target cannot leave a screen-space ghost"), SovObjectiveWaypoint::IsCurrent(F.PC, View));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectiveRescueDoorWaypointTest, "ProjectVelkorran.UI.Objectives.RescueDoorWaypointFollowsPendingWave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectiveRescueDoorWaypointTest::RunTest(const FString&)
{
    FObjectivePresentationWorld F;
    if (!F.State || !F.Presentation) { return false; }
    F.Pawn->PossessedBy(F.PC);
    auto* Mission = F.Mission();
    Mission->Beats[0].RequiredEncounterId = TEXT("Test.Rescue");
    Mission->Beats[0].RequiredProtagonist = Mission->Protagonist;
    Mission->Beats[0].ObjectiveType = ESovObjectiveType::EliminateDisable;
    if (!TestEqual(TEXT("Mission begins"), F.State->BeginMission(Mission), ESovCampaignResult::Applied)) { return false; }
    auto* Director = F.World->SpawnActor<ASovEncounterDirector>();
    auto* Objective = F.World->SpawnActor<ASovCampaignEncounterObjective>();
    auto* Door = F.World->SpawnActor<ASovWorldTransitActor>();
    if (!Director || !Objective || !Door) { return false; }
    Director->EncounterId = TEXT("Test.Rescue");
    FSovObjectivePresentationTestAccess::ActiveEncounter(*Director);
    Objective->MissionId = Mission->MissionId; Objective->CompletionBeat = TEXT("ReachSurvivors");
    Objective->EncounterDirector = Director;
    Door->TransitId = TEXT("Test.RescueAccess"); Door->RequiredMission = Mission->MissionId;
    auto* Coordination = Director->GetCoordinationComponent();
    FSovEncounterWaveReleaseRule Rule; Rule.Wave = 1;
    Rule.Condition = ESovEncounterWaveCondition::TransitDoorOpen; Rule.TransitDoor = Door;
    Coordination->WaveReleaseRules.Add(Rule);
    TArray<TWeakObjectPtr<AActor>> Sources = {Objective};
    FSovObjectiveWaypoint View; const auto Before = F.Save();
    TestTrue(TEXT("Active rescue points to the door needed for its next wave"),
        SovObjectiveWaypoint::Resolve(F.PC, F.Presentation->GetPresentedObjectives(), Sources, View));
    TestTrue(TEXT("Marker follows the physical moving door"), View.Target.Get() == Door && View.Anchor.Get() == Door->MovingBody.Get());
    Door->SetPower(false);
    TestFalse(TEXT("Unpowered door cannot retain a stale usable marker"), SovObjectiveWaypoint::IsCurrent(F.PC, View));
    Door->SetPower(true); Coordination->WaveReleaseRules[0].Wave = 2;
    TestFalse(TEXT("A later wave cannot redirect the current rescue"),
        SovObjectiveWaypoint::Resolve(F.PC, F.Presentation->GetPresentedObjectives(), Sources, View));
    Coordination->WaveReleaseRules[0].Wave = 1; Door->RequiredMission = TEXT("OtherMission");
    TestFalse(TEXT("Foreign mission door is never suggested"),
        SovObjectiveWaypoint::Resolve(F.PC, F.Presentation->GetPresentedObjectives(), Sources, View));
    TestTrue(TEXT("Guidance never supplies campaign completion"), Before == F.Save());
    TestEqual(TEXT("Guidance never opens the door"), Door->GetTransitState(), ESovWorldTransitState::AtOrigin);
    auto* Retry = F.World->SpawnActor<ASovAurelionRequestActor>();
    if (!Retry) { return false; }
    Retry->MissionId = Mission->MissionId; Retry->BeatId = TEXT("ReachSurvivors");
    Retry->RequestId = TEXT("Test.RescueRetry"); Retry->Operation = ESovAurelionRequest::RetryEncounter;
    Retry->RetryObjective = Objective; Retry->RetryDirector = Director; Sources.Add(Retry);
    FSovObjectivePresentationTestAccess::FailedEncounter(*Director);
    TestTrue(TEXT("Loaded or failed rescue points to its authored retry control"),
        SovObjectiveWaypoint::Resolve(F.PC, F.Presentation->GetPresentedObjectives(), Sources, View));
    TestTrue(TEXT("Retry control is the physical target"), View.Target.Get() == Retry && View.Anchor.Get() == Retry->Body.Get());
    TestTrue(TEXT("Failed attempt exposes retry guidance instead of a generic objective"), View.Kind == FSovObjectiveWaypoint::EKind::Retry);
    Retry->RetryObjective = nullptr;
    TestFalse(TEXT("Broken retry binding retires its marker"), SovObjectiveWaypoint::IsCurrent(F.PC, View));
    Retry->RetryObjective = Objective; FSovObjectivePresentationTestAccess::ActiveEncounter(*Director);
    TestFalse(TEXT("A restarted encounter retires the retry marker"), SovObjectiveWaypoint::IsCurrent(F.PC, View));
    TestTrue(TEXT("Retry guidance never mutates the campaign"), Before == F.Save());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovObjectiveWaypointProjectionTest, "ProjectVelkorran.UI.Objectives.WaypointSafeAreaAndBehindCamera",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovObjectiveWaypointProjectionTest::RunTest(const FString&)
{
    FVector2D Position; bool bAtEdge = false;
    const FVector2D Min(100, 80), Max(900, 640);
    TestTrue(TEXT("Visible target projects inside the actual safe rectangle"),
        SovObjectiveWaypoint::FitToSafeRect(FVector2D(320, 240), true, FVector2D(0, -1), Min, Max, Position, bAtEdge));
    TestEqual(TEXT("Visible marker retains its target position"), Position, FVector2D(320, 240));
    TestFalse(TEXT("Visible target is not an edge cue"), bAtEdge);
    TestTrue(TEXT("Off-screen right target gets a bounded bearing"),
        SovObjectiveWaypoint::FitToSafeRect(FVector2D(1500, 360), true, FVector2D(1, 0), Min, Max, Position, bAtEdge));
    TestEqual(TEXT("Right edge respects the safe area"), Position, FVector2D(900, 360));
    TestTrue(TEXT("Off-screen bearing is explicit"), bAtEdge);
    TestTrue(TEXT("Behind-camera target ignores misleading projected coordinates"),
        SovObjectiveWaypoint::FitToSafeRect(FVector2D(320, 240), false, FVector2D(0, 1), Min, Max, Position, bAtEdge));
    TestEqual(TEXT("Behind target points down, never onto the visible world"), Position, FVector2D(500, 640));
    TestTrue(TEXT("Behind cue is an edge cue"), bAtEdge);
    TestFalse(TEXT("Empty safe area draws nothing"),
        SovObjectiveWaypoint::FitToSafeRect(FVector2D::ZeroVector, false, FVector2D::ZeroVector, Min, Min, Position, bAtEdge));
    return true;
}

#undef LOCTEXT_NAMESPACE
#endif
