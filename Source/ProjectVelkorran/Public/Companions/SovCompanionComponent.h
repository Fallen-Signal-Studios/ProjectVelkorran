// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NarrativeSavableComponent.h"
#include "SovCompanionComponent.generated.h"

class ASovCoActionAnchor;
class ASovEncounterDirector;
class ASovPlayerCharacterBase;
class UNarrativeAbilitySystemComponent;
class UNPCActivityComponent;
class USovCoActionGoal;
class USovCoActionActivity;

UENUM(BlueprintType)
enum class ESovCompanionCommandState : uint8 { Idle, MovingToAnchor, Succeeded, Failed };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovCompanionCommandChanged, ESovCompanionCommandState, State, const FString&, Reason);

/** Mission-scoped co-action coordinator over Narrative goals/activities, not a replacement companion brain. */
UCLASS(ClassGroup = (Sovereign), meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovCompanionComponent : public UActorComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()
public:
	USovCompanionComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion") FName CompanionId;
	/** Defeat during a required action fails this active encounter; no resurrection or damage immunity is invented. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion") TObjectPtr<ASovEncounterDirector> RequiredEncounter;
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
	void TryFinishArrival();
	bool TryHiddenFallback();
	bool IsFallbackHiddenFromAllPlayers(const FVector& Destination) const;
	UFUNCTION() void HandleDeath(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, bool bIsDead);
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
