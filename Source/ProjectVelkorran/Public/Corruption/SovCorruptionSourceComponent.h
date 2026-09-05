// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SovCorruptionComponent.h"
#include "GAS/SovCombatTypes.h"
#include "NarrativeSavableComponent.h"
#include "SovCorruptionSourceComponent.generated.h"
class UNarrativeAbilitySystemComponent;
class USovCommandLinkComponent;

/** Enemy hit, existing command-link and contaminated-ally producers. No public amount-award entry point. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovCorruptionSourceComponent : public UActorComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()
public:
	USovCorruptionSourceComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption") TObjectPtr<USovCorruptionProfile> Profile;
	bool ValidateContact(AActor* Target, float& OutFalloff) const;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void Load_Implementation() override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	friend class USovCorruptionComponent;
	friend class ASovCorruptionSourceVolume;
	friend class USovCorruptionInteractableComponent;
	friend struct FSovCorruptionTestAccess;
	bool ValidateSpatialTarget(AActor* Target, float& OutFalloff) const;
	bool IsResolvedFor(AActor* Target) const;
	bool ResolveRemedy(AActor* Player, ESovCorruptionEscape Remedy);
	void BindOwner();
	void ReleaseContacts();
	void ResolveAll(ESovCorruptionEscape Remedy);
	UFUNCTION() void HandleDamageAsSource(const FSovDamageResult& Result);
	UFUNCTION() void HandleDamageAsTarget(const FSovDamageResult& Result);
	UFUNCTION() void HandleDeathState(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, bool bIsDead);
	UPROPERTY(SaveGame) TSet<FName> ResolvedMissions;
	TWeakObjectPtr<UNarrativeAbilitySystemComponent> BoundASC;
	TMap<TWeakObjectPtr<USovCorruptionComponent>, FSovCorruptionSourceHandle> Contacts;
	TSet<FGuid> SeenTransactions;
	TArray<FGuid> TransactionOrder;
	TWeakObjectPtr<USovCommandLinkComponent> ObservedLink;
	FGuid ObservedLinkInstance;
	uint64 DamageSequence = 0;
	bool bEnding = false;
};
