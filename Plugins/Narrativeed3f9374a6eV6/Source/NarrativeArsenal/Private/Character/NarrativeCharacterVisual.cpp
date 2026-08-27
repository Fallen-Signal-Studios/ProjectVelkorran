// Copyright Narrative Tools 2024. 


#include "Character/NarrativeCharacterVisual.h"
#include "NarrativeGameplayTags.h"
#include "NarrativeLogChannels.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "Character/CharacterAppearance.h"
#include "CharacterCreator/CharacterCreatorAttributes.h"
#include "Items/WeaponItem.h"
#include <AbilitySystemComponent.h>
#include <Components/SceneComponent.h>
#include <Engine/AssetManager.h>
#include <Net/UnrealNetwork.h>
#include <GroomComponent.h>
#include <Materials/MaterialInstanceDynamic.h>
#include "Weapons/WeaponVisual.h"
#include "UnrealFramework/NarrativeAnimInstance.h"
#include "Components/EquipmentComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"

// Sets default values
ANarrativeCharacterVisual::ANarrativeCharacterVisual()
{
	CharacterVisualRoot = CreateDefaultSubobject<USceneComponent>("CharacterVisualRoot");

	bBaseAppearanceLoaded = false; 

	bHideUpperBodyInFirstPerson = true;
	UpperBodyHideBone = FName("spine_03");

	SetRootComponent(CharacterVisualRoot);

	bReplicates = true;

	AppearanceAsset = AppliedAppearanceAsset = nullptr;
}

class ANarrativeCharacter* ANarrativeCharacterVisual::GetNarrativeCharacter() const
{
	//See if cached, find if not. 
	if (OwnerCharacter)
	{
		return OwnerCharacter;
	}
	else
	{
		if (INarrativeCharacterOwner* CharOwnerInterface = Cast<INarrativeCharacterOwner>(GetOwner()))
		{
			if (ANarrativeCharacter* NChar = CharOwnerInterface->GetNarrativeCharacter())
			{
				return OwnerCharacter;
			}
		}
	}

	return nullptr;
}

UAbilitySystemComponent* ANarrativeCharacterVisual::GetAbilitySystemComponent() const
{
	if (OwnerCharacter)
	{
		return OwnerCharacter->GetAbilitySystemComponent();
	}

	return nullptr; 
}

void ANarrativeCharacterVisual::BeginPlay()
{
	Super::BeginPlay();

	if (!OwnerCharacter)
	{
		OwnerCharacter = Cast<ANarrativeCharacter>(GetOwner());
	}

	if (!HasAuthority() && AppearanceAsset)
	{
		OnRep_AppearanceAsset();
	}
	
}

void ANarrativeCharacterVisual::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ANarrativeCharacterVisual::Destroyed()
{
	Super::Destroyed();

	TArray<FGameplayTag> VisualSlots;
	SpawnedWeaponVisuals.GenerateKeyArray(VisualSlots);

	for (auto& Slot : VisualSlots)
	{
		RemoveWeaponVisual(Slot);
	}
}

void ANarrativeCharacterVisual::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANarrativeCharacterVisual, AppearanceAsset);
}

void ANarrativeCharacterVisual::OnRep_Owner()
{
	Super::OnRep_Owner();

    //If owner reps after appearance asset, usually due to packet loss, sort it. 
	if(IsValid(AppearanceAsset) && !IsValid(AppliedAppearanceAsset))
	{
	    OnRep_AppearanceAsset();
	}

	
}

void ANarrativeCharacterVisual::HandleUpdateWields_Implementation(const FWeaponWieldState& OldWieldState, const FWeaponWieldState& NewWieldState)
{

	if (ANarrativeCharacter* OwnedChar = GetOwnerCharacter())
	{
		if (UEquipmentComponent* EquipComp = OwnedChar->GetEquipmentComponent())
		{
			check(NewWieldState.EquipWeapons.Num() == NewWieldState.WieldSlots.Num());

			FString RoleStr = HasAuthority() ? "Server" : "Client";
			
			//Put away the old weapons. 
			if (!OldWieldState.EquipSlots.IsEmpty() && !OldWieldState.WieldSlots.IsEmpty())
			{
				checkf(OldWieldState.EquipWeapons.Num() == OldWieldState.WieldSlots.Num(), TEXT("Number of wield slots doesn't match number of equip slots, something has gone wrong. "));

				for (int32 i = 0; i <= OldWieldState.EquipSlots.Num() - 1; ++i)
				{
					const FGameplayTag EquipSlot = OldWieldState.EquipSlots.GetByIndex(i);
					const FGameplayTag WieldSlot = OldWieldState.WieldSlots.GetByIndex(i);

					if (OldWieldState.EquipWeapons.IsValidIndex(i))
					{
						AttachWeaponVisual(OldWieldState.EquipWeapons[i], EquipSlot, FGameplayTag());
					}
					else
					{
						UE_LOG(LogNarrativeCharacterVisual, Warning, TEXT("%s: Failed to find a weapon visual with slot %s. Skipping attach."), *RoleStr, *EquipSlot.ToString());
					}
				}
			}

			if (OwnedChar && NewWieldState.EquipWeapons.IsValidIndex(0))
			{
				//Attach the new weapons to our character.
				for (int32 i = 0; i <= NewWieldState.EquipSlots.Num() - 1; ++i)
				{
					const FGameplayTag EquipSlot = NewWieldState.EquipSlots.GetByIndex(i);
					const FGameplayTag WieldSlot = NewWieldState.WieldSlots.GetByIndex(i);
					
					if (NewWieldState.EquipWeapons.IsValidIndex(i))
					{
						if (UWeaponItem* NewWeapon = NewWieldState.EquipWeapons[i])
						{
							AttachWeaponVisual(NewWeapon, EquipSlot, WieldSlot);
						}
					}
					else
					{
						UE_LOG(LogNarrativeCharacterVisual, Warning, TEXT("%s: Failed to equip new weapon at index %i as index was invalid. EquipSlot %s, WieldSlot %s"), *RoleStr, i, *EquipSlot.ToString(), *WieldSlot.ToString());
					}
				}

				//Finally, set our ABP. We only need to ask 1 weapon for the overlay, since dual wielded weapons need to agree on using same layer
				// and as such will return same ABP anyways. 
				const bool bDualWielding = NewWieldState.EquipWeapons.Num() > 1;
				
				if (UWeaponItem* FirstWeapon = NewWieldState.EquipWeapons[0])
				{
					if (AWeaponVisual* WeaponVisual = GetWeaponVisual(NewWieldState.EquipSlots.GetByIndex(0)))//FirstWeapon->CurrentSlot))
					{
						if (USkeletalMeshComponent* LocalMesh = GetOrCreateMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Character_LocalMesh))
						{
							if (UNarrativeAnimInstance* MeshInstance = Cast<UNarrativeAnimInstance>(LocalMesh->GetAnimInstance()))
							{
								MeshInstance->ApplyOverlayLayer(WeaponVisual->GetWeaponOverlayLayer(true));
								//MeshInstance->LinkAnimClassLayers(WeaponVisual->GetWeaponOverlayLayer(true));
							}
						}

						if (USkeletalMeshComponent* MainMesh = GetMainMesh())
						{
							if (UNarrativeAnimInstance* MeshInstance = Cast<UNarrativeAnimInstance>(MainMesh->GetAnimInstance()))
							{
								MeshInstance->ApplyOverlayLayer(WeaponVisual->GetWeaponOverlayLayer(false));
								//MeshInstance->LinkAnimClassLayers(WeaponVisual->GetWeaponOverlayLayer(false));
								//MeshInstance->LinkAnimClassLayers(WeaponAnimLayer);
							}
						}
					}
					else
					{
						UE_LOG(LogNarrativeCharacterVisual, Warning, TEXT("%s: Failed to find a weapon visual with slot %s. Overlay will not be applied."), *RoleStr, *FirstWeapon->CurrentSlot.ToString());
					}
				}
			}
			else //No new wielded weapons means we're going unarmed. Set our overlay back to unarmed - if no unarmed layer this will still clear out our overlay. 
			{
				if (USkeletalMeshComponent* MainMesh = GetMainMesh())
				{
					if (UNarrativeAnimInstance* MeshInstance = Cast<UNarrativeAnimInstance>(MainMesh->GetAnimInstance()))
					{
						if (IsValid(AppearanceAttributeSet.UnarmedAnimLayer))
						{
							MeshInstance->ApplyOverlayLayer(AppearanceAttributeSet.UnarmedAnimLayer);
						}
						else
						{
							MeshInstance->RemoveOverlayLayer();
						}
					}
				}

				if (USkeletalMeshComponent* LocalMesh = GetOrCreateMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Character_LocalMesh))
				{
					if (UNarrativeAnimInstance* MeshInstance = Cast<UNarrativeAnimInstance>(LocalMesh->GetAnimInstance()))
					{
						if (IsValid(AppearanceAttributeSet.UnarmedAnimLayer))
						{
							MeshInstance->LinkAnimClassLayers(AppearanceAttributeSet.UnarmedAnimLayer);
						}
						else
						{
							MeshInstance->RemoveOverlayLayer();
						}
					}
				}
			}
		}
	}
	else
	{
		FString RoleStr = HasAuthority() ? "Server" : "Client";
		FString LocalStr = IsLocallyControlled() ? "Local" : "Remote";
		UE_LOG(LogTemp, Warning, TEXT("%s %s OwnedCharacter returned nullptr - wields will fail. "), *LocalStr, *RoleStr);
	}


}

