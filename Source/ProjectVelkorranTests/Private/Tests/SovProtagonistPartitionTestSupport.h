// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#if WITH_AUTOMATION_TESTS
#include "Tests/SovProtagonistPartitionTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovLifecycleTestFixtures.h"
#include "Tests/SovTechniqueRuntimeTestFixtures.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Character/CharacterAppearance.h"
#include "Character/PlayerDefinition.h"
#include "CommonGameViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/SovPlayerController.h"
#include "Framework/SovPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/AbilityConfiguration.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "HAL/PlatformProperties.h"
#include "ICommonInputModule.h"
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

/** Shared by the protagonist-partition (C1) and checkpoint-contract (C3) suites. */
namespace SovPartitionTest
{
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
				Mission->DisplayName = FText::FromName(Mission->MissionId);
				Mission->Protagonist = bTarrik ? Tags.Character_Player_Tarrik : Tags.Character_Player_Selene;
				Mission->PawnClass = bTarrik ? ASovPartitionTestTarrik::StaticClass() : ASovPartitionTestSelene::StaticClass();
				Mission->PlayerDefinition = bTarrik ? TarrikDefinition : SeleneDefinition;
				// A well-formed package name for save metadata; asset-existence preflight is not exercised here.
				Mission->Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Tests/ProtagonistPartition/L_Partition.L_Partition")));
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
		TStrongObjectPtr<UGameInstance> Instance;
		TStrongObjectPtr<ULocalPlayer> LocalPlayer;
		TStrongObjectPtr<UCommonGameViewportClient> Viewport;
		UWorld* World = nullptr;
		ASovHandoffRuntimeTestController* PC = nullptr;
		ASovPlayerState* PS = nullptr;
		UNarrativeSaveSubsystem* Save = nullptr;
		bool bLocalOwnerReady = false;
		FString Error;

		/** A save owner additionally gets a game instance whose first local player controller is PC. */
		explicit FPartitionWorld(bool bLocalSaveOwner = false)
		{
			if (bLocalSaveOwner) { CreateLocalSaveOwner(); } else { CreateTransient(); }
		}
		~FPartitionWorld()
		{
			if (!World) { return; }
			if (Instance.IsValid())
			{
				if (LocalPlayer.IsValid()) { LocalPlayer->PlayerController = nullptr; Instance->RemoveLocalPlayer(LocalPlayer.Get()); }
				World->DestroyWorld(false);
				Instance->OnWorldChanged(World, nullptr);
			}
			else { World->DestroyWorld(false); }
			if (GEngine) { GEngine->DestroyWorldContext(World); }
		}
		bool IsValid() const { return World && PC && PS && Save && (!Instance.IsValid() || bLocalOwnerReady); }
		ASovPlayerCharacterBase* Current() const { return Cast<ASovPlayerCharacterBase>(PC->GetPawn()); }
		UNarrativeAbilitySystemComponent* ASC() const { return Cast<UNarrativeAbilitySystemComponent>(PS->GetAbilitySystemComponent()); }
		USovTechniqueComponent* Techniques() const { return Cast<USovTechniqueComponent>(PS->GetSkillTreeComponent()); }
		USovCampaignStateComponent* Campaign() const { return PC->GetCampaignState(); }
		bool Settled(const ASovPlayerCharacterBase* Pawn) const
		{ return Pawn && Current() == Pawn && Pawn->IsCharacterReady() && PC->GetCampaignTransitionState() == ESovCampaignTransitionState::Idle; }
		/** The save admission gate requires a grounded protagonist; the fixture pawn does not tick movement. */
		bool Ground() const
		{
			UCharacterMovementComponent* Movement = Current() ? Current()->GetCharacterMovement() : nullptr;
			if (!Movement) { return false; }
			Movement->SetMovementMode(MOVE_Walking);
			return Movement->IsMovingOnGround();
		}

