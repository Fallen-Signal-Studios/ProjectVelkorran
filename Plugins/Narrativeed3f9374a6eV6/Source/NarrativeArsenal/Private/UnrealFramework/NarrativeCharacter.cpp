// Copyright Narrative Tools 2024. 


#include "UnrealFramework/NarrativeCharacter.h"
#include "Net/UnrealNetwork.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/AbilityConfiguration.h"
#include "Navigation/NavigationMarkerComponent.h"
#include <Components/SkeletalMeshComponent.h>
#include <Components/CapsuleComponent.h>
#include "Components/EquipmentComponent.h"
#include "Character/NarrativeCharacterMovement.h"
#include <GameFramework/CharacterMovementComponent.h>
#include <GameFramework/PlayerState.h>
#include <MotionWarpingComponent.h>
#include "Items/EquippableItem.h"
#include <GameplayEffectTypes.h>
#include <GameplayEffectExtension.h>
#include "NarrativeGameplayTags.h"
#include <AbilitySystemGlobals.h>
#include "Items/WeaponItem.h"
#include <Runtime/AIModule/Classes/Perception/AISense_Damage.h>
#include <UObject/ConstructorHelpers.h>
#include "Engine/World.h"
#include <Engine/Texture2D.h>
#include "Character/CharacterAppearance.h"
#include "Character/NarrativeCharacterVisual.h"
#include <GroomComponent.h>
#include "Character/CharacterDefinition.h"
#include "Character/CharacterMapMarker.h"
#include <Materials/MaterialInstanceDynamic.h>
#include "Tales/NarrativeEvent.h"
#include "Tales/NarrativeCondition.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "Tales/NarrativeTrigger.h"
#include <Engine/AssetManager.h>
#include "PoseSearch/PoseSearchLibrary.h"
#include "ArsenalSettings.h"
#include "Tales/TriggerSet.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "SaveSystemStatics.h"
#include "UnrealFramework/NarrativeGameMode.h"
#include "ArsenalStatics.h"
#include "ChooserFunctionLibrary.h"
#include "NarrativeLogChannels.h"
#include "Navigation/NavigatorGameplayTags.h"
#include "Interaction/InteractionComponent.h"
#include "UnrealFramework/NarrativeAnimInstance.h"
#include "NavigationSystem.h"
#include "GameFramework/WorldSettings.h"
#include "Vehicles/NarrativeVehicleBase.h"
#include "Weapons/WeaponVisual.h"


bool FWeaponWieldState::IsValidWieldState() const
{
	//Basically the only time an equip state isnt valid is if we have equipslots, but weapon pointers aren't valid because they havent repped yet.
	if (!EquipSlots.IsEmpty())
	{
		for (auto& Weap : EquipWeapons)
		{
			if (!IsValid(Weap))
			{
				return false; 
			}
		}
	}

	return true; 
}

// Sets default values
ANarrativeCharacter::ANarrativeCharacter(const class FObjectInitializer& ObjectInitializer) : 
	Super(ObjectInitializer.SetDefaultSubobjectClass<UNarrativeCharacterMovement>(ACharacter::CharacterMovementComponentName))
{
	
	SetReplicates(true);

	//We use a Lyra style setup where we use a hidden mesh and leader pose the other meshes to follow it. So hide main mesh, its simply there to drive anims
	GetMesh()->SetVisibility(false);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->ComponentTags.Add("Body");

	GetMesh()->SetCollisionResponseToChannel(TraceChannel_NarrativeProjectile, ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(TraceChannel_NarrativeCover, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(TraceChannel_NarrativeTraversable, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(TraceChannel_NarrativeClimbable, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(TraceChannel_NarrativeInteraction, ECR_Ignore);

	GetCapsuleComponent()->SetCollisionResponseToChannel(TraceChannel_NarrativeWeapon, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(TraceChannel_NarrativeProjectile, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(TraceChannel_NarrativeCover, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(TraceChannel_NarrativeTraversable, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(TraceChannel_NarrativeClimbable, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(TraceChannel_NarrativeInteraction, ECR_Ignore);

	InventoryComponent = CreateDefaultSubobject<UNarrativeInventoryComponent>("InventoryComponent");

	EquipmentComp = CreateDefaultSubobject<UEquipmentComponent>("EquipmentComp");

	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

	//Nice default values for level scaling. 
	LevelExponentX = 0.07f;
	LevelExponentY = 2.f;
	bReapplyAttributesOnLevelUp = false;
	
	CharacterRandomSeed = -1;
	BaseEyeHeight = 72.f;
	bIsRagdoll = false; 
	bWantsMapMarker = true;

	DefaultCapsuleRotationSetting = ECapsuleRotationSetting::OrientTowardsMovement;
}

ANarrativeCharacter* ANarrativeCharacter::GetNarrativeCharacter() const
{
	ANarrativeCharacter* MutableThis = const_cast<ANarrativeCharacter*>(this);

	return MutableThis; 
}

void ANarrativeCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode /*= 0*/)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		//Add some nice tags so GAS can access move state via tags - could possibly use some sort of map but for now if statements suffice, keep it simple. 
		if (UNarrativeCharacterMovement* NCMC = GetNarrativeCharacterMovement())
		{
			if (NCMC->IsWalking())
			{
				ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Movement_Walking, 1);
			}
			else if(ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Movement_Walking))
			{
				ASC->SetLooseGameplayTagCount(FNarrativeGameplayTags::Get().State_Movement_Walking, 0);
			}

			if (NCMC->IsFalling())
			{
				ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Movement_Falling);
			}
			else if (ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Movement_Falling))
			{
				ASC->SetLooseGameplayTagCount(FNarrativeGameplayTags::Get().State_Movement_Falling, 0);
			}

			if (NCMC->IsSwimming())
			{
				ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Movement_Swimming);
			}
			else if (ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Movement_Swimming))
			{
				ASC->SetLooseGameplayTagCount(FNarrativeGameplayTags::Get().State_Movement_Swimming, 0);
			}

			if (NCMC->IsClimbing())
			{
				ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Movement_Climbing);
			}
			else if (ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Movement_Climbing))
			{
				ASC->SetLooseGameplayTagCount(FNarrativeGameplayTags::Get().State_Movement_Climbing, 0);
			}

			if (NCMC->IsRagdoll())
			{
				ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Movement_Ragdoll);
			}
			else if (ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Movement_Ragdoll))
			{
				ASC->SetLooseGameplayTagCount(FNarrativeGameplayTags::Get().State_Movement_Ragdoll, 0);
			}
		}
	}

}

bool ANarrativeCharacter::IsMoveInputIgnored() const
{
	return Super::IsMoveInputIgnored() || IsMovementLocked();
}

void ANarrativeCharacter::Destroyed()
{
	Super::Destroyed();

	if (CharVisual)
	{
		CharVisual->Destroy();
	}

	/**Destroy child actors when we are destroyed. */
	TArray<AActor*> ChildrenActors;
	GetAllChildActors(ChildrenActors);

	for (auto& Child : ChildrenActors)
	{
		if (Child)
		{
			Child->Destroy();
		}
	}
}

void ANarrativeCharacter::FellOutOfWorld(const class UDamageType& dmgType)
{
	//Super::FellOutOfWorld(dmgType);

	FVector ProjectedLoc; 

	if (AWorldSettings* Settings = GetWorld()->GetWorldSettings())
	{
		if (UNavigationSystemV1::K2_ProjectPointToNavigation(this, GetActorLocation(), ProjectedLoc, nullptr, nullptr, FVector(100.f, 100.f, 999999.f)))
		{
			SetActorLocation(ProjectedLoc + GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		}
		else
		{
			AddActorWorldOffset(FVector(0.f, 0.f, FMath::Abs(Settings->KillZ) + 1000.f));
		}
	}

}


void ANarrativeCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	float ZVel = GetVelocity().Z;

	if (ZVel < 0.f)
	{
		if (UNarrativeCharacterMovement* NCMC = GetNarrativeCharacterMovement())
		{
			const float FallDamagePct = NCMC->FallDamageCurve.GetRichCurve()->Eval(ZVel);

			if (FallDamagePct > 0.1f)
			{
				if (UNarrativeAbilitySystemComponent* NASC = Cast<UNarrativeAbilitySystemComponent>(GetAbilitySystemComponent()))
				{
					NASC->DealDamage(GetMaxHealth() * FallDamagePct);
				}
			}

			//Impacted steep ground, also filter out this happening right after spawning in 
			if (GetNetMode() == NM_Standalone && ZVel < NCMC->EnterRagdollFallZImpactGroundThreshold && GetWorld()->TimeSince(CreationTime) > 3.f)
			{
				RagdollForDuration(3.f);
			}

		}
	}

}

void ANarrativeCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	OnJumpedDelegate.Broadcast();
}

#if WITH_GAMEPLAY_DEBUGGER

