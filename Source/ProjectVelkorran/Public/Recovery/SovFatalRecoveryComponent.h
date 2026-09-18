// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffect.h"
#include "GAS/SovCombatTypes.h"
#include "SovFatalRecoveryComponent.generated.h"
class UNarrativeAbilitySystemComponent;
class ASovEncounterDirector;
class ASovPlayerCharacterBase;
class APlayerController;
class USovCompanionComponent;

UENUM(BlueprintType)
enum class ESovRecoveryState : uint8 { Ready, ResolvingFatal, Rescued, Retrying, Failed };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovRecoveryChanged, ESovRecoveryState, State, const FString&, Reason);

UCLASS()
class PROJECTVELKORRAN_API USovGameplayEffect_RecoveryProtection : public UGameplayEffect
{
	GENERATED_BODY()
public:
	USovGameplayEffect_RecoveryProtection();
};

/** Native fatal routing over the existing encounter and Narrative save ownership. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovFatalRecoveryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USovFatalRecoveryComponent();
	void InitializeWithAbilitySystem(UNarrativeAbilitySystemComponent* ASC);
	/** Encounter death callbacks yield only when this component owns the same fatal pawn. */
	bool OwnsFatalRecovery() const;
	/** Verified defeat of a required companion retries the segment without inventing a player death. */
	bool RequestCompanionFailure(USovCompanionComponent* Source);
	/** Called before a restored entry releases its enemies. */
	bool ProtectRestoredCheckpoint();
	static bool IsSafeRecoveryPosition(const ASovPlayerCharacterBase* Player, const FVector& Position);
	UFUNCTION(BlueprintPure, Category="Campaign|Recovery") ESovRecoveryState GetRecoveryState() const { return State; }
	UPROPERTY(BlueprintAssignable, Category="Campaign|Recovery") FSovRecoveryChanged OnRecoveryChanged;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	friend struct FSovRecoveryTestAccess;
	void Unbind();
	bool IsCurrentContext(uint64 ExpectedEpoch, const UNarrativeAbilitySystemComponent* ASC,
		const ASovPlayerCharacterBase* P, const APlayerController* PC) const;
	void ResolveFatal(uint64 ExpectedEpoch);
	void Retry(uint64 ExpectedEpoch);
	void SetState(ESovRecoveryState Value, const FString& Reason = FString());
	bool ApplyProtection(float Seconds, bool bRescue);
	void ReleaseInputLock();
	ASovPlayerCharacterBase* Player() const;
	ASovEncounterDirector* FindActiveEncounter() const;
	UFUNCTION() void HandleDeath(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, bool bDead);
	UFUNCTION() void HandleDamage(const FSovDamageResult& Result);
	/** A.1: respawn protection lasts 1.5 s or until the protagonist attacks, whichever comes first. */
	UFUNCTION() void HandleDealtDamage(const FSovDamageResult& Result);
	/** Drops respawn protection early. Returns true when protection was actually being held. */
	bool EndRespawnProtection();
	UPROPERTY(Transient) TObjectPtr<UNarrativeAbilitySystemComponent> BoundASC;
	UPROPERTY(Transient) TObjectPtr<ASovEncounterDirector> PendingEncounter;
	TWeakObjectPtr<APlayerController> LockedController;
	FActiveGameplayEffectHandle ProtectionHandle;
	FTimerHandle DecisionTimer;
	FTimerHandle RetryTimer;
	FGuid UsedRescueAttempt;
	FGuid FatalTransaction;
	uint64 Epoch = 0;
	ESovRecoveryState State = ESovRecoveryState::Ready;
	bool bExcludedFatal = false;
	bool bEndingPlay = false;
	bool bOwnInputLock = false;
	bool bResolvingRevive = false;
	bool bCanonicalCompanionFailure = false;
	bool bOwnFailureBusy = false;
};