		/** The game-mode entry: stage (optionally from a save), spawn, possess, then the controller poll. */
		ASovPlayerCharacterBase* Enter(USovCampaignDefinition* Mission, const FNarrativeSavePlayer* Records)
		{
			if (!PC->StageCampaignLoad(Mission, Records, false, Error)) { return nullptr; }
			auto* Pawn = Cast<ASovPartitionTestPawn>(FSovProtagonistPartitionTestAccess::Spawn(PC, Mission));
			if (!Pawn) { Error = TEXT("Campaign pawn did not spawn."); return nullptr; }
			PC->Possess(Pawn);
			if (!Pawn->StageContentReadiness(PS)) { Error = TEXT("Content readiness could not be staged."); return nullptr; }
			PC->InitializeCampaignPawn(Pawn);
			FSovProtagonistPartitionTestAccess::Poll(PC);
			return Pawn;
		}
		/** The authored same-world handoff: production capture, teardown, spawn and restore. */
		ASovPlayerCharacterBase* Handoff(USovCampaignDefinition* Mission)
		{
			if (!FSovProtagonistPartitionTestAccess::StartHandoff(PC, Mission, Error)) { return nullptr; }
			auto* Pawn = Cast<ASovPartitionTestPawn>(FSovProtagonistPartitionTestAccess::Pending(PC));
			if (!Pawn || !Pawn->StageContentReadiness(PS)) { Error = TEXT("Destination readiness could not be staged."); return nullptr; }
			FSovProtagonistPartitionTestAccess::Poll(PC);
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
			const bool bClaimed = Techniques()->ClaimReward(Reward);
			// The proof source has served its purpose. A lingering receiptless encounter would
			// correctly block save admission, which is not what these suites are testing.
			Encounter->Destroy();
			return bClaimed;
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

	private:
		static UWorld::InitializationValues Values()
		{
			return UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false)
				.ShouldSimulatePhysics(false).SetTransactional(false);
		}
		void CreateTransient()
		{
			const UWorld::InitializationValues IVS = Values();
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			SpawnPlayer();
		}
		/** Mirrors the pause-menu fixture: a real game instance, local player, viewport and game mode. */
		void CreateLocalSaveOwner()
		{
			Instance.Reset(NewObject<UGameInstance>());
			LocalPlayer.Reset(NewObject<ULocalPlayer>(GEngine));
			Viewport.Reset(NewObject<UCommonGameViewportClient>(GEngine));
			ICommonInputModule::GetSettings().LoadData();
			const UWorld::InitializationValues IVS = Values();
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS, true);
			if (!World) { return; }
			World->SetGameInstance(Instance.Get());
			if (GEngine)
			{
				auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
				Context.SetCurrentWorld(World); Context.OwningGameInstance = Instance.Get(); Context.GameViewport = Viewport.Get();
				Instance->OnWorldChanged(nullptr, World);
			}
			World->InitWorld(IVS);
			World->UpdateWorldComponents(!FPlatformProperties::RequiresCookedData(), false);
			FURL URL; URL.AddOption(*(TEXT("game=") + ASovLifecycleTestGameMode::StaticClass()->GetPathName()));
			if (!World->SetGameMode(URL)) { return; }
			World->InitializeActorsForPlay(URL);
			Viewport->Init(*Instance->GetWorldContext(), Instance.Get(), false);
			SpawnPlayer();
			if (!PC || !PS) { return; }
			PC->Player = LocalPlayer.Get(); LocalPlayer->PlayerController = PC; PC->SetAsLocalPlayerController(); World->AddController(PC);
			Instance->AddLocalPlayer(LocalPlayer.Get(), IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser());
			bLocalOwnerReady = Instance->GetFirstLocalPlayerController() == PC;
		}
		void SpawnPlayer()
		{
			PC = World->SpawnActor<ASovPartitionTestController>();
			PS = World->SpawnActor<ASovPlayerState>();
			Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
			if (PC && PS) { PC->SetTestPlayerState(PS); }
		}
	};

	inline bool ReadSave(const TArray<uint8>& Bytes, FNarrativeSavePlayer& OutRecords)
	{
		UNarrativeSave* Decoded = Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
		if (!Decoded) { return false; }
		OutRecords = Decoded->PlayerData;
		return OutRecords.IsValid();
	}

	inline int32 CountItems(const ASovPlayerCharacterBase* Pawn, TSubclassOf<UNarrativeItem> Class)
	{
		const UNarrativeInventoryComponent* Inventory = Pawn ? Pawn->GetInventoryComponent() : nullptr;
		int32 Total = 0;
		if (Inventory) { for (const UNarrativeItem* Item : Inventory->FindItemsOfClass(Class)) { if (Item) { Total += Item->GetQuantity(); } } }
		return Total;
	}
	inline int32 Currency(const ASovPlayerCharacterBase* Pawn)
	{
		const UNarrativeInventoryComponent* Inventory = Pawn ? Pawn->GetInventoryComponent() : nullptr;
		return Inventory ? Inventory->GetCurrency() : INDEX_NONE;
	}
	inline float Echo(const FPartitionWorld& W) { return W.ASC()->GetNumericAttribute(UNarrativeAttributeSetBase::GetEchoAttribute()); }

	/** The expected protagonist-owned state for one hero at a point in the campaign. */
	struct FOwnedState
	{
		int32 OwnKits = 0;
		int32 CurrencyAmount = 0;
		int32 EarnedPoints = 0;
		float EchoAmount = 0.f;
	};

	inline void ExpectTarrik(FAutomationTestBase& Test, const FPartitionWorld& W, const FString& When, const FOwnedState& Expected)
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
	inline void ExpectSelene(FAutomationTestBase& Test, const FPartitionWorld& W, const FString& When, const FOwnedState& Expected)
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
	inline void ExpectCampaignBoundary(FAutomationTestBase& Test, const FPartitionWorld& W, const FPartitionCampaign& C, const FString& When, bool bSeleneHasLearned)
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

	/**
	 * The reference two-protagonist campaign both suites build on: Tarrik plays and completes
	 * H0 (3 kits, 40 currency, 5 points, 33 Echo), then Selene plays and completes H1
	 * (2 kits, 7 currency, 3 points, 12 Echo). Selene is active on return.
	 */
	inline bool PlayBothProtagonists(FAutomationTestBase& Test, FPartitionWorld& W, const FPartitionCampaign& C)
	{
		const auto& Tags = FSovGameplayTags::Get();
		if (!Test.TestNotNull(TEXT("Tarrik enters through the managed campaign path"), W.Enter(C.Missions[0], nullptr))) { Test.AddError(W.Error); return false; }
		W.Current()->GetInventoryComponent()->TryAddItemFromClass(USovPartitionTarrikKit::StaticClass(), 3);
		W.Current()->GetInventoryComponent()->SetCurrency(40);
		Test.TestTrue(TEXT("Tarrik earns his own Technique reward"), W.ClaimTechniqueReward(TEXT("Partition.Tarrik"), 5, Tags.Character_Player_Tarrik));
		W.ASC()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 33.f);
		if (!Test.TestTrue(TEXT("Tarrik completes his authored mission"), W.CompleteActiveMission())) { return false; }
		if (!Test.TestNotNull(TEXT("Authored handoff to Selene"), W.Handoff(C.Missions[1]))) { Test.AddError(W.Error); return false; }
		W.Current()->GetInventoryComponent()->TryAddItemFromClass(USovPartitionSeleneKit::StaticClass(), 2);
		W.Current()->GetInventoryComponent()->SetCurrency(7);
		Test.TestTrue(TEXT("Selene earns her own Technique reward"), W.ClaimTechniqueReward(TEXT("Partition.Selene"), 3, Tags.Character_Player_Selene));
		W.ASC()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 12.f);
		return Test.TestTrue(TEXT("Selene completes her authored mission"), W.CompleteActiveMission());
	}
}
#endif
