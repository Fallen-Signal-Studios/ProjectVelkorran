// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <GameplayTagContainer.h>
#include <Components/SkeletalMeshComponent.h>
#include <Engine/StreamableManager.h>
#include "CharacterCreator/CharacterCreatorAttributes.h"
#include <GroomComponent.h>
#include "AbilitySystemInterface.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Items/WeaponItem.h"
#include "NarrativeCharacterVisual.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCharacterAppearanceEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCharacterAppearancePartEvent, FGameplayTag, AppearanceSlot);


/**Seperates the appearance behavior out from NarrativeCharacter. Also handles asyncronously loading the assets before they are applied to the character. */
UCLASS(Blueprintable, BlueprintType)
class NARRATIVEARSENAL_API ANarrativeCharacterVisual : public AActor, public IAbilitySystemInterface, public INarrativeCharacterOwner
{
	GENERATED_BODY()
	
public:	

	friend class AWeaponVisual; 
	friend class ANarrativeCharacter;
	friend class ANarrativePlayerCharacter;
	// Sets default values for this actor's properties
	ANarrativeCharacterVisual();

	virtual class ANarrativeCharacter* GetNarrativeCharacter() const override;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Narrative|Components")
	TObjectPtr<class USceneComponent> CharacterVisualRoot;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void BeginPlay() override; 
	virtual void Tick(float DeltaSeconds) override; 
	virtual void Destroyed() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override; 
	virtual void OnRep_Owner() override;
	
	UFUNCTION(BlueprintNativeEvent, Category = "Character Visual")
	void HandleUpdateWields(const FWeaponWieldState& OldWieldState, const FWeaponWieldState& NewWieldState);
	virtual void HandleUpdateWields_Implementation(const FWeaponWieldState& OldWieldState, const FWeaponWieldState& NewWieldState);

	//Define what should happen to the character visual when a weapon is equipped. 
	UFUNCTION(BlueprintNativeEvent, Category = "Character Visual")
	void HandleWieldWeapon(class UWeaponItem* Weapon);
	virtual void HandleWieldWeapon_Implementation(class UWeaponItem* Weapon);

	//Define what should happen to the character visual when a weapon is equipped. 
	UFUNCTION(BlueprintNativeEvent, Category = "Character Visual")
	void HandleUnWieldWeapon(class UWeaponItem* Weapon);
	virtual void HandleUnWieldWeapon_Implementation(class UWeaponItem* Weapon);

	//Define what should happen to the character visual when a clothing item is equipped 
	UFUNCTION(BlueprintNativeEvent, Category = "Character Visual")
	void HandleEquipClothing(class UEquippableItem_Clothing* Clothing);
	virtual void HandleEquipClothing_Implementation(class UEquippableItem_Clothing* Clothing);

		//Define what should happen to the character visual when a clothing item is UnEquipped 
	UFUNCTION(BlueprintNativeEvent, Category = "Character Visual")
	void HandleUnEquipClothing(const FGameplayTag& Slot);
	virtual void HandleUnEquipClothing_Implementation(const FGameplayTag& Slot);

	//Define how the character visual should update when we switch between third/first person perspective. 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Character Visual")
	void HandlePerspectiveUpdate(const bool bIsFirstPerson);
	virtual void HandlePerspectiveUpdate_Implementation(const bool bIsFirstPerson);

	//Define how we want to hide the characters upper body when in first person mode 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Character Visual")
	void HideUpperBody(const bool bWantsHide);
	virtual void HideUpperBody_Implementation(const bool bWantsHide);

	//Initialize from character and appearance asset - will replicate appearance to clients so they sync appearance 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Character Visual")
	void InitializeFromCharacterAndAppearance(class ANarrativeCharacter* NarrativeCharacter, UCharacterAppearance* Appearance);
	virtual void InitializeFromCharacterAndAppearance_Implementation(class ANarrativeCharacter* NarrativeCharacter, UCharacterAppearance* Appearance);
	
	//Initialize from character and raw attribute set - useful for character creator data. Not replicated yet. 
	UFUNCTION(BlueprintNativeEvent, Category = "Character Visual")
	void InitializeFromCharacterAndAttributes(class ANarrativeCharacter* NarrativeCharacter, const FCharacterCreatorAttributeSet& Attributes);
	virtual void InitializeFromCharacterAndAttributes_Implementation(class ANarrativeCharacter* NarrativeCharacter, const FCharacterCreatorAttributeSet& Attributes);

		//Returns our weapon visual. This just a generic actor that each weapon item defines, and holds the weapons static mesh and FX assets. 
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	class AWeaponVisual* GetWeaponVisual(UPARAM(meta = (Categories = "Narrative.Equipment.Slot.Weapon"))const FGameplayTag& WeaponSlot) const;

