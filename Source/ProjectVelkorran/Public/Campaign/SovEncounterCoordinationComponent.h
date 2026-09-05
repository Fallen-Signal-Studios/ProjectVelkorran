// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GAS/SovBotAttackCoordinator.h"
#include "Campaign/SovEncounterTypes.h"
#include "SovEncounterCoordinationComponent.generated.h"
class ASovEncounterDirector;
class ASovNPCCharacterBase;
class UBrainComponent;
UENUM(BlueprintType)
enum class ESovEncounterDecisionTier : uint8 { Combatant, Supporting };
UENUM(BlueprintType)
enum class ESovEncounterRole : uint8 { Line, Shield, Marksman, Controller, Commander, Duelist, Brute, Swarm, Corruptor, Objective };
USTRUCT(BlueprintType)
struct FSovEncounterCompositionMember
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ParticipantId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ESovEncounterDecisionTier Tier = ESovEncounterDecisionTier::Combatant;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ESovEncounterRole Role = ESovEncounterRole::Line;
	/** All actors already belong to the entry snapshot. No late spawn bypasses recovery. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0")) int32 Wave = 0;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovEncounterWaveChanged, int32, Wave);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovEncounterPressureChanged, bool, bRelief, float, Intensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSovOffscreenAttackWarning, FGuid, WarningId, AActor*, Source, float, MinimumLeadSeconds);

/** Composition and escalation only; Narrative still owns individual attack choice and its token leases. */
UCLASS(ClassGroup=(Sovereign), BlueprintType, meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovEncounterCoordinationComponent : public UActorComponent, public ISovBotAttackCoordinator
{
	GENERATED_BODY()
public:
	USovEncounterCoordinationComponent();
	void InitializeCoordination();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Composition") TArray<FSovEncounterCompositionMember> Composition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Composition") int32 MaximumCombatants = 16;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Composition") int32 MaximumSupporting = 24;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Composition") int32 MeleeAttackerSlots = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Composition") TMap<ESovEncounterRole, int32> SimultaneousRoleQuotas;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Composition") float SupportingDecisionInterval = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Composition") float SupportingAttackInterval = 1.f;
	/** Explicitly disable in declared duels/failure challenges. No resources are granted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Pressure") bool bAllowLowResourceRelief = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Pressure") float ReliefDuration = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Pressure") float ReliefCooldown = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Pressure") float ReliefAttackInterval = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Warnings") bool bRequireOffscreenRangedWarning = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Warnings") float WarningLeadSeconds = 0.75f;
	UPROPERTY(BlueprintAssignable, Category="Encounter") FSovEncounterWaveChanged OnWaveChanged;
	UPROPERTY(BlueprintAssignable, Category="Encounter") FSovEncounterPressureChanged OnPressureChanged;
	UPROPERTY(BlueprintAssignable, Category="Encounter") FSovOffscreenAttackWarning OnOffscreenAttackWarning;
	/** Presentation calls this only after a readable cue is actually displayed/played. Receipt is attempt/source scoped. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Encounter") bool AcknowledgeOffscreenWarning(FGuid WarningId);
	/** A/B only, same actor and identity. Active attacks cannot change decision tier. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Encounter") bool SetDecisionTier(FName ParticipantId, ESovEncounterDecisionTier Tier);
	UFUNCTION(BlueprintPure, Category="Encounter") int32 GetCurrentWave() const { return CurrentWave; }
	UFUNCTION(BlueprintPure, Category="Encounter") bool IsPressureReliefActive() const;
	UFUNCTION(BlueprintPure, Category="Encounter") bool ValidateComposition(FString& Error) const;
	virtual bool CanAdmitAttack(UNarrativeAbilitySystemComponent* Source, AActor* Target,
		const UNarrativeCombatAbility* Ability, FGameplayAbilitySpecHandle Handle) const override;
	virtual FGuid ReserveAttack(UNarrativeAbilitySystemComponent* Source, AActor* Target,
		const UNarrativeCombatAbility* Ability, FGameplayAbilitySpecHandle Handle) override;
	virtual void ReleaseAttack(FGuid ReservationId) override;
	virtual bool IsAttackReservationCurrent(FGuid ReservationId, const UNarrativeAbilitySystemComponent* Source,
		const AActor* Target, FGameplayAbilitySpecHandle Handle) const override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
	friend struct FSovCoordinationTestAccess;
	UFUNCTION() void HandleEncounterState(ESovEncounterState Previous, ESovEncounterState Current);
	void ResetAttempt();
	void RefreshComposition();
	void StageParticipant(FName Id, ASovNPCCharacterBase* Character);
	void ReleaseStagedParticipant(FName Id);
	FSovEncounterCompositionMember Member(FName Id) const;
	bool IsSourceOnscreen(const AActor* Source, const AActor* Target) const;
	bool IsBoundSource(UNarrativeAbilitySystemComponent* Source, FName& OutId) const;
	void UpdateWarnings();
	TWeakObjectPtr<ASovEncounterDirector> Director;
	FGuid BoundAttempt;
	int32 CurrentWave = 0;
	bool bValidComposition = false;
	bool bReliefReported = false;
	bool bRefreshing = false;
	bool bReserving = false;
	double ReliefUntil = 0.;
	double NextReliefAt = 0.;
	double NextReliefAttackAt = 0.;
	struct FReservation { TWeakObjectPtr<UNarrativeAbilitySystemComponent> ASC; TWeakObjectPtr<AActor> Avatar; TWeakObjectPtr<AActor> Target; FGameplayAbilitySpecHandle Handle; ESovEncounterRole Role; bool bMelee = false; };
	struct FWarning { FGuid Id; TWeakObjectPtr<AActor> Source; double CreatedAt = 0.; double AcknowledgedAt = 0.; bool bAcknowledged = false; };
	struct FStaged { TWeakObjectPtr<UNarrativeAbilitySystemComponent> ASC; TWeakObjectPtr<ASovNPCCharacterBase> Character; TWeakObjectPtr<class ANarrativeNPCController> ThreatController; TArray<TWeakObjectPtr<UActorComponent>> DisabledTicks; uint8 MovementMode = 0; uint8 CustomMovementMode = 0; bool bHidden = false; bool bCollision = true; bool bOwnsBusy = false; bool bOwnsInvulnerability = false; };
	TMap<FGuid, FReservation> Reservations;
	TMap<FName, FWarning> Warnings;
	TMap<FName, FStaged> Staged;
	TMap<FName, double> NextAttackAt;
	TMap<FName, ESovEncounterDecisionTier> RuntimeTiers;
	TArray<TWeakObjectPtr<UNarrativeAbilitySystemComponent>> BoundASCs;
	TMap<TWeakObjectPtr<UActorComponent>, float> PriorDecisionIntervals;
};