void ANarrativeCharacterVisual::HandleWieldWeapon_Implementation(class UWeaponItem* Weapon)
{	
	/*if (ANarrativeCharacter* OwnedChar = GetOwnerCharacter())
	{
		if (Weapon && OwnedChar)
		{
			AttachWeaponVisual(Weapon, Weapon->CurrentSlot, Weapon->WieldedSlot);

			if (AWeaponVisual* WeaponVisual = GetWeaponVisual(Weapon->CurrentSlot))//WeaponItem->EquippableSlot))
			{
				//if (UNarrativeAnimInstance* MeshInstance = OwnerCharacter->GetCharacterAnimInstance())
				{
					//Override should overrule this 
					//if (!MeshInstance->HasOverrideLayer())
					{
						if (USkeletalMeshComponent* LocalMesh = GetOrCreateMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Character_LocalMesh))
						{
							if (UAnimInstance* MeshInstance = LocalMesh->GetAnimInstance())
							{
								MeshInstance->LinkAnimClassLayers(WeaponVisual->GetWeaponOverlayLayer(true));
								//MeshInstance->LinkAnimClassLayers(WeaponVisual->Weapon1PAnimLayer);
							}
						}

						if (USkeletalMeshComponent* MainMesh = GetMainMesh())
						{
							if (UAnimInstance* MeshInstance = MainMesh->GetAnimInstance())
							{
								MeshInstance->LinkAnimClassLayers(WeaponVisual->GetWeaponOverlayLayer(false));
							}
						}
					}
				}
			}
		}
	}*/

}

void ANarrativeCharacterVisual::HandleUnWieldWeapon_Implementation(class UWeaponItem* Weapon)
{
	/*if(Weapon)
	{
		AttachWeaponVisual(Weapon, Weapon->CurrentSlot, Weapon->WieldedSlot);
		//ClearAnimBPOverride();
		
		if (USkeletalMeshComponent* MainMesh = GetMainMesh())
		{
			if (UAnimInstance* MeshInstance = MainMesh->GetAnimInstance())
			{
				MeshInstance->LinkAnimClassLayers(AppearanceAttributeSet.UnarmedAnimLayer);
			}
		}

		if (USkeletalMeshComponent* LocalMesh = GetOrCreateMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Character_LocalMesh))
		{
			if (UAnimInstance* MeshInstance = LocalMesh->GetAnimInstance())
			{
				MeshInstance->LinkAnimClassLayers(AppearanceAttributeSet.UnarmedAnimLayer);
			}
		}
	}*/
}

void ANarrativeCharacterVisual::HandleEquipClothing_Implementation(class UEquippableItem_Clothing* Clothing)
{
	if (Clothing)
	{
		FCharacterCreatorAttribute_Mesh& MeshData = Clothing->ClothingMeshData;

		if (Clothing->FormSpecificMeshData.Contains(AppearanceAttributeSet.FormTag))
		{
			MeshData = Clothing->FormSpecificMeshData[AppearanceAttributeSet.FormTag];
		}

		SetMeshAppearance(Clothing->CurrentSlot, MeshData);
	}
}

void ANarrativeCharacterVisual::HandleUnEquipClothing_Implementation(const FGameplayTag& Slot)
{
	if (Slot.IsValid())
	{
		ResetMeshToBaseAppearance(Slot);
	}
}

void ANarrativeCharacterVisual::HandlePerspectiveUpdate_Implementation(const bool bIsFirstPerson)
{
	//Ensure only local character visuals update for their perspective 
	if (OwnerCharacter)
	{
		if (AController* PController = OwnerCharacter->GetOwningController())
		{
			if (PController->IsLocalController())
			{
				//First, update our head meshes - we don't want these in first person. 
				TArray<UMeshComponent*> HeadMeshes;
				GetHeadMeshes(HeadMeshes);

				for (auto& HeadMesh : HeadMeshes)
				{
					if (HeadMesh)
					{
						//HeadMesh->SetVisibility(!bIsFirstPerson, false);
						HeadMesh->SetFirstPersonPrimitiveType(bIsFirstPerson ? EFirstPersonPrimitiveType::WorldSpaceRepresentation : EFirstPersonPrimitiveType::None);
					}
				}

				//Torso and hands meshes need to be hidden as first person has 1P versions of these it uses. TODO needs prim type not hide 
				if (USkeletalMeshComponent* Hands = GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Hands))
				{
					//Hands->SetVisibility(!bIsFirstPerson, true);
					Hands->SetFirstPersonPrimitiveType(bIsFirstPerson ? EFirstPersonPrimitiveType::WorldSpaceRepresentation : EFirstPersonPrimitiveType::None);
				}

				if (USkeletalMeshComponent* Torso = GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Torso))
				{
					//Torso->SetVisibility(!bIsFirstPerson, true);
					Torso->SetFirstPersonPrimitiveType(bIsFirstPerson ? EFirstPersonPrimitiveType::WorldSpaceRepresentation : EFirstPersonPrimitiveType::None);
				}

				//Hide from the spine up - TODO possible add Legs_1P slot instead would be a cleaner approach 
				if (bHideUpperBodyInFirstPerson)
				{
					HideUpperBody(bIsFirstPerson);
				}

				//Show the local mesh, ie the first person mesh  
				TArray<UMeshComponent*> LocalMeshes;
				GetAllLocalMeshes(LocalMeshes);

				for (UMeshComponent* LocalMesh : LocalMeshes)
				{
					//Actual base local mesh shouldn't be effected, that needs to stay invisible, children want update 
					if (LocalMesh && LocalMesh != GetLocalMesh())
					{
						LocalMesh->SetVisibility(bIsFirstPerson, true);
						LocalMesh->SetFirstPersonPrimitiveType(bIsFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
					}
				}

				//Ask our weapon visuals to update also 
				for (auto& WeaponVisualKVP : SpawnedWeaponVisuals)
				{
					if (AWeaponVisual* Weapon = WeaponVisualKVP.Value)
					{
						Weapon->HandlePerspectiveUpdate(bIsFirstPerson);
					}
				}
			}
		}
	}
}