	virtual void SetGroomAppearance(FGameplayTag Slot, const FCharacterCreatorAttribute_Groom& GroomData);
	virtual void SetMeshAppearance(FGameplayTag Slot, const FCharacterCreatorAttribute_Mesh& MeshData);
	virtual void ResetMeshToBaseAppearance(FGameplayTag Slot);

	UPROPERTY(BlueprintAssignable, Category = "Character Visual")
	FCharacterAppearanceEvent OnBaseAppearanceApplied;

	/** Fired whenever a modular mesh or groom slot finishes applying or changing. */
	UPROPERTY(BlueprintAssignable, Category = "Character Visual")
	FCharacterAppearancePartEvent OnAppearancePartChanged;

	//Called after base meshes are set
	UFUNCTION(BlueprintNativeEvent, Category = "Character Visual")
	void BaseAppearanceApplied();
	virtual void BaseAppearanceApplied_Implementation();

	bool bBaseAppearanceLoaded;

	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacterVisual")
	virtual void SetAnimBPOverride(TSubclassOf<class UAnimInstance> NewAnimBP);

	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacterVisual")
	virtual void ClearAnimBPOverride();

	//Return the mesh at the provided slot. 
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	USkeletalMeshComponent* GetSkeletalMeshComponent(UPARAM(meta = (Categories = "Narrative.Equipment.Slot.Mesh"))const FGameplayTag& Slot) const;

	//Return the static mesh at the provided slot. 
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	UStaticMeshComponent* GetStaticMeshComponent(UPARAM(meta = (Categories = "Narrative.Equipment.Slot.Mesh"))const FGameplayTag& Slot) const;

	//Check whether the visual has any assets its trying to load right now to apply  for
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	bool HasLoadHandles() const;

protected:

	UFUNCTION()
	virtual void OnBaseMeshesReady();

	UFUNCTION()
	virtual void OnMeshAppearanceReady(FGameplayTag Slot, FCharacterCreatorAttribute_Mesh MeshData);

	UFUNCTION()
	virtual void OnGroomAppearanceReady(FGameplayTag Slot, FCharacterCreatorAttribute_Groom GroomData);

	UFUNCTION()
	virtual void OnWeaponVisualClassReady(class UWeaponItem* WeaponItem);

	//More generic function for getting groom, SM, SK, etc. 
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	UMeshComponent* GetMeshComponent(UPARAM(meta = (Categories = "Narrative.Equipment.Slot"))const FGameplayTag& Slot);

	USkeletalMeshComponent* GetOrCreateMeshComponent(const FGameplayTag& Tag);
	UStaticMeshComponent* GetOrCreateStaticMeshComponent(const FGameplayTag& Tag);
	UGroomComponent* GetOrCreateGroomComponent(const FGameplayTag& Tag);

	/**Our appearance asset */
	UPROPERTY(ReplicatedUsing= OnRep_AppearanceAsset)
	TObjectPtr<class UCharacterAppearance> AppearanceAsset;
	
	/** We use this to ensure we don't re-apply the same appearance multiple times. AppearanceAsset
	 * can be valid but the appearance won't be applied yet if we're waiting for OnRep_Owner. 
	 */
	UPROPERTY()
	TObjectPtr<class UCharacterAppearance> AppliedAppearanceAsset;
	
	/**Our default appearance attribute set*/
	UPROPERTY()
	FCharacterCreatorAttributeSet AppearanceAttributeSet;

	/**Points to Character*/
	UPROPERTY(BlueprintReadOnly, Category = "Character Visual")
	TObjectPtr<class ANarrativeCharacter> OwnerCharacter;

	/**We store our async load requests in here*/
	TSharedPtr<FStreamableHandle> BaseAppearanceLoadHandle; 

	TMap<FGameplayTag, TSharedPtr<FStreamableHandle>> GroomLoadHandles;
	TMap<FGameplayTag, TSharedPtr<FStreamableHandle>> MeshLoadHandles;
	TMap<FGameplayTag, TSharedPtr<FStreamableHandle>> WeaponLoadHandles;

	//We need to defer applying any meshes or weapons until our base appearance is loaded so base appearance doesn't override them - those go in here 
	UPROPERTY(VisibleAnywhere, Category = "Character Visual")
	TArray<TObjectPtr<UEquippableItem_Clothing>> DeferredMeshes;

	UPROPERTY(VisibleAnywhere, Category = "Character Visual")
	TArray<TObjectPtr<UWeaponItem>> DeferredWeapons;

	//You generally want the upper half of the body hiding in first person. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Visual | Perspective")
	bool bHideUpperBodyInFirstPerson;

	//In order to hide upper body, we'll hide all bones from this bone upwards - generally this is a spine bone. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Visual | Perspective")
	FName UpperBodyHideBone;

	/**The skeletal meshes added to the visual */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Visual")
	TMap<FGameplayTag, class USkeletalMeshComponent*> MeshComponents;

