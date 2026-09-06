// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Components/SovStatusComponent.h"
#include "Effects/SovGameplayEffect_Status.h"
#include "GameFramework/Actor.h"
#include "NarrativeSavableActor.h"
#include "SovStatusCheckpointTestFixtures.generated.h"

class UNarrativeAbilitySystemComponent;
class UNarrativeAttributeSetBase;

UCLASS(Transient, NotBlueprintable)
class USovStatusCheckpointSafePeriodicEffect : public USovGameplayEffect_Status
{
	GENERATED_BODY()
public:
	USovStatusCheckpointSafePeriodicEffect();
};

UCLASS(Transient, NotBlueprintable)
class USovStatusCheckpointUnsafePeriodicEffect : public USovGameplayEffect_Status
{
	GENERATED_BODY()
public:
	USovStatusCheckpointUnsafePeriodicEffect();
};

UCLASS(Transient, NotBlueprintable)
class USovStatusCheckpointAggregateEffect : public USovGameplayEffect_Status
{
	GENERATED_BODY()
public:
	USovStatusCheckpointAggregateEffect();
};

UCLASS(Transient, NotBlueprintable)
class USovStatusCheckpointContinuousResourceEffect : public USovGameplayEffect_Status
{
	GENERATED_BODY()
public:
	USovStatusCheckpointContinuousResourceEffect();
};

/** Adds authored test definitions before ASC readiness, without exposing production internals. */
UCLASS(Transient, NotBlueprintable)
class USovStatusCheckpointTestComponent : public USovStatusComponent
{
	GENERATED_BODY()
public:
	void AddDefinitionOverride(USovStatusDefinition* Definition) { StatusDefinitionOverrides.Add(Definition); }
};

/** Content-free save participant. Uses the production ASC, attributes, status owner and save interface. */
UCLASS(Transient, NotBlueprintable)
class ASovStatusCheckpointTestActor : public AActor, public IAbilitySystemInterface, public INarrativeSavableActor
{
	GENERATED_BODY()
public:
	ASovStatusCheckpointTestActor();
	void InitializeCombat(bool bInitializeStatus = true);
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual FGuid GetActorGUID_Implementation() const override { return StableId; }
	virtual void SetActorGUID_Implementation(const FGuid& Value) override { StableId = Value; }
	virtual bool ShouldRespawn_Implementation() const override { return false; }
	virtual void Load_Implementation() override { ++ActorLoadCalls; }

	UPROPERTY() TObjectPtr<UNarrativeAbilitySystemComponent> ASC;
	UPROPERTY() TObjectPtr<UNarrativeAttributeSetBase> Attributes;
	UPROPERTY() TObjectPtr<USovStatusCheckpointTestComponent> Status;
	UPROPERTY(SaveGame) int32 SavedMarker = 17;
	FGuid StableId = FGuid::NewGuid();
	int32 ActorLoadCalls = 0;
};

/** Real dynamic delegate observation, including non-BeginPlay EditorContext fixtures. */
UCLASS(Transient, NotBlueprintable)
class USovStatusCheckpointObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 RestoredCount = 0;
	int32 ExpiredCount = 0;
	TArray<FGameplayTag> RestoredTags;
	UPROPERTY() TObjectPtr<UNarrativeAbilitySystemComponent> ASC;
	UPROPERTY() TObjectPtr<AActor> OwnerActor;
	UPROPERTY() TObjectPtr<AActor> ReplacementAvatar;
	bool bReplaceOnFirstRestore = false;
	UFUNCTION(CallInEditor) void OnStatusChanged(FGameplayTag RequestTag, FGameplayTag StateTag,
		ESovStatusChangeReason Reason, int32 StackCount, AActor* SourceActor);
};