void ANarrativeCharacter::DescribeSelfToGameplayDebugger(FGameplayDebuggerCategory* DebuggerCategory) const
{
	if (DebuggerCategory)
	{
		UWorld* World = GetWorld();

		if (!World)
		{
			return;
		}

		ANarrativeGameState* GS = Cast<ANarrativeGameState>(World->GetGameState());

		if (!GS)
		{
			return;
		}

		FString RoleStr = HasAuthority() ? "{green} Server" : "{red} Client";
		FString LocalStr = IsLocallyControlled() ? "Local" : "Remote";
		DebuggerCategory->AddTextLine(FString::Printf(TEXT("%s %s CHARACTER INFO FOR %s"), *LocalStr, *RoleStr, *GetCharacterName().ToString()));
		
		const float TOD = GS->GetTimeOfDay();
		DebuggerCategory->AddTextLine(FString::Printf(TEXT("Factions (%d)"), Triggers.Num()));

		for (auto& Faction : GetFactions())
		{
			if (Faction.IsValid())
			{
				DebuggerCategory->AddTextLine(*Faction.ToString());
			}
		}

		DebuggerCategory->AddTextLine(FString::Printf(TEXT("Time of day: %f. Day advance speed: %f"), TOD, GS->GetTimeOfDayAdvanceSpeed()));

		DebuggerCategory->AddTextLine(FString::Printf(TEXT("Triggers: (%d)"), Triggers.Num()));

		for (auto& Trigger : Triggers)
		{
			if (Trigger)
			{
				FString EvtString = "";

				for (auto& Evt : Trigger->TriggerEvents)
				{
					EvtString += Evt->GetGraphDisplayText() + ",";
				}

				if (!EvtString.Len())
				{
					EvtString = "Nothing";
				}

				if (Trigger->GetActive())
				{
					DebuggerCategory->AddTextLine(FString::Printf(TEXT("{green} On %s, %s (active)"), *Trigger->GetDescription(), *EvtString));
				}
				else
				{
					DebuggerCategory->AddTextLine(FString::Printf(TEXT("On %s,  %s"), *Trigger->GetDescription(), *EvtString));
				}

			}
		}

		DebuggerCategory->AddTextLine(TEXT("WIELD STATE:"));
		DebuggerCategory->AddTextLine(FString::Printf(TEXT("Equip Slots: %s"), *(WieldState.EquipSlots).ToString()));
		DebuggerCategory->AddTextLine(FString::Printf(TEXT("Equip Slots: %s"), *(WieldState.WieldSlots).ToString()));

		int32 i = 0;
		for (auto& WieldWeapon : WieldState.EquipWeapons)
		{
			DebuggerCategory->AddTextLine(FString::Printf(TEXT("WEAP %d: %s"), i, *GetNameSafe(WieldWeapon)));
			++i;
		}
		
		if (CharVisual)
		{
			DebuggerCategory->AddTextLine(FString::Printf(TEXT("{green} CHARVISUAL POINTER SET to %s"), *GetNameSafe(CharVisual)));

			for (auto& WeaponVisualKVP : CharVisual->SpawnedWeaponVisuals)
			{
				if (WeaponVisualKVP.Value)
				{
					FString Attached = WeaponVisualKVP.Value->bAttachedSuccesfully ? "ATTACHED" : "FAILEDATTACH";
					DebuggerCategory->AddTextLine(FString::Printf(TEXT("{green} Weapon Visual %s is in slot %s - %s"), *GetNameSafe(WeaponVisualKVP.Value), *WeaponVisualKVP.Key.ToString(), *Attached));
				}
				else
				{
					DebuggerCategory->AddTextLine(FString::Printf(TEXT("{red} Weapon Visual NONE is in slot %s"), *GetNameSafe(WeaponVisualKVP.Value)));
				}
			}
		}
		else
		{
			DebuggerCategory->AddTextLine(FString::Printf(TEXT("{red} CHARVISUAL POINTER NOT SET")));
		}

		if (UNarrativeAnimInstance* AnimInst = GetCharacterAnimInstance())
		{
			DebuggerCategory->AddTextLine(FString::Printf(TEXT("Override layer: %s"), *AnimInst->GetOverrideLayerTag().ToString()));

			FString HasASC = AnimInst->ApplyTagsHandle.IsValid() ? "YES" : "NO";
			DebuggerCategory->AddTextLine(FString::Printf(TEXT("HasASC: %s"), *HasASC));

		}


		bool bWantsLookAt;
		FVector LookAtLocation = GetHeadLookAtLocation(bWantsLookAt);

		if (bWantsLookAt)
		{
			DebuggerCategory->AddShape(FGameplayDebuggerShape::MakePoint(LookAtLocation, 30.0f, FColor::Red, "Look At Location"));
		}

	}
}

#endif 

void ANarrativeCharacter::HandleVehicleImpact_Implementation(class ANarrativeVehicleBase* Vehicle, UPrimitiveComponent* OverlappedComponent, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Vehicle)
	{
		const float VelSpeed = Vehicle->GetVelocity().Length();
		if (VelSpeed > 200.f)
		{
			if (GetAttachParentActor() != Vehicle && !IsRagdoll(false))
			{
				const float DesiredDamage = FMath::GetMappedRangeValueClamped(Vehicle->VehicleImpactCharacterDamage, FVector2D(GetMaxHealth() / 5.f, GetMaxHealth()), VelSpeed);
				const FVector ImpulseDir = GetVelocity().GetClampedToSize(300.f, 2000.f) + FVector(0.f, 0.f, 1800.f);
			
				//Use 0 as the damage amount - we need to apply vehicle damage using spec on vehicle, so target data is filled correctly. 
				RagdollWithDamageAndImpulse(3.f, ImpulseDir, 0.f);

				FHitResult Hit = SweepResult;

				//Hit is required for gas to have valid target data so just construct one if needed 
				if (!Hit.IsValidBlockingHit())
				{
					Hit = FHitResult(this, GetCapsuleComponent(), GetActorLocation(), Vehicle->GetActorRotation().Vector());
					Hit.bBlockingHit = true;
					Hit.bStartPenetrating = false; 
				}

				Vehicle->DealVehicleDamage(AbilitySystemComponent, DesiredDamage, Hit);
			}
		}
	} 
}

class UAbilitySystemComponent* ANarrativeCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float ANarrativeCharacter::CalcSightStrength_Implementation(const FVector& Start, const FVector& End, const AActor* Looker)
{
	//Basic implementation for calculating how strongly we can see someone, based on how far away they are, whether they are crouched, etc. BP can override. 
	const FVector Offset = Start - End;

	//3 meters or closer is full strength sighted, otherwise 50m or more is least sighted 
	const float DistanceSeenPct = FMath::GetMappedRangeValueClamped(FVector2D(300.f, 5000.f), FVector2D(1.f, 0.f), Offset.Length());
	const float Dot = FVector::DotProduct(Offset.GetSafeNormal(), GetActorForwardVector());
	const bool bCrouched = GetCharacterMovement()->IsCrouching();

	return DistanceSeenPct * (bCrouched ? 0.5f : 1.f);
}

bool ANarrativeCharacter::IsAlive() const
{
	if (AbilitySystemComponent)
	{
		return !AbilitySystemComponent->IsDead();
	}

	return true; 
}

FVector ANarrativeCharacter::GetRootBoneLocation() const
{
	return GetFloorLocation(2.f);
}

FVector ANarrativeCharacter::GetFloorLocation(const float ZOffset) const
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		return GetActorLocation() - FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight()) + FVector(0.f, 0.f, ZOffset);
	}
	return GetActorLocation();
}

UCharacterDefinition* ANarrativeCharacter::GetCharacterDefinition() const
{
	return nullptr;
}

class UNarrativeInventoryComponent* ANarrativeCharacter::GetInventoryComponent() const
{
	return InventoryComponent;
}

class UNarrativeInteractionComponent* ANarrativeCharacter::GetInteractionComponent() const
{
	if (GetController())
	{
		return Cast<UNarrativeInteractionComponent>(GetController()->GetComponentByClass(UNarrativeInteractionComponent::StaticClass()));
	}

	return nullptr;
}

AController* ANarrativeCharacter::GetOwningController() const
{
	return GetController();
}

void ANarrativeCharacter::OnDefinitionSet_Implementation(UCharacterDefinition* NewDefinition)
{
	//When our characters definition is set, try loading all the data we need now that we're spawned, such as our appearance, dialogue, and so on. 
	if (IsValid(NewDefinition))
	{
		if (UNarrativeAbilitySystemComponent* NarrativeASC =
			Cast<UNarrativeAbilitySystemComponent>(GetAbilitySystemComponent()))
		{
			NarrativeASC->SetDefinitionOwnedTags(NewDefinition->DefaultOwnedTags);
		}
		else if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			ASC->AddLooseGameplayTags(NewDefinition->DefaultOwnedTags);
		}

		if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
		{
			FStreamableDelegate LoadDel = FStreamableDelegate::CreateUObject(this, &ANarrativeCharacter::HandleCharacterDefinitionDataLoaded, NewDefinition->GetPrimaryAssetId());

			//Load players stuff before we load NPC stuff  
			const TAsyncLoadPriority Priority = IsPlayerControlled() ? 1 : 0;
			CharacterDefinitionLoadHandle = Manager->LoadPrimaryAsset(NewDefinition->GetPrimaryAssetId(), {"SpawnedData"}, LoadDel, Priority);
		}
	}
}

