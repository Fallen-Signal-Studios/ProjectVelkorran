// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayEffectTypes.h"
#include "NarrativeCharacter.h"
#include "GAS/AttackComboAnimSet.h"
#include "Animation/PoseSnapshot.h"
#include "NarrativeAnimInstance.generated.h"



/**
 * Narrative anim instance class 
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:

	UNarrativeAnimInstance();

	//Bind the ASC. Will automatically retry again every 0.5s until binding succeeds. 
	UFUNCTION(BlueprintCallable, Category = "Narrative Anim Instance")
	void BindASC();

	//Search for an animset on the anim instance via its tag. Return AnimSet and boolean saying whether animset was found
	// @param bSearchLinkedLayers whether to ask our linked layer if it has the anim in its anim set, ie Pistol anim layer has a special grenade throw anim etc. 
	UFUNCTION(BlueprintPure, Category = "Anim Sets")
	UNarrativeAnimSet* GetAnimSet(UPARAM(meta = (Categories = "Narrative.Anim.AnimSets"))const FGameplayTag& AnimSetTag, const bool bSearchedLinkedLayers, bool& bOutFoundAnimSet);

	//Return the main character mesh. This works even if the AnimInstance is applied to a character visual instead of a character.
	UFUNCTION(BlueprintPure, Category = "Anim Sets", meta = (BlueprintThreadSafe))
	USkeletalMeshComponent* GetCharacterMesh() const;

	//Return the owning character
	UFUNCTION(BlueprintPure, Category = "Anim Sets", meta = (BlueprintThreadSafe))
	ANarrativeCharacter* GetCharacterRef() const;
	
	//Return the owning character
	UFUNCTION(BlueprintPure, Category = "Anim Sets", meta = (BlueprintThreadSafe))
	ANarrativeCharacterVisual* GetCharacterVisualRef() const;

	//Return the owning characters main ABP - the one on character mesh
	UFUNCTION(BlueprintPure, Category = "Anim Sets", meta = (BlueprintThreadSafe))
	UNarrativeAnimInstance* GetMainABPRef() const;

	//Return if we're overriding with a layer at present 
	UFUNCTION(BlueprintPure, Category = "Narrative Anim Instance")
	bool HasOverrideLayer() const;

	//Apply an override layer to this anim instance with the given blend time
	UFUNCTION(BlueprintCallable, Category = "Narrative Anim Instance")
	virtual bool ApplyOverrideLayer(UPARAM(meta = (Categories = "Narrative.Anim.OverrideLayer"))FGameplayTag LayerTag, const float BlendInTime);

	//Remove the given override layer over a set amount of time, blending back into our normal ABP logic. 
	UFUNCTION(BlueprintCallable, Category = "Narrative Anim Instance")
	virtual void RemoveOverrideLayer(const float BlendOutTime);

	//Apply an override layer to this anim instance 
	UFUNCTION(BlueprintCallable, Category = "Narrative Anim Instance")
	virtual UNarrativeAnimInstance* ApplyOverlayLayer(const TSubclassOf<UNarrativeAnimInstance>& OverlayClass, const bool bApplyToOverride=true);
	
	//Remove our override layer from this anim instance 
	UFUNCTION(BlueprintCallable, Category = "Narrative Anim Instance")
	virtual void RemoveOverlayLayer();
	
	//Stop all montages
	UFUNCTION(BlueprintCallable, Category = "Narrative Anim Instance")
	void BPStopAllMontages(const float BlendOutTime);

	//Return the override layers anim instance
	UFUNCTION(BlueprintPure, Category = "Narrative Anim Instance")
	UNarrativeAnimInstance* GetOverrideLayerAnimInstance() const;

	//Return the override layers tag
	UFUNCTION(BlueprintPure, Category = "Narrative Anim Instance")
	FGameplayTag GetOverrideLayerTag() const;

	UPROPERTY(BlueprintReadOnly, Category = "Narrative Anim Instance")
	FVector TraversalLedgeLocation;

	UPROPERTY(BlueprintReadOnly, Category = "Narrative Anim Instance")
	FVector LocalLedgeLocation;
	
	UPROPERTY(BlueprintReadOnly, Category = "Narrative Anim Instance")
	FQuat TraversalLedgeRotation;

	UPROPERTY(BlueprintReadOnly, Category = "Narrative Anim Instance")
	FTransform TraversalLedgeTransform;

	UPROPERTY(BlueprintReadOnly, Category = "Narrative Anim Instance")
	FQuat DirectionToLedge;

	//The time we want to blend the override layer in over 
	UPROPERTY(BlueprintReadOnly, Category = "Narrative Anim Instance")
	float OverrideLayerBlendInTime;

	//The time we want to blend the override layer out over 
	UPROPERTY(BlueprintReadOnly, Category = "Narrative Anim Instance")
	float OverrideLayerBlendOutTime;

	//These tags will be applied to the character running this animation blueprint whilst it is on them 
	UPROPERTY(EditDefaultsOnly, Category = "GameplayTags")
	FGameplayTagContainer ApplyTags;

	FActiveGameplayEffectHandle ApplyTagsHandle; 

protected:

	FTimerHandle TimerHandle_OverrideLayerBlendedOut;

	UFUNCTION()
	virtual void OverrideLayerBlendedOut();

	virtual void NativeUpdateAnimation(float DeltaSeconds) override; 
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUninitializeAnimation() override; 

	//Owning narrative character
	UPROPERTY(BlueprintReadOnly, Category = "Narrative Anim Instance")
	TObjectPtr<class ANarrativeCharacter> NarrativeCharacterRef;

	//Whether or not this AnimInstance is applied to a 3P or a 1P mesh setup - this is NOT whether we are currently in first person. 
	UPROPERTY(BlueprintReadOnly, Category = "Narrative Anim Instance")
	bool bIsThirdPersonABP;

	//Whether or not we have an override layer applied
	UPROPERTY(BlueprintReadOnly, Category = "Narrative Anim Instance")
	bool bHasOverrideLayer;

	//The tag identifying the last override layer we had on - added this in case we need to check for whatever reason 
	UPROPERTY(BlueprintReadOnly, Category = "Narrative Anim Instance")
	FGameplayTag LastOverrideLayer;

	//The tag identifying the current override layer 
	UPROPERTY(BlueprintReadOnly, Category = "Narrative Anim Instance")
	FGameplayTag CurrentOverrideLayer;
	
	//The tag identifying the current override layer 
	UPROPERTY(BlueprintReadOnly, Category = "Narrative Anim Instance")
	TObjectPtr<UNarrativeAnimInstance> CurrentOverlayLayer;
	
	/**Brilliant container type that ships with GAS - lets us bind variables directly to gameplaytags. */
	UPROPERTY(EditDefaultsOnly, Category = "GameplayTags")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;

	/**Tagged animsets - this lets us have a generic, extensible, and BP friendly way of mapping tags to Combo Sets. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "arrative Anim Instance", meta = (Categories = "Narrative.Anim.AnimSets", ForceInlineRow))
	TMap<FGameplayTag, TObjectPtr<UNarrativeAnimSet>> TaggedAnimSets;

	/**Tagged override layers - this lets us have a generic, extensible, and BP friendly way of mapping tags to override layers. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Narrative Anim Instance", meta = (Categories = "Narrative.Anim.OverrideLayer", ForceInlineRow))

	TMap<FGameplayTag, TSubclassOf<UAnimInstance>> TaggedOverrideLayers;

//Provides an easy framework for anything using this animinstance to blend out of sequencer, which requires a snapshot
//as sequencer can't blend out on the fly, only using pre-made keyframes. 
//
//At least I couldn't find a way of doing it that didn't seem overkill. 
public:

	UFUNCTION(BlueprintCallable, Category = "Narrative Anim Instance")
	virtual void BlendOutOfSequencer();
	/** Reuses the existing authored snapshot blend path after a native representation handoff. */
	bool BlendFromRepresentationPose(const FPoseSnapshot& Pose);

protected:

	//Set to true when we need to blend out of sequencer. 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Narrative Anim Instance")
	bool bWantsBlendOutOfSequencer;

	//The last frame of our character in sequencer. Its up to the ABP to blend back in from this snapshot if bWantsBlendOutOfSequencer is true. 
	UPROPERTY(BlueprintReadOnly, Category = "Narrative Anim Instance")
	FPoseSnapshot SequencerPoseSnapshot;
};

/**
 * Base class for ABP_Biped, ABP_Quadreped, etc. Base character class. 
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeCharacterAnimInstance : public UNarrativeAnimInstance
{
	GENERATED_BODY()

};

/**
 * Special layer used for ragdoll that contains a pose snapshot.
 */
UCLASS()
class NARRATIVEARSENAL_API URagdollAnimInstance : public UNarrativeAnimInstance
{
	GENERATED_BODY()

public:

	URagdollAnimInstance() {};

	UPROPERTY(BlueprintReadOnly, Category = "Ragdoll Anim Instance")
	FPoseSnapshot RagdollGetUpSnapshot;

	virtual FPoseSnapshot& CreateRagdollSnapshot();

};