void ANarrativeCharacterVisual::HideUpperBody_Implementation(const bool bWantsHide)
{
	//By default, to hide the upper body we hide the spine_03 bone generally, but you can overrride this for more custom behavior. 
	if (USkeletalMeshComponent* CharMesh = GetMainMesh())
	{
		if (bWantsHide)
		{
			CharMesh->HideBoneByName(UpperBodyHideBone, PBO_None);
		}
		else
		{
			CharMesh->UnHideBoneByName(UpperBodyHideBone);
		}
	}
}

void ANarrativeCharacterVisual::InitializeFromCharacterAndAppearance_Implementation(class ANarrativeCharacter* NarrativeCharacter, UCharacterAppearance* Appearance)
{
	check(NarrativeCharacter);
	if (NarrativeCharacter)
	{
		OwnerCharacter = NarrativeCharacter;
		AppearanceAsset = Appearance;
		OnRep_AppearanceAsset();
	}
}

void ANarrativeCharacterVisual::InitializeFromCharacterAndAttributes_Implementation(class ANarrativeCharacter* NarrativeCharacter, const FCharacterCreatorAttributeSet& Attributes)
{

	const bool bReinitting = IsValid(Attributes.BaseMesh);

	check(NarrativeCharacter);
	if (NarrativeCharacter && Attributes.BaseMesh)
	{
		//FString RoleStr = HasAuthority() ? "Server" : "Client";
		//UE_LOG(LogTemp, Warning, TEXT("%s: initializing from character attributes"), *RoleStr);

		FString RoleStr = HasAuthority() ? "Server" : "Client";
		FString LocalStr = IsLocallyControlled() ? "Local" : "Remote";
		UE_LOG(LogNarrativeNet, Warning, TEXT("%s %s InitializeFromCharacterAndAttributes for character %s with appearance %s"), *LocalStr, *RoleStr, *NarrativeCharacter->GetCharacterName().ToString(), *GetNameSafe(AppearanceAsset));
		
		OwnerCharacter = NarrativeCharacter;
		AppearanceAttributeSet = Attributes;

		//Add the form tag to the character so we can check what form this character is.  
		if (Attributes.FormTag.IsValid())
		{
			if (UAbilitySystemComponent* ASC = NarrativeCharacter->GetAbilitySystemComponent())
			{
				ASC->SetLooseGameplayTagCount(Attributes.FormTag, 1);
			}
		}

		//Reset any previous appearance meshes that may have been set 
		for (auto& MeshComp : MeshComponents)
		{
			if (MeshComp.Value)
			{
				MeshComp.Value->SetSkeletalMesh(nullptr);
			}
		}

		if (USkeletalMeshComponent* BaseCharacterMesh = NarrativeCharacter->GetMesh())
		{
			MeshComponents.Add(FNarrativeGameplayTags::Get().Equipment_Slot_Character_Mesh, BaseCharacterMesh);

			BaseCharacterMesh->SetVisibility(!Attributes.bHideBaseMesh);
			BaseCharacterMesh->SetSkeletalMesh(Attributes.BaseMesh);

			UAnimInstance* Inst = BaseCharacterMesh->GetAnimInstance();

			BaseCharacterMesh->SetAnimInstanceClass(Attributes.BaseMeshAnimBP);

			// FGameplayAbilityActorInfo caches the avatar AnimInstance. Narrative can
			// replace that instance while applying an appearance, so refresh the ASC
			// before any ability tries to play a replicated montage on the new ABP.
			if (UAbilitySystemComponent* ASC = NarrativeCharacter->GetAbilitySystemComponent())
			{
				ASC->RefreshAbilityActorInfo();
			}

			if (Attributes.UnarmedAnimLayer)
			{
				BaseCharacterMesh->LinkAnimClassLayers(Attributes.UnarmedAnimLayer);
			}

			//Attach ourselves to the character mesh
			AttachToComponent(BaseCharacterMesh, FAttachmentTransformRules::SnapToTargetIncludingScale);
		}

		//For first person setups we need a local 1P mesh - we create it here so designers dont need to remember to add one themselves in the appearance asset. 
		//Server also requires this for replicating camera position, but only for players 
		const bool bIsPlayer = OwnerCharacter && OwnerCharacter->IsPlayerControlled();

		//Auth and client controlled proxies need the local mesh - one issue is replication, do we want server to create rep version of this?
		if (IsLocallyControlled() || (HasAuthority() && bIsPlayer))
		{
			if (USkeletalMeshComponent* LocalMesh = GetOrCreateMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Character_LocalMesh))
			{
				LocalMesh->SetVisibility(false, false);
				LocalMesh->SetOnlyOwnerSee(true);
				LocalMesh->SetCastShadow(false);
				LocalMesh->SetTickGroup(TG_PrePhysics); // Needed so arms update before camera does and arms dont lag behind camera by a frame 
				LocalMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
				LocalMesh->SetSkeletalMesh(Attributes.BaseMesh); //usually is the same as third person skeleton which will be invis
				LocalMesh->SetAnimInstanceClass(Attributes.BaseLocalMeshAnimBP);
				LocalMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
			}
		}

		//Load all the base meshes and materials, so we can tell owner when appearance is fully loaded. 
		TArray<FSoftObjectPath> MeshDataAssetPaths;

		for (auto& BodyPart : Attributes.Meshes)
		{
			auto& MeshData = BodyPart.Value;

			if (MeshData.Mesh.ToSoftObjectPath().IsValid())
			{
				MeshDataAssetPaths.Add(MeshData.Mesh.ToSoftObjectPath());
			}

			//Load the anim bp if this mesh needs one 
			if (!MeshData.bUseLeaderPose && MeshData.MeshAnimBP.ToSoftObjectPath().IsValid())
			{
				MeshDataAssetPaths.Add(MeshData.MeshAnimBP.ToSoftObjectPath());
			}

			for (auto& MeshMat : MeshData.MeshMaterials)
			{
				FSoftObjectPath MaterialPath = MeshMat.Material.ToSoftObjectPath();

				if (MaterialPath.IsValid())
				{
					MeshDataAssetPaths.Add(MaterialPath);
				}

			}
		}

		if(MeshDataAssetPaths.Num() > 0)
		{
			if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
			{
				FStreamableDelegate MeshLoadDel = FStreamableDelegate::CreateUObject(this, &ANarrativeCharacterVisual::OnBaseMeshesReady);

				int32 LoadPriority = 0;

				//Should fix our local character not loading assets quickly enough 
				if (IsLocallyControlled())
				{
					LoadPriority = INT_MAX;
				}

				BaseAppearanceLoadHandle = Manager->LoadAssetList(MeshDataAssetPaths, MeshLoadDel, LoadPriority);
			}
		}
		else
		{
			OnBaseMeshesReady();
		}


		//We need to re-apply all weapon meshes and clothing meshes as charvis has reloaded. 
	}
}

UMeshComponent* ANarrativeCharacterVisual::GetMeshComponent(const FGameplayTag& Tag)
{

	if (MeshComponents.Contains(Tag))
	{
		return MeshComponents[Tag];
	}

	if (StaticMeshComponents.Contains(Tag))
	{
		return StaticMeshComponents[Tag];
	}

	if (GroomComponents.Contains(Tag))
	{
		return GroomComponents[Tag];
	}

	return nullptr; 
}