void ANarrativeCharacter::OnCharacterDefinitionDataLoaded(FPrimaryAssetId LoadedId)
{
	if (UCharacterDefinition* CDef = GetCharacterDefinition())
	{
		//Our characters data is ready. Lets apply the appearance, trigger sets, and so on 
		if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
		{
			//1. Set the characters appearance. 
			if (TSoftObjectPtr<UCharacterAppearanceBase> BaseAppearance = GetDefaultAppearance())
			{
				if (UCharacterAppearance* LoadedAppearance = Cast<UCharacterAppearance>(BaseAppearance.LoadSynchronous()))
				{
					if (HasAuthority())
					{
						ChangeAppearance(LoadedAppearance);
					}
				}
			}
			else
			{
				checkf(false, TEXT("Appearance for %s not yet valid! Is your appearance/chardef in a plugin content folder? If so, move it to the main Game content folder to fix this issue."), *GetNameSafe(this));
			}

			//2. Apply any trigger sets our character needs 
			TArray<UTriggerSet*> DefaultTriggers;

			for (auto& TriggerSoftPtr : GetDefaultTriggerSets())
			{
				if (TriggerSoftPtr.IsValid())
				{
					if (UTriggerSet* TSet = TriggerSoftPtr.LoadSynchronous())
					{
						DefaultTriggers.Add(TSet);
					}
				}
				else
				{
					UE_LOG(LogNarrativeCharacter, Warning, TEXT("Null trigger set for character %s!"), *GetNameSafe(this));
				}
			}

			if (DefaultTriggers.Num())
			{
				ApplyTriggerSets(DefaultTriggers);
			}

			CharacterDefinitionLoadHandle.Reset();
		}
	}
}

void ANarrativeCharacter::OnPostCharacterDefinitionDataLoaded(FPrimaryAssetId LoadedId)
{

}

void ANarrativeCharacter::HandleCharacterDefinitionDataLoaded(FPrimaryAssetId LoadedId)
{
	//Causes the character to initialize from the definition/load its saved data
	OnCharacterDefinitionDataLoaded(LoadedId);

	//Lets us do anything we want to do only after definition/load is applied! 
	OnPostCharacterDefinitionDataLoaded(LoadedId);

	CharacterDefinitionLoadHandle.Reset();
}

bool ANarrativeCharacter::IsCharacterPendingLoad() const
{	 
	//No char visual yet means our appearance isn't yet loaded 
	if (!CharVisual)
	{
		return true; 
	}

	//If our character is loading meshes, its not ready 
	if (!CharVisual->bBaseAppearanceLoaded || CharVisual->HasLoadHandles())
	{
		return true;
	}

	return CharacterDefinitionLoadHandle.IsValid();
}


void ANarrativeCharacter::SpawnCharacterVisual_Implementation(class UCharacterAppearance* DefaultAppearance)
{
	if (!CharVisual && HasAuthority())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.bNoFail = true;
		SpawnParams.Owner = this;

		CharVisual = GetWorld()->SpawnActor<ANarrativeCharacterVisual>(GetCharacterVisualClass(DefaultAppearance), FTransform(), SpawnParams);
		OnRep_CharVisual();

		if (CharVisual)
		{
			//CharVisual->OnBaseAppearanceApplied.AddDynamic(this, &ANarrativeCharacter::OnCharacterVisualInitialized);
		}
		else
		{
			UE_LOG(LogNarrativeCharacter, Error, TEXT("Character Visual failed to spawn. This shouldn't happen. Is your GetCharacterVisualClass() returning a valid visual?"));
		}
	}
}

TSubclassOf<class ANarrativeCharacterVisual> ANarrativeCharacter::GetCharacterVisualClass_Implementation(class UCharacterAppearance* DefaultAppearance) const
{
	if (DefaultAppearance)
	{
		return DefaultAppearance->GetAppearanceAttributes(this).CharacterVisualClass;
	}

	return nullptr; 
}

void ANarrativeCharacter:: OnRep_CharVisual()
{
	//Seems very edgecase, but CharVisual can OnRep whilst being already loaded, causing fixups to fail? 
	if (CharVisual && CharVisual->bBaseAppearanceLoaded)
	{
		FString RoleStr = HasAuthority() ? "Server" : "Client";
	
		UE_LOG(LogNarrativeNet, Warning, TEXT("%s: RARE Character %s has OnRepped with bBaseAppearance already loaded. "), *RoleStr, *GetCharacterName().ToString());

		if (!HasAuthority())
		{
			/* In networked games, the visual may have not been available when our items replicated back - in fact this is typically the case.
			What we need to do is iterate our items, and re-equip them now that the visual is ready. */
			for (auto& Item : InventoryComponent->GetItems())
			{
				if (UEquippableItem* Equippable = Cast<UEquippableItem>(Item))
				{
					if (Equippable->IsEquipped())
					{
						Equippable->HandleEquip();
					}
				}           
			}

			//Recall this as our character visual may not have been ready when these initially repped back. 
			OnRep_WieldState(FWeaponWieldState());
		}
	}
}

void ANarrativeCharacter::OnCharacterVisualInitialized()
{
	//NPC and Player versions of this each grant items in here - that way appearance related items are guaranteed that the weapon visual is loaded in.
	CharacterVisualInitialized.Broadcast(this);

	//NPC and Player characters now have their factions granted so we can add the map marker 
	RegisterCharacterMapMarker();

	ensureMsgf(IsValid(CharVisual), TEXT("OnCharacterVisualInitialized was called on %s but CharVisual pointer was null. This will cause fixups to fail."), *GetCharacterName().ToString());
	
	FString RoleStr = HasAuthority() ? "Server" : "Client";
	
	UE_LOG(LogNarrativeNet, Warning, TEXT("%s: Character %s is performing post visual fixups on our items. "), *RoleStr, *GetCharacterName().ToString());

	if (!HasAuthority())
	{
		/* In networked games, the visual may have not been available when our items replicated back - in fact this is typically the case.
		What we need to do is iterate our items, and re-equip them now that the visual is ready. */
		for (auto& Item : InventoryComponent->GetItems())
		{
			if (UEquippableItem* Equippable = Cast<UEquippableItem>(Item))
			{
				if (Equippable->IsEquipped())
				{
					Equippable->HandleEquip();
				}
			}           
		}

		//Recall this as our character visual may not have been ready when these initially repped back.
		OnRep_WieldState(FWeaponWieldState());
	}
}

void ANarrativeCharacter::ChangeAppearance_Implementation(class UCharacterAppearance* DefaultAppearance)
{
	if (DefaultAppearance)
	{
		//TODO we really need to load the new character visual's art first, then delete existing one, otherwise character will temporarily pop-out
		//whilst new visual's loading takes place. Possibly a static LoadAppearanceData function that lets us load the appearance first could be useful here. 
		if (CharVisual)
		{
			CharVisual->Destroy();
			CharVisual = nullptr; 
		}

		SpawnCharacterVisual(DefaultAppearance);
		ApplyAppearance(DefaultAppearance);
	}
}

void ANarrativeCharacter::ApplyAppearance_Implementation(class UCharacterAppearance* DefaultAppearance)
{
	//Here, we'll spawn the character visual defined in the character appearance asset.
	
	if (DefaultAppearance)
	{
		if (CharVisual)
		{
			CharVisual->InitializeFromCharacterAndAppearance(this, DefaultAppearance);
			//CharVisual->AttachToActor(this, FAttachmentTransformRules::SnapToTargetIncludingScale);
		}

		//SetAppearanceFromCreatorData(DefaultAppearance->CharacterAttributes);
	}
}

void ANarrativeCharacter::InitNewCharacter_Implementation(UCharacterDefinition* NewDefinition)
{
	if (NewDefinition && !bInitializedNewCharacter)
	{
		bInitializedNewCharacter = true;

		UArsenalStatics::AddFactionsToActor(this, NewDefinition->DefaultFactions);
	
		if (HasAuthority())
		{
			UE_LOG(LogNarrativeNet, Display, TEXT("Server is granting items for %s"), *GetCharacterName().ToString());
			if (UNarrativeInventoryComponent* Inventory = GetInventoryComponent())
			{
				Inventory->SetCurrency(NewDefinition->DefaultCurrency);

				for (auto& DefaultRoll : GetDefaultItemLoadout())
				{
					TArray<FItemAddResult> Results;
					Inventory->TryAddFromLootTable(DefaultRoll, Results);
				}
			}
		}

	}
}
 
