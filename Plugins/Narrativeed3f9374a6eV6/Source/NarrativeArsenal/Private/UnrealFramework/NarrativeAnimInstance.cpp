// Copyright Narrative Tools 2024. 


#include "UnrealFramework/NarrativeAnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "IMediaControls.h"
#include "NarrativeLogChannels.h"
#include "GameFramework/Character.h"
#include "Character/NarrativeCharacterVisual.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Weapons/WeaponVisual.h"

UNarrativeAnimInstance::UNarrativeAnimInstance()
{
	bHasOverrideLayer = false; 
	bWantsBlendOutOfSequencer = false; 
}

void UNarrativeAnimInstance::BindASC()
{
	if (ANarrativeCharacter* CharOwner = GetCharacterRef())
	{
		//If apply tags handle is valid we've already bound our ASC and can skip this. 
		if (UNarrativeAbilitySystemComponent* ASC = Cast<UNarrativeAbilitySystemComponent>(CharOwner->GetAbilitySystemComponent()))
		{
			GameplayTagPropertyMap.Initialize(this, ASC);

			if (!ApplyTagsHandle.IsValid())
			{
				ApplyTagsHandle = ASC->AddDynamicTagsGameplayEffect(ApplyTags);
			}
		}
		else
		{
			FTimerHandle DummyHandle; 
			GetWorld()->GetTimerManager().SetTimer(DummyHandle, this, &UNarrativeAnimInstance::BindASC, 0.5f, false);
		}
	}
	else
	{
		if (AActor* OwningActor = GetOwningActor())
		{
			FString RoleStr = OwningActor->HasAuthority() ? "Server" : "Client";
			UE_LOG(LogNarrativeNet, Warning, TEXT("%s Actor %s with AnimInstance %s couldn't bind ASC as CharOwner was null"), *RoleStr, *GetNameSafe(OwningActor), *GetNameSafe(this));
		}
	}
}

UNarrativeAnimSet* UNarrativeAnimInstance::GetAnimSet(const FGameplayTag& AnimSetTag, const bool bSearchedLinkedLayers, bool& bOutFoundAnimSet)
{
	bOutFoundAnimSet = false;

	if (bSearchedLinkedLayers)
	{
		if (UNarrativeAnimInstance* LinkedInst = Cast<UNarrativeAnimInstance>(GetLinkedAnimLayerInstanceByClass(UNarrativeAnimInstance::StaticClass(), true)))
		{
			if (UNarrativeAnimSet* AnimSet = LinkedInst->GetAnimSet(AnimSetTag, false, bOutFoundAnimSet))
			{
				return AnimSet;	
			}
		}
	}

	
	if (TaggedAnimSets.Contains(AnimSetTag))
	{
		bOutFoundAnimSet = true; 
		return TaggedAnimSets[AnimSetTag];
	}

	return nullptr;
}

USkeletalMeshComponent* UNarrativeAnimInstance::GetCharacterMesh() const
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwningActor()))
	{
		return Character->GetMesh();
	}
	else if (ANarrativeCharacterVisual* CharVisual = Cast<ANarrativeCharacterVisual>(GetOwningActor()))
	{
		return CharVisual->GetMainMesh();
	}

	return nullptr; 
}

ANarrativeCharacter* UNarrativeAnimInstance::GetCharacterRef() const
{
	if (INarrativeCharacterOwner* Character = Cast<INarrativeCharacterOwner>(GetOwningActor()))
	{
		return Character->GetNarrativeCharacter();
	}

	return nullptr; 
}

ANarrativeCharacterVisual* UNarrativeAnimInstance::GetCharacterVisualRef() const
{
	if (INarrativeCharacterOwner* Character = Cast<INarrativeCharacterOwner>(GetOwningActor()))
	{
		if (ANarrativeCharacter* NChar = Character->GetNarrativeCharacter())
		{
			return NChar->GetCharacterVisual();
		}
	}

	return nullptr;
}