USkeletalMeshComponent* ANarrativeCharacterVisual::GetOrCreateMeshComponent(const FGameplayTag& Tag)
{
	if (MeshComponents.Contains(Tag))
	{
		return MeshComponents[Tag];
	}

	//Only local players need local mesh 
	if (Tag == FNarrativeGameplayTags::Get().Equipment_Slot_Character_LocalMesh && OwnerCharacter && !OwnerCharacter->IsLocallyControlled())
	{
		return nullptr; 
	}

	//Never create 1P Meshes for other players, only local 
	if (Is1PMeshTag(Tag) && !IsLocallyControlled())
	{
		return nullptr; 
	}

	//If we dont have a mesh mapped, add one
	if (USkeletalMeshComponent* NewMesh = Cast<USkeletalMeshComponent>(AddComponentByClass(USkeletalMeshComponent::StaticClass(), false, FTransform::Identity, false)))
	{
		//Body meshes usually follow main character mesh - we want to tick them after the main mesh has update so any CopyPoseFromMesh anim nodes have the most up-to-date anim state and dont lag by one frame. 
		NewMesh->SetTickGroup(TG_PostPhysics);

		//We dont want decals right now, users can remove this if required 
		NewMesh->SetReceivesDecals(false);

		if (Is1PMeshTag(Tag))
		{
			NewMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
			//NewMesh->SetOnlyOwnerSee(true);
		}

		MeshComponents.Add(Tag, NewMesh);
		
		return NewMesh;
	}

	return nullptr; 
}

UStaticMeshComponent* ANarrativeCharacterVisual::GetOrCreateStaticMeshComponent(const FGameplayTag& Tag)
{
	if (StaticMeshComponents.Contains(Tag))
	{
		return StaticMeshComponents[Tag];
	}

	//If we dont have a StaticMesh mapped, add one
	if (UStaticMeshComponent* NewStaticMesh = Cast<UStaticMeshComponent>(AddComponentByClass(UStaticMeshComponent::StaticClass(), false, FTransform::Identity, false)))
	{
		//Body StaticMeshes usually follow main character StaticMesh - we want to tick them after the main StaticMesh has update so any CopyPoseFromStaticMesh anim nodes have the most up-to-date anim state and dont lag by one frame. 
		NewStaticMesh->SetTickGroup(TG_PostPhysics);

		//We dont want decals right now, users can remove this if required 
		NewStaticMesh->SetReceivesDecals(false);
		NewStaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		StaticMeshComponents.Add(Tag, NewStaticMesh);

		return NewStaticMesh;
	}

	return nullptr;
}

UGroomComponent* ANarrativeCharacterVisual::GetOrCreateGroomComponent(const FGameplayTag& Tag)
{
	if (GroomComponents.Contains(Tag))
	{
		return GroomComponents[Tag];
	}

	//If we dont have a mesh mapped, add one
	if (UGroomComponent* NewGroom = Cast<UGroomComponent>(AddComponentByClass(UGroomComponent::StaticClass(), true, FTransform::Identity, false)))
	{
		//Grooms need to attach to the head ideally 
		if (USkeletalMeshComponent* Face = GetFaceMesh())
		{
			NewGroom->AttachToComponent(Face, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		else
		{
			NewGroom->AttachToComponent(GetBodyMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}

		GroomComponents.Add(Tag, NewGroom);
		
		return NewGroom;
	}

	return nullptr; 
}

class USkeletalMeshComponent* ANarrativeCharacterVisual::GetLeaderMesh_Implementation()
{
	return GetMainMesh();
}

class USkeletalMeshComponent* ANarrativeCharacterVisual::GetMainMesh()
{
	return GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Character_Mesh);
}

class USkeletalMeshComponent* ANarrativeCharacterVisual::GetFaceMesh()
{
	return GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Face);
}

class USkeletalMeshComponent* ANarrativeCharacterVisual::GetLocalMesh()
{
	return GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Character_LocalMesh);
}

void ANarrativeCharacterVisual::GetAllMeshes(TArray<class USkeletalMeshComponent*>& OutMeshes)
{
	MeshComponents.GenerateValueArray(OutMeshes);
}

void ANarrativeCharacterVisual::GetAllLocalMeshes(TArray<class UMeshComponent*>& OutLocalMeshes)
{
	OutLocalMeshes.Add(GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Character_LocalMesh));
	OutLocalMeshes.Add(GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Hands_1P));
	OutLocalMeshes.Add(GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Torso_1P));
}

class USkeletalMeshComponent* ANarrativeCharacterVisual::GetBodyMesh()
{
	if (USkeletalMeshComponent* BodyMesh = GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Body))
	{
		return BodyMesh;
	}

	//If we don't have a body mesh, we are probably using the main mesh as the body mesh 
	return GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Character_Mesh);
}

FTransform ANarrativeCharacterVisual::GetHeadTransformWS()
{
	if(GetMainMesh())
	{
		return GetMainMesh()->GetBoneTransform(FName("head"), RTS_World);
	}
	return FTransform::Identity;
}

void ANarrativeCharacterVisual::GetHeadMeshes(TArray<class UMeshComponent*>& OutHeadMeshes) const
{
	//All grooms need to be hidden
	for (auto& GroomComp : GroomComponents)
	{
		if (GroomComp.Value)
		{
			OutHeadMeshes.Add(GroomComp.Value);
		}
	}

	//Helmets, faces, and all facial hair needs hidden 
	OutHeadMeshes.Add(GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Face));
	OutHeadMeshes.Add(GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Helmet));
	OutHeadMeshes.Add(GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Mesh_Beard));
	OutHeadMeshes.Add(GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Mesh_Eyebrows));
	OutHeadMeshes.Add(GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Mesh_Eyelashes));
	OutHeadMeshes.Add(GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Mesh_Moustache));
	OutHeadMeshes.Add(GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Mesh_Fuzz));
	OutHeadMeshes.Add(GetSkeletalMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Mesh_Hair));

	// Some appearance assets author helmets or facial pieces as static meshes.
	OutHeadMeshes.Add(GetStaticMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Face));
	OutHeadMeshes.Add(GetStaticMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Helmet));
	OutHeadMeshes.Add(GetStaticMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Mesh_Beard));
	OutHeadMeshes.Add(GetStaticMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Mesh_Eyebrows));
	OutHeadMeshes.Add(GetStaticMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Mesh_Eyelashes));
	OutHeadMeshes.Add(GetStaticMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Mesh_Moustache));
	OutHeadMeshes.Add(GetStaticMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Mesh_Fuzz));
	OutHeadMeshes.Add(GetStaticMeshComponent(FNarrativeGameplayTags::Get().Equipment_Slot_Mesh_Hair));
}

class ANarrativeCharacter* ANarrativeCharacterVisual::GetOwnerCharacter() 
{
	//See if cached, find if not. 
	if (OwnerCharacter)
	{
		return OwnerCharacter;
	}
	else
	{
		if (INarrativeCharacterOwner* CharOwnerInterface = Cast<INarrativeCharacterOwner>(GetOwner()))
		{
			if (ANarrativeCharacter* NChar = CharOwnerInterface->GetNarrativeCharacter())
			{
				OwnerCharacter = NChar;
				return OwnerCharacter;
			}
		}
	}

	return nullptr; 
}

FCharacterCreatorAttributeSet ANarrativeCharacterVisual::GetCreatorAttributes() const
{
	return AppearanceAttributeSet;
}

bool ANarrativeCharacterVisual::AddWeaponVisual(class UWeaponItem* WeaponItem)
{
	if (IsValid(WeaponItem) && HasAuthority())
	{
		const FGameplayTag Slot = WeaponItem->CurrentSlot; //WeaponItem->EquippableSlot;

		if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
		{
			if (WeaponLoadHandles.Contains(Slot))
			{
				if (WeaponLoadHandles[Slot].IsValid())
				{
					WeaponLoadHandles[Slot]->CancelHandle();
					WeaponLoadHandles[Slot].Reset();
				}
			}

			TArray<FSoftObjectPath> WeaponDataAssetPaths = { WeaponItem->WeaponVisualClass.ToSoftObjectPath() };

			UE_LOG(LogNarrativeCharacterVisual, Verbose, TEXT("no defer needed, %s loading weapon visual %s at slot %s"), *(GetOwner()->GetHumanReadableName()), *WeaponItem->WeaponVisualClass.ToSoftObjectPath().ToString(), *Slot.ToString());

			FStreamableDelegate WeaponLoadDel = FStreamableDelegate::CreateUObject(this, &ANarrativeCharacterVisual::OnWeaponVisualClassReady, WeaponItem);
			TSharedPtr<FStreamableHandle> Handle = Manager->LoadAssetList(WeaponDataAssetPaths, WeaponLoadDel);
			WeaponLoadHandles.Add(Slot, Handle);

			return true;
		}

	}

	return false; 
}