bool ANarrativeCharacter::SetEventActive(class UNarrativeEvent* Event, const bool bActivate)
{
	if (Event)
	{
		TArray<UCharacterDefinition*> CharTargets = Event->GetCharacterTargets();
		TArray<ANarrativeCharacter*> CharActorTargets;

		//Events can either be ran on the owner, or optionally specified targets. Gather those. 
		if (CharTargets.Num())
		{
			if (UWorld* World = GetWorld())
			{
				if (UNarrativeCharacterSubsystem* NPCS = World->GetSubsystem<UNarrativeCharacterSubsystem>())
				{
					for (auto& NPCTarget : CharTargets)
					{
						if (ANarrativeCharacter* Character = NPCS->FindCharacter(NPCTarget))
						{
							CharActorTargets.Add(Character);
						}
					}
				}
			}
		}
		else
		{
			CharActorTargets.Add(this);
		}

		//Run the event on all specified targets 
		for (auto& CharTarget : CharActorTargets)
		{
			//Ensure all conditions are met
			for (auto& Cond : Event->Conditions)
			{	
				if (Cond)
				{
					//Need to check who to run condition on 
					TArray<UCharacterDefinition*> CondTargets = Cond->GetCharacterTargets();
					TArray<ANarrativeCharacter*> CondActorTargets;

					if (CondTargets.Num())
					{
						if (UWorld* World = GetWorld())
						{
							if (UNarrativeCharacterSubsystem* NPCS = World->GetSubsystem<UNarrativeCharacterSubsystem>())
							{
								for (auto& NPCTarget : CharTargets)
								{
									if (ANarrativeCharacter* Character = NPCS->FindCharacter(NPCTarget))
									{
										CondActorTargets.Add(Character);
									}
								}
							}
						}
					}
					else
					{
						CondActorTargets.Add(this);
					}

					for (auto& Target : CondActorTargets)
					{
						if (Cond->CheckCondition(Target, nullptr, nullptr) == Cond->bNot)
						{
							UE_LOG(LogNarrativeCharacter, Warning, TEXT("Event %s not running on %s as cond %s failed"), *Event->GetGraphDisplayText(), *CharTarget->GetHumanReadableName(), *Cond->GetGraphDisplayText());

							return false;
						}
					}
				}

			}

			//TODO we should update ExecuteEvents parameters, they aren't really as relevant any more
			if (bActivate)
			{
				Event->OnActivate(CharTarget, nullptr, nullptr);
			}
			else
			{
				Event->OnDeactivate(CharTarget, nullptr, nullptr);
			}


			return true; 
		}

	}

	return false; 
}

bool ANarrativeCharacter::IsCameraInsideHead() const
{
	//Always false for NPCs etc, players override this and do something with it 
	return false; 
}

bool ANarrativeCharacter::IsMovementLocked() const
{
	return HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Movement_Lock);
}

void ANarrativeCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && CharacterRandomSeed < 0)
	{
		SetRandomSeed(FMath::Rand32());
	}
}

void ANarrativeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	ECapsuleRotationSetting CapRot = GetCapsuleRotationSettings();

	/*In networked games we have a problem. When going first person, we need your capsule to face the camera yaw.
	 *
	 * However other players see you in the third person, and if they want to orient you toward movement, you will look one direction
	 * but they will see you looking the wrong way.
	 *
	 * Instead of disabling first person in networked games, we opt to switch orient to movement off for networked games. You can modify this code
	 * if that isn't the correct fix for your games needs. 
	 */
	if (GetNetMode() != NM_Standalone && CapRot == ECapsuleRotationSetting::OrientTowardsMovement)
	{
		CapRot = ECapsuleRotationSetting::UseControllerYawSmoothed;
	}
	
	if (UNarrativeCharacterMovement* NCMC = GetNarrativeCharacterMovement())
	{
		bUseControllerRotationYaw = false;
		NCMC->bUseControllerDesiredRotation = false;
		NCMC->bOrientRotationToMovement = false;

		//Ensure only 1 of these is actually on at a time as they all conflict with each other 
		switch (CapRot)
		{
			case ECapsuleRotationSetting::OrientTowardsMovement:
			{
				NCMC->bOrientRotationToMovement = true; 
			}
			break;

			case ECapsuleRotationSetting::UseControllerYawDirect:
			{
				NCMC->bUseControllerDesiredRotation = true;
				NCMC->RotationRate = FRotator(0, -1.f, 0);
			}
			break;

			case ECapsuleRotationSetting::UseControllerYawSmoothed:
			{
				NCMC->bUseControllerDesiredRotation = true;
				NCMC->RotationRate = FRotator(0, 500.f, 0);
			}
			break;

			default:
			{
				
			}
			break;
		}
	}
}

void ANarrativeCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	//InitializeEquipmentComponent();
}

void ANarrativeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANarrativeCharacter, WieldState);
	DOREPLIFETIME(ANarrativeCharacter, bIsRagdoll);
	DOREPLIFETIME(ANarrativeCharacter, ReplicatedMoveIgnoreActors);
	DOREPLIFETIME(ANarrativeCharacter, CharVisual);

	DOREPLIFETIME_CONDITION(ANarrativeCharacter, CharacterRandomSeed, COND_InitialOnly);
}

void ANarrativeCharacter::TeleportSucceeded(bool bIsATest)
{
	if (!bIsATest)
	{
		OnTeleported.Broadcast();
	}

	Super::TeleportSucceeded(bIsATest);
}

bool ANarrativeCharacter::RegisterCharacterMapMarker()
{
	if (bWantsMapMarker)
	{
		//Register marker once our definition is loaded, since this is the first time our factions are initialized. 
		MapMarker = NewObject<UCharacterMapMarker>(this);

		if (MapMarker)
		{
			MapMarker->ActorOwner = this;

			//If we're local we want to see our character on world maps. Generally we don't want to show other characters on the world map, though we'll allow minimaps. This should probably be configurable. 
			if (IsLocallyControlled() && IsPlayerControlled())
			{
				MapMarker->AddDomains(FGameplayTagContainer(FNavigatorGameplayTags::Get().NavigatorTypes_Worldmap));
				//MapMarker->RemoveDomains(FGameplayTagContainer(FNavigatorGameplayTags::Get().NavigatorTypes_Compass));
			}

			MapMarker->DefaultMarkerSettings.MarkerTitleText = FText::FromString(GetHumanReadableName());
			MapMarker->RegisterMarker();
			return true;
		}
	}

	return false; 
}

FGameplayAbilitySpecHandle ANarrativeCharacter::AddAbility(TSubclassOf<class UNarrativeGameplayAbility> Ability, UObject* SourceObject)
{
	// Grant abilities, but only on the server	
	if (IsValid(Ability) && HasAuthority() && AbilitySystemComponent)
	{
		if (!SourceObject)
		{
			SourceObject = this;
		}

		const int32 Level = GetCharacterLevel();
		const auto& InputTag = Ability.GetDefaultObject()->InputTag;

		for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
		{
			if ((Spec.SourceObject == SourceObject) && Spec.Ability->GetClass() == Ability)
			{
				UE_LOG(LogNarrativeAbilities, Verbose, TEXT("tried granting ability %s that was already granted"), *GetNameSafe(Ability));
				return FGameplayAbilitySpecHandle();
			}
		}
				
		UE_LOG(LogNarrativeAbilities, Verbose, TEXT("Granting ability %s to %s"), *GetNameSafe(Ability), *GetHumanReadableName());

		if (!InputTag.MatchesTagExact(FNarrativeGameplayTags::Get().Narrative_Input_None))
		{
			auto AbilitySpec = FGameplayAbilitySpec(Ability, Level, INDEX_NONE, SourceObject);
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(InputTag);
			FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GiveAbility(AbilitySpec);

			if (!Handle.IsValid())
			{
				UE_LOG(LogNarrativeAbilities, Verbose, TEXT("Granting ability %s returned invalid handle on %s"), *GetNameSafe(Ability), *GetHumanReadableName());
			}

			return Handle; 
		}
		else
		{
			auto AbilitySpec = FGameplayAbilitySpec(Ability, Level, INDEX_NONE, SourceObject);
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(FNarrativeGameplayTags::Get().Narrative_Input_None);
			return AbilitySystemComponent->GiveAbility(AbilitySpec);
		}
	}

	return FGameplayAbilitySpecHandle();
}

TArray<FGameplayAbilitySpecHandle> ANarrativeCharacter::GrantAbilities(TArray<TSubclassOf<class UNarrativeGameplayAbility>> Abilities, UObject* SourceObject)
{
	TArray<FGameplayAbilitySpecHandle> Handles;

	for (TSubclassOf<UNarrativeGameplayAbility>& Ability : Abilities)
	{
		Handles.Add(AddAbility(Ability, SourceObject));
	}

	return Handles; 
}

void ANarrativeCharacter::RemoveAbilities(TArray<FGameplayAbilitySpecHandle> Abilities)
{
	// Grant abilities, but only on the server	
	if (GetLocalRole() != ROLE_Authority || !AbilitySystemComponent)
	{
		return;
	}

	if (UCharacterDefinition* CDef = GetCharacterDefinition())
	{
		for (auto& Spec : Abilities)
		{
			AbilitySystemComponent->ClearAbility(Spec);
		}
	}
}

