// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NarrativeSavableComponent.h"
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "SovConvergenceCompanionState.generated.h"
class USovCampaignDefinition;
class ASovPlayerCharacterBase;
class UNarrativeAbilitySystemComponent;

/** Single mission companion save/spawn owner. Narrative remains the disk and actor-record serializer. */
UCLASS()
class PROJECTVELKORRAN_API USovConvergenceCompanionState : public UActorComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()
public:
	virtual ENarrativeRestorePhase GetSaveRestorePhase() const override { return ENarrativeRestorePhase::Companions; }
	bool StageHandoff(USovCampaignDefinition* Mission, FGameplayTag Incoming, ASovPlayerCharacterBase* Outgoing, FString& Reason, FName HandoffBeat = NAME_None);
	bool StageSavedRecord(const TArray<uint8>& Bytes, USovCampaignDefinition* Mission, FGameplayTag Lead, FString& Reason, const TArray<uint8>* CampaignBytes = nullptr);
	bool StageInitialCompanion(USovCampaignDefinition* Mission, FGameplayTag Lead, FString& Reason);
	bool PollStaged(FString& Reason);
	bool CommitStaged(ASovPlayerCharacterBase* Leader, FString& Reason);
	bool FinishEncounterRestore(ASovPlayerCharacterBase* Leader, FString& Reason);
	bool IsEncounterRestorePending() const { return bEncounterRestorePending; }
	void RollbackStaged();
	bool HasStagedProxy() const { return IsValid(Staged); }
	ASovProtagonistCompanionCharacter* GetActiveCompanion() const { return Active; }
	virtual void PrepareForSave_Implementation() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void Load_Implementation() override;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	friend struct FSovConvergenceTestAccess;
	friend struct FSovCompanionApproachTestAccess;
	bool RequiresCompanion(const USovCampaignDefinition* Mission) const;
	bool SavedMissionRequiresCompanion(const USovCampaignDefinition* Mission, const TArray<uint8>* CampaignBytes, bool& bRequired, FString& Reason) const;
	static bool ResolveSavedKitGrants(const FSovProtagonistSnapshot& Kit,
		const TArray<TSubclassOf<UGameplayAbility>>& Curated, TArray<FSovCompanionKitGrant>& Grants, FString& Reason);
	bool StageSnapshot(const FSovCompanionProxySnapshot& Snapshot, USovCampaignDefinition* Mission, FString& Reason, bool bRestoreRecord = true);
	void DestroyOwnedProxy(ASovProtagonistCompanionCharacter* Proxy);
	UPROPERTY(SaveGame) bool bHasSavedCompanion = false;
	UPROPERTY(SaveGame) FSovCompanionProxySnapshot SavedCompanion;
	UPROPERTY(Transient) TObjectPtr<ASovProtagonistCompanionCharacter> Active;
	UPROPERTY(Transient) TObjectPtr<ASovProtagonistCompanionCharacter> Staged;
	UPROPERTY(Transient) TObjectPtr<ASovProtagonistCompanionCharacter> IncomingProxy;
	UPROPERTY(Transient) TObjectPtr<USovCampaignDefinition> StagedMission;
	UPROPERTY(Transient) FSovCompanionProxySnapshot PendingSnapshot;
	/** Handoff preparation may stage live companion resources for the incoming pawn; cancellation restores the previous ledger record. */
	UPROPERTY(Transient) FSovProtagonistSnapshot PreviousIncomingSnapshot;
	bool bHasPreviousIncomingSnapshot = false;
	bool bRestoreActorRecord = false;
	bool bRecordApplied = false;
	bool bSaveCaptureValid = true;
	bool bEncounterRestorePending = false;
	FString EncounterRestoreError;
};
