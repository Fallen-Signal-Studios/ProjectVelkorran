// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Campaign/SovEncounterTypes.h"
#include "GAS/SovCombatTypes.h"
#include "NarrativeSavableComponent.h"
#include "TimerManager.h"
#include "SovAurelionThermalFractureComponent.generated.h"

class ASovEncounterDirector;
class ASovPlayerCharacterBase;
class ASovPlayerController;
class ASovProtagonistCompanionCharacter;
class UNarrativeAbilitySystemComponent;
class USovCampaignStateComponent;
class USovCampaignDefinition;
class UAbilitySystemComponent;
struct FGameplayEffectSpec;

/** Evidence of an actual control application, player heat hit and nonlethal Poise fracture. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovAurelionThermalFractureReceipt
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) FName EncounterId;
    UPROPERTY(SaveGame, BlueprintReadOnly) FGuid AttemptId;
    UPROPERTY(SaveGame, BlueprintReadOnly) FGuid FrostApplicationId;
    UPROPERTY(SaveGame, BlueprintReadOnly) FGuid HeatTransactionId;
    UPROPERTY(SaveGame, BlueprintReadOnly) FGuid PayoffTransactionId;
    bool IsComplete() const
    { return !EncounterId.IsNone() && AttemptId.IsValid() && FrostApplicationId.IsValid()
        && HeatTransactionId.IsValid() && PayoffTransactionId.IsValid() && HeatTransactionId != PayoffTransactionId; }
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovAurelionThermalFractureCompleted,
    const FSovAurelionThermalFractureReceipt&, Receipt);

/** E4's local frost-to-heat interaction. Does not complete encounters, kill the elite, or award campaign facts.
 * Narrative captures its receipt inside the elite's existing component record. Loading retires live proof;
 * the campaign journal owns committed outcomes, and an entry retry requires a new physical interaction. */