class UNarrativeCharacterMovement* ANarrativeCharacter::GetNarrativeCharacterMovement() const
{
	return Cast<UNarrativeCharacterMovement>(GetCharacterMovement());
}

void ANarrativeCharacter::SetRandomSeed(const int32 NewSeed)
{
	CharacterRandomSeed = NewSeed;
}

void ANarrativeCharacter::SetWieldState(const FWeaponWieldState& NewWieldState)
{
	//TODO validate weaponstoequip can actually be equipped together? 
	FWeaponWieldState OldWieldState = WieldState;
	WieldState = NewWieldState;

	WieldState.EquipWeapons.Empty();

	//Send weapons through too, since the client may not know the weapons are equipped yet so cant look them up via EquipSlots (needs to rep)
	for (int32 i = 0; i <= WieldState.EquipSlots.Num() - 1; ++i)
	{
		const FGameplayTag EquipSlot = WieldState.EquipSlots.GetByIndex(i);

		if (EquipmentComp)
		{
			if (UWeaponItem* WeaponItem = EquipmentComp->GetEquippedWeaponAtSlot(EquipSlot))
			{
				WieldState.EquipWeapons.Add(WeaponItem);
			}
		}
	}

	OnRep_WieldState(OldWieldState);

	//Used by the character visual to restore our wields if we load game later once weapon visuals are actually ready. 
	SavedWieldState = WieldState;
}

bool ANarrativeCharacter::CanApplyWieldState() const
{
	//Everything has replicated back...
	if (!WieldState.IsValidWieldState())
	{
		return false; 
	}
	
	/**
	 * Ideally we should also check for the sake of not re-apply too many times:
	 * Visuals for the weapons exist in CharVisuals SpawnedWeaponVisuals.
	 * Overlay isn't already applied
	 * Weapon items exist and are repped back to our client. 
	 */
	return true; 
}

void ANarrativeCharacter::OnRep_WieldState(const FWeaponWieldState& OldWieldState)
{
	//If this returns false we're fine, this will keep being called until we're good to go. 
	if (CanApplyWieldState())
	{
		FString RoleStr = HasAuthority() ? "Server" : "Client";
		FString LocalStr = IsLocallyControlled() ? "Local" : "Remote";
		FString CharName = GetCharacterName().ToString();
		UE_LOG(LogNarrativeNet, Warning, TEXT("%s %s OnRep_WieldState for  %s. "), *LocalStr, *RoleStr, *CharName);
		
		//Notify unwield on our old weapons. 
		if (!OldWieldState.EquipSlots.IsEmpty() && !OldWieldState.WieldSlots.IsEmpty())
		{
			checkf(OldWieldState.EquipSlots.Num() == OldWieldState.WieldSlots.Num(), TEXT("Number of wield slots doesn't match number of equip slots, something has gone wrong. "));

			//Tell our weapons they are wielded, and let them handle being wielded. 
			for (int32 i = 0; i <= OldWieldState.EquipSlots.Num() - 1; ++i)
			{
				const FGameplayTag EquipSlot = OldWieldState.EquipSlots.GetByIndex(i);
				const FGameplayTag WieldSlot = OldWieldState.WieldSlots.GetByIndex(i);

				UE_LOG(LogNarrativeNet, Warning, TEXT("%s %s %s Unwielding old weapon at slot %s "), *LocalStr, *RoleStr, *CharName, *WieldSlot.ToString());
				
				if (EquipmentComp)
				{
					EquipmentComp->UnwieldWeapon(WieldSlot);
				}
			}
		}

		//Notify wield on the new ones.  
		if (!WieldState.EquipSlots.IsEmpty() && !WieldState.WieldSlots.IsEmpty())
		{
			checkf(WieldState.EquipSlots.Num() == WieldState.WieldSlots.Num(), TEXT("Number of wield slots doesn't match number of equip slots, something has gone wrong. "));

			//Tell our weapons they are wielded, and let them handle being wielded. 
			for (int32 i = 0; i <= WieldState.EquipSlots.Num() - 1; ++i)
			{
				const FGameplayTag EquipSlot = WieldState.EquipSlots.GetByIndex(i);
				const FGameplayTag WieldSlot = WieldState.WieldSlots.GetByIndex(i);

				if (EquipmentComp)
				{
					if (WieldState.EquipWeapons.IsValidIndex(i))//EquipmentComp->GetEquippedWeaponAtSlot(EquipSlot))
					{
						UE_LOG(LogNarrativeNet, Warning, TEXT("%s %s %s Wielding new weapon at slot %s into WieldSlot %s. Weapon Ptr is %s"), *LocalStr, *RoleStr, *CharName, *EquipSlot.ToString(), *WieldSlot.ToString(), *GetNameSafe(WieldState.EquipWeapons[i]));
						EquipmentComp->WieldWeapon(WieldState.EquipWeapons[i], WieldSlot);
					}
					else
					{
						UE_LOG(LogNarrativeNet, Warning, TEXT("%s %s %s OnRep_WieldState failed to equip new weapon since weapon ptr for %s was invalid"), *LocalStr, *RoleStr, *CharName, *EquipSlot.ToString());
					}
				}
			}
		}

		//Keep the old equippedweapon visual ptr updated as some legacy still uses it. TODO remove
		if (GetWeapon())
		{
			EquippedWeapon = GetWeapon();
		}
		else
		{
			EquippedWeapon = nullptr; 
		}

		//So far, we've just asked weapons to do their own wield logic. Now, ask CharVisual to handle applying the new anim state/weapon attachments. 
		if (CharVisual)
		{
			CharVisual->HandleUpdateWields(OldWieldState, WieldState);
		}
		else
		{
			UE_LOG(LogNarrativeNet, Warning, TEXT("%s visual isn't ready for %s on character %s"), *FString(__FUNCTION__), *LocalStr, *GetCharacterName().ToString());
		}

		//Update equipped weapon tag. 
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			if (WieldState.WieldSlots.IsEmpty())
			{
				ASC->SetLooseGameplayTagCount(FNarrativeGameplayTags::Get().State_Weapon_Equipped, 0);
			}
			else
			{
				ASC->SetLooseGameplayTagCount(FNarrativeGameplayTags::Get().State_Weapon_Equipped, 1);
			}
		}
	}
	else
	{
		FString RoleStr = HasAuthority() ? "Server" : "Client";
		FString LocalStr = IsLocallyControlled() ? "Local" : "Remote";
		UE_LOG(LogNarrativeNet, Warning, TEXT("%s %s OnRep_WieldState failed to apply wields as one of the WieldState.IsValidWieldState() was false (missing weap ptr). "), *LocalStr, *RoleStr);
	}
}

class UNarrativeAnimInstance* ANarrativeCharacter::GetCharacterAnimInstance() const
{
	return Cast<UNarrativeAnimInstance>(GetMesh()->GetAnimInstance());
}

class AWeaponVisual* ANarrativeCharacter::GetEquippedWeaponVisual() const
{
	if (UWeaponItem* Weapon = GetWeapon(true))
	{
		return GetWeaponVisual(Weapon->CurrentSlot);
	}

	return nullptr;
}

class AWeaponVisual* ANarrativeCharacter::GetWieldedWeaponVisual(const bool bMainhand /*= true*/) const
{
	if (UWeaponItem* Weapon = GetWeapon(bMainhand))
	{
		return GetWeaponVisual(Weapon->CurrentSlot);
	}

	return nullptr;
}

class AWeaponVisual* ANarrativeCharacter::GetWeaponVisual(const FGameplayTag& WeaponSlot) const
{
	if (CharVisual)
	{
		return CharVisual->GetWeaponVisual(WeaponSlot);
	}

	return nullptr; 
}

class UWeaponItem* ANarrativeCharacter::GetWeapon(const bool bMainhand) const
{
	if (EquipmentComp)
	{
		if (bMainhand)
		{
			return EquipmentComp->GetWieldedWeaponAtSlot(FNarrativeGameplayTags::Get().Weapon_WieldSlot_Mainhand);
		}
		else
		{
			return EquipmentComp->GetWieldedWeaponAtSlot(FNarrativeGameplayTags::Get().Weapon_WieldSlot_Offhand);
		}
	}
	return EquippedWeapon;
}

TArray<class UWeaponItem*> ANarrativeCharacter::GetWieldedWeapons() const
{
	if (EquipmentComp)
	{
		return EquipmentComp->GetWieldedWeapons();
	}

	return {};
}

FText ANarrativeCharacter::GetCharacterName() const
{
	return FText::GetEmpty();
}

float ANarrativeCharacter::GetAttackRange() const
{
	if (UWeaponItem* Weapon = GetWeapon())
	{
		return Weapon->GetAttackRange();
	}

	return 150.f;
}

