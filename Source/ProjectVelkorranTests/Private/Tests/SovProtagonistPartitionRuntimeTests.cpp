// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovProtagonistPartitionTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovTechniqueRuntimeTestFixtures.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Character/CharacterAppearance.h"
#include "Character/PlayerDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerController.h"
#include "Framework/SovPlayerState.h"
#include "GAS/AbilityConfiguration.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "Items/InventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "NarrativeSave.h"
#include "Progression/SovTechniqueComponent.h"
#include "Progression/SovTechniqueRewardSource.h"
#include "Sovereign/SovGameplayTags.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/Script.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS
/**
 * Drives the controller's own handoff and load sequencing. The fixture pawn supplies only
 * content readiness; every snapshot, Technique, faction, Echo and record decision is production.
 */
struct FSovProtagonistPartitionTestAccess
{
	static ASovPlayerCharacterBase* Spawn(ASovPlayerController* PC, USovCampaignDefinition* Mission)
	{ return PC->SpawnCampaignPawn(Mission, FTransform::Identity, PC->PendingProtagonist); }
	static bool StartHandoff(ASovPlayerController* PC, USovCampaignDefinition* Mission, FString& Error)
	{ return PC->StartPawnHandoff(Mission, Mission->Protagonist, FTransform::Identity, NAME_None, FGuid(), Error); }
	static ASovPlayerCharacterBase* Pending(const ASovPlayerController* PC) { return PC->PendingPawn; }
	static void Poll(ASovPlayerController* PC) { PC->PollCampaignInitialization(PC->TransitionEpoch); }
};

namespace
{
	using FAccess = FSovProtagonistPartitionTestAccess;

	/** Assets outlive each world, so a save written in one world can load in another. */
	struct FPartitionCampaign
	{
		TArray<TStrongObjectPtr<UObject>> Keep;
		UPlayerDefinition* TarrikDefinition = nullptr;
		UPlayerDefinition* SeleneDefinition = nullptr;
		TArray<USovCampaignDefinition*> Missions;
		FGameplayTag TarrikLearned, SeleneLearned;

		FPartitionCampaign()
		{
			const auto& Tags = FSovGameplayTags::Get();
			// Arbitrary registered tags standing in for authored mission knowledge.
			TarrikLearned = Tags.Echo_Source_WeakPointBreak;
			SeleneLearned = Tags.Ability_Echo_Tarrik_CinderJudgement;
			TarrikDefinition = MakeDefinition(FNarrativeGameplayTags::Get().Narrative_Factions_Heroes);
			SeleneDefinition = MakeDefinition(FNarrativeGameplayTags::Get().Narrative_Factions_Bandits);
			// H0 Tarrik -> H1 Selene -> H2 Tarrik -> ... authored alternation, never free switching.
			for (int32 Index = 0; Index < 7; ++Index)
			{
				const bool bTarrik = Index % 2 == 0;
				auto* Mission = NewObject<USovCampaignDefinition>(GetTransientPackage());
				Keep.Emplace(Mission);
				Mission->MissionId = FName(*FString::Printf(TEXT("M%02d_PartitionH%d"), Index + 1, Index));
				Mission->Protagonist = bTarrik ? Tags.Character_Player_Tarrik : Tags.Character_Player_Selene;
				Mission->PawnClass = bTarrik ? ASovPartitionTestTarrik::StaticClass() : ASovPartitionTestSelene::StaticClass();
				Mission->PlayerDefinition = bTarrik ? TarrikDefinition : SeleneDefinition;
				Mission->EntryEchoReserve = bTarrik ? 10.f : 20.f;
				FSovCampaignBeatDefinition Done; Done.BeatId = TEXT("Done");
				Done.GrantedKnowledge.AddTag(bTarrik ? TarrikLearned : SeleneLearned);
				if (Index == 0)
				{
					// A canon fact written by Tarrik: campaign-wide by design, not protagonist-owned.
					FSovCampaignStateWrite Fact; Fact.Key = Tags.State_CommandLink_Active;
					Fact.Value = Tags.Character_Player_Tarrik; Fact.bCanonProtected = true;
					Done.StateWrites.Add(Fact);
				}
				Mission->Beats.Add(Done);
				if (!Missions.IsEmpty()) { Missions.Last()->AllowedSuccessorMissions.Add(Mission->MissionId); }
				Missions.Add(Mission);
			}
		}
		UPlayerDefinition* MakeDefinition(FGameplayTag Faction)
		{
			auto* Definition = NewObject<UPlayerDefinition>(GetTransientPackage());
			auto* Abilities = NewObject<UAbilityConfiguration>(GetTransientPackage());
			auto* Appearance = NewObject<UCharacterAppearance>(GetTransientPackage());
			Keep.Emplace(Definition); Keep.Emplace(Abilities); Keep.Emplace(Appearance);
			Abilities->DefaultAttributes = UGameplayEffect::StaticClass();
			Definition->AbilityConfiguration = Abilities;
			Definition->DefaultAppearance = Appearance;
			Definition->DefaultFactions.AddTag(Faction);
			return Definition;
		}
	};

