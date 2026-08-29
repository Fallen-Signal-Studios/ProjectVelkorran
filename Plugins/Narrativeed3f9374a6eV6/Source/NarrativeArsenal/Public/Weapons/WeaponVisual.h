// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapons/WeaponAnimPose.h"
#include "GameplayTagContainer.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Templates/SubclassOf.h"
#include "Animation/AnimInstance.h"
#include "WeaponVisual.generated.h"

/*
 * Since lots of members are required to safely attach weapon once it reps to client, we rep them all in one go using a struct,
 * which is guaranteed to have everything rep in 1 go rather than piecemeal which gets a bit hacky. 
 */
USTRUCT(BlueprintType)
struct FWeaponVisualAttachState
{
	GENERATED_BODY()

	FWeaponVisualAttachState()
	{
		WeaponOwner = nullptr;
		VisualOwner = nullptr;
		CharOwner = nullptr; 
		EquippedSlot = FGameplayTag();
		WieldedSlot = FGameplayTag();
	};
	
	//The slot that the weapon visual is equipped into
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	FGameplayTag EquippedSlot;

	//The slot that the weapon visual is wielded into
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	FGameplayTag WieldedSlot;

	//The weapon item that created this weapon visual when it was equipped 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	class UWeaponItem* WeaponOwner;
	
	//The character visual that created this weapon visual when it was equipped. We replicate this directly because charactervisual may not be available yet from owner char on clients. This guarantees it is. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = "Weapon Visual")
	class ANarrativeCharacter* CharOwner;
	
	//The character visual that created this weapon visual when it was equipped. We replicate this directly because charactervisual may not be available yet from owner char on clients. This guarantees it is. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite,  Category = "Weapon Visual")
	class ANarrativeCharacterVisual* VisualOwner;

	bool CanAttach() const;
	
};

UCLASS()
class NARRATIVEARSENAL_API AWeaponVisual : public AActor, public INarrativeCharacterOwner
{
	GENERATED_BODY()
	
public:	

	friend class ANarrativeCharacterVisual;

	// Sets default values for this actor's properties
	AWeaponVisual(const FObjectInitializer& ObjectInitializer);

	virtual class ANarrativeCharacter* GetNarrativeCharacter() const override;
	virtual void BeginPlay() override;
	virtual void Destroyed() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override; 

	/*Define what anim overlay the weapon should apply.This lets us select a different overlay for shield, dual wield, etc by overriding this.
	
	*@param WeaponsToEquip an array with all the weapons being equipped - this is essential because the anim overlay we want will vary based on what weapon combos we're using ie Sword/Shield, Sword/Sword, Pistol/Pistol, etc. 
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Character Visual")
	TSubclassOf<class UNarrativeAnimInstance> GetWeaponOverlayLayer(bool bFirstPersonMesh);
	virtual TSubclassOf<class UNarrativeAnimInstance> GetWeaponOverlayLayer_Implementation(bool bFirstPersonMesh);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
	class USkeletalMeshComponent* WeaponMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
	class USkeletalMeshComponent* LocalWeaponMesh;



protected:

	/** Anim BP we'll apply to the owner when the weapon is unholstered. Override GetWeaponOverlay() if you need a dynamic value. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Weapon Visuals")
	TSubclassOf<class UNarrativeAnimInstance> DefaultWeaponAnimLayer;

	/** Anim BP we'll apply to the owner when the weapon is unholstered in dual wielding configuration.  */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Weapon Visuals")
	TSubclassOf<class UNarrativeAnimInstance> DualWieldWeaponAnimLayer;