void ANarrativeCharacterVisual::AttachWeaponVisual(class UWeaponItem* WeaponItem, const FGameplayTag& EquipSlot, const FGameplayTag& WieldSlot)
{
	if (WeaponItem)
	{
		//check(WeaponItem->EquippedSlot.IsValid());

		if (AWeaponVisual* WeaponVisual = GetWeaponVisual(EquipSlot))//WeaponItem->EquippableSlot))
		{
			//First, attach the 3P Weapon mesh to the 3P character mesh.
			//TODO attach transform information will have to check what wield slot the weapon is in. 
			const FWeaponAttachmentConfig AttachConfig = !WieldSlot.IsValid() ? WeaponItem->GetWeaponHolsterAttachConfig(EquipSlot) : WeaponItem->GetWeaponWieldAttachConfig(WieldSlot);
			const FTransform AttachOffset = AttachConfig.Offset;
			const FName AttachSocket = AttachConfig.SocketName;

			FString RoleStr = HasAuthority() ? "Server" : "Client";
			FString LocalStr = IsLocallyControlled() ? "Local" : "Remote";
			
			if (AttachConfig.SocketName == NAME_None && AttachConfig.Offset.Equals(FTransform::Identity))
			{
				FString FailedTo = !WieldSlot.IsValid() ? "Holster - CurrentSlot was not repped yet." : "Unholster - WieldSlot was not repped yet.";

				UE_LOG(LogNarrativeNet, Warning, TEXT("%s %s Failed to %s weapon because AttachConfig couldn't get fetched"), *LocalStr, *RoleStr, *FailedTo);
			}

			UE_LOG(LogNarrativeNet, Warning, TEXT("%s %s Attached weapon %s. EquipSlot %s, WieldSlot %s"), *LocalStr, *RoleStr, *GetNameSafe(this), *EquipSlot.ToString(), *WieldSlot.ToString());

			//if (HasAuthority())
			{
				WeaponVisual->AttachState.WieldedSlot = WieldSlot;
			}
			
			USkeletalMeshComponent* ComponentToUse = nullptr;

			//If the socket exists on the body, try that - metahumans often define one of these so weapon attaches properly - only for holsters though, weapon_r
			// generally is what you want on the base mesh 
			if (USkeletalMeshComponent* BodyMesh = GetBodyMesh())
			{
				if (BodyMesh->DoesSocketExist(AttachSocket))
				{
					ComponentToUse = BodyMesh;
				}
			}

			//Otherwise attach it the the main mesh as a fallback - we never want to not attach it. 
			if (!ComponentToUse)
			{
				if (USkeletalMeshComponent* MainMesh = GetMainMesh())
				{
					if (MainMesh->DoesSocketExist(AttachSocket))
					{
						ComponentToUse = MainMesh;
					}
				}
			}

			if (ComponentToUse && WeaponVisual->WeaponMesh)
			{
				//FString RoleStr = HasAuthority() ? "Server" : "Client";
				//UE_LOG(LogNarrativeNet, Warning, TEXT("%s: attaching %s to %s with socket %s"), *RoleStr, *GetNameSafe(WeaponVisual->WeaponMesh), *GetNameSafe(ComponentToUse), *AttachSocket.ToString());

				WeaponVisual->WeaponMesh->AttachToComponent(ComponentToUse, FAttachmentTransformRules::KeepRelativeTransform, AttachSocket);
				WeaponVisual->WeaponMesh->SetRelativeTransform(AttachOffset);
			}
			else
			{
				UE_LOG(LogNarrativeCharacterVisual, Warning, TEXT("ANarrativeCharacterVisual::AttachWeaponVisual could not find a suitable mesh to attach your weapon to, as socket %s didn't exist."), *AttachSocket.ToString());
			}

			//Next attach the local weapon to the local mesh.
			if (WeaponVisual->LocalWeaponMesh)
			{
				USkeletalMeshComponent* LocalComponentToUse = GetLocalMesh();

				//Holstering should attach to character mesh rather than local mesh, since local mesh doesnt have a holster socket. 
				if (!WieldSlot.IsValid())
				{
					LocalComponentToUse = ComponentToUse;
				}

				if (LocalComponentToUse)
				{
					if (LocalComponentToUse->DoesSocketExist(AttachSocket))
					{
						UE_LOG(LogNarrativeNet, Warning, TEXT("%s %s ATTACH LOCAL COMP"), *LocalStr, *RoleStr);
						WeaponVisual->LocalWeaponMesh->AttachToComponent(LocalComponentToUse, FAttachmentTransformRules::KeepRelativeTransform, AttachSocket);
						WeaponVisual->LocalWeaponMesh->SetRelativeTransform(AttachOffset);
					}
					else
					{
						UE_LOG(LogNarrativeCharacterVisual, Warning, TEXT("ANarrativeCharacterVisual::AttachWeaponVisual could not find a suitable mesh to attach your local weapon to, as socket %s didn't exist."), *AttachSocket.ToString());
					}
				}
			}

			//TODO this function gets called more than we may expect, leading to this getting called multiple times which may not be desired. 
			if (!WieldSlot.IsValid())
			{
				WeaponVisual->OnHolstered();
			}
			else
			{
				WeaponVisual->OnWielded();
			}
		}
	}
}

void ANarrativeCharacterVisual::RemoveWeaponVisual(const FGameplayTag& WeaponSlot)
{
	//Destroy the weapon visual - client will update SpawnedWeaponVisuals when their weapon destroys locally
	if (HasAuthority())
	{
		if (SpawnedWeaponVisuals.Contains(WeaponSlot))
		{
			if (AWeaponVisual* WeaponVisual = SpawnedWeaponVisuals[WeaponSlot])
			{
				WeaponVisual->Destroy();
			}
		}

		UE_LOG(LogNarrativeNet, Warning, TEXT("%s removing weapon visual at slot %s"), *GetNameSafe(OwnerCharacter), *WeaponSlot.ToString());

		SpawnedWeaponVisuals.Remove(WeaponSlot);
	}

}

bool ANarrativeCharacterVisual::IsLocallyControlled()
{
	if (ANarrativeCharacter* OwnedChar = GetOwnerCharacter())
	{
		return OwnedChar && OwnedChar->IsPlayerControlled() && OwnedChar->IsLocallyControlled();
	}

	return false; 
}

bool ANarrativeCharacterVisual::Is1PMeshTag(const FGameplayTag& MeshSlot) const
{
	return MeshSlot == FNarrativeGameplayTags::Get().Equipment_Slot_Hands_1P || MeshSlot == FNarrativeGameplayTags::Get().Equipment_Slot_Torso_1P;
}

void ANarrativeCharacterVisual::OnRep_AppearanceAsset()
{
	if (AppearanceAsset)
	{
		FString RoleStr = HasAuthority() ? "Server" : "Client";
		UE_LOG(LogNarrativeNet, Warning, TEXT("%s: OnRep_AppearanceAsset is %s, owner is %s"), *RoleStr, *GetNameSafe(AppearanceAsset), *GetNameSafe(Owner));

		//If this check fails due to Owner not being ready, OnRep_Owner will fix this up. 
		if (INarrativeCharacterOwner* CharOwnerInterface = Cast<INarrativeCharacterOwner>(GetOwner()))
		{
			if (ANarrativeCharacter* NChar = CharOwnerInterface->GetNarrativeCharacter())
			{
				//Ensure we don't apply appearance multiple times. 
				if (AppliedAppearanceAsset != AppearanceAsset)
				{
					AppliedAppearanceAsset = AppearanceAsset;
					InitializeFromCharacterAndAttributes(NChar, AppearanceAsset->GetAppearanceAttributes(NChar));
				}
			}
		}
	}
}