void ANarrativeCharacter::SetRagdoll(const bool bWantsRagdoll)
{
	if (!HasAuthority())
	{
		ServerStartRagdoll(bWantsRagdoll);
	}

	if (bIsRagdoll != bWantsRagdoll)
	{
		if (bWantsRagdoll && !CanRagdoll())
		{
			return;
		}

		if (!bWantsRagdoll && !CanExitRagdoll())
		{
			return; 
		}

		bIsRagdoll = bWantsRagdoll;
		OnRep_bIsRagdoll();
	}
}

void ANarrativeCharacter::RagdollForDuration(const float Duration)
{
	GetWorldTimerManager().ClearTimer(RagdollTimerHandle);

	SetRagdoll(true);

	GetWorldTimerManager().SetTimer(RagdollTimerHandle, this, &ANarrativeCharacter::GetUpFromTimedRagdoll, Duration, false);
}

void ANarrativeCharacter::RagdollWithDamageAndImpulse(const float Duration, const FVector& Impulse, const float Damage)
{
	RagdollForDuration(Duration);
	
	if (UNarrativeAbilitySystemComponent* NASC = Cast<UNarrativeAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		if (Damage >= 0.f)
		{
			NASC->DealDamage(Damage);
		}
	}

	if (USkeletalMeshComponent* PMesh = GetMesh())
	{
		PMesh->AddImpulse(Impulse, "pelvis",  true);
	}
}

void ANarrativeCharacter::ServerStartRagdoll_Implementation(const bool bWantsRagdoll)
{
	SetRagdoll(bWantsRagdoll);
}

bool ANarrativeCharacter::CanRagdoll() const
{
	//If we've died, we definitely want to allow the ragdoll
	if (!IsAlive())
	{
		return true;
	}

	//If we're attached to a car, seat, etc we shouldn't ragdoll 
	return !IsValid(GetAttachParentActor()); 
}

bool ANarrativeCharacter::CanExitRagdoll() const
{
	//If we've died, we dont want to leave ragdoll 
	if (!IsAlive())
	{
		return false;
	}

	return true; 
}

void ANarrativeCharacter::GetUpFromTimedRagdoll()
{
	SetRagdoll(false);

}

void ANarrativeCharacter::OnRep_bIsRagdoll()
{
	if (bIsRagdoll)
	{
		if (UNarrativeCharacterMovement* NCMC = GetNarrativeCharacterMovement())
		{
			NCMC->SetMovementMode(MOVE_Custom, CMOVE_Ragdoll);
		}
	}
	else
	{
		if (UNarrativeCharacterMovement* NCMC = GetNarrativeCharacterMovement())
		{
			NCMC->SetMovementMode(MOVE_Walking);
		}
	}
}

void ANarrativeCharacter::OnRep_ReplicatedMoveIgnoreActors()
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->ClearMoveIgnoreActors();

		for (auto& Actor : ReplicatedMoveIgnoreActors)
		{
			if (IsValid(Actor))
			{
				Capsule->IgnoreActorWhenMoving(Actor, true);
			}
		}
	}
}

class UNarrativeAbilitySystemComponent* ANarrativeCharacter::GetNarrativeAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ANarrativeCharacter::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->GetOwnedGameplayTags(TagContainer);
	}
}

bool ANarrativeCharacter::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		return ASC->HasMatchingGameplayTag(TagToCheck);
	}

	return false;
}

bool ANarrativeCharacter::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		return ASC->HasAllMatchingGameplayTags(TagContainer);
	}

	return false;
}

bool ANarrativeCharacter::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		return ASC->HasAnyMatchingGameplayTags(TagContainer);
	}

	return false;
}

UAISense_Sight::EVisibilityResult ANarrativeCharacter::CanBeSeenFrom(const FCanBeSeenFromContext& Context, FVector& OutSeenLocation, int32& OutNumberOfLoSChecksPerformed, int32& OutNumberOfAsyncLosCheckRequested, float& OutSightStrength, int32* UserData /*= nullptr*/, const FOnPendingVisibilityQueryProcessedDelegate* Delegate /*= nullptr*/)
{

	FHitResult Hit;
	FVector OurEyesLoc;
	FRotator OurEyesRot;

	const FName HeadTipName("head_tip");

	if (GetMesh() && GetMesh()->DoesSocketExist(HeadTipName))
	{
		OurEyesLoc = GetMesh()->GetSocketLocation(HeadTipName);
	}
	else
	{
		GetActorEyesViewPoint(OurEyesLoc, OurEyesRot);
	}

	//OutNumberOfLoSChecksPerformed++;

	//bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Context.ObserverLocation, OurEyesLoc, ECC_Visibility, QueryParams, FCollisionResponseParams::DefaultResponseParam);

	//Lets see if we can hit the character
	if (PerformSightTrace(Hit, Context.ObserverLocation, GetActorLocation(), Context, OutSightStrength, OutSeenLocation, OutNumberOfLoSChecksPerformed))
	{
		return UAISense_Sight::EVisibilityResult::Visible;
	}

	//That failed, try the head instead 
	if (PerformSightTrace(Hit, Context.ObserverLocation, OurEyesLoc, Context, OutSightStrength, OutSeenLocation, OutNumberOfLoSChecksPerformed))
	{
		return UAISense_Sight::EVisibilityResult::Visible;
	}

	return UAISense_Sight::EVisibilityResult::NotVisible;
}

bool ANarrativeCharacter::PerformSightTrace(FHitResult& Hit, const FVector& Start, const FVector& End, const FCanBeSeenFromContext& Context, float& OutSightStrength, FVector& OutSeenLocation, int32& OutNumberOfLoSChecksPerformed)
{
	// we need to do tests ourselves
	const FCollisionQueryParams QueryParams = FCollisionQueryParams(SCENE_QUERY_STAT(AILineOfSight), true, Context.IgnoreActor);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams, FCollisionResponseParams::DefaultResponseParam);

	if (!bHit || (Hit.GetActor() && Hit.GetActor()->IsOwnedBy(this)))
	{
		const FVector Offset = Start - End;

		//3 meters or closer is full strength sighted, otherwise 50m or more is least sighted 
		const float DistanceSeenPct = FMath::GetMappedRangeValueClamped(FVector2D(300.f, 5000.f), FVector2D(1.f, 0.f), Offset.Length());
		const float Dot = FVector::DotProduct(Offset.GetSafeNormal(), GetActorForwardVector());
		const bool bCrouched = GetCharacterMovement()->IsCrouching();

		OutSightStrength = CalcSightStrength(Start, End, Context.IgnoreActor);;

		OutSeenLocation = End;

		return true; 
	}

	return false; 
}

static TAutoConsoleVariable<int32> CForceCapsuleRotationSetting(
	TEXT("n.cmc.ForceCapsuleRotationSetting"),
	-1,
	TEXT("Whether to force a certain capsule rotation setting -1 Off, 0=NoRotation, 1=OrientToMovement 2=OrientToControlRotYaw 3=OrientToControlRotYawSmoothed"),
	ECVF_Default);


ECapsuleRotationSetting ANarrativeCharacter::GetCapsuleRotationSettings_Implementation() const
{
	if (UNarrativeCharacterMovement* NCMC = GetNarrativeCharacterMovement())
	{
		if (CForceCapsuleRotationSetting.GetValueOnGameThread() >= 0)
		{
			GEngine->AddOnScreenDebugMessage(0, 5.f, FColor::Red, FString::Printf(TEXT("CapsuleRotationSetting: Override set to %d"), CForceCapsuleRotationSetting.GetValueOnGameThread()) );
			return static_cast<ECapsuleRotationSetting>(CForceCapsuleRotationSetting.GetValueOnGameThread());
		}
		
		if (NCMC->IsClimbing() || IsRagdoll())
		{
			return ECapsuleRotationSetting::NoRotation;
		}
		else if (NCMC->HasCover())
		{
			//Weapon aiming from cover should use snappy, direct yaw. 
			if (HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Weapon_IsAiming))
			{
				return ECapsuleRotationSetting::UseControllerYawDirect;
			}
			else // Strafing cover should orient towards movement. 
			{
				return ECapsuleRotationSetting::OrientTowardsMovement;
			}
		}
		else
		{
			//If we're moving somewhere or in a cinematic we want to rotate the player, so skip yaw in that case. 
			if (HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Movement_PostponePathUpdates) || HasMatchingGameplayTag(FNarrativeGameplayTags::Get().Camera_FirstPerson_Follow3PHeadRotation) || HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled))
			{
				return ECapsuleRotationSetting::NoRotation;
			}

			//If camera is locked to the head we do not want to rotate the capsule - this will rotate head with it and break what we're trying to do 
			if (UAnimInstance* AI = GetCharacterAnimInstance())
			{ 
				if (AI->GetCurveValue("Use3PHeadRotationAlpha") > 0.f)
				{
					return ECapsuleRotationSetting::NoRotation;
				} 
			}

			//Any time we're playing root motion we really dont want this. 
			if (IsPlayingNetworkedRootMotionMontage())
			{
				//return ECapsuleRotationSetting::NoRotation;
			}
			
			if (IsCameraInsideHead() || NCMC->IsMovementMode(MOVE_Swimming))
			{
				//First person should always use controller rotation yaw unless attached to something and no orient to movement regardless of weapon 
				return IsValid(GetAttachParentActor()) ? ECapsuleRotationSetting::NoRotation : ECapsuleRotationSetting::UseControllerYawDirect;
			}
			else // we're not in first person, or swimming
			{
				if (UWeaponItem* Weapon = GetWeapon(true))
				{
					return Weapon->OwnerCapsuleRotationSetting;
				}
				else
				{
					return DefaultCapsuleRotationSetting;
				}
			}
		}
	}

	return ECapsuleRotationSetting::NoRotation;
}

