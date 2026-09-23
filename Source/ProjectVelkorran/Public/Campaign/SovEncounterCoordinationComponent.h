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
class USovCommandLinkComponent;
class ASovWorldTransitActor;
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
/** Opt-in native evidence; no external event/Boolean can grant a reinforcement. */
UENUM(BlueprintType)
enum class ESovEncounterWaveCondition : uint8 { CommandSourceDefeated, TransitDoorOpen, AcceptedCrucibleLink };
USTRUCT(BlueprintType)
struct FSovEncounterWaveReleaseRule
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="63")) int32 Wave = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ESovEncounterWaveCondition Condition = ESovEncounterWaveCondition::CommandSourceDefeated;
	/** Required-for-victory actors already released; unknown ownership holds the gate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="40")) int32 MaximumLivingReleasedHostiles = 0;
	/** Exact registered link owner and component, captured active at attempt entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CommandLinkParticipantId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CommandLinkComponentName;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly) TObjectPtr<ASovWorldTransitActor> TransitDoor;
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
	/** Empty preserves the established required-defeat wave policy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Composition") TArray<FSovEncounterWaveReleaseRule> WaveReleaseRules;
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
	/** Health fraction that opens relief on its own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Pressure", meta=(ClampMin="0", ClampMax="1")) float ReliefHealthFraction = .25f;
	/** Health fraction that opens relief once the shield is depleted, before one more burst becomes lethal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Pressure", meta=(ClampMin="0", ClampMax="1")) float ReliefShieldDepletedHealthFraction = .5f;
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
	/** Director-only representation ownership hooks; neither resets waves nor steals other suspension leases. */
	bool CanChangeRepresentation(FName ParticipantId) const;
	bool HasUnreleasedWaves() const;
	/** Restore only the wave index committed by the completed native phase's save record. */
	bool RestoreCompletedWaveState(int32 ReleasedWave);
	/** Completed phase only: retire this coordinator while the director retains its freeze. */
	bool ReleaseCompletedPhaseBindings();
	bool CanPromoteRepresentation(FName ParticipantId) const;
	void ReleaseRepresentationActor(FName ParticipantId, ASovNPCCharacterBase* Character);
	void RefreshRepresentationBindings();
	virtual bool CanAdmitAttack(UNarrativeAbilitySystemComponent* Source, AActor* Target,
		const UNarrativeCombatAbility* Ability, FGameplayAbilitySpecHandle Handle) const override;
	virtual FGuid ReserveAttack(UNarrativeAbilitySystemComponent* Source, AActor* Target,
		const UNarrativeCombatAbility* Ability, FGameplayAbilitySpecHandle Handle) override;
	/** Admit an already-active Blueprint weapon at its authoritative release frame. */
	FGuid ReserveActiveAttack(UNarrativeAbilitySystemComponent* Source, AActor* Target,
		const UNarrativeCombatAbility* Ability, FGameplayAbilitySpecHandle Handle);
	virtual void ReleaseAttack(FGuid ReservationId) override;
	virtual bool IsAttackReservationCurrent(FGuid ReservationId, const UNarrativeAbilitySystemComponent* Source,
		const AActor* Target, FGameplayAbilitySpecHandle Handle) const override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
	bool CanAdmitAttackInternal(UNarrativeAbilitySystemComponent* Source, AActor* Target,
		const UNarrativeCombatAbility* Ability, FGameplayAbilitySpecHandle Handle, bool bRequireActive) const;
	FGuid ReserveAttackInternal(UNarrativeAbilitySystemComponent* Source, AActor* Target,
		const UNarrativeCombatAbility* Ability, FGameplayAbilitySpecHandle Handle, bool bRequireActive);
	friend struct FSovCoordinationTestAccess;
	friend struct FSovCrucibleRuntimeTestAccess;
	UFUNCTION() void HandleEncounterState(ESovEncounterState Previous, ESovEncounterState Current);
	void ResetAttempt();
	bool BindWaveRules();
	bool CanReleaseEventWave(int32 Wave) const;
	bool IsWaveContextCurrent(FGuid Attempt, uint64 Generation, int32 Wave) const;
	USovCommandLinkComponent* ResolveRuleLink(const FSovEncounterWaveReleaseRule& Rule) const;
	void RefreshComposition();
	void StageParticipant(FName Id, ASovNPCCharacterBase* Character);
	void RefreshStagedPresentation(FName Id);
	UFUNCTION() void HandleStagedVisualReady(class ANarrativeCharacter* Character);
	void ReleaseStagedParticipant(FName Id);
	FSovEncounterCompositionMember Member(FName Id) const;
	bool IsSourceOnscreen(const AActor* Source, const AActor* Target) const;
	bool IsBoundSource(UNarrativeAbilitySystemComponent* Source, FName& OutId) const;
	void UpdateWarnings();
	TWeakObjectPtr<ASovEncounterDirector> Director;
	FGuid BoundAttempt;
	uint64 BoundGeneration = 0;
	struct FWaveActor { TWeakObjectPtr<ASovNPCCharacterBase> Character; TWeakObjectPtr<UNarrativeAbilitySystemComponent> ASC; bool bRequired = false; int32 Wave = 0; };
	struct FWaveRule { FSovEncounterWaveReleaseRule Rule; TWeakObjectPtr<USovCommandLinkComponent> Link; FName SourceId; FGuid LinkInstance; };
	TMap<FName, FWaveActor> WaveActors;
	TMap<int32, FWaveRule> BoundWaveRules;
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
	struct FStagedPresentation { TWeakObjectPtr<AActor> Actor; bool bHidden = false; bool bCollision = true; };
	struct FStaged { FGuid Identity = FGuid::NewGuid(); TArray<FStagedPresentation> Presentation; TWeakObjectPtr<UNarrativeAbilitySystemComponent> ASC; TWeakObjectPtr<ASovNPCCharacterBase> Character; TWeakObjectPtr<class ANarrativeNPCController> ThreatController; TArray<TWeakObjectPtr<UActorComponent>> DisabledTicks; uint8 MovementMode = 0; uint8 CustomMovementMode = 0; bool bHidden = false; bool bCollision = true; bool bOwnsBusy = false; bool bOwnsInvulnerability = false; };
	TMap<FGuid, FReservation> Reservations;
	TMap<FName, FWarning> Warnings;
	TMap<FName, FStaged> Staged;
	TMap<FName, double> NextAttackAt;
	TMap<FName, ESovEncounterDecisionTier> RuntimeTiers;
	TArray<TWeakObjectPtr<UNarrativeAbilitySystemComponent>> BoundASCs;
	TMap<TWeakObjectPtr<UActorComponent>, float> PriorDecisionIntervals;
};