class AWeaponVisual* ANarrativeCharacterVisual::GetWeaponVisual(const FGameplayTag& WeaponSlot) const
{
	if (SpawnedWeaponVisuals.Contains(WeaponSlot))
	{
		return SpawnedWeaponVisuals[WeaponSlot];
	}

	return nullptr; 
}

void ANarrativeCharacterVisual::SetGroomAppearance(FGameplayTag Slot, const FCharacterCreatorAttribute_Groom& GroomData)
{
	//In order for groom attributes to not be hard loaded, we soft ref them, and as such need to load them now. 
	if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
	{
		if (GroomLoadHandles.Contains(Slot))
		{
			if (GroomLoadHandles[Slot].IsValid())
			{
				GroomLoadHandles[Slot]->CancelHandle();
				GroomLoadHandles[Slot].Reset();
			}
		}

		TArray<FSoftObjectPath> GroomDataAssetPaths;

		if (!GroomData.GroomAsset.ToSoftObjectPath().IsValid())
		{
			return;
		}
		GroomDataAssetPaths.Add(GroomData.GroomAsset.ToSoftObjectPath());
		
		if (!GroomData.GroomBindingAsset.ToSoftObjectPath().IsValid())
		{
			return;
		}
		GroomDataAssetPaths.Add(GroomData.GroomBindingAsset.ToSoftObjectPath());
		
		for (auto& GroomMat : GroomData.GroomMaterials)
		{
			FSoftObjectPath MaterialPath = GroomMat.Material.ToSoftObjectPath();

			if (MaterialPath.IsValid())
			{
				GroomDataAssetPaths.Add(MaterialPath);
			}

		}

		FStreamableDelegate GroomLoadDel = FStreamableDelegate::CreateUObject(this, &ANarrativeCharacterVisual::OnGroomAppearanceReady, Slot, GroomData);
		TSharedPtr<FStreamableHandle> Handle = Manager->LoadAssetList(GroomDataAssetPaths, GroomLoadDel);
		GroomLoadHandles.Add(Slot, Handle);
	}
}

void ANarrativeCharacterVisual::SetMeshAppearance(FGameplayTag Slot, const FCharacterCreatorAttribute_Mesh& MeshData)
{
	//In order for mesh attributes to not be hard loaded, we soft ref them, and as such need to load them now. 
	if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
	{
		if (MeshLoadHandles.Contains(Slot))
		{
			if (MeshLoadHandles[Slot].IsValid())
			{
				MeshLoadHandles[Slot]->CancelHandle();
				MeshLoadHandles[Slot].Reset();
			}
		}

		TArray<FSoftObjectPath> MeshDataAssetPaths;

		if (MeshData.Mesh.ToSoftObjectPath().IsValid())
		{
			MeshDataAssetPaths.Add(MeshData.Mesh.ToSoftObjectPath());
			MeshDataAssetPaths.Add(MeshData.Mesh1P.ToSoftObjectPath());
			MeshDataAssetPaths.Add(MeshData.StaticMesh.ToSoftObjectPath());
		}

		for (auto& MeshMat : MeshData.MeshMaterials)
		{
			FSoftObjectPath MaterialPath = MeshMat.Material.ToSoftObjectPath();

			if (MaterialPath.IsValid())
			{
				MeshDataAssetPaths.Add(MaterialPath);
			}
		}

		FStreamableDelegate MeshLoadDel = FStreamableDelegate::CreateUObject(this, &ANarrativeCharacterVisual::OnMeshAppearanceReady, Slot, MeshData);
		TSharedPtr<FStreamableHandle> Handle = Manager->LoadAssetList(MeshDataAssetPaths, MeshLoadDel);
		MeshLoadHandles.Add(Slot, Handle);
	}
}

void ANarrativeCharacterVisual::ResetMeshToBaseAppearance(FGameplayTag Slot)
{
	if (USkeletalMeshComponent* MeshComp = GetSkeletalMeshComponent(Slot))
	{
		
		//Check if this slot is already hiding something, and unhide it if so. 
		if (CurrentHides.Contains(Slot))
		{
			for (auto& HiddenSlot : CurrentHides[Slot])
			{
				if (UMeshComponent* MeshToUnhide = GetMeshComponent(HiddenSlot))
				{
					MeshToUnhide->SetVisibility(true, true);
				}
			}

			CurrentHides.Remove(Slot);
		}

		
		if (AppearanceAttributeSet.Meshes.Contains(Slot))
		{
			FCharacterCreatorAttribute_Mesh MeshData = AppearanceAttributeSet.Meshes[Slot];
			SetMeshAppearance(Slot, MeshData);
		}
		else //No attribute data exists for this slot, just clear it to empty 
		{
			MeshComp->SetSkeletalMesh(nullptr);

			//Hands and torso slots need to update our 1P meshes also 
			if (Slot == FNarrativeGameplayTags::Get().Equipment_Slot_Hands)
			{
				ResetMeshToBaseAppearance(FNarrativeGameplayTags::Get().Equipment_Slot_Hands_1P);
			}
			else if (Slot == FNarrativeGameplayTags::Get().Equipment_Slot_Torso)
			{
				ResetMeshToBaseAppearance(FNarrativeGameplayTags::Get().Equipment_Slot_Torso_1P);
			}
		}
	}
	else if (UStaticMeshComponent* SMComp = GetStaticMeshComponent(Slot))
	{
		if (AppearanceAttributeSet.Meshes.Contains(Slot))
		{
			FCharacterCreatorAttribute_Mesh MeshData = AppearanceAttributeSet.Meshes[Slot];
			SetMeshAppearance(Slot, MeshData);
		}
		else //No attribute data exists for this slot, just clear it to empty 
		{
			SMComp->SetStaticMesh(nullptr);
		}
	}

	OnAppearancePartChanged.Broadcast(Slot);
}

void ANarrativeCharacterVisual::OnBaseMeshesReady()
{
	bBaseAppearanceLoaded = true;

	UE_LOG(LogNarrativeCharacterVisual, Verbose, TEXT("%s: Applying base meshes"), *OwnerCharacter->GetHumanReadableName());

	for (auto& BodyPart : AppearanceAttributeSet.Meshes)
	{
		OnMeshAppearanceReady(BodyPart.Key, BodyPart.Value);
	}

	OnBaseAppearanceApplied.Broadcast();
	
	if (OwnerCharacter)
	{
		//Seemed that rarely this would init before pointer repped back? Either way just set it here beforehand to make sure. 
		if (!OwnerCharacter->CharVisual)
		{
			OwnerCharacter->CharVisual = this;
		}
		
		OwnerCharacter->OnCharacterVisualInitialized();
	}
	else
	{
		UE_LOG(LogNarrativeNet, Warning, TEXT("BADTHING OwnerCharacter was null. GetOwner is %s, CharacterOwner ptr is %s"), *GetNameSafe(GetOwner()), *GetNameSafe(OwnerCharacter));
	}
	
	BaseAppearanceApplied();

	if (BaseAppearanceLoadHandle)
	{
		BaseAppearanceLoadHandle.Reset();
	}
}

