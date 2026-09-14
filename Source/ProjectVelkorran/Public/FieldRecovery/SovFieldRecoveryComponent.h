// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "NarrativeSavableComponent.h"
#include "SovFieldRecoveryComponent.generated.h"
class UNarrativeAbilitySystemComponent;
class USovGameplayAbility_FieldRecovery;
class ASovTechniqueSafePoint;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovFieldRecoveryChargesChanged, int32, Charges, int32, Capacity);

/** Per-protagonist field charges live in the existing PawnRecord, independently of passive health regeneration. */
UCLASS(ClassGroup=(Sovereign), BlueprintType, meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovFieldRecoveryComponent : public UActorComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()
public:
	USovFieldRecoveryComponent();
	bool InitializeWithAbilitySystem(UNarrativeAbilitySystemComponent* AbilitySystem);
	bool IsInitialized() const;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Field Recovery", meta=(ClampMin="1",ClampMax="10")) int32 Capacity = 2;
	/** Editable prototype; the TDD specifies a fixed fraction, not its numeric amount. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Field Recovery", meta=(ClampMin="0.01",ClampMax="1")) float HealthFraction = 0.35f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Field Recovery", meta=(ClampMin="0.1",ClampMax="5")) float UseSeconds = 1.f;
	/** Default charge consumption is at completed healing. Opt in only for explicitly authored early consumption. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Field Recovery") bool bConsumeOnStart = false;
	UFUNCTION(BlueprintPure, Category="Field Recovery") int32 GetCharges() const { return Charges; }
	UFUNCTION(BlueprintPure, Category="Field Recovery") bool CanUse() const;
	UFUNCTION(BlueprintPure, Category="Field Recovery") bool IsUsing() const { return UseId.IsValid(); }
	/** A caller cannot authorize a refill with a boolean: the marked safe point verifies geometry, mission pawn and nearby threats. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Field Recovery") bool RefillAtSafePoint(ASovTechniqueSafePoint* Point, FString& Error);
	UPROPERTY(BlueprintAssignable, Category="Field Recovery") FSovFieldRecoveryChargesChanged OnChargesChanged;
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;
	virtual void Serialize(FArchive& Ar) override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	friend class USovGameplayAbility_FieldRecovery;
	friend struct FSovFieldRecoveryTestAccess;
	/** The owning client's HUD follows the authoritative count; charges are never spent or refilled here. */
	UFUNCTION() void OnRep_Charges();
	bool ValidProfile() const;
	bool HasLiveOwner() const;
	FGuid BeginUse(USovGameplayAbility_FieldRecovery* Ability);
	bool CompleteUse(USovGameplayAbility_FieldRecovery* Ability, FGuid Id, float& OutHealed);
	void CancelUse(USovGameplayAbility_FieldRecovery* Ability, FGuid Id);
	bool IsCurrentUse(const USovGameplayAbility_FieldRecovery* Ability, FGuid Id) const;
	UPROPERTY(SaveGame) int32 SavedSchemaVersion = 1;
	UPROPERTY(SaveGame, ReplicatedUsing=OnRep_Charges) int32 Charges = 2;
	UPROPERTY(SaveGame) FGameplayTag SavedProtagonist;
	TWeakObjectPtr<UNarrativeAbilitySystemComponent> ASC;
	TWeakObjectPtr<USovGameplayAbility_FieldRecovery> UsingAbility;
	FGuid UseId;
	double StartedAt = 0.;
	float CommittedDuration = 0.f;
	float CommittedFraction = 0.f;
	bool bChargedAtStart = false;
	bool bInitializedOnce = false;
	bool bStateValid = true;
	bool bMutating = false;
	uint64 StateEpoch = 0;
};
