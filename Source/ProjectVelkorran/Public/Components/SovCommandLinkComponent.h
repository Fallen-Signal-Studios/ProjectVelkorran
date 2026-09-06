// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "SovCommandLinkComponent.generated.h"

class FLifetimeProperty;
class ANarrativeCharacter;
class UAbilitySystemComponent;
class UGameplayEffect;
class UNarrativeAbilitySystemComponent;

/** Durable encounter state for one authored command/network link. */
UENUM(BlueprintType)
enum class ESovCommandLinkState : uint8
{
	Inactive UMETA(DisplayName = "Inactive"),
	Active UMETA(DisplayName = "Active"),
	Severed UMETA(DisplayName = "Severed")
};

/** Exact outcome of one authority request to sever a command link. */
UENUM(BlueprintType)
enum class ESovCommandLinkSeverResolution : uint8
{
	Invalid UMETA(DisplayName = "Invalid"),
	Inactive UMETA(DisplayName = "Inactive"),
	NotHostile UMETA(DisplayName = "Not Hostile"),
	Immune UMETA(DisplayName = "Immune"),
	AlreadySevered UMETA(DisplayName = "Already Severed"),
	NewlySevered UMETA(DisplayName = "Newly Severed")
};

/** Immutable identity for one successful Active-to-Severed transaction. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCommandLinkSeverResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Command Link")
	FGuid TransactionId;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Command Link")
	FGuid LinkInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Command Link")
	FName LinkId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Command Link")
	TObjectPtr<AActor> LinkOwner = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Command Link")
	TObjectPtr<AActor> CommandSource = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Command Link")
	TObjectPtr<AActor> SeveredBy = nullptr;

	/** True only when the severer is hostile to at least one link participant. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Command Link")
	bool bEligibleForEchoReward = false;

	/** Snapshot of the actors whose coordination state changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Command Link")
	TArray<TObjectPtr<AActor>> AffectedActors;
};

/** Durable link state. Actor references are resolved by the encounter's stable participant IDs. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCommandLinkSnapshot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly) FName LinkId;
	UPROPERTY(SaveGame, BlueprintReadOnly) ESovCommandLinkState State = ESovCommandLinkState::Inactive;
	UPROPERTY(SaveGame, BlueprintReadOnly) FGuid LinkInstanceId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FGuid LastSeverTransactionId;
};

/** Atomic replication payload so client callbacks never observe half a sever. */
USTRUCT()
struct FSovCommandLinkReplicationState
{
	GENERATED_BODY()

	UPROPERTY()
	ESovCommandLinkState State = ESovCommandLinkState::Inactive;

	UPROPERTY()
	uint32 Revision = 0;

	UPROPERTY()
	FGuid LinkInstanceId;

	UPROPERTY()
	FGuid LastSeverTransactionId;

	UPROPERTY()
	FName LinkId = NAME_None;

	UPROPERTY()
	TObjectPtr<AActor> CommandSource = nullptr;

	UPROPERTY()
	TObjectPtr<AActor> LastSeveredBy = nullptr;

	UPROPERTY()
	bool bLastSeverEligibleForEchoReward = false;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> LastSeverAffectedActors;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSovCommandLinkStateChangedSignature,
	ESovCommandLinkState, OldState,
	ESovCommandLinkState, NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSovCommandLinkSeveredSignature,
	const FSovCommandLinkSeverResult&, Result);

/**
 * Server-owned coordinator for one authored combat command/network link.
 *
 * Add this component to the handler, commander, or device that owns the link.
 * Encounter setup registers the affected actors. The component contributes one
 * Active/Severed tag count to every participant, optionally applies an active
 * Gameplay Effect, and guarantees that each link instance can be severed once.
 */