UNarrativeAnimInstance* UNarrativeAnimInstance::GetMainABPRef() const
{
	if (INarrativeCharacterOwner* Character = Cast<INarrativeCharacterOwner>(GetOwningActor()))
	{
		if (ANarrativeCharacter* NChar = Character->GetNarrativeCharacter())
		{
			if (USkeletalMeshComponent* Mesh = NChar->GetMesh())
			{
				return Cast<UNarrativeAnimInstance>(Mesh->GetAnimInstance());
			}
		}
	}

	return nullptr;
}

bool UNarrativeAnimInstance::HasOverrideLayer() const
{
	return bHasOverrideLayer;
}

bool UNarrativeAnimInstance::ApplyOverrideLayer(FGameplayTag LayerTag, const float BlendInTime)
{
	if (TaggedOverrideLayers.Contains(LayerTag))
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_OverrideLayerBlendedOut);

		OverrideLayerBlendInTime = BlendInTime;
		
		LinkAnimClassLayers(TaggedOverrideLayers[LayerTag]);

		CurrentOverrideLayer = LayerTag; 
		bHasOverrideLayer = true; 

		//If we have an overlay such as pistol, bow etc give our override that overlay in case it wants to use it also 
		if (CurrentOverlayLayer)
		{
			if (UNarrativeAnimInstance* OverrideInst = GetOverrideLayerAnimInstance())
			{
				OverrideInst->ApplyOverlayLayer(CurrentOverlayLayer->GetClass(), false);
			}
		}
		
		
		return true;
	}

	return false;
}

void UNarrativeAnimInstance::RemoveOverrideLayer(const float BlendOutTime)
{
	if (TaggedOverrideLayers.Contains(CurrentOverrideLayer))
	{
		OverrideLayerBlendOutTime = BlendOutTime;

		//UnlinkAnimClassLayers(TaggedOverrideLayers[CurrentOverrideLayer]);

		if (BlendOutTime <= KINDA_SMALL_NUMBER)
		{
			OverrideLayerBlendedOut();
		}
		else
		{
			//Wait until blended out before we stop evaluating 
			GetWorld()->GetTimerManager().SetTimer(TimerHandle_OverrideLayerBlendedOut, this, &UNarrativeAnimInstance::OverrideLayerBlendedOut, BlendOutTime + 0.05f, false);

		}

		//Can leave CurrentOverrideLayer set to a valid tag, might be nice to know what the last layer we applied was 
		//Since we're leaving valid atm, we need to remove tags manually 
		 
		LastOverrideLayer = CurrentOverrideLayer;

		//Causes the layer to start blending out, removal is deferred until blend is done 
		bHasOverrideLayer = false;
	}
}

UNarrativeAnimInstance* UNarrativeAnimInstance::ApplyOverlayLayer(const TSubclassOf<UNarrativeAnimInstance>& OverlayClass, const bool bApplyToOverride)
{
	if (IsValid(OverlayClass))
	{
		//If we have any override layer like swimming for example then 
		if (bApplyToOverride)
		{
			if (UNarrativeAnimInstance* OverrideInst = GetOverrideLayerAnimInstance())
			{
				OverrideInst->ApplyOverlayLayer(OverlayClass, false);
			}
		}
		
		LinkAnimClassLayers(OverlayClass);

		//Linking above doesn't actually return linked inst so just find it. 
		CurrentOverlayLayer = Cast<UNarrativeAnimInstance>(GetLinkedAnimLayerInstanceByClass(OverlayClass));
	}

	return nullptr;
}

void UNarrativeAnimInstance::RemoveOverlayLayer()
{
	if (CurrentOverlayLayer)
	{
		UnlinkAnimClassLayers(CurrentOverlayLayer->GetClass());

		if (UNarrativeAnimInstance* OverrideInst = GetOverrideLayerAnimInstance())
		{
			OverrideInst->RemoveOverlayLayer();
		}
	}
}

void UNarrativeAnimInstance::BPStopAllMontages(const float BlendOutTime)
{
	StopAllMontages(BlendOutTime);
}

