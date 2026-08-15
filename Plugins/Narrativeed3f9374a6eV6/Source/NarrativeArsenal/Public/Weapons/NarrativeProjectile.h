// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "NarrativeProjectile.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProjectileTargetDataDelegate, const FGameplayAbilityTargetDataHandle&, Data);

UCLASS()
class NARRATIVEARSENAL_API ANarrativeProjectile : public AActor, public INarrativeCharacterOwner
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ANarrativeProjectile();

	virtual class ANarrativeCharacter* GetNarrativeCharacter() const override;

	//Ability Task listens to this to broadcast projectile hitting something. 
	UPROPERTY(BlueprintAssignable, Category = "Projectile")
	FProjectileTargetDataDelegate OnProjectileTargetData;

protected:

	/**Projectile should call this whenever it wants to broadcast the fact it has generated some target data. */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	virtual void SetProjectileTargetData(const FGameplayAbilityTargetDataHandle& TargetHandle);

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	


};
