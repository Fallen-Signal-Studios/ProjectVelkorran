// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "SovCinderRequiemLine.generated.h"
class UAbilitySystemComponent;
class UGameplayEffect;
/** Immutable paid Requiem payload. It survives ability recovery and weapon changes. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovCinderRequiemLine : public AActor
{
	GENERATED_BODY()
public:
	ASovCinderRequiemLine();
	void InitializeLine(UAbilitySystemComponent* Source, AActor* InstigatorActor,
		const FGameplayEffectContextHandle& Context, FGameplayTag AbilityTag,
		const FVector& Start, const FVector& End, float Spacing, float Interval, float Radius,
		TSubclassOf<UGameplayEffect> DamageEffect, TSubclassOf<UGameplayEffect> BurnEffect,
		float Damage, float Poise, float BurnDamage, float BurnDuration);
	void StartLine();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UFUNCTION(BlueprintImplementableEvent, Category="Sovereign|Cinderline Requiem")
	void ReceiveLineDetonation(FVector Origin, float Radius, int32 NodeIndex);
private:
	void DetonateNext();
	UFUNCTION() void OnRep_LastDetonatedNode();
	UPROPERTY(ReplicatedUsing=OnRep_LastDetonatedNode) TArray<FVector_NetQuantize> DetonationPoints;
	UPROPERTY(Replicated) float BlastRadius = 220.f;
	UPROPERTY(ReplicatedUsing=OnRep_LastDetonatedNode) int32 LastDetonatedNode = -1;
	UPROPERTY() TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
	UPROPERTY() TWeakObjectPtr<AActor> SourceActor;
	UPROPERTY() TSubclassOf<UGameplayEffect> DamageEffectClass;
	UPROPERTY() TSubclassOf<UGameplayEffect> BurnEffectClass;
	UPROPERTY() FGameplayEffectContextHandle SourceContext;
	FGameplayTag EchoAbilityTag;
	TSet<TWeakObjectPtr<UAbilitySystemComponent>> DamagedTargets;
	TSet<TWeakObjectPtr<AActor>> DamagedScenery;
	FTimerHandle DetonationTimer;
	float DetonationInterval = 0.035f;
	float BaseDamage = 60.f;
	float PoiseDamage = 100.f;
	float BurnDamagePerTick = 5.f;
	float BurnSeconds = 4.f;
	int32 NextNode = 0;
	int32 LastPresentedNode = -1;
	bool bInitialized = false;
	bool bStarted = false;
};