void ANarrativeCharacter::AddDefaultAbilities()
{
	//Use same flag to safeguard adding default abilities more than once 
	if (UCharacterDefinition* CDef = GetCharacterDefinition())
	{
		if (UAbilityConfiguration* AbilityConfig = CDef->AbilityConfiguration)
		{
			// The PlayerState ASC survives pawn respawn, so use the stable config
			// asset as the source identity instead of the transient pawn.
			GrantAbilities(AbilityConfig->DefaultAbilities, AbilityConfig);

			UE_LOG(LogNarrativeAbilities, Verbose, TEXT("Granting %s their default abilities"), *GetHumanReadableName());
		}
	}
	else
	{
		UE_LOG(LogNarrativeAbilities, Warning, TEXT("cant grant abilities on %s as no CDef YET!"), *GetNameSafe(this));
	}
}

void ANarrativeCharacter::RemoveCharacterAbilities()
{
	if (GetLocalRole() != ROLE_Authority || !AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->ClearAllAbilities();

}

void ANarrativeCharacter::InitializeAttributes()
{
	check(AbilitySystemComponent);

	if (!AbilitySystemComponent)
	{
		return;
	}

	if (UCharacterDefinition* CDef = GetCharacterDefinition())
	{
		if (UAbilityConfiguration* AbilityConfig = CDef->AbilityConfiguration)
		{
			if (!AbilityConfig->DefaultAttributes)
			{
				UE_LOG(LogNarrativeAbilities, Error, TEXT("%s() Missing DefaultAttributes for %s. Please fill in the character's definition."), *FString(__FUNCTION__), *GetName());
				return;
			}

			// Replace any persistent attribute effect retained by a PlayerState ASC.
			AbilitySystemComponent->ClearTrackedDefaultAttributesEffect();

			// Can run on Server and Client
			FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
			EffectContext.AddSourceObject(this);

			FGameplayEffectSpecHandle NewHandle = AbilitySystemComponent->MakeOutgoingSpec(AbilityConfig->DefaultAttributes, GetCharacterLevel(), EffectContext);
			if (NewHandle.IsValid())
			{
				const FActiveGameplayEffectHandle ActiveGEHandle =
					AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(
						*NewHandle.Data.Get(),
						AbilitySystemComponent.Get());
				AbilitySystemComponent->TrackDefaultAttributesEffect(ActiveGEHandle);
			}

			AbilitySystemComponent->OnDeathStateChanged.AddUniqueDynamic(this, &ANarrativeCharacter::HandleDeath);
		}
	}
}

void ANarrativeCharacter::AddStartupEffects()
{
	if (GetLocalRole() != ROLE_Authority || !AbilitySystemComponent || AbilitySystemComponent->bStartupEffectsApplied)
	{
		return;
	}

	if (UCharacterDefinition* CDef = GetCharacterDefinition())
	{
		if (UAbilityConfiguration* AbilityConfig = CDef->AbilityConfiguration)
		{
			// Respawn deliberately reapplies startup effects. Remove any persistent
			// handles left on the PlayerState ASC before doing so.
			AbilitySystemComponent->ClearTrackedStartupEffects();

			FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
			EffectContext.AddSourceObject(this);

			for (TSubclassOf<UGameplayEffect> GameplayEffect : AbilityConfig->StartupEffects)
			{
				FGameplayEffectSpecHandle NewHandle = AbilitySystemComponent->MakeOutgoingSpec(GameplayEffect, GetCharacterLevel(), EffectContext);
				if (NewHandle.IsValid())
				{
					const FActiveGameplayEffectHandle ActiveGEHandle =
						AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(
							*NewHandle.Data.Get(),
							AbilitySystemComponent.Get());
					AbilitySystemComponent->TrackStartupEffect(ActiveGEHandle);
				}
			}

			AbilitySystemComponent->bStartupEffectsApplied = true;
		}
	}

}


void ANarrativeCharacter::SetHealth(float Health)
{
	if (AttributeSetBase)
	{
		AttributeSetBase->SetHealth(Health);
	}
}

void ANarrativeCharacter::SetStamina(float Stamina)
{
	if (AttributeSetBase)
	{
		AttributeSetBase->SetStamina(Stamina);
	}
}

void ANarrativeCharacter::OnXPChanged(const float OldXP, const float NewXP)
{
	//Did we level up? 
	if (XPToLevel(NewXP) > XPToLevel(OldXP))
	{
		OnLevelUp(XPToLevel(NewXP));
	}
}

void ANarrativeCharacter::OnLevelUp_Implementation(const int32 NewLevel)
{
	
	//Whenever we level up, reapply our base attributes. This will re-apply them with our level, which will allow things like MaxHealth to scale with our level.
	if (bReapplyAttributesOnLevelUp && AbilitySystemComponent)
	{
		InitializeAttributes();
	}
}

int32 ANarrativeCharacter::XPToLevel(const float XP) const 
{
	return FMath::TruncToInt(LevelExponentX * FMath::Sqrt(XP)) + 1;
}

float ANarrativeCharacter::LevelToXP(const int32 Level) const
{
	return FMath::Pow((Level - 1) / LevelExponentX, LevelExponentY);
}

float ANarrativeCharacter::GetPercentToNextLevel() const
{
	float XP = GetXP();
	const int32 OurLevel = GetCharacterLevel();

	const float XPForOurLevel = LevelToXP(OurLevel);
	const float XPForNextLevel = LevelToXP(OurLevel + 1);

	const float OurProgressFromLevelStart = XP - XPForOurLevel;
	const float DiffBetweenLevels = XPForNextLevel - XPForOurLevel;

	return OurProgressFromLevelStart / DiffBetweenLevels;

}

int32 ANarrativeCharacter::GetCharacterLevel() const
{
	
	return FMath::Max(1, XPToLevel(GetXP()));
}

float ANarrativeCharacter::GetXP() const
{
	if (AttributeSetBase)
	{
		return AttributeSetBase->GetXP();
	}

	return 0.0f;
}

float ANarrativeCharacter::GetStealthRating() const
{
	if (AttributeSetBase)
	{
		return AttributeSetBase->GetStealthRating();
	}

	return 0.0f;
}

float ANarrativeCharacter::GetHealth() const
{
	if (AttributeSetBase)
	{
		return AttributeSetBase->GetHealth();
	}

	return 0.0f;
}

float ANarrativeCharacter::GetMaxHealth() const
{
	if (AttributeSetBase)
	{
		return AttributeSetBase->GetMaxHealth();
	}

	return 0.0f;
}

float ANarrativeCharacter::GetStamina() const
{
	if (AttributeSetBase)
	{
		return AttributeSetBase->GetStamina();
	}

	return 0.0f;
}

float ANarrativeCharacter::GetMaxStamina() const
{
	if (AttributeSetBase)
	{
		return AttributeSetBase->GetMaxStamina();
	}

	return 0.0f;
}

void ANarrativeCharacter::HandleDeath_Implementation(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead)
{
	//We do a nice generic death implementation in here - if NPro users need more than this they can simply override this function in C++/BP

	FString DeadStr = bIsDead ? "dead" : "alive";
	
	UE_LOG(LogNarrativeNet, Warning, TEXT("%s: Message, death state is now: %s"), *GetCharacterName().ToString(), *DeadStr);

	//If being revived, reset our players attributes
	if (!bIsDead)
	{
		InitializeAttributes();
		AddStartupEffects();
	}

	/**Destroy child actors, stop movement, disable collision except for interaction for looting, disable our map marker.*/
	TArray<AActor*> ChildrenActors;
	GetAllChildActors(ChildrenActors);

	for (auto& Child : ChildrenActors)
	{
		if (Child)
		{
			Child->SetActorHiddenInGame(bIsDead);
		}
	}

	SetRagdoll(bIsDead);

	/*Bit of an edge case, you can remove this for your games needs. Essentially when we exit ragdoll it wants a blend out, and a get up anim.
	but in our current setup we actually put you back to your player start, and snap you right out of the ragdoll. So just manually handle that here - no get up anim. 
	We move your ragdoll to the player start because ragdolls aren't currently net-synced, meaning if you get up all simmed proxies will see you snap
	back to the authed ragdoll position. */
	if (!bIsDead)
	{
		if (UNarrativeAnimInstance* Anim = GetCharacterAnimInstance())
		{
			Anim->OverrideLayerBlendOutTime = 0.f;
			Anim->StopAllMontages(0.f);
		}
	}
	
	//Ragdoll needs capsule collision on but pawns shouldn't collide 
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, bIsDead ? ECR_Ignore : ECR_Block);
	}

	//Ragdolled mesh needs to be lootable
	if (USkeletalMeshComponent* CharMesh = GetMesh())
	{
		CharMesh->SetCollisionResponseToChannel(TraceChannel_NarrativeInteraction, bIsDead ? ECR_Block : ECR_Ignore);
	}

	if (bIsDead)
	{
		if (MapMarker)
		{
			MapMarker->RemoveMarker();
		}
	}
	else
	{
		if (MapMarker)
		{
			MapMarker->RegisterMarker();
		}
	}

}