UNarrativeAnimInstance* UNarrativeAnimInstance::GetOverrideLayerAnimInstance() const
{
	if (TaggedOverrideLayers.Contains(CurrentOverrideLayer))
	{
		return Cast<UNarrativeAnimInstance>(GetLinkedAnimLayerInstanceByClass(TaggedOverrideLayers[CurrentOverrideLayer]));
	}

	return nullptr; 
}

FGameplayTag UNarrativeAnimInstance::GetOverrideLayerTag() const
{
	return CurrentOverrideLayer;
}

void UNarrativeAnimInstance::OverrideLayerBlendedOut()
{
	if (TaggedOverrideLayers.Contains(CurrentOverrideLayer))
	{
		UnlinkAnimClassLayers(TaggedOverrideLayers[CurrentOverrideLayer]);
		CurrentOverrideLayer = FGameplayTag::EmptyTag;
	}
}

void UNarrativeAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	NarrativeCharacterRef = Cast<ANarrativeCharacter>(GetOwningActor());

	Super::NativeUpdateAnimation(DeltaSeconds);
}

void UNarrativeAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	if (AActor* OwningActor = GetOwningActor())
	{
		/**TODO we can possibly re-apply squashed overlay layers in here.
		 *
		 * UE5 skeletal animation track will remove our overlay layers and they wont be automatically restored. If we're force holstering, this is fine,
		 * but if we're not force holstering and want to re-wield our weapon we'll be stuck with our weapon on but no overlay layer.
		 *
		 * We do cache the overlay layer in an object, so theoretically in here we can check if it is valid
		 */
		
		//Third person ABP exists on the narrative character, first person is always on visual 
		if (AWeaponVisual* WeapVisual = Cast<AWeaponVisual>(OwningActor))
		{
			bIsThirdPersonABP = GetOwningComponent() == WeapVisual->WeaponMesh;
		}
		else 
		{
			bIsThirdPersonABP = OwningActor->GetClass()->IsChildOf<ANarrativeCharacter>();
		}

		//Try bind any ASC stuff to the anim instance - this may retry if ASC isn't ready yet. 
		BindASC();
	}

}

void UNarrativeAnimInstance::NativeUninitializeAnimation()
{
	if (AActor* OwningActor = GetOwningActor())
	{
		if (UNarrativeAbilitySystemComponent* ASC = Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor)))
		{
			if (ApplyTagsHandle.IsValid())
			{
				ASC->RemoveActiveGameplayEffect(ApplyTagsHandle);
			}
		}
	}
	
	Super::NativeUninitializeAnimation();
}

FPoseSnapshot& URagdollAnimInstance::CreateRagdollSnapshot()
{
	SnapshotPose(RagdollGetUpSnapshot);

	return RagdollGetUpSnapshot;
}

void UNarrativeAnimInstance::BlendOutOfSequencer()
{
	if (!bWantsBlendOutOfSequencer)
	{
		//UE crashes if this is empty so just dont call it if so  
		if (USkeletalMeshComponent* SkeletalMeshComponent = GetSkelMeshComponent())
		{
			if (SkeletalMeshComponent->GetComponentSpaceTransforms().Num() <= 0)
			{
				return;
			}
		}

		SnapshotPose(SequencerPoseSnapshot);

		//ABP can set back to false on become relevant. 
		bWantsBlendOutOfSequencer = true;
	}

}

bool UNarrativeAnimInstance::BlendFromRepresentationPose(const FPoseSnapshot& Pose)
{
	USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!Mesh || !Mesh->GetSkeletalMeshAsset() || !Pose.bIsValid || Pose.LocalTransforms.IsEmpty()
		|| Pose.LocalTransforms.Num() != Pose.BoneNames.Num() || Pose.SkeletalMeshName != Mesh->GetSkeletalMeshAsset()->GetFName()) { return false; }
	for (int32 Index = 0; Index < Pose.BoneNames.Num(); ++Index)
	{
		if (Mesh->GetBoneIndex(Pose.BoneNames[Index]) == INDEX_NONE || Pose.LocalTransforms[Index].ContainsNaN()) { return false; }
	}
	SequencerPoseSnapshot = Pose;
	bWantsBlendOutOfSequencer = true;
	return true;
}