void ANarrativeCharacterVisual::OnMeshAppearanceReady(FGameplayTag Slot, FCharacterCreatorAttribute_Mesh MeshData)
{
	if (MeshLoadHandles.Contains(Slot))
	{
		if (MeshLoadHandles[Slot].IsValid())
		{
			MeshLoadHandles[Slot]->CancelHandle();
		}
		MeshLoadHandles[Slot].Reset();
		MeshLoadHandles.Remove(Slot);
	}

	//Check if this slot is already hiding something, and unhide it if so. 
	if (CurrentHides.Contains(Slot))
	{
		for (auto& HiddenSlot : CurrentHides[Slot])
		{
			if (UMeshComponent* MeshToUnhide = GetMeshComponent(HiddenSlot))
			{
				MeshToUnhide->SetVisibility(true, true);
			}
		}

		CurrentHides.Remove(Slot);
	}

	//Check if new mesh in this slot needs any hides, and add them if so. 
	if (!MeshData.HideSlots.IsEmpty())
	{
		for (auto& HideSlot : MeshData.HideSlots)
		{
			FGameplayTagContainer NewHides;
			if (UMeshComponent* MeshToHide = GetMeshComponent(HideSlot))
			{
				NewHides.AddTagFast(HideSlot);
				MeshToHide->SetVisibility(false, true);
			}

			CurrentHides.FindOrAdd(Slot, NewHides);
		}
	}


	if (MeshData.bIsStaticMesh)
	{
		if (UStaticMeshComponent* MeshComp = GetOrCreateStaticMeshComponent(Slot))
		{
			//Want to to ensure it actually loaded 
			if (!(!MeshData.StaticMesh.ToSoftObjectPath().IsValid() || MeshData.StaticMesh.IsValid()))
			{
				UE_LOG(LogNarrativeCharacterVisual, Warning, TEXT("%s: Setting slot %s but mesh was null"), *OwnerCharacter->GetHumanReadableName(), *Slot.ToString());
			}

			UStaticMesh* MeshAsset = MeshData.StaticMesh.LoadSynchronous();

			if (OwnerCharacter)
			{
				UE_LOG(LogNarrativeCharacterVisual, Verbose, TEXT("%s: Setting slot %s static mesh to %s"), *OwnerCharacter->GetHumanReadableName(), *Slot.ToString(), *GetNameSafe(MeshAsset));
			}

			MeshComp->SetStaticMesh(MeshAsset);

			MeshComp->AttachToComponent(GetMainMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, MeshData.MeshAttachSocket);
			MeshComp->SetRelativeTransform(MeshData.MeshAttachOffset);

			if (IsValid(MeshAsset))
			{
				//Check if we have any custom materials to apply the newly created mesh
				for (int32 i = 0; i < MeshAsset->GetStaticMaterials().Num(); ++i)
				{
					if (MeshData.MeshMaterials.IsValidIndex(i))
					{
						FCreatorMeshMaterial MeshMat = MeshData.MeshMaterials[i];

						//Set the material and apply parameters if required 
						if (MeshMat.VectorParams.Num() <= 0 && MeshMat.ScalarParams.Num() <= 0)
						{
							MeshComp->SetMaterial(i, MeshMat.Material.LoadSynchronous());
						}
						else
						{
							UMaterialInstanceDynamic* MID = MeshComp->CreateDynamicMaterialInstance(i, MeshMat.Material.LoadSynchronous());

							for (auto& VParam : MeshMat.VectorParams)
							{
								if (VParam.VectorTagID.IsValid())
								{
									const FLinearColor ParameterValue = AppearanceAttributeSet.GetVectorValue(VParam.VectorTagID);

									for (auto& VParamName : VParam.ParameterNames)
									{
										MID->SetVectorParameterValue(VParamName, ParameterValue);
									}
								}
							}

							for (auto& SParam : MeshMat.ScalarParams)
							{
								if (SParam.ScalarTagID.IsValid())
								{
									const float ParameterValue = AppearanceAttributeSet.GetScalarValue(SParam.ScalarTagID);

									for (auto& SParamName : SParam.ParameterNames)
									{
										MID->SetScalarParameterValue(SParamName, ParameterValue);
									}
								}
							}
						}
					}
				}
			}
		}
	}
	else
	{
		if (USkeletalMeshComponent* MeshComp = GetOrCreateMeshComponent(Slot))
		{
			//Want to to ensure it actually loaded 
			if (!(!MeshData.Mesh.ToSoftObjectPath().IsValid() || MeshData.Mesh.IsValid()))
			{
				UE_LOG(LogNarrativeCharacterVisual, Warning, TEXT("%s: Setting slot %s but mesh was null"), *OwnerCharacter->GetHumanReadableName(), *Slot.ToString());
			}

			USkeletalMesh* MeshAsset = MeshData.Mesh.LoadSynchronous();

			if (Is1PMeshTag(Slot) && MeshData.Mesh1P.ToSoftObjectPath().IsValid())
			{
				MeshAsset = MeshData.Mesh1P.LoadSynchronous();
			}

			if (OwnerCharacter)
			{
				UE_LOG(LogNarrativeCharacterVisual, Verbose, TEXT("%s: Setting slot %s mesh to %s"), *OwnerCharacter->GetHumanReadableName(), *Slot.ToString(), *GetNameSafe(MeshAsset));
			}

			//Keep 1P Hands & Torso synced with 3P meshes. 
			if (IsLocallyControlled())
			{
				if (Slot == FNarrativeGameplayTags::Get().Equipment_Slot_Torso)
				{
					OnMeshAppearanceReady(FNarrativeGameplayTags::Get().Equipment_Slot_Torso_1P, MeshData);
				}

				if (Slot == FNarrativeGameplayTags::Get().Equipment_Slot_Hands)
				{
					OnMeshAppearanceReady(FNarrativeGameplayTags::Get().Equipment_Slot_Hands_1P, MeshData);
				}
			}

			MeshComp->SetSkeletalMesh(MeshAsset);

			if (!MeshData.MeshAttachSocket.IsNone())
			{
				MeshComp->AttachToComponent(GetMainMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, MeshData.MeshAttachSocket);
			}

			MeshComp->SetRelativeTransform(MeshData.MeshAttachOffset);

			if (MeshData.bUseLeaderPose)
			{
				//Local hands and torso should actually follow local arms mesh
				if (IsValid(GetLocalMesh()) && Is1PMeshTag(Slot))
				{
					MeshComp->SetLeaderPoseComponent(GetLocalMesh());
					MeshComp->SetCastShadow(false);
				}
				else
				{
					//Shouldnt happen, we dont want arms/hands 1P parenting to leader 
					MeshComp->SetLeaderPoseComponent(GetLeaderMesh());
				}
			}
			else
			{
				MeshComp->SetLeaderPoseComponent(nullptr);
				MeshComp->SetAnimInstanceClass(MeshData.MeshAnimBP.LoadSynchronous());
			}

			if (IsValid(MeshAsset))
			{
				//Check if we have any custom materials to apply the newly created mesh
				for (int32 i = 0; i < MeshAsset->GetMaterials().Num(); ++i)
				{
					if (MeshData.MeshMaterials.IsValidIndex(i))
					{
						FCreatorMeshMaterial MeshMat = MeshData.MeshMaterials[i];

						//Set the material and apply parameters if required 
						if (MeshMat.VectorParams.Num() <= 0 && MeshMat.ScalarParams.Num() <= 0)
						{
							MeshComp->SetMaterial(i, MeshMat.Material.LoadSynchronous());
						}
						else
						{
							UMaterialInstanceDynamic* MID = MeshComp->CreateDynamicMaterialInstance(i, MeshMat.Material.LoadSynchronous());

							for (auto& VParam : MeshMat.VectorParams)
							{
								if (VParam.VectorTagID.IsValid())
								{
									const FLinearColor ParameterValue = AppearanceAttributeSet.GetVectorValue(VParam.VectorTagID);

									for (auto& VParamName : VParam.ParameterNames)
									{
										MID->SetVectorParameterValue(VParamName, ParameterValue);
									}
								}
							}

							for (auto& SParam : MeshMat.ScalarParams)
							{
								if (SParam.ScalarTagID.IsValid())
								{
									const float ParameterValue = AppearanceAttributeSet.GetScalarValue(SParam.ScalarTagID);

									for (auto& SParamName : SParam.ParameterNames)
									{
										MID->SetScalarParameterValue(SParamName, ParameterValue);
									}
								}
							}
						}
					}
				}

				//Set the morphs
				for (auto& Morph : MeshData.Morphs)
				{
					if (Morph.ScalarTag.IsValid())
					{
						const float MorphVal = AppearanceAttributeSet.GetScalarValue(Morph.ScalarTag);

						for (auto& MorphName : Morph.MorphNames)
						{
							MeshComp->SetMorphTarget(MorphName, MorphVal);
						}
					}
				}
			}
		}
	}

	OnAppearancePartChanged.Broadcast(Slot);
}