	/** Anim BP we'll apply to the 1P owner when the weapon is equipped. If not set DefaultWeaponAnimLayer will be used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Weapon Visuals")
	TSubclassOf<class UNarrativeAnimInstance> Weapon1PAnimLayer;

		/** Anim BP we'll apply to the 1P owner when the weapon is equipped. If not set DefaultWeaponAnimLayer will be used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Weapon Visuals")
	TSubclassOf<class UNarrativeAnimInstance> DualWieldWeapon1PAnimLayer;

	/** Form specific overrides for WeaponAnimLayer */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Weapon Visuals", meta = (ForceInlineRow, Categories = "Narrative.CharacterCreator.Forms"))
	TMap<FGameplayTag, TSubclassOf<class UNarrativeAnimInstance>> FormSpecificLayers;

	// Collection of cached damage state data. The key is the SourceObject of the AnimNotifyEventReference and the value is the array of elements cached for sweep checks.
	TMap<const FAnimNotifyEvent*, FDamageStateDataContainer> CachedDamageStateData;
	
	// Collection of all actors we've hit so far, so we can enforce 1 hit per actor.  
	UPROPERTY()
	TArray<TObjectPtr<AActor>> CachedHitActors; 

	// Cached attachments meshes for attachments that are added to the weapon - BPReadWrite so we can suport having "Default" attachments, like IronSights. 
	UPROPERTY(BlueprintReadWrite, Category = "Attachments")
	TMap<FGameplayTag, class UStaticMeshComponent*> AttachmentMeshComps;

	// We use a second map for local attachments, that are only for local player ie first person 
	UPROPERTY(BlueprintReadWrite, Category = "Attachments")
	TMap<FGameplayTag, class UStaticMeshComponent*> LocalAttachmentMeshComps;

	// Attachment meshes will have their mesh set to this if no attachment is equipped in that slot
	UPROPERTY(BlueprintReadWrite, Category = "Attachments")
	TMap<FGameplayTag, class UStaticMesh*> AttachmentMeshDefaultMeshes;

	// The latest notify event that will be used for collision checks
	UPROPERTY()
	FAnimNotifyEventReference CurrentNotifyEvent;

	UPROPERTY()
	TArray<FWeaponCollisionData> CollisionData;

	//Register a default attachment on the weapon, like ironsights. 
	UFUNCTION(BlueprintCallable, Category = "Attachments")
	void RegisterDefaultAttachment(const FGameplayTag& Slot, class UStaticMeshComponent* Mesh, class UStaticMeshComponent* LocalMesh);

	//Define how the weapon visual should update when we switch between third/first person perspective. 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Character Visual")
	void HandlePerspectiveUpdate(const bool bIsFirstPerson);
	virtual void HandlePerspectiveUpdate_Implementation(const bool bIsFirstPerson);

public:

	//Called when weapon is wielded. 
	virtual void OnWielded();

	//Called when weapon is holstered.
	virtual void OnHolstered();