	/**The static meshes added to the visual */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Visual")
	TMap<FGameplayTag, class UStaticMeshComponent*> StaticMeshComponents;

	/**The grooms added to this visual. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Visual")
	TMap<FGameplayTag, class UGroomComponent*> GroomComponents;

	/** We use this to track if a slot is hiding other slots. ie Mesh.Helmet hides -> {Groom.Hair, Groom.Beard} */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Visual")
	TMap<FGameplayTag, FGameplayTagContainer> CurrentHides;

	/**Spawned weapon visuals, can be accessed via map */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Character Visual")
	TMap<FGameplayTag, TObjectPtr<class AWeaponVisual>> SpawnedWeaponVisuals;

protected:

	friend class UWeaponItem;

	//Spawn a weapon visual, attach to us 
	UFUNCTION(BlueprintCallable, Category = "Character Visual")
	bool AddWeaponVisual(class UWeaponItem* WeaponItem);

	UFUNCTION(BlueprintCallable, Category = "Character Visual")
	void AttachWeaponVisual(class UWeaponItem* WeaponItem, const FGameplayTag& EquipSlot, const FGameplayTag& WieldSlot);

	/** Low-level socket handoff. This deliberately bypasses weapon transition requests. */
	bool CommitWeaponVisualAttachment(class UWeaponItem* WeaponItem, const FGameplayTag& EquipSlot, const FGameplayTag& WieldSlot);

	/** Applies only character animation layers, without replaying equipment or attachment state. */
	void ApplyWieldAnimationLayers(const FWeaponWieldState& WieldState);

	/** Prevents each asynchronously loaded weapon visual from replaying the same saved wield restore. */
	bool bHasRestoredSavedWieldState = false;

	UFUNCTION(BlueprintCallable, Category = "Character Visual")
	void RemoveWeaponVisual(UPARAM(meta = (Categories = "Narrative.Equipment.Slot.Weapon"))const FGameplayTag& WeaponSlot);

	bool IsLocallyControlled();

	bool Is1PMeshTag(const FGameplayTag& MeshSlot) const;

	UFUNCTION()
	virtual void OnRep_AppearanceAsset();

public:

	//The mesh that clothing pieces will follow if desired. Returns Character.GetMesh() by default. 
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Character Visual")
	class USkeletalMeshComponent* GetLeaderMesh();
	virtual class USkeletalMeshComponent* GetLeaderMesh_Implementation();

	//Return the main mesh - basically Character->GetMesh()
	UFUNCTION(BlueprintPure, Category = "Character Visual", meta = (BlueprintThreadSafe))
	class USkeletalMeshComponent* GetMainMesh();
	
	//Return the face mesh - can be null, your character doesn't require a face mesh 
	UFUNCTION(BlueprintPure, Category = "Character Visual")
	class USkeletalMeshComponent* GetFaceMesh();

	//Return the local mesh - the local 1P mesh arms/hands etc follow. 
	UFUNCTION(BlueprintPure, Category = "Character Visual", meta = (BlueprintThreadSafe))
	class USkeletalMeshComponent* GetLocalMesh();

	//Return all character meshes./
	UFUNCTION(BlueprintPure, Category = "Character Visual", meta = (BlueprintThreadSafe))
	void GetAllMeshes(TArray<class USkeletalMeshComponent*>& OutMeshes);

	//Return the local mesh as well as 1P hands/arms./
	UFUNCTION(BlueprintPure, Category = "Character Visual", meta = (BlueprintThreadSafe))
	void GetAllLocalMeshes(TArray<class UMeshComponent*>& OutLocalMeshes);

	//Return the body mesh - metahumans have one of these, and this is also often used for manny etc where we want to keep base mesh invisible. 
	UFUNCTION(BlueprintPure, Category = "Character Visual", meta = (BlueprintThreadSafe))
	class USkeletalMeshComponent* GetBodyMesh();

	//Returns Head transform in world space
	UFUNCTION(BlueprintPure, Category = "Character Visual", meta = (BlueprintThreadSafe))
	FTransform GetHeadTransformWS();
	
	//Return all meshes considered to be "head meshes". These meshes will be hidden when the camera clips into the head, typically because we're in first person mode. 
	UFUNCTION(BlueprintPure, Category = "Character Visual")
	void GetHeadMeshes(TArray<class UMeshComponent*>& OutHeadMeshes) const;

	//Get owner character
	UFUNCTION(BlueprintPure, Category = "Character Visual", meta = (BlueprintThreadSafe))
	class ANarrativeCharacter* GetOwnerCharacter();

	//Get character visual attributes 
	UFUNCTION(BlueprintPure, Category = "Character Visual", meta = (BlueprintThreadSafe))
	FCharacterCreatorAttributeSet GetCreatorAttributes() const;
};