void ANarrativeCharacterVisual::OnGroomAppearanceReady(FGameplayTag Slot, FCharacterCreatorAttribute_Groom GroomData)
{
	if (GroomLoadHandles.Contains(Slot))
	{
		if (GroomLoadHandles[Slot].IsValid())
		{
			GroomLoadHandles[Slot]->CancelHandle();
		}
		GroomLoadHandles[Slot].Reset();
		GroomLoadHandles.Remove(Slot);
	}

	if (UGroomComponent* Groom = GetOrCreateGroomComponent(Slot))
	{
		Groom->SetBindingAsset(GroomData.GroomBindingAsset.LoadSynchronous());
		Groom->SetGroomAsset(GroomData.GroomAsset.LoadSynchronous());

		//Check if we have any custom materials to apply the newly created groom - we dont async load groom mats but that should be okay as they are very small
		for (int32 i = 0; i < Groom->GetMaterials().Num(); ++i)
		{
			if (GroomData.GroomMaterials.IsValidIndex(i))
			{
				FCreatorMeshMaterial GroomMat = GroomData.GroomMaterials[i];

				Groom->SetMaterial(i, GroomMat.Material.LoadSynchronous());

				//Set the material and apply parameters if required 
				if (GroomMat.VectorParams.Num() <= 0 && GroomMat.ScalarParams.Num() <= 0)
				{
					Groom->SetMaterial(i, GroomMat.Material.LoadSynchronous());
				}
				else
				{
					UMaterialInstanceDynamic* MID = Groom->CreateDynamicMaterialInstance(i, GroomMat.Material.LoadSynchronous());

					for (auto& VParam : GroomMat.VectorParams)
					{
						if (VParam.VectorTagID.IsValid())
						{
							const FLinearColor ParameterValue = AppearanceAttributeSet.GetVectorValue(VParam.VectorTagID);

							for (auto& VParamName : VParam.ParameterNames)
							{
								MID->SetVectorParameterValue(VParamName, ParameterValue);
							}
						}
					}

					for (auto& SParam : GroomMat.ScalarParams)
					{
						if (SParam.ScalarTagID.IsValid())
						{
							const float ParameterValue = AppearanceAttributeSet.GetScalarValue(SParam.ScalarTagID);

							for (auto& SParamName : SParam.ParameterNames)
							{
								MID->SetScalarParameterValue(SParamName, ParameterValue);
							}
						}
					}
				}

			}
		}
	}

	OnAppearancePartChanged.Broadcast(Slot);
}

void ANarrativeCharacterVisual::OnWeaponVisualClassReady(class UWeaponItem* WeaponItem)
{
	if (WeaponItem)
	{
		FGameplayTag Slot = WeaponItem->CurrentSlot;
	
		//Clear out any old weapon visual 
		//RemoveWeaponVisual(Slot);

		if (WeaponLoadHandles.Contains(Slot))
		{
			if (WeaponLoadHandles[Slot].IsValid())
			{
				WeaponLoadHandles[Slot]->ReleaseHandle();
			}
			WeaponLoadHandles[Slot].Reset();
			WeaponLoadHandles.Remove(Slot);
		}
		
		check(WeaponItem->WeaponVisualClass.IsValid());
		
		if (WeaponItem->WeaponVisualClass && OwnerCharacter && OwnerCharacter->HasAuthority())
		{
			if (AWeaponVisual* SpawnedWeaponVisual = GetWorld()->SpawnActorDeferred<AWeaponVisual>(WeaponItem->WeaponVisualClass.LoadSynchronous(), FTransform::Identity, GetOwner()))
			{
				SpawnedWeaponVisual->WeaponOwner = WeaponItem;
				SpawnedWeaponVisual->VisualOwner = this; 
				
				//Fill out the struct, this should all rep at once to clients meaning everything should safely replicate for our attach 
				SpawnedWeaponVisual->AttachState.VisualOwner = this;
				SpawnedWeaponVisual->AttachState.WeaponOwner = WeaponItem;
				SpawnedWeaponVisual->AttachState.CharOwner = OwnerCharacter;
				SpawnedWeaponVisual->AttachState.EquippedSlot = Slot;
				SpawnedWeaponVisual->AttachState.WieldedSlot = WeaponItem->WieldedSlot;
				
				SpawnedWeaponVisuals.Add(Slot, SpawnedWeaponVisual);

				SpawnedWeaponVisual->FinishSpawning(FTransform::Identity, /*bIsDefaultTransform=*/ true);

				//SpawnedWeaponVisual->UpdateWeaponAttachment();
				SpawnedWeaponVisual->OnRep_AttachState();
				
				//AttachWeaponVisual(WeaponItem, true);
			}
		}

		//If our character has saved wields, we're waiting for weapon visuals to load. Check if they are ready. Not multiplayer atm. 
		if (GetNetMode() == NM_Standalone && OwnerCharacter && !OwnerCharacter->SavedWieldState.EquipSlots.IsEmpty())
		{
			bool bReadyToRestoreWields = true;

			for (auto& EquipSlot : OwnerCharacter->SavedWieldState.EquipSlots)
			{
				if (!GetWeaponVisual(EquipSlot))
				{
					bReadyToRestoreWields = false; 
					break;
				}  
			}

			if (bReadyToRestoreWields)
			{
				OwnerCharacter->SetWieldState(OwnerCharacter->SavedWieldState);
			}
		}

	}
}

void ANarrativeCharacterVisual::BaseAppearanceApplied_Implementation()
{
	//Want to start loading grooms once the meshes are applied, otherwise they will have binding issues 
	for (auto& GroomPart : AppearanceAttributeSet.Grooms)
	{
		SetGroomAppearance(GroomPart.Key, GroomPart.Value);
	}
}

void ANarrativeCharacterVisual::SetAnimBPOverride(TSubclassOf<class UAnimInstance> NewAnimBP)
{
	if (USkeletalMeshComponent* MainMesh = GetMainMesh())
	{
		if (IsValid(NewAnimBP))
		{
			MainMesh->SetAnimInstanceClass(NewAnimBP);
		}
		else
		{
			MainMesh->SetAnimInstanceClass(AppearanceAttributeSet.BaseMeshAnimBP);
		}

		// Runtime AnimBP overrides recreate the AnimInstance. Keep GAS montage
		// replication pointed at the replacement instead of the destroyed instance.
		if (ANarrativeCharacter* NarrativeCharacter = OwnerCharacter.Get())
		{
			if (UAbilitySystemComponent* ASC = NarrativeCharacter->GetAbilitySystemComponent())
			{
				ASC->RefreshAbilityActorInfo();
			}
		}
	}
}

void ANarrativeCharacterVisual::ClearAnimBPOverride()
{
	SetAnimBPOverride(nullptr);
}

USkeletalMeshComponent* ANarrativeCharacterVisual::GetSkeletalMeshComponent(const FGameplayTag& Slot) const
{
	if (MeshComponents.Contains(Slot))
	{
		return MeshComponents[Slot];
	}

	return nullptr; 
}

UStaticMeshComponent* ANarrativeCharacterVisual::GetStaticMeshComponent(const FGameplayTag& Slot) const
{
	if (StaticMeshComponents.Contains(Slot))
	{
		return StaticMeshComponents[Slot];
	}

	return nullptr;
}

bool ANarrativeCharacterVisual::HasLoadHandles() const
{
	return WeaponLoadHandles.Num() > 0 || MeshLoadHandles.Num() > 0 || GroomLoadHandles.Num() > 0;
}