	/**
	 * Gives specialized weapon visuals a chance to stage a physical attachment
	 * change. Returning true means the visual owns the request and will call
	 * CommitDeferredAttachment when its authoritative handoff is reached.
	 * Ordinary weapon visuals return false and retain Narrative's immediate path.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Attachments")
	bool HandleAttachmentRequest(
		const FGameplayTag& EquipSlot,
		const FGameplayTag& TargetWieldSlot);
	virtual bool HandleAttachmentRequest_Implementation(
		const FGameplayTag& EquipSlot,
		const FGameplayTag& TargetWieldSlot);

	//Called when weapon is wielded. 
	UFUNCTION(BlueprintImplementableEvent, Category = "Attachments")
	void BPHandleWield();

	//Called when weapon is holstered.
	UFUNCTION(BlueprintImplementableEvent, Category = "Attachments")
	void BPHandleHolster();

	//Called when weapon is attached successfully to owner. 
	UFUNCTION(BlueprintNativeEvent, Category = "Attachments")
	void HandleAttachedToOwner();
	virtual void HandleAttachedToOwner_Implementation();
	
	UFUNCTION(BlueprintNativeEvent, Category = "Attachments")
	void HandleAddAttachment(class UWeaponAttachmentItem* Attachment, const FWeaponAttachmentSlotConfig& WeaponSlotConfig);

	UFUNCTION(BlueprintNativeEvent, Category = "Attachments")
	void HandleRemoveAttachment(class UWeaponAttachmentItem* Attachment);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual")
	class ANarrativeCharacter* CharacterOwner;

	//Attach state of the weapon. This contains all the info needed to attach the weapon. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_AttachState, Category = "Weapon Visual")
	FWeaponVisualAttachState AttachState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Visual Debug")
	bool bAttachedSuccesfully;
	
	//The weapon item that created this weapon visual when it was equipped 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_WeaponOwner, Category = "Weapon Visual")
	class UWeaponItem* WeaponOwner;

	//The character visual that created this weapon visual when it was equipped. We replicate this directly because charactervisual may not be available yet from owner char on clients. This guarantees it is. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_VisualOwner, Category = "Weapon Visual")
	class ANarrativeCharacterVisual* VisualOwner;

	//Allows us to use the weapons geometry in some way to sweep for hits. 
	UFUNCTION(BlueprintCallable, meta=(AutoCreateRefTerm="ActorsToIgnore"), Category = "Weapon Visual")
	bool SweepForHits(const FVector& Start, const FVector& End, const FQuat& Rot, const FVector& CapsuleSize, TArray<FHitResult>& OutHits);

	// Caches animation socket data for retrieval at a later time. 
	UFUNCTION(BlueprintCallable, meta=(AutoCreateRefTerm="SocketName"), Category = "Weapon Visual | Attack Caching")
	void CacheAnimationTransform(const FAnimNotifyEventReference& AnimNotifyEventRef);

	// Performs a collision sweep on current and previous animation data that has not been calculated yet. This ensures that collision is accurate when at low framerate.
	UFUNCTION(BlueprintCallable, Category = "Weapon Visual | Attack Caching")
	void PerformCollisionCheck(TArray<FHitResult>& OutHits);

	// Cleans up attack data so that it is ready to be used again
	UFUNCTION(BlueprintCallable, Category = "Weapon Visual | Attack Caching")
	void CleanupAttackData();
	
	/**
	 * Caches collision data so that we can query collision while performing attacks.
	 * @param bForceUpdate Whether we want to forcefully generate collision data even if it was already done
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon Visual | Attack Caching")
	void CacheCollisionData(bool bForceUpdate);

	// Fetches capsule primitives that will be considered for collision during sweep melee attacks
	// @note Primitives that do not have capsule collision geometry will be ignored!
	UFUNCTION(BlueprintNativeEvent, Category = "Weapon Visual | Attack Caching")
	TArray<UPrimitiveComponent*> GetCollidingPrimitives();

	UFUNCTION(BlueprintPure, Category = "Weapon Visual")
	USkeletalMeshComponent* GetRelevantWeaponMesh() const;

	UFUNCTION(BlueprintPure, Category = "Weapon Visual")
	TArray<USkeletalMeshComponent*> GetWeaponMeshes() const;

protected: 

	/** Commit a previously deferred socket handoff without re-entering the request hook. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Attachments", meta = (BlueprintProtected = "true"))
	bool CommitDeferredAttachment(
		const FGameplayTag& EquipSlot,
		const FGameplayTag& TargetWieldSlot);

	/** Last physical attachment applied on this machine. Used to suppress duplicate callbacks. */
	FGameplayTag AppliedWieldSlot;
	bool bHasAppliedAttachment = false;

	UFUNCTION()
	virtual void UpdateWeaponAttachment();

	UFUNCTION()
	virtual void ApplyAttachState();
	
	UFUNCTION()
	virtual void OnRep_AttachState();
	
	UFUNCTION()
	virtual void OnRep_WeaponOwner();
	
	UFUNCTION()
	virtual void OnRep_VisualOwner();

	virtual void OnRep_Owner() override;
};