	struct FPartitionWorld
	{
		// Actor interface events (PrepareForSave, Load, SetActorGUID) dispatch through
		// AActor::ProcessEvent, which a world that never began play otherwise skips silently.
		FEditorScriptExecutionGuard ScriptGuard;
		UWorld* World = nullptr;
		ASovHandoffRuntimeTestController* PC = nullptr;
		ASovPlayerState* PS = nullptr;
		UNarrativeSaveSubsystem* Save = nullptr;
		FString Error;

		FPartitionWorld()
		{
			const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			PC = World->SpawnActor<ASovHandoffRuntimeTestController>();
			PS = World->SpawnActor<ASovPlayerState>();
			Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
			if (PC && PS) { PC->SetTestPlayerState(PS); }
		}
		~FPartitionWorld()
		{
			if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		}
		bool IsValid() const { return World && PC && PS && Save; }
		ASovPlayerCharacterBase* Current() const { return Cast<ASovPlayerCharacterBase>(PC->GetPawn()); }
		UNarrativeAbilitySystemComponent* ASC() const { return Cast<UNarrativeAbilitySystemComponent>(PS->GetAbilitySystemComponent()); }
		USovTechniqueComponent* Techniques() const { return Cast<USovTechniqueComponent>(PS->GetSkillTreeComponent()); }
		USovCampaignStateComponent* Campaign() const { return PC->GetCampaignState(); }
		bool Settled(const ASovPlayerCharacterBase* Pawn) const
		{ return Pawn && Current() == Pawn && Pawn->IsCharacterReady() && PC->GetCampaignTransitionState() == ESovCampaignTransitionState::Idle; }

		/** The game-mode entry: stage (optionally from a save), spawn, possess, then the controller poll. */
		ASovPlayerCharacterBase* Enter(USovCampaignDefinition* Mission, const FNarrativeSavePlayer* Records)
		{
			if (!PC->StageCampaignLoad(Mission, Records, false, Error)) { return nullptr; }
			auto* Pawn = Cast<ASovPartitionTestPawn>(FAccess::Spawn(PC, Mission));
			if (!Pawn) { Error = TEXT("Campaign pawn did not spawn."); return nullptr; }
			PC->Possess(Pawn);
			if (!Pawn->StageContentReadiness(PS)) { Error = TEXT("Content readiness could not be staged."); return nullptr; }
			PC->InitializeCampaignPawn(Pawn);
			FAccess::Poll(PC);
			return Pawn;
		}
		/** The authored same-world handoff: production capture, teardown, spawn and restore. */
		ASovPlayerCharacterBase* Handoff(USovCampaignDefinition* Mission)
		{
			if (!FAccess::StartHandoff(PC, Mission, Error)) { return nullptr; }
			auto* Pawn = Cast<ASovPartitionTestPawn>(FAccess::Pending(PC));
			if (!Pawn || !Pawn->StageContentReadiness(PS)) { Error = TEXT("Destination readiness could not be staged."); return nullptr; }
			FAccess::Poll(PC);
			return Pawn;
		}
		bool CompleteActiveMission() const
		{ return Campaign()->CompleteBeat(TEXT("Done")) == ESovCampaignResult::Applied; }

