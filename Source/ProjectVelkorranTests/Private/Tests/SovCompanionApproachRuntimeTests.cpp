// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovCompanionApproachTestFixtures.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovAurelionMissionDefinition.h"
#include "Companions/SovConvergenceCompanionState.h"
#include "Companions/SovCompanionComponent.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "AI/NarrativeNPCController.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "Characters/SovTarrikCharacter.h"
#include "Characters/SovSeleneCharacter.h"
#include "Character/PlayerDefinition.h"
#include "AI/NPCDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
struct FSovCompanionApproachTestAccess
{
	static ASovProtagonistCompanionCharacter* Staged(const USovConvergenceCompanionState* State) { return State->Staged; }
	static void SetSavedMembership(USovConvergenceCompanionState* State, bool bPresent)
	{ State->bHasSavedCompanion = bPresent; }
	// Restore fixture: seed accepted prior handoff records, then exercise production
	// journal validation and companion admission. This does not call a gameplay bypass.
	static void SeedPriorHandoff(USovCampaignStateComponent* State, FName BeatId)
	{
		const auto* Mission = State->GetActiveMission(); const auto* Beat = Mission->FindBeat(BeatId);
		auto& Entry = State->Journal.AddDefaulted_GetRef(); Entry.EventId = FGuid::NewGuid();
		Entry.Sequence = State->Journal.Num(); Entry.MissionId = Mission->MissionId; Entry.BeatId = BeatId;
		Entry.Protagonist = Beat->RequiredProtagonist; Entry.HandoffToProtagonist = Beat->HandoffToProtagonist;
		Entry.HandoffAnchorId = Beat->RequiredHandoffAnchorId; Entry.HandoffRequestId = FGuid::NewGuid();
		auto& Record = State->Missions.FindChecked(Mission->MissionId);
		Record.CompletedBeats.Add(BeatId); Record.ObjectiveStates.Add(BeatId, ESovObjectiveState::Succeeded);
		State->ActiveProtagonist = Beat->HandoffToProtagonist;
	}
	static void SelectDepartureForCommit(USovCampaignStateComponent* State,
		USovAurelionContraryWitnessMissionDefinition* Mission, bool bComplete)
	{
		State->ActiveMission = Mission;
		auto& Record = State->Missions.FindOrAdd(Mission->MissionId);
		Record.bSucceeded = bComplete;
		Record.CompletedBeats.Remove(TEXT("SeparateDepartures"));
		if (bComplete) { Record.CompletedBeats.Add(TEXT("SeparateDepartures")); }
	}
};
namespace
{
	struct FApproachWorld
	{
#if WITH_EDITOR
		FEditorScriptExecutionGuard ScriptGuard;
#endif
		UWorld* World = nullptr;
		FApproachWorld()
		{
			const auto IVS = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
		}
		~FApproachWorld() { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
	};
	USovCampaignDefinition* MakeApproachMission(ASovHandoffRuntimeTestController* PC)
	{
		auto* Mission = NewObject<USovCampaignDefinition>(PC); PC->KeepAlive.Add(Mission);
		const auto& Tags = FSovGameplayTags::Get();
		Mission->MissionId = TEXT("M12_ApproachLifecycleTest"); Mission->Protagonist = Tags.Character_Player_Tarrik;
		Mission->PawnClass = ASovTarrikCharacter::StaticClass();
		auto* TarrikDefinition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(TarrikDefinition); Mission->PlayerDefinition = TarrikDefinition;
		auto* SeleneDefinition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(SeleneDefinition);
		FSovCampaignProtagonistProfile Selene; Selene.Protagonist = Tags.Character_Player_Selene;
		Selene.PawnClass = ASovSeleneCharacter::StaticClass(); Selene.PlayerDefinition = SeleneDefinition; Mission->AlternateProtagonists.Add(Selene);
		for (bool bSelene : {false, true})
		{
			auto& Profile = Mission->ProtagonistCompanions.AddDefaulted_GetRef();
			Profile.Protagonist = bSelene ? Tags.Character_Player_Selene : Tags.Character_Player_Tarrik;
			Profile.CompanionId = bSelene ? FName(TEXT("Selene")) : FName(TEXT("Tarrik"));
			Profile.EntryAnchorTag = bSelene ? FName(TEXT("SeleneEntry")) : FName(TEXT("TarrikEntry"));
			Profile.CompanionClass = ASovProtagonistCompanionCharacter::StaticClass();
			auto* NPC = NewObject<UNPCDefinition>(PC); PC->KeepAlive.Add(NPC); Profile.CompanionDefinition = NPC;
			Mission->AllowedCompanionIds.Add(Profile.CompanionId);
		}
		FSovCampaignBeatDefinition Cut; Cut.BeatId = TEXT("SoloCut"); Cut.RequiredProtagonist = Tags.Character_Player_Tarrik;
		Cut.HandoffToProtagonist = Tags.Character_Player_Selene; Cut.RequiredHandoffAnchorId = TEXT("SoloAnchor"); Cut.bIsolatedPerspectiveCut = true;
		FSovCampaignBeatDefinition Join; Join.BeatId = TEXT("SharedEntry"); Join.RequiredProtagonist = Tags.Character_Player_Selene;
		Join.HandoffToProtagonist = Tags.Character_Player_Tarrik; Join.RequiredHandoffAnchorId = TEXT("SharedAnchor"); Join.PrerequisiteBeats = {Cut.BeatId};
		FSovCampaignBeatDefinition Swap = Cut; Swap.BeatId = TEXT("SharedSwap"); Swap.RequiredHandoffAnchorId = TEXT("SwapAnchor");
		Swap.bIsolatedPerspectiveCut = false; Swap.PrerequisiteBeats = {Join.BeatId};
		FSovCampaignBeatDefinition Finish; Finish.BeatId = TEXT("Finish"); Finish.RequiredProtagonist = Tags.Character_Player_Selene;
		Finish.PrerequisiteBeats = {Swap.BeatId};
		Mission->Beats = {Cut, Join, Swap, Finish}; Mission->CompanionActivationBeat = Join.BeatId;
		return Mission;
	}
	bool SaveBytes(UObject* Object, TArray<uint8>& Bytes)
	{
		Bytes.Reset(); FMemoryWriter Writer(Bytes); FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		Archive.ArIsSaveGame = true; Object->Serialize(Archive); return !Archive.IsError();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCompanionApproachMembershipTest, "ProjectVelkorran.Campaign.Companion.SeparateApproachesAndSavedMembership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCompanionApproachMembershipTest::RunTest(const FString& Parameters)
{
	FApproachWorld F; if (!F.World) { return false; }
	auto* PC = F.World->SpawnActor<ASovHandoffRuntimeTestController>(); auto* Pawn = F.World->SpawnActor<ASovHandoffRuntimeTestPawn>();
	if (!PC || !Pawn) { return false; } PC->Possess(Pawn);
	auto* Mission = MakeApproachMission(PC); auto* Campaign = PC->GetCampaignState(); auto* Companions = PC->GetConvergenceCompanionState();
	FString Reason; const auto Selene = FSovGameplayTags::Get().Character_Player_Selene;
	if (!TestTrue(TEXT("Separate approach definition validates without generic Resonance"), Mission->ValidateDefinition(Reason))) { AddError(Reason); return false; }
	TestEqual(TEXT("Actual campaign starts on Tarrik's approach"), Campaign->BeginMission(Mission), ESovCampaignResult::Applied);
	TestTrue(TEXT("Solo entry does not require an unplayed partner kit or companion anchors"), Companions->StageInitialCompanion(Mission, Mission->Protagonist, Reason));
	TestFalse(TEXT("Solo entry creates no protagonist proxy"), Companions->HasStagedProxy());
	TestTrue(TEXT("Authored separate approach cut is admitted without a live partner"), Companions->StageHandoff(Mission, Selene, Pawn, Reason, TEXT("SoloCut")));
	TestFalse(TEXT("Perspective cut creates no staged companion"), Companions->HasStagedProxy());
	TestNull(TEXT("Perspective cut never publishes a companion"), Companions->GetActiveCompanion());
	TestFalse(TEXT("Unrequested/default handoff cannot skip native beat context"), Companions->StageHandoff(Mission, Selene, Pawn, Reason));
	TestFalse(TEXT("A blocked later handoff cannot activate the partnership early"), Companions->StageHandoff(Mission, Selene, Pawn, Reason, TEXT("SharedSwap")));
	TArray<uint8> SoloCampaignBytes, NoCompanionBytes, ForgedCompanionBytes;
	Companions->PrepareForSave_Implementation();
	TestTrue(TEXT("Solo approach companion absence is serializable"), SaveBytes(Companions, NoCompanionBytes));
	TestTrue(TEXT("Actual campaign serializes"), SaveBytes(Campaign, SoloCampaignBytes));
	TestTrue(TEXT("Solo campaign journal independently validates"), USovCampaignStateComponent::ValidateSerializedSave(SoloCampaignBytes, Reason));
	TestTrue(TEXT("Solo checkpoint restores absence from its saved campaign journal"), Companions->StageSavedRecord(NoCompanionBytes, Mission, Mission->Protagonist, Reason, &SoloCampaignBytes));
	TestFalse(TEXT("Gated membership never guesses without its campaign journal"), Companions->StageSavedRecord(NoCompanionBytes, Mission, Mission->Protagonist, Reason));
	FSovCompanionApproachTestAccess::SetSavedMembership(Companions, true); SaveBytes(Companions, ForgedCompanionBytes);
	FSovCompanionApproachTestAccess::SetSavedMembership(Companions, false);
	TestFalse(TEXT("Injected companion membership is rejected before the meeting"), Companions->StageSavedRecord(ForgedCompanionBytes, Mission, Mission->Protagonist, Reason, &SoloCampaignBytes));
	FSovCompanionApproachTestAccess::SeedPriorHandoff(Campaign, TEXT("SoloCut"));
	FSovCompanionApproachTestAccess::SeedPriorHandoff(Campaign, TEXT("SharedEntry"));
	TArray<uint8> SharedCampaignBytes; SaveBytes(Campaign, SharedCampaignBytes);
	TestTrue(TEXT("Restore fixture's ordered handoff journal independently validates"), USovCampaignStateComponent::ValidateSerializedSave(SharedCampaignBytes, Reason));
	TestFalse(TEXT("Shared checkpoint cannot silently respawn a missing companion from a fresh kit"), Companions->StageSavedRecord(NoCompanionBytes, Mission, Mission->Protagonist, Reason, &SharedCampaignBytes));
	TestTrue(TEXT("Loading an earlier solo checkpoint uses saved progression, despite the later live gate"), Companions->StageSavedRecord(NoCompanionBytes, Mission, Mission->Protagonist, Reason, &SoloCampaignBytes));
	TestFalse(TEXT("Commit rechecks the actual restored campaign before accepting absent membership"), Companions->CommitStaged(Pawn, Reason));
	TestFalse(TEXT("Normal shared swap still requires the actual living companion"), Companions->StageHandoff(Mission, Selene, Pawn, Reason, TEXT("SharedSwap")));
	Companions->PrepareForSave_Implementation(); TArray<uint8> RejectedBytes;
	TestFalse(TEXT("Shared route cannot save without its required companion"), SaveBytes(Companions, RejectedBytes));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCompanionFirstSharedEntryTest, "ProjectVelkorran.Campaign.Companion.FirstSharedEntryCopiesOutgoingAndRestoresIncoming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCompanionFirstSharedEntryTest::RunTest(const FString& Parameters)
{
	FApproachWorld F; if (!F.World) { return false; }
	auto* PC = F.World->SpawnActor<ASovHandoffRuntimeTestController>();
	auto* PS = F.World->SpawnActor<ASovPlayerState>(); auto* FirstTarrik = F.World->SpawnActor<ASovHandoffRuntimeTestPawn>();
	if (!PC || !PS || !FirstTarrik) { return false; }
	PC->SetTestPlayerState(PS); auto* Mission = MakeApproachMission(PC); FString Reason;
	auto* Campaign = PC->GetCampaignState(); auto* Companions = PC->GetConvergenceCompanionState();
	auto* ASC = Cast<UNarrativeAbilitySystemComponent>(PS->GetAbilitySystemComponent()); if (!ASC) { return false; }
	auto* TarrikDefinition = Mission->PlayerDefinition.Get();
	auto* SeleneDefinition = Mission->AlternateProtagonists[0].PlayerDefinition.Get();
	const auto PreparePlayer = [PC, PS](ASovHandoffRuntimeTestPawn* Pawn, UPlayerDefinition* Definition)
	{
		if (!Pawn->PrepareCampaignInitialization(Definition)) { return false; }
		PC->Possess(Pawn); return Pawn->StageTestReadiness(PS, true);
	};
	if (!TestTrue(TEXT("Actual Tarrik begins with ready native player systems"), PreparePlayer(FirstTarrik, TarrikDefinition)
		&& FirstTarrik->CompleteCampaignDataInitialization(false))) { return false; }
	TestEqual(TEXT("First approach begins"), Campaign->BeginMission(Mission), ESovCampaignResult::Applied);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 61.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 11.f);
	FSovProtagonistSnapshot TarrikKit;
	if (!TestTrue(TEXT("Actual first-approach kit is captured and retained"), PS->CaptureProtagonistSnapshot(FirstTarrik, TarrikKit, Reason)
		&& PS->StoreProtagonistSnapshot(TarrikKit))) { AddError(Reason); return false; }
	FSovCompanionApproachTestAccess::SeedPriorHandoff(Campaign, TEXT("SoloCut"));
	PC->UnPossess(); FirstTarrik->Destroy(); ASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Tarrik);
	auto* Selene = F.World->SpawnActor<ASovCompanionApproachTestSelene>(); if (!Selene) { return false; }
	if (!TestTrue(TEXT("Actual separate Selene approach becomes ready"), PreparePlayer(Selene, SeleneDefinition)
		&& Selene->CompleteCampaignDataInitialization(false))) { return false; }
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 73.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 29.f);
	const auto SourceGrant = ASC->GiveAbility(FGameplayAbilitySpec(USovWeakPointFireTestAbility::StaticClass(), 3));
	auto& SeleneProfile = Mission->ProtagonistCompanions[1];
	SeleneProfile.CompanionClass = ASovCompanionApproachTestProxy::StaticClass();
	SeleneProfile.CompanionDefinition.Get()->NPCClassPath = ASovCompanionApproachTestProxy::StaticClass();
	SeleneProfile.CuratedCompanionAbilities = {USovWeakPointFireTestAbility::StaticClass(), USovWeakPointMeleeTestAbility::StaticClass()};
	auto* Anchor = F.World->SpawnActor<AActor>(); if (!Anchor) { return false; } Anchor->Tags.Add(SeleneProfile.EntryAnchorTag);
	if (!TestTrue(TEXT("First shared handoff stages actual outgoing Selene without inventing an incoming companion"),
		Companions->StageHandoff(Mission, Mission->Protagonist, Selene, Reason, TEXT("SharedEntry")))) { AddError(Reason); return false; }
	auto* Proxy = FSovCompanionApproachTestAccess::Staged(Companions);
	if (!TestNotNull(TEXT("Native deferred spawn produced the companion"), Proxy)) { return false; }
	auto* AI = Cast<ANarrativeNPCController>(Proxy->GetController());
	if (!TestNotNull(TEXT("Actual Narrative controller owns the staged proxy"), AI)) { return false; }
	TestTrue(TEXT("AI possession retains the staged campaign owner"), Proxy->GetOwner() == PC && Proxy->GetController() == AI);
	AI->UnPossess();
	TestTrue(TEXT("AI unpossession retains campaign ownership without retaining the AI"), Proxy->GetOwner() == PC && Proxy->GetController() == nullptr);
	AI->Possess(Proxy);
	TestTrue(TEXT("Actual AI reacquisition preserves the same campaign owner"), Proxy->GetOwner() == PC && Proxy->GetController() == AI);
	// This minimal world has no global BeginPlay. Start the real controller's activity
	// component lifecycle so native Regroup registration runs with its normal owner.
	if (!AI->HasActorBegunPlay()) { AI->DispatchBeginPlay(); }
	if (!TestTrue(TEXT("Native staged initialization applies copied resources and unlocked kit"), Companions->PollStaged(Reason)))
	{ AddError(Reason); return false; }
	auto* ProxyASC = Proxy->GetNarrativeAbilitySystemComponent();
	TestTrue(TEXT("The proxy owns a separate ASC"), ProxyASC && ProxyASC != ASC && ProxyASC->GetAvatarActor() == Proxy);
	if (!ProxyASC) { return false; }
	TestEqual(TEXT("Outgoing Health transfers to the companion"), ProxyASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 73.f);
	TestEqual(TEXT("Outgoing Echo transfers to the companion"), ProxyASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetEchoAttribute()), 29.f);
	const auto* CopiedGrant = ProxyASC->FindAbilitySpecFromClass(USovWeakPointFireTestAbility::StaticClass());
	TestTrue(TEXT("Actual unlocked upgrade level is copied"), CopiedGrant && CopiedGrant->Level == 3);
	TestNull(TEXT("The listed but locked ability remains unavailable"), ProxyASC->FindAbilitySpecFromClass(USovWeakPointMeleeTestAbility::StaticClass()));
	TestNotNull(TEXT("Staging leaves the outgoing player's ability grant intact"), ASC->FindAbilitySpecFromHandle(SourceGrant));
	TestEqual(TEXT("Staging does not spend outgoing Echo"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetEchoAttribute()), 29.f);
	FSovProtagonistSnapshot StillTarrik;
	TestTrue(TEXT("The previous incoming kit remains retained"), PS->FindProtagonistSnapshot(Mission->Protagonist, StillTarrik));
	TestEqual(TEXT("First meeting preserves Tarrik's own saved Health"), StillTarrik.Resources.Health, 61.f);
	TestEqual(TEXT("First meeting preserves Tarrik's own saved Echo"), StillTarrik.Resources.Echo, 11.f);
	TestFalse(TEXT("Companion publication waits for shared-entry journal commit"), Companions->CommitStaged(Selene, Reason));
	TestNull(TEXT("Early commit failure leaves no active companion"), Companions->GetActiveCompanion());
	// The controller's tested transition owns outgoing ASC teardown; reproduce that
	// external boundary before exercising the native incoming snapshot and companion commit.
	ASC->CancelAllAbilities(); ASC->ClearAllAbilities();
	PC->UnPossess(); Selene->Destroy(); ASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Selene);
	auto* ReturnedTarrik = F.World->SpawnActor<ASovHandoffRuntimeTestPawn>(); if (!ReturnedTarrik) { return false; }
	if (!TestTrue(TEXT("Incoming pawn stages before saved-kit application"), PreparePlayer(ReturnedTarrik, TarrikDefinition))) { return false; }
	if (!TestTrue(TEXT("Production snapshot restore recovers Tarrik's first-approach kit"), PS->RestoreProtagonistSnapshot(ReturnedTarrik, TarrikKit, false, Reason)
		&& ReturnedTarrik->CompleteCampaignDataInitialization(false))) { AddError(Reason); return false; }
	FSovCompanionApproachTestAccess::SeedPriorHandoff(Campaign, TEXT("SharedEntry"));
	if (!TestTrue(TEXT("Actual companion commit succeeds after incoming readiness and shared-entry commit"), Companions->CommitStaged(ReturnedTarrik, Reason)))
	{ AddError(Reason); return false; }
	TestTrue(TEXT("Committed active membership is the same staged native actor"), Companions->GetActiveCompanion() == Proxy && Proxy->GetOwner() == PC);
	TestTrue(TEXT("Companion leader is the actual restored Tarrik"), Proxy->GetCompanionComponent()->GetCurrentLeader() == ReturnedTarrik);
	TestFalse(TEXT("Successful commit consumes staging ownership"), Companions->HasStagedProxy());
	TestFalse(TEXT("Committed proxy becomes visible"), Proxy->IsHidden());
	TestEqual(TEXT("Tarrik retains his own Health after commit"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 61.f);
	TestEqual(TEXT("Tarrik retains his own Echo after commit"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetEchoAttribute()), 11.f);
	TestEqual(TEXT("Companion still retains Selene's Echo after the player ASC changed"), ProxyASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetEchoAttribute()), 29.f);
	Companions->PrepareForSave_Implementation(); TArray<uint8> SharedBytes;
	TestTrue(TEXT("Successful first shared entry produces a real serializable companion record"), SaveBytes(Companions, SharedBytes));
	// Exercise the no-new-stage commit used when an existing partner is rebound
	// during restoration. Only a completed M13 departure replaces Regroup.
	auto* Departure = NewObject<USovAurelionContraryWitnessMissionDefinition>(PC);
	PC->KeepAlive.Add(Departure); Departure->bAllowJointResonance = false;
	Departure->AllowedCompanionIds.Add(TEXT("Selene"));
	FSovCompanionApproachTestAccess::SelectDepartureForCommit(Campaign, Departure, false);
	if (!TestTrue(TEXT("Incomplete departure retains ordinary companion regroup"),
		Companions->CommitStaged(ReturnedTarrik, Reason))) { AddError(Reason); return false; }
	TestFalse(TEXT("Incomplete departure has no self-hold"), Proxy->GetCompanionComponent()->HasAcceptedHoldPosition(Proxy));
	const FVector SavedPose = Proxy->GetActorLocation();
	FSovCompanionApproachTestAccess::SelectDepartureForCommit(Campaign, Departure, true);
	if (!TestTrue(TEXT("Completed departure rebinds the active partner"),
		Companions->CommitStaged(ReturnedTarrik, Reason))) { AddError(Reason); return false; }
	TestTrue(TEXT("Completed departure accepts a self-hold before returning from commit"),
		Proxy->GetCompanionComponent()->HasAcceptedHoldPosition(Proxy));
	TestTrue(TEXT("Departure intent preserves the saved companion transform"), Proxy->GetActorLocation().Equals(SavedPose, .01f));
	if (!TestTrue(TEXT("Repeated completed commit restores the same standing intent"),
		Companions->CommitStaged(ReturnedTarrik, Reason))) { AddError(Reason); return false; }
	TestTrue(TEXT("Repeated completed commit still holds the companion"),
		Proxy->GetCompanionComponent()->HasAcceptedHoldPosition(Proxy));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCompanionApproachDefinitionTest, "ProjectVelkorran.Campaign.Companion.ApproachActivationDefinitionValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCompanionApproachDefinitionTest::RunTest(const FString& Parameters)
{
	FApproachWorld F; if (!F.World) { return false; }
	auto* PC = F.World->SpawnActor<ASovHandoffRuntimeTestController>(); if (!PC) { return false; }
	auto* Mission = MakeApproachMission(PC); FString Reason;
	TestTrue(TEXT("Companion partnership is independent of generic Resonance permission"), Mission->ValidateDefinition(Reason));
	Mission->Beats[0].bIsolatedPerspectiveCut = false;
	TestFalse(TEXT("Pre-meeting handoff must explicitly preserve separate approaches"), Mission->ValidateDefinition(Reason)); Mission->Beats[0].bIsolatedPerspectiveCut = true;
	Mission->Beats[1].bIsolatedPerspectiveCut = true;
	TestFalse(TEXT("Shared entry cannot also be an isolated cut"), Mission->ValidateDefinition(Reason)); Mission->Beats[1].bIsolatedPerspectiveCut = false;
	Mission->Beats[2].bIsolatedPerspectiveCut = true;
	TestFalse(TEXT("Late handoff cannot silently dissolve the established partnership"), Mission->ValidateDefinition(Reason)); Mission->Beats[2].bIsolatedPerspectiveCut = false;
	Mission->CompanionActivationBeat = TEXT("Missing");
	TestFalse(TEXT("Activation must name an existing handoff"), Mission->ValidateDefinition(Reason)); Mission->CompanionActivationBeat = TEXT("SharedEntry");
	Mission->bAllowJointResonance = true; Mission->AllowedResonanceTypes = {ESovResonanceType::SupportSever};
	TestFalse(TEXT("Generic Resonance cannot be offered before the meeting"), Mission->ValidateDefinition(Reason));
	Mission->ResonancePrerequisiteBeats = {Mission->CompanionActivationBeat};
	TestTrue(TEXT("Explicit shared-entry prerequisite admits generic Resonance when authored"), Mission->ValidateDefinition(Reason));
	return true;
}
#endif
