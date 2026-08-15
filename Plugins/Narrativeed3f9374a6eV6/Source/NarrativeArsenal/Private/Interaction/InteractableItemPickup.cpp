// Copyright Narrative Tools 2025.


#include "Interaction/InteractableItemPickup.h"

#include "Net/UnrealNetwork.h"
#include "NarrativeArsenal.h"
#include "Items/NarrativeItem.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "SaveSystemStatics.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Items/InventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "ArsenalStatics.h"


#define LOCTEXT_NAMESPACE "ItemPickup"

void AItemPickup::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	//As an optimization dont let pickups change their class later - uncomment if you dont want this 
	DOREPLIFETIME_CONDITION(AItemPickup, PickupConfig, COND_InitialOnly);
}

void AItemPickup::BeginPlay()
{
	Super::BeginPlay();

	if (!USaveSystemStatics::LoadSingleActor(this))
	{
		RefreshPickup(PickupConfig);
	}
}

void AItemPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	//If manually destroyed remove the save record so it doesn't spawn back in later. 
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		USaveSystemStatics::RemoveSingleActor(this);
	}
	else //otherwise save pickup. 
	{
		USaveSystemStatics::SaveSingleActor(this);
	}
}

#if WITH_EDITOR
void AItemPickup::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(AItemPickup, PickupConfig))
	{
		RefreshPickup(PickupConfig);
	}
}

void AItemPickup::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	// Invalidate duplicated GUID so we can generate another one
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		PickupSaveGUID.Invalidate();
		USaveSystemStatics::CreateSaveGuid(PickupSaveGUID);
	}
}
#endif 

AItemPickup::AItemPickup(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>("PickupMesh");
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupMesh->SetCollisionResponseToChannel(TraceChannel_NarrativeInteraction, ECR_Block);

	PickupInteractable = CreateDefaultSubobject<UPickupInteractable>("PickupInteractable");

	SetRootComponent(PickupMesh);

	USaveSystemStatics::CreateSaveGuid(PickupSaveGUID);

	bReplicates = true; 
}

bool AItemPickup::TakePickup(ANarrativeCharacter* Taker)
{
	if (IsValid(PickupConfig.PickupClass) && PickupConfig.QuantityToGive > 0)
	{
		//TODO allow clients to check that their inventory has space, and optionally hide the item if we do
		if (HasAuthority())
		{
			if (Taker)
			{
				if (UNarrativeInventoryComponent* Inventory = Taker->GetInventoryComponent())
				{
					FItemAddResult AddResult = Inventory->TryAddItemFromClass(PickupConfig.PickupClass, PickupConfig.QuantityToGive);

					if (AddResult.AmountGiven > 0)
					{
						PickupConfig.QuantityToGive -= AddResult.AmountGiven;

						if (PickupConfig.QuantityToGive <= 0)
						{
							Destroy();
						}
					}
				}

				return true;
			}
		}
	}


	return false;
}

void AItemPickup::SetPickup(const FPickupConfiguration& InPickupConfig)
{
	if (HasAuthority())
	{
		PickupConfig = InPickupConfig;
		OnRep_PickupConfig();
	}
}

FPickupConfiguration AItemPickup::GetPickupConfig() const
{
	return PickupConfig;
}

FGuid AItemPickup::GetActorGUID_Implementation() const
{
	return PickupSaveGUID;
}

void AItemPickup::SetActorGUID_Implementation(const FGuid& SavedGUID)
{
	PickupSaveGUID = SavedGUID;
}

bool AItemPickup::ShouldRespawn_Implementation() const
{
	return true; 
}

void AItemPickup::Load_Implementation()
{
	//When we load back in, need to refresh pickup
	RefreshPickup(PickupConfig);
}

void AItemPickup::OnPickupDataReady_Implementation(FPickupMeshData Data)
{
	//Pickup data has been streamed in, and we can apply it to the pickup mesh. 
	if (PickupMeshLoadHandle.IsValid())
	{
		PickupMeshLoadHandle->CancelHandle();
	}

	if (PickupInteractable)
	{
		if (const UNarrativeItem* ItemCDO = GetDefault<UNarrativeItem>(PickupConfig.PickupClass))
		{
			PickupInteractable->SetInteractableNameText(ItemCDO->DisplayName);
		}
	}

	if (UStaticMesh* PickupMeshAsset = Data.PickupMesh.LoadSynchronous())
	{
		if (PickupMesh)
		{
			PickupMesh->SetStaticMesh(PickupMeshAsset);

			//Check if we have any custom materials to apply the newly created mesh
			for (int32 i = 0; i < PickupMeshAsset->GetStaticMaterials().Num(); ++i)
			{
				if (Data.PickupMeshMaterials.IsValidIndex(i))
				{
					PickupMesh->SetMaterial(i, Data.PickupMeshMaterials[i].LoadSynchronous());
				}
			}

		}
	}
}

void AItemPickup::OnRep_PickupConfig()
{
	RefreshPickup(PickupConfig);
}

void AItemPickup::RefreshPickup_Implementation(const FPickupConfiguration& InPickupConfig)
{
	if (IsValid(PickupConfig.PickupClass) && PickupConfig.QuantityToGive > 0)
	{


		if (const UNarrativeItem* ItemCDO = GetDefault<UNarrativeItem>(PickupConfig.PickupClass))
		{
			FPickupMeshData MeshData = ItemCDO->GetPickupMeshData(PickupConfig.QuantityToGive);

			if(!MeshData.PickupMesh.IsNull())
			{
				//Load the pickup mesh and materials in  
				TArray<FSoftObjectPath> MeshDataAssetPaths;
				MeshDataAssetPaths.Add(MeshData.PickupMesh.ToSoftObjectPath());

				for (auto& Mat : MeshData.PickupMeshMaterials)
				{
					MeshDataAssetPaths.Add(Mat.ToSoftObjectPath());
				}

				if (MeshDataAssetPaths.Num() > 0)
				{

					//In editor immediately load - also apply the offset since actor factories are buggy so dont do that there. 
#if WITH_EDITOR
					OnPickupDataReady(MeshData);

					return;
#else

					if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
					{
						FStreamableDelegate MeshLoadDel = FStreamableDelegate::CreateUObject(this, &AItemPickup::OnPickupDataReady, MeshData);

						PickupMeshLoadHandle = Manager->LoadAssetList(MeshDataAssetPaths, MeshLoadDel);
					}
#endif 
				}
			}
		}
	}	
	else
	{
		if (PickupMesh)
		{
			PickupMesh->SetStaticMesh(nullptr);
		}
	}
}

UPickupInteractable::UPickupInteractable()
{
	InteractableActionText = LOCTEXT("PickupActionText", "Take");
}

bool UPickupInteractable::Interact(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp)
{
	if (AItemPickup* Pickup = GetOuterAItemPickup())
	{
		if (ANarrativeCharacter* NChar = Cast<ANarrativeCharacter>(Interactor))
		{
			return Pickup->TakePickup(NChar);
		}
	}
	return false;
}

bool UPickupInteractable::CanInteract_Implementation(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp, FText& OutErrorText)
{
	if (AItemPickup* Pickup = GetOuterAItemPickup())
	{
		if (ANarrativeCharacter* NChar = Cast<ANarrativeCharacter>(Interactor))
		{
			if (UNarrativeInventoryComponent* Inventory = NChar->GetInventoryComponent())
			{
				const int32 Space = Inventory->GetSpaceForItem(Pickup->GetPickupConfig().PickupClass, OutErrorText);

				return Space > 0;
			}
		}
	}

	return false; 
}

#undef LOCTEXT_NAMESPACE