		bool ClaimTechniqueReward(FName RewardId, int32 Points, FGameplayTag Protagonist) const
		{
			auto* Encounter = World->SpawnActor<ASovTechniqueTestEncounter>();
			if (!Encounter) { return false; }
			Encounter->SetTestState(ESovEncounterState::Succeeded);
			auto* Reward = NewObject<USovTechniqueRewardSource>(Encounter);
			Encounter->AddInstanceComponent(Reward); Reward->RegisterComponent();
			Reward->RewardId = RewardId; Reward->Points = Points; Reward->Protagonist = Protagonist;
			Reward->Proof = ESovTechniqueRewardProof::EncounterComplete; Reward->Encounter = Encounter;
			return Techniques()->ClaimReward(Reward);
		}
		/** Narrative's player record through its real save object and byte serialization. */
		bool WriteSave(TArray<uint8>& OutBytes) const
		{
			TStrongObjectPtr<UNarrativeSave> Snapshot(NewObject<UNarrativeSave>());
			return Save->CreateActorRecord(PC, Snapshot->PlayerData.ControllerData)
				&& Save->CreateActorRecord(PS, Snapshot->PlayerData.PlayerStateData)
				&& Current() && Save->CreateActorRecord(Current(), Snapshot->PlayerData.PawnData)
				&& UGameplayStatics::SaveGameToMemory(Snapshot.Get(), OutBytes);
		}
	};

	bool ReadSave(const TArray<uint8>& Bytes, FNarrativeSavePlayer& OutRecords)
	{
		UNarrativeSave* Decoded = Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
		if (!Decoded) { return false; }
		OutRecords = Decoded->PlayerData;
		return OutRecords.IsValid();
	}

	int32 CountItems(const ASovPlayerCharacterBase* Pawn, TSubclassOf<UNarrativeItem> Class)
	{
		const UNarrativeInventoryComponent* Inventory = Pawn ? Pawn->GetInventoryComponent() : nullptr;
		int32 Total = 0;
		if (Inventory) { for (const UNarrativeItem* Item : Inventory->FindItemsOfClass(Class)) { if (Item) { Total += Item->GetQuantity(); } } }
		return Total;
	}
	int32 Currency(const ASovPlayerCharacterBase* Pawn)
	{
		const UNarrativeInventoryComponent* Inventory = Pawn ? Pawn->GetInventoryComponent() : nullptr;
		return Inventory ? Inventory->GetCurrency() : INDEX_NONE;
	}
	float Echo(const FPartitionWorld& W) { return W.ASC()->GetNumericAttribute(UNarrativeAttributeSetBase::GetEchoAttribute()); }

	/** The expected protagonist-owned state for one hero at a point in the campaign. */
	struct FOwnedState
	{
		int32 OwnKits = 0;
		int32 CurrencyAmount = 0;
		int32 EarnedPoints = 0;
		float EchoAmount = 0.f;
	};