UCLASS(ClassGroup = (Sovereign), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovCommandLinkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovCommandLinkComponent();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Command Link")
	ESovCommandLinkState GetCommandLinkState() const
	{
		return ReplicationState.State;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Command Link")
	bool IsCommandLinkActive() const
	{
		return ReplicationState.State == ESovCommandLinkState::Active;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Command Link")
	FName GetLinkId() const
	{
		return ReplicationState.LinkId != NAME_None
			? ReplicationState.LinkId
			: LinkId;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Command Link")
	FGuid GetLinkInstanceId() const { return ReplicationState.LinkInstanceId; }

	/** Authority-selected actor whose death/destruction ends this link instance. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Command Link")
	AActor* GetCommandSource() const
	{
		return ReplicationState.CommandSource.Get();
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Command Link")
	TArray<AActor*> GetLinkedActors() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Command Link")
	bool HasValidCommandLinkConfiguration() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Command Link")
	bool ContainsLinkedActor(const AActor* Actor) const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Command Link")
	bool IncludesOwnerAsParticipant() const
	{
		return bIncludeOwnerAsParticipant;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Command Link|Presentation")
	bool IsWeakPointRevealEnabledOnSever() const
	{
		return bRevealWeakPointsOnSever;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Command Link|Presentation")
	float GetWeakPointRevealDuration() const
	{
		return FMath::Max(WeakPointRevealDuration, 0.0f);
	}

	/** Adds one encounter actor to this coordinator without activating the link. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Command Link")
	bool RegisterLinkedActor(AActor* Actor);

	/**
	 * Removes one encounter actor and its link contributions.
	 * A stale destroyed reference may still be supplied so encounter teardown can
	 * clean membership before the reference is cleared by garbage collection.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Command Link")
	bool UnregisterLinkedActor(AActor* Actor);

	/**
	 * Configures the stable encounter identity before a live link instance exists.
	 * Runtime coordinators may use this after all spawned participants are ready;
	 * active, severed, and death-deactivated instances reject reconfiguration.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Command Link")
	bool ConfigureLinkId(FName InLinkId);

	/** Starts a fresh link instance. Null uses the component owner as its source. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Command Link")
	bool ActivateCommandLink(AActor* InCommandSource);

	/** Restores the authored starting state and creates a fresh instance identity. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Command Link")
	void ResetCommandLink();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Command Link|Checkpoint")
	FSovCommandLinkSnapshot CaptureCommandLinkState() const;
	/** No synthetic Sever event, reveal, or reward is emitted by restoration. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Command Link|Checkpoint")
	bool RestoreCommandLinkState(const FSovCommandLinkSnapshot& Snapshot, AActor* CommandSource, const TArray<AActor*>& Participants);

	/** The only gameplay mutation that may produce a successful sever result. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Command Link")
	ESovCommandLinkSeverResolution TrySeverCommandLink(
		AActor* SeveredBy,
		FSovCommandLinkSeverResult& OutResult);

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Command Link|Presentation")
	FSovCommandLinkStateChangedSignature OnCommandLinkStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Command Link|Presentation")
	FSovCommandLinkSeveredSignature OnCommandLinkSevered;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Stable authored identity within an encounter or mission package. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Command Link")
	FName LinkId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Command Link")
	bool bStartsActive = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Command Link")
	bool bSeverable = true;

	/** Existing DeviceDisable immunity also protects an authored command node. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Command Link")
	bool bRespectDeviceDisableImmunity = true;

	/** Hostility is evaluated at sever time, never trusted from the ability graph. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Command Link|Echo")
	bool bCanGrantSeleneEcho = true;

	/** The owner participates in link state in addition to registered actors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Command Link")
	bool bIncludeOwnerAsParticipant = true;

	/** Optional buff/coordination effect removed atomically when the link ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Command Link|Effect")
	TSubclassOf<UGameplayEffect> ActiveLinkEffectClass;

	/** Placed encounter instances may assign members here or register at runtime. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Replicated, Category = "Sovereign|Command Link")
	TArray<TObjectPtr<AActor>> LinkedActors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Command Link|Presentation")
	bool bRevealWeakPointsOnSever = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Command Link|Presentation", meta = (ClampMin = "0.0", Units = "s"))
	float WeakPointRevealDuration = 5.0f;

private:
	TArray<AActor*> BuildParticipantSnapshot() const;
	int32 PruneInvalidLinkedActors();
	UAbilitySystemComponent* ResolveAbilitySystem(AActor* Actor) const;
	bool IsDisruptionImmune() const;
	bool IsHostileToAnyParticipant(const AActor* Instigator) const;
	void BindParticipant(AActor* Actor);
	void UnbindParticipant(AActor* Actor);
	void UnbindAllParticipants();
	void ReconcileParticipant(AActor* Actor);
	void RemoveParticipantContributions(AActor* Actor);
	void ReconcileAllParticipants();
	void ClearAllParticipantContributions();
	void ApplyActiveEffect(AActor* Actor, UAbilitySystemComponent* TargetAbilitySystem);
	void RemoveActiveEffect(AActor* Actor);
	void DeactivateWithoutSever();
	void ProcessDeferredMutation();
	void WakeOwnerForReplication() const;
	void WakeParticipantForReplication(
		AActor* Actor,
		UAbilitySystemComponent* AbilitySystem) const;
	void RevealParticipantWeakPoints(
		const TArray<TObjectPtr<AActor>>& Participants,
		AActor* RevealInstigator) const;
	FSovCommandLinkSeverResult BuildLastSeverResult() const;

	UFUNCTION()
	void HandleParticipantDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleParticipantASCInitialized();

	UFUNCTION()
	void HandleParticipantDeathStateChanged(
		AActor* ChangedActor,
		UNarrativeAbilitySystemComponent* ChangedActorASC,
		bool bIsDead);

	UFUNCTION()
	void OnRep_ReplicationState(
		const FSovCommandLinkReplicationState& OldState);

	UPROPERTY(ReplicatedUsing = OnRep_ReplicationState, Transient)
	FSovCommandLinkReplicationState ReplicationState;

	TSet<TWeakObjectPtr<AActor>> ActiveTagRecipients;
	TSet<TWeakObjectPtr<AActor>> SeveredTagRecipients;
	TMap<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle> ActiveEffectHandles;
	TSet<TWeakObjectPtr<ANarrativeCharacter>> BoundParticipantCharacters;
	TSet<TWeakObjectPtr<UNarrativeAbilitySystemComponent>> BoundParticipantAbilitySystems;
	bool bCommandLinkMutationInProgress = false;
	bool bDeferredDeactivateRequested = false;
	bool bDeferredResetRequested = false;
};