UCLASS(ClassGroup=(Sovereign), BlueprintType, meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovAurelionThermalFractureComponent : public UActorComponent, public INarrativeSavableComponent
{
    GENERATED_BODY()
public:
    USovAurelionThermalFractureComponent();
    /** Optional explicit binding. Otherwise the unique director registering this elite is resolved. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Thermal Fracture") TObjectPtr<ASovEncounterDirector> EncounterDirector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Thermal Fracture") TObjectPtr<AActor> FrostAnchor;
    /** Unique actor tag of the clean frost setup mark. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Thermal Fracture") FName FrostAnchorId = TEXT("Aurelion.Crucible.CleanFrost");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Thermal Fracture", meta=(ClampMin="0.25", ClampMax="10.0")) float FractureWindowSeconds = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Thermal Fracture", meta=(ClampMin="50", ClampMax="300")) float FrostAnchorReach = 150.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Thermal Fracture", meta=(ClampMin="100", ClampMax="2500")) float FrostSetupRange = 1500.f;
    /** Leaves a usable approach area between the authored heat control and a moving Elite. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Thermal Fracture", meta=(ClampMin="100", ClampMax="600")) float HeatConfirmRange = 550.f;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Aurelion|Thermal Fracture") FString LastError;
    UPROPERTY(BlueprintAssignable, Category="Aurelion|Thermal Fracture") FSovAurelionThermalFractureCompleted OnThermalFractureCompleted;
    /** Rebinds readiness/delegates only. It never opens a window or supplies a success receipt. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Aurelion|Thermal Fracture") bool InitializeBindings();
    /** Contextual setup uses Selene's real control payload with no ammo, stamina or Echo charge.
     * Selene must already physically occupy the named clean mark. This does not teleport her. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Aurelion|Thermal Fracture") bool RequestFrostSetup(ASovPlayerCharacterBase* Tarrik, FString& Error);
    /** Contextual, nonlethal heat impact. Requires nearby Tarrik and visibility during a real frost window. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Aurelion|Thermal Fracture") bool RequestConfirmFracture(ASovPlayerCharacterBase* Tarrik, FString& Error);
    UFUNCTION(BlueprintPure, Category="Aurelion|Thermal Fracture") bool HasCompletedFracture(const ASovEncounterDirector* Director, const FGuid& AttemptId) const;
    /** Completed receipt for the bound director's current attempt; false while unbound. */
    UFUNCTION(BlueprintPure, Category="Aurelion|Thermal Fracture") bool HasCompletedCurrentFracture() const;
    UFUNCTION(BlueprintPure, Category="Aurelion|Thermal Fracture") FSovAurelionThermalFractureReceipt GetFractureReceipt() const { return Receipt; }
    UFUNCTION(BlueprintPure, Category="Aurelion|Thermal Fracture") float GetFractureWindowRemainingSeconds() const;
    virtual void PrepareForSave_Implementation() override;
    virtual void Load_Implementation() override;
    virtual bool LoadMissingSaveRecord() override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend struct FSovAurelionThermalTestAccess;
    struct FContext
    {
        TWeakObjectPtr<ASovEncounterDirector> Director;
        TWeakObjectPtr<ASovPlayerCharacterBase> Player;
        TWeakObjectPtr<ASovPlayerController> Controller;
        TWeakObjectPtr<ASovProtagonistCompanionCharacter> Selene;
        TWeakObjectPtr<USovCampaignStateComponent> Campaign;
        TWeakObjectPtr<USovCampaignDefinition> Mission;
        TWeakObjectPtr<UNarrativeAbilitySystemComponent> PlayerASC, SeleneASC, TargetASC;
        FGuid AttemptId;
        FName EncounterId;
        int32 PlayerReadyEpoch = 0, SeleneReadyEpoch = 0;
        uint64 Generation = 0, TransitionEpoch = 0, PlayerActorInfoEpoch = 0, SeleneActorInfoEpoch = 0, TargetActorInfoEpoch = 0;
    };
    bool CaptureContext(FContext& Out) const;
    bool IsContextCurrent(const FContext& Candidate, bool bRequireActiveSetup) const;
    bool ValidateCleanAnchor(const FContext& Candidate, FString& Error) const;
    bool HasLineOfSight(AActor* Source, AActor* Target) const;
    void BindHeroInterruptions();
    void HandleHeroInterrupt(FGameplayTag Tag, int32 Count);
    void Retire();
    void CloseWindow();
    void Unbind();
    void HandleControlApplied(UAbilitySystemComponent* ASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);
    void ExecuteFracture(FContext Captured, FGuid FrostId, FGuid HeatId);
    UFUNCTION() void HandleOwnerASCInitialized();
    UFUNCTION() void HandleEncounterState(ESovEncounterState Previous, ESovEncounterState Current);
    UFUNCTION() void HandleDamage(const FSovDamageResult& Result);
    UPROPERTY(SaveGame) FSovAurelionThermalFractureReceipt SavedReceipt;
    UPROPERTY(Transient) FSovAurelionThermalFractureReceipt Receipt;
    TWeakObjectPtr<ASovEncounterDirector> BoundDirector;
    TWeakObjectPtr<UNarrativeAbilitySystemComponent> BoundASC;
    FDelegateHandle ControlDelegate;
    struct FInterruptBinding
    { TWeakObjectPtr<UNarrativeAbilitySystemComponent> ASC; FGameplayTag Tag; FDelegateHandle Handle; };
    TArray<FInterruptBinding> InterruptBindings;
    FContext Context;
    FGuid FrostApplicationId, ObservedPayoffId;
    FActiveGameplayEffectHandle FrostHandle;
    TSet<FActiveGameplayEffectHandle> ObservedControlHandles;
    float WindowEndsAt = 0.f;
    float NextFrostSetupAt = 0.f;
    FTimerHandle PayoffTimer;
    const FGameplayEffectContext* ExpectedPayoffContext = nullptr;
    bool bPayoffPending = false, bPayoffExecuting = false, bFrostSetupExecuting = false, bConfirmExecuting = false;
    bool bObservedPoiseBreak = false, bEnding = false;
};