	void ExpectTarrik(FAutomationTestBase& Test, const FPartitionWorld& W, const FString& When, const FOwnedState& Expected)
	{
		const ASovPlayerCharacterBase* Pawn = W.Current();
		const auto& Factions = FNarrativeGameplayTags::Get();
		Test.TestTrue(When + TEXT(": Tarrik is the settled controlled protagonist"), W.Settled(Pawn) && Pawn->IsA<ASovPartitionTestTarrik>());
		Test.TestEqual(When + TEXT(": Tarrik's own kit"), CountItems(Pawn, USovPartitionTarrikKit::StaticClass()), Expected.OwnKits);
		Test.TestEqual(When + TEXT(": no Selene kit in Tarrik's inventory"), CountItems(Pawn, USovPartitionSeleneKit::StaticClass()), 0);
		Test.TestEqual(When + TEXT(": Tarrik's currency"), Currency(Pawn), Expected.CurrencyAmount);
		Test.TestEqual(When + TEXT(": Tarrik's earned Technique points"), W.Techniques()->GetEarnedTechniquePoints(), Expected.EarnedPoints);
		Test.TestEqual(When + TEXT(": Tarrik's available Technique points"), W.Techniques()->GetAvailableTechniquePoints(), Expected.EarnedPoints);
		Test.TestTrue(When + TEXT(": Tarrik's Technique ledger validates for Tarrik"), W.Techniques()->IsTechniqueStateValid());
		Test.TestTrue(When + TEXT(": Tarrik's factions exactly"), W.PS->GetFactions().HasTagExact(Factions.Narrative_Factions_Heroes)
			&& !W.PS->GetFactions().HasTagExact(Factions.Narrative_Factions_Bandits));
		Test.TestEqual(When + TEXT(": Tarrik's Echo"), Echo(W), Expected.EchoAmount);
	}
	void ExpectSelene(FAutomationTestBase& Test, const FPartitionWorld& W, const FString& When, const FOwnedState& Expected)
	{
		const ASovPlayerCharacterBase* Pawn = W.Current();
		const auto& Factions = FNarrativeGameplayTags::Get();
		Test.TestTrue(When + TEXT(": Selene is the settled controlled protagonist"), W.Settled(Pawn) && Pawn->IsA<ASovPartitionTestSelene>());
		Test.TestEqual(When + TEXT(": Selene's own kit"), CountItems(Pawn, USovPartitionSeleneKit::StaticClass()), Expected.OwnKits);
		Test.TestEqual(When + TEXT(": no Tarrik kit in Selene's inventory"), CountItems(Pawn, USovPartitionTarrikKit::StaticClass()), 0);
		Test.TestEqual(When + TEXT(": Selene's currency"), Currency(Pawn), Expected.CurrencyAmount);
		Test.TestEqual(When + TEXT(": Selene's earned Technique points"), W.Techniques()->GetEarnedTechniquePoints(), Expected.EarnedPoints);
		Test.TestEqual(When + TEXT(": Selene's available Technique points"), W.Techniques()->GetAvailableTechniquePoints(), Expected.EarnedPoints);
		Test.TestTrue(When + TEXT(": Selene's Technique ledger validates for Selene"), W.Techniques()->IsTechniqueStateValid());
		Test.TestTrue(When + TEXT(": Selene's factions exactly"), W.PS->GetFactions().HasTagExact(Factions.Narrative_Factions_Bandits)
			&& !W.PS->GetFactions().HasTagExact(Factions.Narrative_Factions_Heroes));
		Test.TestEqual(When + TEXT(": Selene's Echo"), Echo(W), Expected.EchoAmount);
	}
	/** Shared by design: canon facts and mission completion. Partitioned by design: knowledge. */
	void ExpectCampaignBoundary(FAutomationTestBase& Test, const FPartitionWorld& W, const FPartitionCampaign& C, const FString& When, bool bSeleneHasLearned)
	{
		const auto& Tags = FSovGameplayTags::Get();
		const USovCampaignStateComponent* Campaign = W.Campaign();
		Test.TestEqual(When + TEXT(": Tarrik's canon fact is visible campaign-wide"), Campaign->GetStateValue(Tags.State_CommandLink_Active), Tags.Character_Player_Tarrik);
		Test.TestTrue(When + TEXT(": the first mission's completion is shared progression"), Campaign->IsMissionComplete(C.Missions[0]->MissionId));
		Test.TestTrue(When + TEXT(": Tarrik keeps what Tarrik learned"), Campaign->HasKnowledge(Tags.Character_Player_Tarrik, FGameplayTagContainer(C.TarrikLearned)));
		Test.TestFalse(When + TEXT(": Selene never inherits Tarrik's knowledge"), Campaign->HasKnowledge(Tags.Character_Player_Selene, FGameplayTagContainer(C.TarrikLearned)));
		Test.TestFalse(When + TEXT(": Tarrik never inherits Selene's knowledge"), Campaign->HasKnowledge(Tags.Character_Player_Tarrik, FGameplayTagContainer(C.SeleneLearned)));
		Test.TestEqual(When + TEXT(": Selene's own knowledge"), Campaign->HasKnowledge(Tags.Character_Player_Selene, FGameplayTagContainer(C.SeleneLearned)), bSeleneHasLearned);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovProtagonistPartitionHandoffTest, "ProjectVelkorran.Campaign.ProtagonistPartition.RepeatedHandoffsKeepOwnedStateSeparate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovProtagonistPartitionHandoffTest::RunTest(const FString& Parameters)
{
	FPartitionCampaign C;
	FPartitionWorld W; if (!W.IsValid()) { AddError(TEXT("World creation failed")); return false; }
	const auto& Tags = FSovGameplayTags::Get();

	if (!TestNotNull(TEXT("Tarrik enters through the managed campaign path"), W.Enter(C.Missions[0], nullptr))) { AddError(W.Error); return false; }
	ExpectTarrik(*this, W, TEXT("Tarrik first visit"), { 0, 0, 0, 10.f });
	auto* Tarrik = W.Current();
	Tarrik->GetInventoryComponent()->TryAddItemFromClass(USovPartitionTarrikKit::StaticClass(), 3);
	Tarrik->GetInventoryComponent()->SetCurrency(40);
	TestTrue(TEXT("Tarrik earns his own Technique reward"), W.ClaimTechniqueReward(TEXT("Partition.Tarrik"), 5, Tags.Character_Player_Tarrik));
	W.ASC()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 33.f);
	const FGameplayAbilitySpecHandle TarrikGrant = Tarrik->AddAbility(UNarrativeGameplayAbility::StaticClass(), C.TarrikDefinition);
	const FActiveGameplayEffectHandle TarrikEffect = W.ASC()->ApplyGameplayEffectToSelf(
		GetDefault<USovPartitionTransientCombatEffect>(), 1.f, W.ASC()->MakeEffectContext());
	TestTrue(TEXT("Tarrik's transient combat state is live before handoff"), TarrikGrant.IsValid() && TarrikEffect.IsValid());
	TestTrue(TEXT("Tarrik completes his authored mission"), W.CompleteActiveMission());
	ExpectTarrik(*this, W, TEXT("Tarrik before first handoff"), { 3, 40, 5, 33.f });

	if (!TestNotNull(TEXT("Authored handoff to Selene"), W.Handoff(C.Missions[1]))) { AddError(W.Error); return false; }
	ExpectSelene(*this, W, TEXT("Selene first visit"), { 0, 0, 0, 20.f });
	TestNull(TEXT("Tarrik's granted ability spec does not survive into Selene's kit"), W.ASC()->FindAbilitySpecFromHandle(TarrikGrant));
	FGameplayEffectQuery TransientQuery; TransientQuery.EffectDefinition = USovPartitionTransientCombatEffect::StaticClass();
	TestEqual(TEXT("Tarrik's active combat effect does not survive into Selene"), W.ASC()->GetActiveEffects(TransientQuery).Num(), 0);
	TestTrue(TEXT("Selene's identity alone is on the shared ASC"), W.ASC()->HasMatchingGameplayTag(Tags.Character_Player_Selene)
		&& !W.ASC()->HasMatchingGameplayTag(Tags.Character_Player_Tarrik));
	AddInfo(FString::Printf(TEXT("Selene first-visit Health on the shared ASC with no authored DefaultAttributes effect: %.1f"),
		W.ASC()->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute())));
	ExpectCampaignBoundary(*this, W, C, TEXT("After first handoff"), false);

