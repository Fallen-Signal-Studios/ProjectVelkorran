// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include <AbilitySystemInterface.h>
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include <GameplayTagAssetInterface.h>
#include "UnrealFramework/NarrativeCharacter.h"
#include "Navigation/PathFollowingComponent.h"
#include "AI/NarrativeThreatMemory.h"
#include "Perception/AIPerceptionTypes.h"
#include "NarrativeNPCController.generated.h"

struct FPathFollowingResult;
class UNarrativeAbilitySystemComponent;

/**
 * NPC Controller for NPCs spawned by the Narrative NPC subsystem. 
 */
UCLASS()
class NARRATIVEARSENAL_API ANarrativeNPCController : public AAIController, 
	public IAbilitySystemInterface, public IGameplayTagAssetInterface, public INarrativeTeamAgentInterface, public INarrativeCharacterOwner
{
	GENERATED_BODY()
	
public:

	friend class UNarrativeAbilitySystemComponent;

	ANarrativeNPCController(const FObjectInitializer& ObjectInitializer);

	//Interfaces 
	virtual void BeginPlay() override; 
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override; 
	class UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual FGameplayTagContainer GetFactions() const override;
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual void Destroyed() override;
	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
	virtual void SetPawn(APawn* InPawn) override;
	virtual bool ShouldPostponePathUpdates() const override;

	virtual class ANarrativeCharacter* GetNarrativeCharacter() const override;

	/** Uses the controller's existing Perception component; does not install a second sensor. */
	UFUNCTION(BlueprintCallable, Category = "Narrative|Threat")
	void RefreshThreatMemory();
	/** Server-side producer adapter for authored sensor, Echo, command and spoof events. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Narrative|Threat")
	bool ReportThreatObservation(AActor* Target, ENarrativeThreatSource Source, FVector Position,
		float Strength = 1.f, float Confidence = 1.f, float Lifetime = 8.f);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Narrative|Threat")
	bool ShareThreatWith(ANarrativeNPCController* Recipient, AActor* Target);
	UFUNCTION(BlueprintPure, Category = "Narrative|Threat")
	bool CanDirectlyTargetThreat(AActor* Target) const;
	UFUNCTION(BlueprintPure, Category = "Narrative|Threat")
	bool GetBestThreatMemory(AActor* Target, FNarrativeThreatMemory& OutMemory) const;
	UFUNCTION(BlueprintPure, Category = "Narrative|Threat")
	TArray<FNarrativeThreatMemory> GetThreatDebugSnapshot() const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Narrative|Threat")
	void ForgetThreat(AActor* Target);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Narrative|Threat")
	void ClearThreatMemory();
	/** Existing encounter/restore owners hold independent, idempotent suspension contributions. */
	void SetThreatMemorySuspended(UObject* SuspensionOwner, bool bSuspend);
	bool IsThreatMemorySuspended() const;
	/** Selected archetypes must opt into these nonvisual sensing capabilities. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative|Threat") bool bRequireThreatMemoryForTargeting = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative|Threat") bool bAcceptNetworkThreats = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative|Threat") bool bAcceptEchoThreats = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative|Threat") bool bShareThreatsWithFaction = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative|Threat", meta = (ClampMin = "0", ClampMax = "10000"))
	float ThreatShareRadius = 2500.f;
	bool IsThreatMemoryManaged() const;

	#if ENABLE_VISUAL_LOG
	virtual void GrabDebugSnapshot(FVisualLogEntry* Snapshot) const override;
#endif

	//Tells the AI controller it needs to destroy itself and its pawn. 
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "NarrativeNPCController")
	void CleanUp(const float RemovePawnDelay);

	UFUNCTION(BlueprintPure, Category = "NarrativeNPCController")
	class UBehaviorTree* GetCurrentTree();

	void StopBehaviorTree();

	//Grab the NPCs data asset 
	UFUNCTION(BlueprintPure, Category = "NarrativeNPCController")
	class UNPCDefinition* GetNPCData() const;
	
	//Grab the NPCs name
	UFUNCTION(BlueprintPure, Category = "NarrativeNPCController")
	FText GetNPCName() const;

	//Check whether our controlled NPC is alive
	UFUNCTION(BlueprintPure, Category = "NarrativeNPCController")
	bool IsAlive() const;

	//Grab the controlled NPC. This will return nullptr if NPC is controlling a car, or some other pawn. Use GetOwnedNPC() for a version that returns the NPC regardless of what GetPawn() is 
	UFUNCTION(BlueprintPure, Category = "NarrativeNPCController")
	ANarrativeNPCCharacter* GetControlledNPC() const;

	UFUNCTION(BlueprintPure, Category = "NarrativeNPCController")
	ANarrativeNPCCharacter* GetOwnedNPC() const;

	/**
	 * Native, server-only attack-token lease helpers.
	 *
	 * Abilities that bypass the normal Behavior Tree token task can use these
	 * without exposing the controller's mutable token pointer. A lease serial
	 * prevents a delayed ability cleanup from returning a newer token that the
	 * controller acquired after its original token was stolen or returned.
	 */
	bool CanAcquireAttackTokenFor(
		const UNarrativeAbilitySystemComponent* TargetToAttack) const;
	bool TryAcquireAttackTokenFor(
		UNarrativeAbilitySystemComponent* TargetToAttack,
		uint64& OutLeaseSerial,
		bool& bOutNewlyAcquired);
	bool IsAttackTokenLeaseCurrent(
		uint64 LeaseSerial,
		const UNarrativeAbilitySystemComponent* ExpectedTarget) const;
	bool IsAttackTokenReservedFor(
		const UNarrativeAbilitySystemComponent* ExpectedTarget) const;
	bool ReleaseAttackTokenLease(
		uint64 LeaseSerial,
		bool bReturnTokenAfterRelease);

protected:
	
	/**The NPC activity component, stores the behaviour tree and current state and can write that to disk.*/
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Narrative|Components")
	TObjectPtr<class UNPCActivityComponent> NPCActivityComponent;

	//NPCs interaction component 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative|Interaction")
	TObjectPtr<class UNPCInteractionComponent> InteractionComponent;

	//Request an attack token from the target ASC. Return true if we successfully claimed the token and can attack. 
	UFUNCTION(BlueprintCallable, Category = "Attack Tokens")
	bool RequestAttackToken(UNarrativeAbilitySystemComponent* TargetToAttack);

	//Give our token back to the current ASC - called automatically by RequestAttackToken if we already have one. 
	UFUNCTION(BlueprintCallable, Category = "Attack Tokens")
	bool ReturnToken();

	//Called by our token granted when our granted token was stolen. 
	void TokenStolen();

	//The current attack token we've claimed 
	UPROPERTY(BlueprintReadOnly, Category = "Attack Tokens")
	TObjectPtr<class UNarrativeAbilitySystemComponent> GrantedToken;

private:
	UFUNCTION() void HandleThreatPerception(AActor* Actor, FAIStimulus Stimulus);
	UFUNCTION() void HandleThreatPerceptionActivated(UActorComponent* Component, bool bReset);
	UFUNCTION() void HandleThreatPerceptionDeactivated(UActorComponent* Component);
	void InvalidateCachedThreatPerception();
	void BindThreatPerception();
	void ClearInvalidThreatTarget();
	bool IsThreatTargetEligible(AActor* Target) const;
	bool IsThreatTargetCloaked(AActor* Target) const;
	bool IsThreatPerceptionReady() const;
	void ShareFreshThreats();
	UPROPERTY(Transient) TArray<FNarrativeThreatMemory> ThreatMemory;
	UPROPERTY(Transient) TWeakObjectPtr<class UAIPerceptionComponent> ThreatPerception;
	bool bThreatMemoryManaged = false;
	bool bRefreshingThreatMemory = false;
	bool bThreatPerceptionWasReady = false;
	bool bThreatPerceptionExplicitlyDeactivated = false;
	bool bInvalidatingThreatPerception = false;
	bool bClearingThreatTarget = false;
	uint64 ThreatMemoryGeneration = 0;
	uint64 PawnAssignmentGeneration = 0;
	bool bPawnThreatCleanupPending = false;
	UPROPERTY(Transient) TArray<TWeakObjectPtr<UObject>> ThreatSuspensionOwners;
	float ThreatUpdateAccumulator = 0.f;
	TWeakObjectPtr<AActor> InvestigationTarget;
	FVector InvestigationPosition = FVector::ZeroVector;
	bool bOwnsInvestigationLocation = false;

	void SetGrantedAttackToken(
		UNarrativeAbilitySystemComponent* NewGrantedToken);
	void AdvanceAttackTokenLeaseSerial();
	void ForceReleaseAttackToken();

	/** Monotonic, server-local identity for token ownership and each reservation. */
	uint64 AttackTokenLeaseSerial = 0;

	/** One direct ability may reserve the current token against BT return/steal. */
	uint64 ReservedAttackTokenLeaseSerial = 0;
	bool bReturnAttackTokenWhenReservationEnds = false;

protected:

	//Gives our NPC controller a chance to react to death.
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	void HandleDeath(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead);
	virtual void HandleDeath_Implementation(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead);

protected:

	//Bit of a test. We're going to check if attempting to traverse when our Move fails due to being blocked is a workable solution. 
	virtual void OnMoveComplete(FAIRequestID RequestID, const FPathFollowingResult& Result);

	FRotator SmoothTargetRotation;
 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NarrativeNPCController")
	float SmoothFocusInterpSpeed = 30.0f;
 
public:

	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn) override;

public:

	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	FORCEINLINE class UNPCActivityComponent* GetActivityComponent() const {return NPCActivityComponent; };
	
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	FORCEINLINE class UNPCInteractionComponent* GetInteractionComponent() const {return InteractionComponent;};

protected:

	//We cache this because GetPawn() won't return our character if we started possessing a car, horse, etc. We'll need the OwnedCharacter. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative")
	TObjectPtr<class ANarrativeNPCCharacter> OwnedCharacter; 
};
