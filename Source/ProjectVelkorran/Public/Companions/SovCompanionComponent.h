// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NarrativeSavableComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "AITypes.h"
#include "GameplayEffectTypes.h"
#include "Campaign/SovEncounterDirector.h"
#include "GAS/SovDamageSourcePolicy.h"
#include "SovCompanionComponent.generated.h"

class ASovCoActionAnchor;
class ASovEncounterDirector;
class ASovPlayerCharacterBase;
class UNarrativeAbilitySystemComponent;
class UNPCActivityComponent;
class USovCoActionGoal;
class USovCoActionActivity;
class USovCompanionCommandGoal;
class UGameplayAbility;
class UPrimitiveComponent;
class ANarrativeCharacter;
struct FSovDamageResult;

UENUM(BlueprintType)
enum class ESovCompanionCommand : uint8 { FocusTarget, HoldPosition, DefendPerson, MoveToAnchor, Interact, ExecuteCoAction, Regroup };

UENUM(BlueprintType)
enum class ESovCompanionCommandState : uint8 { Idle, MovingToAnchor, Succeeded, Failed };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovCompanionCommandChanged, ESovCompanionCommandState, State, const FString&, Reason);

/** Mission-scoped co-action coordinator over Narrative goals/activities, not a replacement companion brain. */
UCLASS(ClassGroup = (Sovereign), meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovCompanionComponent : public UActorComponent, public INarrativeSavableComponent, public ISovDamageSourcePolicy
{
	GENERATED_BODY()
public:
	USovCompanionComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion") FName CompanionId;
	/** Already-unlocked ability classes the authored AI is permitted to select. Empty means movement only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Companion|Combat") TArray<TSubclassOf<UGameplayAbility>> CuratedAbilities;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Companion|Combat", meta=(ClampMin="0.15",ClampMax="0.25")) float ContributionFraction = .2f;
	/** How long after an encounter scope opens the companion may fight before the player has dealt damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Companion|Combat", meta=(ClampMin="0",ClampMax="30",ForceUnits="s"))
	float OpeningContributionSeconds = 8.f;
	/** Budget below which a new attack is refused outright rather than clamped to a near-zero swing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Companion|Combat", meta=(ClampMin="0"))
	float MinimumMeaningfulContribution = 5.f;
	/** True while the opening allowance is open; the ordinary budget governs once it closes. */
	UFUNCTION(BlueprintPure, Category="Companion|Combat") bool IsInOpeningContribution() const;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Companion|Rescue") bool bMayRescue = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Companion|Rescue", meta=(ClampMin="100",ClampMax="2500")) float RescueRange = 1000.f;
	/** Explicit mission mark used only for hidden recovery at 25 m separation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Companion|Recovery") TObjectPtr<AActor> RecoveryAnchor;
	/** Authored split phases retain separation instead of pulling a protagonist through a locked route. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Companion|Recovery") bool bInAuthoredSplitPhase = false;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Companion") bool SetLeader(ASovPlayerCharacterBase* Player, FString& Reason);
	ASovPlayerCharacterBase* GetCurrentLeader() const { return Leader; }
	UFUNCTION(BlueprintPure, Category="Companion") bool CanRequestCommand(ASovPlayerCharacterBase* Player, ESovCompanionCommand Command, AActor* Target, FString& Reason) const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Companion") bool RequestCommand(ASovPlayerCharacterBase* Player, ESovCompanionCommand Command, AActor* Target, FString& Reason);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Companion") void CancelContextCommand();
	UFUNCTION(BlueprintPure, Category="Companion|Rescue") bool CanProvideRescue(ASovPlayerCharacterBase* Player, FString& Reason) const;
	virtual bool LimitSovDamage(AActor* Target, const FGameplayEffectContextHandle& Context, float& ShieldDamage, float& HealthDamage, float& PoiseDamage) const override;
	bool IsCommandCurrent(const USovCompanionCommandGoal* Goal) const;
	/** Includes a real accepted goal awaiting a suspended Narrative activity's selection. */
	bool HasAcceptedHoldPosition(const AActor* Target) const;
	void TickContextCommand(USovCompanionCommandGoal* Goal);
	/** Target of the currently owned native command attack; no fallback to arbitrary controller focus. */
	UFUNCTION(BlueprintPure, Category="Companion|Combat")
	static ANarrativeCharacter* ResolveCommandAttackTarget(ANarrativeCharacter* Character);
	void NotifyCommandInterrupted(USovCompanionCommandGoal* Goal);
	/** Defeat during a required action fails this active encounter; no resurrection or damage immunity is invented. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion") TObjectPtr<ASovEncounterDirector> RequiredEncounter;
	/** Optional ordinary-companion recovery owner. RequiredEncounter retains its fail/retry meaning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Companion|Recovery") TObjectPtr<ASovEncounterDirector> RecoveryEncounter;
	UFUNCTION(BlueprintPure, Category="Companion|Recovery") bool IsDisabled() const { return bDisabled; }
	UFUNCTION(BlueprintPure, Category = "Companion") bool CanRequestCoAction(ASovPlayerCharacterBase* Player, ASovCoActionAnchor* Anchor, FString& Reason) const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Companion") bool RequestCoAction(ASovPlayerCharacterBase* Player, ASovCoActionAnchor* Anchor, FString& Reason);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Companion") void CancelCoAction();
	UFUNCTION(BlueprintPure, Category = "Companion") ESovCompanionCommandState GetCommandState() const { return CommandState; }
	UPROPERTY(BlueprintAssignable, Category = "Companion") FSovCompanionCommandChanged OnCommandStateChanged;
	bool IsRequestCurrent(const USovCoActionGoal* Goal) const;
	void NotifyPathResult(USovCoActionGoal* Goal, bool bReached);
	void NotifyActivityInterrupted(USovCoActionGoal* Goal);
	virtual void Load_Implementation() override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION() void OnRep_CommandState();
	UPROPERTY(ReplicatedUsing = OnRep_CommandState) ESovCompanionCommandState CommandState = ESovCompanionCommandState::Idle;
private:
	friend class ASovCoActionAnchor;
	friend struct FSovCoActionTestAccess;
	bool ValidateRequest(ASovPlayerCharacterBase* Player, ASovCoActionAnchor* Anchor, bool bExisting, FString& Reason) const;
	void FinishCommand(bool bSucceeded, const FString& Reason);
	bool HasMissionPermission(ASovPlayerCharacterBase* Player) const;
	void ReleaseLeaderOwnership();
	bool TryRecoverSeparation();
	UFUNCTION() void ObserveContribution(const FSovDamageResult& Result);
	UFUNCTION() void ResetContribution(bool bStarted);
	UPROPERTY(Transient) TObjectPtr<ASovPlayerCharacterBase> Leader;
	UPROPERTY(Transient) TObjectPtr<UNarrativeAbilitySystemComponent> LeaderASC;
	UPROPERTY(Transient) TObjectPtr<USovCompanionCommandGoal> CommandGoal;
	FGameplayAbilitySpecHandle OwnedCommandAttack;
	float PlayerContribution = 0.f;
	float CompanionContribution = 0.f;
	double ContributionScopeOpenedAt = 0.;
	/** First attackable focus in this scope; travel from the encounter spawn does not consume the opening allowance. */
	double OpeningCombatAt = -1.;
	float CommandAttackStarted = 0.f;
	float NextCommandAttack = 0.f;
	float NextCommandDefense = 0.f;
	bool bCommandInterrupted = false;
	FAIRequestID CommandMoveId;
	TWeakObjectPtr<AActor> OwnedFocus;
	TWeakObjectPtr<AActor> PreviousFocus;
	TWeakObjectPtr<UPrimitiveComponent> LeaderMovementPrimitive;
	TWeakObjectPtr<UPrimitiveComponent> CompanionMovementPrimitive;
	TWeakObjectPtr<AActor> CollisionLeader;
	bool bOwnsLeaderIgnore = false;
	bool bOwnsCompanionIgnore = false;
	float NextMoveAttempt = 0.f;
	void TryFinishArrival();
	bool TryHiddenFallback();
	bool IsFallbackHiddenFromAllPlayers(const FVector& Destination) const;
	UFUNCTION() void HandleDeath(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, bool bIsDead);
	UFUNCTION() void HandleRecoveryEncounter(ESovEncounterState Previous, ESovEncounterState Current);
	UPROPERTY(Replicated) bool bDisabled = false;
	FActiveGameplayEffectHandle RecoveryProtection;
	UPROPERTY(Transient) TObjectPtr<ASovCoActionAnchor> ActiveAnchor;
	UPROPERTY(Transient) TObjectPtr<ASovPlayerCharacterBase> RequestingPlayer;
	UPROPERTY(Transient) TObjectPtr<USovCoActionGoal> ActiveGoal;
	UPROPERTY(Transient) TObjectPtr<USovCoActionActivity> Activity;
	UPROPERTY(Transient) TObjectPtr<UNPCActivityComponent> Activities;
	UPROPERTY(Transient) TObjectPtr<UNarrativeAbilitySystemComponent> BoundASC;
	FGuid RequestId;
	float RequestedAt = 0.f;
	float ArrivedAt = -1.f;
	bool bMutation = false;
	bool bOwnsBusyTag = false;
	bool bUsedFallback = false;
	bool bPathFailed = false;
	bool bActivityInterrupted = false;
};
