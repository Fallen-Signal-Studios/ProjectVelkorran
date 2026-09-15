// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "GAS/SovCombatTypes.h"
#include "Resonance/SovResonanceTypes.h"
#include "SovResonanceComponent.generated.h"
class USovResonanceTargetComponent;
class USovResonanceTicket;
class USovCompanionComponent;
class USovCampaignDefinition;
class USovProtectionInterceptReceipt;
class UNarrativeAbilitySystemComponent;
class UPrimitiveComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovResonanceChanged, const FSovResonanceInteraction&, Interaction);

/** Mission-local offers and exact paired GAS ownership; there is no shared resource meter. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovResonanceComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USovResonanceComponent();
	UFUNCTION(BlueprintPure, Category="Resonance") FSovResonanceInteraction GetInteraction() const { return Interaction; }
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Resonance") bool RegisterPartner(USovCompanionComponent* Companion, FString& Reason);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Resonance") bool ConfirmOffer(FGuid InteractionId, FString& Reason);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Resonance") void CancelInteraction();
	UPROPERTY(BlueprintAssignable, Category="Resonance") FSovResonanceChanged OnInteractionChanged;
	static USovResonanceComponent* FindActive(UWorld* World);
	/**
	 * The coordinator that owns Context's Resonance: Context's own when it is a player's pawn, otherwise its
	 * companion leader's, otherwise the single standalone player's (FindActive). Callers resolve from the actor
	 * involved rather than from "the" player, so a later multi-player mode changes only this resolution.
	 * Resonance remains standalone-only.
	 */
	static USovResonanceComponent* FindForActor(UWorld* World, const AActor* Context);
	/** Native receipt path only; no Blueprint can fabricate a protection award. */
	void NotifyProtectionIntercept(USovProtectionInterceptReceipt* Receipt, const FSovDamageResult& Result);
	bool IsTicketCurrent(const USovResonanceTicket* Ticket) const;
	void NotifyParticipationEnded(const USovResonanceTicket* Ticket);
	bool IsPermitted(ESovResonanceType Type, const AActor* Target = nullptr) const;
	AActor* GetTarrik() const;
	AActor* GetSelene() const;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	friend class USovResonanceTargetComponent;
	friend struct FSovResonanceTestAccess;
	void ObserveExposure(USovResonanceTargetComponent* Target, AActor* Instigator);
	bool MakeOffer(ESovResonanceType Type, USovResonanceTargetComponent* Target, float Pressure = 0.f);
	bool ValidatePair(bool bDuringCommit, FString& Reason) const;
	bool IsContextValid() const;
	void BindASCs();
	void Finish(bool bSucceeded, const FString& Reason);
	void ReleaseOwnedParticipation();
	bool ApplyPayoff(float DeltaTime);
	void ApplyReleaseDamage(AActor* Source, const FVector& Origin, float Damage, float Radius);
	void ApplyCorridor();
	UFUNCTION() void ObserveDamage(const FSovDamageResult& Result);
	UFUNCTION() void OnRep_Interaction();
	UPROPERTY(ReplicatedUsing=OnRep_Interaction) FSovResonanceInteraction Interaction;
	UPROPERTY(Transient) TObjectPtr<USovCompanionComponent> Partner;
	UPROPERTY(Transient) TObjectPtr<USovResonanceTargetComponent> ContextTarget;
	UPROPERTY(Transient) TObjectPtr<USovCampaignDefinition> InteractionMission;
	UPROPERTY(Transient) TObjectPtr<UNarrativeAbilitySystemComponent> PlayerASC;
	UPROPERTY(Transient) TObjectPtr<UNarrativeAbilitySystemComponent> PartnerASC;
	UPROPERTY(Transient) TArray<TObjectPtr<USovResonanceTicket>> Tickets;
	FGameplayAbilitySpecHandle PlayerHandle;
	FGameplayAbilitySpecHandle PartnerHandle;
	TArray<TWeakObjectPtr<AActor>> OwnedMovementIgnores;
	TWeakObjectPtr<UPrimitiveComponent> BreachCapsule;
	TWeakObjectPtr<AActor> RoutedThreat;
	float RoutedUntil = 0.f;
	float NextAllowedTime = 0.f;
	float CommittedAt = 0.f;
	bool bOwnsAvailableTag = false;
	bool bMutation = false;
	bool bParticipantEnded = false;
	bool bPayoffStarted = false;
	TSet<FGuid> SeenTransactions;
};