	auto* Selene = W.Current();
	Selene->GetInventoryComponent()->TryAddItemFromClass(USovPartitionSeleneKit::StaticClass(), 2);
	Selene->GetInventoryComponent()->SetCurrency(7);
	TestTrue(TEXT("Selene earns her own Technique reward"), W.ClaimTechniqueReward(TEXT("Partition.Selene"), 3, Tags.Character_Player_Selene));
	W.ASC()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 12.f);
	TestTrue(TEXT("Selene completes her authored mission"), W.CompleteActiveMission());
	ExpectCampaignBoundary(*this, W, C, TEXT("After Selene learns"), true);

	// Three further round trips: owned records must neither merge nor overwrite each other.
	for (int32 Index = 2; Index < C.Missions.Num(); ++Index)
	{
		const FString When = FString::Printf(TEXT("Handoff %d"), Index);
		if (!TestNotNull(*(When + TEXT(" completes")), W.Handoff(C.Missions[Index]))) { AddError(W.Error); return false; }
		if (Index % 2 == 0) { ExpectTarrik(*this, W, When, { 3, 40, 5, 33.f }); }
		else { ExpectSelene(*this, W, When, { 2, 7, 3, 12.f }); }
		ExpectCampaignBoundary(*this, W, C, When, true);
		if (Index + 1 < C.Missions.Num() && !TestTrue(*(When + TEXT(" mission completes")), W.CompleteActiveMission())) { return false; }
	}
	TestNull(TEXT("A returning Tarrik does not regain a transient ability grant"), W.ASC()->FindAbilitySpecFromHandle(TarrikGrant));
	TestEqual(TEXT("A returning Tarrik does not regain a transient combat effect"), W.ASC()->GetActiveEffects(TransientQuery).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovProtagonistPartitionSaveLoadTest, "ProjectVelkorran.Campaign.ProtagonistPartition.SaveSwitchMutateReloadEitherProtagonist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovProtagonistPartitionSaveLoadTest::RunTest(const FString& Parameters)
{
	FPartitionCampaign C;
	const auto& Tags = FSovGameplayTags::Get();
	TArray<uint8> SeleneActiveSave, TarrikActiveSave;
	{
		FPartitionWorld W; if (!W.IsValid()) { AddError(TEXT("World creation failed")); return false; }
		if (!W.Enter(C.Missions[0], nullptr)) { AddError(W.Error); return false; }
		W.Current()->GetInventoryComponent()->TryAddItemFromClass(USovPartitionTarrikKit::StaticClass(), 3);
		W.Current()->GetInventoryComponent()->SetCurrency(40);
		W.ClaimTechniqueReward(TEXT("Partition.Tarrik"), 5, Tags.Character_Player_Tarrik);
		W.ASC()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 33.f);
		W.CompleteActiveMission();
		if (!W.Handoff(C.Missions[1])) { AddError(W.Error); return false; }
		W.Current()->GetInventoryComponent()->TryAddItemFromClass(USovPartitionSeleneKit::StaticClass(), 2);
		W.Current()->GetInventoryComponent()->SetCurrency(7);
		W.ClaimTechniqueReward(TEXT("Partition.Selene"), 3, Tags.Character_Player_Selene);
		W.ASC()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 12.f);
		W.CompleteActiveMission();
		if (!TestTrue(TEXT("Save while Selene is active"), W.WriteSave(SeleneActiveSave))) { return false; }

		// Mutate after the save, then switch and mutate the other protagonist too.
		W.Current()->GetInventoryComponent()->TryAddItemFromClass(USovPartitionSeleneKit::StaticClass(), 5);
		W.Current()->GetInventoryComponent()->SetCurrency(107);
		if (!W.Handoff(C.Missions[2])) { AddError(W.Error); return false; }
		ExpectTarrik(*this, W, TEXT("Tarrik after Selene's post-save mutation"), { 3, 40, 5, 33.f });
		W.Current()->GetInventoryComponent()->TryAddItemFromClass(USovPartitionTarrikKit::StaticClass(), 1);
		W.Current()->GetInventoryComponent()->SetCurrency(540);
		if (!TestTrue(TEXT("Save while Tarrik is active"), W.WriteSave(TarrikActiveSave))) { return false; }
		// A later live mutation that no reload may observe.
		W.Current()->GetInventoryComponent()->SetCurrency(9999);
	}

	FNarrativeSavePlayer SeleneRecords, TarrikRecords;
	if (!TestTrue(TEXT("Selene-active save decodes"), ReadSave(SeleneActiveSave, SeleneRecords))
		|| !TestTrue(TEXT("Tarrik-active save decodes"), ReadSave(TarrikActiveSave, TarrikRecords))) { return false; }
	{
		FPartitionWorld W; if (!W.IsValid()) { AddError(TEXT("World creation failed")); return false; }
		if (!TestNotNull(TEXT("Reload the Selene-active save"), W.Enter(C.Missions[1], &SeleneRecords))) { AddError(W.Error); return false; }
		ExpectSelene(*this, W, TEXT("Reloaded Selene"), { 2, 7, 3, 12.f });
		ExpectCampaignBoundary(*this, W, C, TEXT("Reloaded Selene"), true);
		if (!TestNotNull(TEXT("Handoff from the reloaded Selene"), W.Handoff(C.Missions[2]))) { AddError(W.Error); return false; }
		ExpectTarrik(*this, W, TEXT("Tarrik from the Selene-active save"), { 3, 40, 5, 33.f });
	}
	{
		FPartitionWorld W; if (!W.IsValid()) { AddError(TEXT("World creation failed")); return false; }
		if (!TestNotNull(TEXT("Reload the Tarrik-active save"), W.Enter(C.Missions[2], &TarrikRecords))) { AddError(W.Error); return false; }
		ExpectTarrik(*this, W, TEXT("Reloaded Tarrik"), { 4, 540, 5, 33.f });
		ExpectCampaignBoundary(*this, W, C, TEXT("Reloaded Tarrik"), true);
		TestTrue(TEXT("Tarrik completes his reloaded mission"), W.CompleteActiveMission());
		if (!TestNotNull(TEXT("Handoff from the reloaded Tarrik"), W.Handoff(C.Missions[3]))) { AddError(W.Error); return false; }
		ExpectSelene(*this, W, TEXT("Selene from the Tarrik-active save"), { 7, 107, 3, 12.f });
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovProtagonistPartitionMissingRecordTest, "ProjectVelkorran.Campaign.ProtagonistPartition.MissingProtagonistRecordFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovProtagonistPartitionMissingRecordTest::RunTest(const FString& Parameters)
{
	FPartitionCampaign C;
	const auto& Tags = FSovGameplayTags::Get();
	TArray<uint8> TarrikOnlySave, SeleneActiveSave;
	{
		FPartitionWorld W; if (!W.IsValid()) { AddError(TEXT("World creation failed")); return false; }
		if (!W.Enter(C.Missions[0], nullptr)) { AddError(W.Error); return false; }
		W.Current()->GetInventoryComponent()->TryAddItemFromClass(USovPartitionTarrikKit::StaticClass(), 3);
		W.Current()->GetInventoryComponent()->SetCurrency(40);
		W.ClaimTechniqueReward(TEXT("Partition.Tarrik"), 5, Tags.Character_Player_Tarrik);
		W.CompleteActiveMission();
		if (!TestTrue(TEXT("Save before Selene has ever been played"), W.WriteSave(TarrikOnlySave))) { return false; }
		FSovProtagonistSnapshot TarrikRecord;
		if (!TestTrue(TEXT("Tarrik's record exists"), W.PS->FindProtagonistSnapshot(Tags.Character_Player_Tarrik, TarrikRecord))) { return false; }
		TestFalse(TEXT("No Selene record exists yet"), W.PS->FindProtagonistSnapshot(Tags.Character_Player_Selene, TarrikRecord));
		if (!W.Handoff(C.Missions[1])) { AddError(W.Error); return false; }
		FString Error;
		TestFalse(TEXT("Tarrik's record is refused by Selene's pawn"), W.PS->RestoreProtagonistSnapshot(W.Current(), TarrikRecord, false, Error));
		ExpectSelene(*this, W, TEXT("Selene after refusing Tarrik's record"), { 0, 0, 0, 20.f });
		if (!TestTrue(TEXT("Save while Selene is active"), W.WriteSave(SeleneActiveSave))) { return false; }
	}

	FNarrativeSavePlayer TarrikOnly, SeleneActive;
	if (!ReadSave(TarrikOnlySave, TarrikOnly) || !ReadSave(SeleneActiveSave, SeleneActive)) { AddError(TEXT("Saves did not decode")); return false; }
	// A Selene-active campaign whose PlayerState predates any Selene record (legacy or damaged save).
	FNarrativeSavePlayer Damaged = SeleneActive;
	Damaged.PlayerStateData = TarrikOnly.PlayerStateData;
	{
		FPartitionWorld W; if (!W.IsValid()) { AddError(TEXT("World creation failed")); return false; }
		// The refusal is the behaviour under test: it must be reported exactly once.
		AddExpectedError(TEXT("lacks a matching protagonist snapshot"), EAutomationExpectedErrorFlags::Contains, 1);
		const ASovPlayerCharacterBase* Selene = W.Enter(C.Missions[1], &Damaged);
		if (!TestNotNull(TEXT("The Selene pawn is created for the attempted load"), Selene)) { AddError(W.Error); return false; }
		TestEqual(TEXT("Load fails explicitly"), W.PC->GetCampaignTransitionState(), ESovCampaignTransitionState::Failed);
		TestFalse(TEXT("Selene never becomes ready"), Selene->IsCharacterReady());
		TestEqual(TEXT("Selene does not borrow Tarrik's kit"), CountItems(Selene, USovPartitionTarrikKit::StaticClass()), 0);
		TestEqual(TEXT("Selene does not borrow Tarrik's currency"), Currency(Selene), 0);
		TestEqual(TEXT("Selene does not borrow Tarrik's Technique points"), W.Techniques()->GetAvailableTechniquePoints(), 0);
		FSovProtagonistSnapshot Borrowed;
		TestFalse(TEXT("Tarrik's record was not rekeyed as Selene's"), W.PS->FindProtagonistSnapshot(Tags.Character_Player_Selene, Borrowed));
	}
	return true;
}
#endif