void ANarrativeCharacter::ApplyTriggerSets_Implementation(const TArray<class UTriggerSet*>& DefaultSet)
{
	for (auto& TSet : DefaultSet)
	{
		if (IsValid(TSet))
		{
			for (auto& Trigger : TSet->Triggers)
			{
				AddTrigger(Trigger);
			}
		}
	}
}

TArray<FLootTableRoll> ANarrativeCharacter::GetDefaultItemLoadout() const
{
	if (UCharacterDefinition* CDef = GetCharacterDefinition())
	{
		return CDef->DefaultItemLoadout;
	}

	return {};
}

TSoftObjectPtr<UCharacterAppearanceBase> ANarrativeCharacter::GetDefaultAppearance() const
{
	if (UCharacterDefinition* CDef = GetCharacterDefinition())
	{
		return CDef->DefaultAppearance;
	}

	return {};
}

TArray<TSoftObjectPtr<class UTriggerSet>> ANarrativeCharacter::GetDefaultTriggerSets() const
{
	if (UCharacterDefinition* CDef = GetCharacterDefinition())
	{
		return CDef->TriggerSets;
	}

	return {};
}

class UNarrativeTrigger* ANarrativeCharacter::AddTrigger(class UNarrativeTrigger* Template)
{
	if (Template)
	{
		if (UNarrativeTrigger* NewTrigger = DuplicateObject<UNarrativeTrigger>(Template, this))
		{
			NewTrigger->OwnerCharacter = this;
			NewTrigger->Initialize();
			Triggers.Add(NewTrigger);

			return NewTrigger;
		}
	}

	return nullptr; 
}

bool ANarrativeCharacter::RemoveTrigger(class UNarrativeTrigger* Trigger)
{
	if (Trigger)
	{
		Triggers.Remove(Trigger);
		return true;
	}

	return false;
}

bool ANarrativeCharacter::IsRagdoll(const bool bCheckGettingUp) const
{
	if (bCheckGettingUp)
	{
		if (UNarrativeCharacterMovement* NCMC = GetNarrativeCharacterMovement())
		{
			if (IsValid(NCMC->RagdollGetUpFromBackMontage) && IsValid(NCMC->RagdollGetUpFromFrontMontage))
			{
				if (UNarrativeAnimInstance* CharAnimInst = GetCharacterAnimInstance())
				{
					//Return true if we're getting up now 
					if (CharAnimInst->Montage_IsPlaying(NCMC->RagdollGetUpFromBackMontage) || CharAnimInst->Montage_IsPlaying(NCMC->RagdollGetUpFromFrontMontage))
					{
						return true;
					}
				}
			}
		}
	}

	return bIsRagdoll;
}

void ANarrativeCharacter::SetIgnoreActorWhenMoving(AActor* IgnoreActor, const bool bShouldIgnore)
{
	if (IsValid(IgnoreActor))
	{
		if (bShouldIgnore)
		{
			ReplicatedMoveIgnoreActors.AddUnique(IgnoreActor);
		}
		else
		{
			ReplicatedMoveIgnoreActors.Remove(IgnoreActor);
		}

		OnRep_ReplicatedMoveIgnoreActors();
	}
}

FCollisionQueryParams ANarrativeCharacter::GetIgnoreCharacterParams() const
{
	FCollisionQueryParams CQP;

	CQP.AddIgnoredActor(this);

	TArray<AActor*> C;
	GetAllChildActors(C);

	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors, true, true);


	CQP.AddIgnoredActors(C);
	CQP.AddIgnoredActors(AttachedActors);

	return CQP;
}

bool ANarrativeCharacter::TryAttachWarp(bool PressedJump, FVector2D InputVector, float OptionalInBlendTime)
{
	//Dont evaluate transform if this isn't valid 
	if (!TraversalTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("No montage traversal Table found"))
		return false;
	}

	//Normalize for gamepads
	InputVector.Normalize();

	InputVector.X = FMath::RoundToFloat(InputVector.X);
	InputVector.Y = FMath::RoundToFloat(InputVector.Y);

	if (!IsPlayingAttachWarpMontage)
	{
		if (UNarrativeCharacterMovement* NCMC = GetNarrativeCharacterMovement())
		{
			if (NCMC->TryFindAttachTransform(AttachWarpProps, PressedJump, InputVector, OptionalInBlendTime))
			{
				if (UNarrativeAnimInstance* AnimInstance = Cast<UNarrativeAnimInstance>(GetMesh()->GetAnimInstance()))
				{
					//TODO why is animinstance not just grabbing these values...
					const FVector LocalLedge = GetMesh()->GetBoneTransform(FName("root"), RTS_World).InverseTransformPositionNoScale(AttachWarpProps.LedgeTransform.GetLocation());
					AnimInstance->LocalLedgeLocation = LocalLedge;
					AnimInstance->TraversalLedgeLocation = AttachWarpProps.LedgeTransform.GetLocation();
					AnimInstance->TraversalLedgeRotation = AttachWarpProps.LedgeTransform.GetRotation();
					AnimInstance->TraversalLedgeTransform = AttachWarpProps.LedgeTransform;
					AnimInstance->DirectionToLedge = (AttachWarpProps.LedgeTransform.GetLocation() - GetActorLocation()).GetSafeNormal().ToOrientationQuat();

					//if (!IsValid(AttachWarpProps.SelectedMontage))
					{
						TraversalMontages.Empty();

						//If callee provided a montage just use that, else fall back to chooser. 
						if (AttachWarpProps.SelectedMontage)
						{
							TraversalMontages.Add(AttachWarpProps.SelectedMontage);
						}
						else
						{
							TraversalMontages = UChooserFunctionLibrary::EvaluateChooserMulti(this, TraversalTable, UAnimMontage::StaticClass());
						}

						FPoseSearchBlueprintResult Result;
						UPoseSearchLibrary::MotionMatch(GetMesh()->GetAnimInstance(), TraversalMontages, "PoseHistory", FPoseSearchContinuingProperties(), FPoseSearchFutureProperties(), Result);

						const UAnimMontage* MontageToPlay = Cast<UAnimMontage>(Result.SelectedAnim);
						AttachWarpProps.SelectedMontage = const_cast<UAnimMontage*>(MontageToPlay);

						if (!AttachWarpProps.SelectedMontage)
						{
							UE_LOG(LogTemp, Warning, TEXT("No montage found by motion matching"))
								return false;
						}


						AttachWarpProps.PlayRate = Result.WantedPlayRate;
						AttachWarpProps.StartTime = Result.SelectedTime;
					}

					//Server will play in playattachwarp. 
					if (!HasAuthority() || GetNetMode() == NM_Standalone)
					{
						NCMC->PlayTraversalAnim(AttachWarpProps);
					}

					if (GetNetMode() != NM_Standalone)
					{
						ServerPlayAttachWarp(AttachWarpProps);
					}
				}
				return true;
			}
		}

	}
	return false;
}

void ANarrativeCharacter::PlayAttachWarp(const FAttachWarpProps& InAttachWarpProps)
{
	AttachWarpProps = InAttachWarpProps;

	if (UNarrativeCharacterMovement* NCMC = GetNarrativeCharacterMovement())
	{
		NCMC->PlayTraversalAnim(AttachWarpProps);

		if (GetNetMode() != NM_Standalone)
		{
			ServerPlayAttachWarp(AttachWarpProps);
		}
	}
}

void ANarrativeCharacter::ServerPlayAttachWarp_Implementation(FAttachWarpProps InTraversalProps)
{
	AttachWarpProps = InTraversalProps;
	GetNarrativeCharacterMovement()->PlayTraversalAnim(InTraversalProps);
	MultiCastPlayAttachWarp(InTraversalProps);
}

void ANarrativeCharacter::MultiCastPlayAttachWarp_Implementation(FAttachWarpProps InTraversalProps)
{
	AttachWarpProps = InTraversalProps;
	if(GetLocalRole() == ROLE_SimulatedProxy)
	{
		GetNarrativeCharacterMovement()->PlayTraversalAnim(InTraversalProps);
	}
}


UNarrativeCharacterOwner::UNarrativeCharacterOwner(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

};

FAttachWarpProps::FAttachWarpProps()
{
	ActionType = ETraversalActionType::Mantle;
}
