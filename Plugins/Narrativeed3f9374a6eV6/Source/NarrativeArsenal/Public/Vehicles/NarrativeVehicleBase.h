// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeVehicleBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSeedSet, int32, NewSeed);

/**Base class for all pawn-based vehicles in Narrative Pro. Some mounts such as horses extend
NPCCharacter instead, but for cars, helis, water vehidles etc this is the base you'll want. 

Vehicles also have ASCs to handle taking damage, running effects, etc. */
UCLASS(abstract, BlueprintType)
class NARRATIVEARSENAL_API ANarrativeVehicleBase : public APawn, public INarrativeCharacterOwner, public IAbilitySystemInterface, public IGameplayTagAssetInterface, public INarrativeTeamAgentInterface, public INarrativeImpactInterface, public INarrativeSavableActor
{
	GENERATED_BODY()

public:

	// Sets default values for this pawn's properties
	ANarrativeVehicleBase(const FObjectInitializer& ObjectInitializer);

	//INTERFACES 
	class ANarrativeCharacter* GetNarrativeCharacter() const override;
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void HandleVehicleImpact_Implementation(class ANarrativeVehicleBase* Vehicle, UPrimitiveComponent* OverlappedComponent, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
	
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;

	ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	virtual FGameplayTagContainer GetFactions() const override;

	virtual FGuid GetActorGUID_Implementation() const override;
	
	virtual void BeginPlay() override; 
	virtual void Destroyed() override; 

	virtual void PossessedBy(AController* NewController) override; 
	virtual void UnPossessed() override; 

	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeVehicle|Attributes")
	float GetHealth() const;

	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeVehicle|Attributes")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeVehicle|Attributes")
	virtual int32 GetVehicleLevel() const;

	//Set the characters random seed, useful if you need to override the default one that gets assigned.
	void SetRandomSeed(const int32 NewSeed);

	// Mass Actor Management
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeVehicle|Mass")
	void SetManagedByMass(bool bManagedByMass);

	// Deal vehicle damage to some actor - this can be called whenever needed
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeVehicle")
	virtual void DealVehicleDamage(class UAbilitySystemComponent* DamageASC, const float DamageAmount, const FHitResult& Hit);

protected:

	/**  The main skeletal mesh associated with this Vehicle */
	UPROPERTY(Category = Vehicle, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USkeletalMeshComponent> Mesh;

	/** A hidden slightly enlarged version of vehicle mesh we generate overlaps and use for vehicle damage.  */
	UPROPERTY(Category = Vehicle, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USkeletalMeshComponent> ImpactMesh;

	/** Nav modifier for this vehicle, helps NPCs etc path around the vehicle  */
	UPROPERTY(Category = Vehicle, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UNavModifierComponent> VehicleNavModifier;

	/** ASC for the vehicle to allow it to support health, death, etc.  */
	UPROPERTY(Category = Vehicle, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UNarrativeAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Category = Vehicle, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UNarrativeAttributeSetBase> AttributeSetBase;

	// Contains some default abilities to grant, attributes, etc. 
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Abilities")
	TObjectPtr<class UAbilityConfiguration> AbilityConfiguration;

	// Damage effect to apply when vehicle impacts a character
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Damage")
	TSubclassOf<class UGameplayEffect> VehicleDamageEffect; 

	// This is used as a range we map our velocity onto, with 0.2-1.0 of max health being dealth to the vehicle as a result of the impact. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Damage")
	FVector2D VehicleImpactSelfDamage; 

	// Used to save this vehicle to disk. Can be zero'ed if you don't want the vehicle saving. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saving")
	FGuid VehicleSaveGUID;

	// This vehicles random seed, generated once and synced on client-server. Can be used for anything this character needs. 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Narrative Character")
	int32 VehicleRandomSeed;
	
	virtual void InitializeVehicleASC();

	//Default abilities for vehicle - not sure if these will technically be supported, more likely we'll just route abilities through the owning character. 
	virtual void AddDefaultAbilities();
	virtual void InitializeAttributes();
	virtual void AddStartupEffects();

	//Handle what should happen when the vehicle "dies"
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	void HandleDeath(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead);
	virtual void HandleDeath_Implementation(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead);
	
	//Handle what should happen when the base vehicle mesh hits something. 
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	void OnVehicleMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	virtual void OnVehicleMeshHit_Implementation(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	//Handle what should happen when the vehicle collision mesh overlaps something. This mesh doesn't stop the vehicle moving
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	void OnCollisionMeshOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult);
	virtual void OnCollisionMeshOverlap_Implementation(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult);
	
public:
	
	// This is used as a range we map our velocity onto, with 0.2-1.0 of max health being dealt to the character we hit as a result of the impact. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Damage")
	FVector2D VehicleImpactCharacterDamage;
	
	// We read this curve using impact normal size as the input and curve provides damage to deal 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Damage")
	TObjectPtr<UCurveFloat> VehicleImpactObjectDamageCurve;

	/** Name of the MeshComponent. Use this name if you want to prevent creation of the component (with ObjectInitializer.DoNotCreateDefaultSubobject). */
	static FName VehicleMeshComponentName;

	UPROPERTY(BlueprintAssignable, Category = "Narrative|VehicleSeed")
	FSeedSet OnSeedSet;

	/** Returns Mesh subobject **/
	class USkeletalMeshComponent* GetMesh() const { return Mesh; }
	class USkeletalMeshComponent* GetOverlapMesh() const { return ImpactMesh; }

	virtual void SetVehicleSaveGuid(const FGuid& NewGUID);
};
