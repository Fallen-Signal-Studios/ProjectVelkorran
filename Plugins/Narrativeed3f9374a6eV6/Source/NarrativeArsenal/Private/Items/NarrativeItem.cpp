// Copyright Narrative Tools 2022. 

#include "Items/NarrativeItem.h"
#include "Items/InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"
#include "Engine/BlueprintGeneratedClass.h"
#include <GameFramework/PlayerState.h>
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "AI/Activities/NPCActivityComponent.h"
#include <Serialization/ObjectAndNameAsStringProxyArchive.h>
#include <Templates/SubclassOf.h>
#include <Serialization/MemoryReader.h>
#include <Serialization/MemoryWriter.h>

#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInterface.h"
#include "VerseVM/VVMCustomAttributeHandler.h"

#define LOCTEXT_NAMESPACE "Item"

#define ItemStat_DisplayName "DisplayName"
#define ItemStat_Weight "Weight"
#define ItemStat_Quantity "Quantity"
#define ItemStat_RechargeDuration "RechargeDuration"
#define ItemStat_StackWeight "StackWeight"
#define ItemStat_MaxStackSize "MaxStackSize"
#define ItemStat_BaseValue "BaseValue"

UNarrativeItem::UNarrativeItem()
{
	DisplayName = LOCTEXT("ItemName", "Item");
	UseActionText = LOCTEXT("UseActionText", "Use");
	Weight = 0.f;
	bStackable = true;
	Quantity = 1;
	MaxStackSize = 2;
	RepKey = 0;
	LastUseTime = -FLT_MAX;
	BaseValue = 10;
	bWantsTickByDefault = false; 
	bIsBusy = false; 
	bAddDefaultUseOption = true; 

	FString NameString = GetName();

	//Add the Default Speaker to the Quest 
	int32 UnderscoreIndex = -1;

	if (NameString.FindChar(TCHAR('_'), UnderscoreIndex))
	{
		//remove item name prefix
		DisplayName = FText::FromString(FName::NameToDisplayString(NameString.RightChop(UnderscoreIndex + 1), false));
	}
	else
	{
		DisplayName = FText::FromString(NameString);
	}

	Stats.Add(FNarrativeItemStat(LOCTEXT("WeightStatDisplayText", "Weight"), ItemStat_Weight, LOCTEXT("WeightStatTooltip", "The weight of the item.")));
	Stats.Add(FNarrativeItemStat(LOCTEXT("QuantityStatDisplayText", "Quantity"), ItemStat_Quantity, LOCTEXT("QuantityStatTooltip", "The amount of the item you have.")));

	//Add a use action by default
	//UseActions = {};
	//UseActions.Add(CreateDefaultSubobject<UNarrativeItemUseAction>("DefaultItemUseAction"));

}

void UNarrativeItem::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	//Narrative inventory needs items to support user added blueprint replicated variables 
	UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(GetClass());
	if (BPClass != NULL)
	{
		BPClass->GetLifetimeBlueprintReplicationList(OutLifetimeProps);
	}

	DOREPLIFETIME(UNarrativeItem, Quantity);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeItem, bActive, COND_None, REPNOTIFY_OnChanged);
	DOREPLIFETIME_CONDITION(UNarrativeItem, LastUseTime, COND_OwnerOnly);
	//DOREPLIFETIME_CONDITION(UNarrativeItem, ItemGUID, COND_InitialOnly);
}

bool UNarrativeItem::IsSupportedForNetworking() const
{
	return true;
}

class UWorld* UNarrativeItem::GetWorld() const
{
	return World;
}

void UNarrativeItem::PostLoad()
{
	Super::PostLoad();

	if (!PickupMesh.IsNull() && PickupMeshData.PickupMesh.IsNull())
	{
		PickupMeshData.PickupMesh = PickupMesh;
		PickupMeshData.PickupMeshMaterials.Empty();

		if (UStaticMesh* PickupMeshAsset = PickupMeshData.PickupMesh.LoadSynchronous())
		{
			//Check if we have any custom materials to apply the newly created mesh
			for (int32 i = 0; i < PickupMeshAsset->GetStaticMaterials().Num(); ++i)
			{
				PickupMeshData.PickupMeshMaterials.Add(TSoftObjectPtr<UMaterialInterface>(PickupMeshAsset->GetStaticMaterials()[i].MaterialInterface));
			}
		}


	}
}

int32 UNarrativeItem::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{

	if (OwningInventory)
	{
		if (AActor* Owner = OwningInventory->GetOwner())
		{
			return Owner->GetFunctionCallspace(Function, Stack);
		}

	}

	return FunctionCallspace::Local;
}

bool UNarrativeItem::CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack)
{
	if (OwningInventory)
	{
		if (AActor* Owner = OwningInventory->GetOwner())
		{
			bool bProcessed = false;

			FWorldContext* const Context = GEngine->GetWorldContextFromWorld(GetWorld());
			if (Context != nullptr)
			{
				for (FNamedNetDriver& Driver : Context->ActiveNetDrivers)
				{
					if (Driver.NetDriver != nullptr && Driver.NetDriver->ShouldReplicateFunction(Owner, Function))
					{
						Driver.NetDriver->ProcessRemoteFunction(Owner, Function, Parameters, OutParms, Stack, this);
						bProcessed = true;
					}
				}
			}

			return bProcessed;
		}
	}

	return Super::CallRemoteFunction(Function, Parameters, OutParms, Stack);
}

#if WITH_EDITOR
void UNarrativeItem::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName ChangedPropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	//UPROPERTY clamping doesn't support using a variable to clamp so we do in here instead
	if (ChangedPropertyName == GET_MEMBER_NAME_CHECKED(UNarrativeItem, Quantity))
	{
		Quantity = FMath::Clamp(Quantity, 1, GetMaxStackSize());
	}
	else if (ChangedPropertyName == GET_MEMBER_NAME_CHECKED(UNarrativeItem, bStackable))
	{
		if (!bStackable)
		{
			Quantity = 1;
		}
	}
}

bool UNarrativeItem::CanEditChange(const FProperty* InProperty) const
{
	return Super::CanEditChange(InProperty);
}

#endif

void UNarrativeItem::Serialize(FArchive& Ar)
{

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		Super::Serialize(Ar);
		return;
	}

	//When we serialize, make sure to serialize our fragment subobjects to disk 
	if (Ar.IsLoading())
	{
		Super::Serialize(Ar);

		//Load saved fragments back in from disk.
		for (auto& SavedFragment : SavedFragments)
		{
			for (auto& Fragment : Fragments)
			{
				//Generally we dont have multiple fragments of same type so can use class as the ID
				if (Fragment && Fragment->GetClass() == SavedFragment.Fragment)
				{
					FMemoryReader MemReader(SavedFragment.Data);
					FObjectAndNameAsStringProxyArchive ProxyAr(MemReader, true);
					ProxyAr.ArIsSaveGame = true;

					Fragment->Serialize(ProxyAr);
				}
			}
		}
	}
	else
	{
		//Serialize our saved fragments to disk 
		SavedFragments.Empty();

		for (auto& ItemFragment : Fragments)
		{
			if (ItemFragment)
			{
				FSavedItemFragment SaveFrag;
				SaveFrag.Fragment = ItemFragment->GetClass();

				FMemoryWriter MemWriter(SaveFrag.Data);
				FObjectAndNameAsStringProxyArchive ProxyAr(MemWriter, true);
				ProxyAr.ArIsSaveGame = true;

				ItemFragment->Serialize(ProxyAr);

				SavedFragments.Add(SaveFrag);
			}
		}

		Super::Serialize(Ar);
	}

}

bool UNarrativeItem::HasAuthority() const
{
	//Fixes a rare case where not having this set can cause server to keep trying to call Server RPCs causing stack overflow. 
	if (!OwningInventory)
	{
		return true; 
	}

	return OwningInventory->GetOwnerRole() >= ROLE_Authority;
}


void UNarrativeItem::OnRep_bActive(const bool bOldActive)
{
	if (bActive)
	{
		Activated();
	}
	else
	{
		Deactivated();
	}
}

void UNarrativeItem::SetActive(const bool bNewActive, const bool bForce)
{
	++CinematicStateRevision;
	if (bCanActivate)
	{
		if (bNewActive != bActive || bForce)
		{
			bActive = bNewActive;
			MarkDirtyForReplication();
			OnRep_bActive(!bActive);
		}
	}

}

void UNarrativeItem::SetBusy(const bool bNewBusy)
{
	++CinematicStateRevision;
	if (bIsBusy != bNewBusy)
	{
		bIsBusy = bNewBusy;
		OnItemModified.Broadcast();
	}

	bIsBusy = bNewBusy;
}

void UNarrativeItem::OnRep_Quantity(const int32 OldQuantity)
{
	OnItemModified.Broadcast();
}

void UNarrativeItem::SetQuantity(const int32 NewQuantity)
{
	++QuantityRevision;
	// Publish the write token before delegates, including an intentional same-value write.
	MarkDirtyForReplication();
	if (NewQuantity != Quantity)
	{
		const int32 OldQuantity = Quantity;
		Quantity = FMath::Clamp(NewQuantity, 0, GetMaxStackSize());
		OnRep_Quantity(OldQuantity);
	}
}

bool UNarrativeItem::CanUseItemWith_Implementation(class UNarrativeItem* TestItem) const
{
	return false; 
}

bool UNarrativeItem::ShowActiveInUI_Implementation() const
{
	return IsActive();
}

void UNarrativeItem::SetLastUseTime(const float NewLastUseTime)
{
	LastUseTime = NewLastUseTime;
	MarkDirtyForReplication();
}

float UNarrativeItem::GetItemScore_Implementation() const
{
	//If score hasnt been filled out, use item value as a fallback
	if (FMath::IsNearlyZero(BaseScore))
	{
		return BaseValue;
	}

	return BaseScore; 
}

AController* UNarrativeItem::GetOwningController() const
{
	//Grab owning controller, and grab character. 
	if (APawn* OwnerPawn = GetOwningPawn())
	{
		return OwnerPawn->GetController();
	}

	return nullptr;
}

APawn* UNarrativeItem::GetOwningPawn() const
{
	//We might need to grab owning pawn before OwningInventory has been set by UInventoryComponent::OnRep_Items() - grab via the playerstate outer instead 
	if (APlayerState* PS = Cast<APlayerState>(GetOuter()))
	{
		if (APawn* P = PS->GetPawn())
		{
			return P;
		}
	}
	else
	{
		if (APawn* P = Cast<APawn>(GetOuter()))
		{
			return P;
		}
	}

	if (OwningInventory)
	{
		return OwningInventory->GetOwningPawn();
	}
	return nullptr;
}

ANarrativeCharacter* UNarrativeItem::GetOwningNarrativeCharacter() const
{
	//Grab owning controller, and grab character. 
	if (APawn* OwnerPawn = GetOwningPawn())
	{
		if (ANarrativeCharacter* NChar = Cast<ANarrativeCharacter>(OwnerPawn))
		{
			return NChar; 
		}

		//Try find via controller, this will fail for clients so not great. 
		if (INarrativeCharacterOwner* CharInt = Cast<INarrativeCharacterOwner>(OwnerPawn->GetController()))
		{
			return CharInt->GetNarrativeCharacter();
		}
	}

	return nullptr; 
}

TArray<UNarrativeItemUseAction*> UNarrativeItem::GetItemUseActions_Implementation() const
{

	TArray<UNarrativeItemUseAction*> DefaultActions = {};

	if (bAddDefaultUseOption)
	{
		DefaultActions.Add(NewObject<UNarrativeItemUseAction>(UNarrativeItemUseAction::StaticClass()));
	}

	for (auto& Fragment : Fragments)
	{
		if (Fragment)
		{
			DefaultActions.Append(Fragment->GetItemUseActions());
		}
	}

	return DefaultActions;
}

void UNarrativeItem::ServerTryUse_Implementation(UNarrativeItem* OtherItem /*= nullptr*/)
{
	TryUse(OtherItem);
}

bool UNarrativeItem::TryUse(UNarrativeItem* OtherItem /*= nullptr*/)
{
	UNarrativeInventoryComponent* Inventory = OwningInventory;
	
	if (Inventory)
	{
		if (CanUse())
		{
			//Ask server to use item. 
			if (!HasAuthority())
			{
				ServerTryUse(OtherItem);
			}

			const float UseTime = GetWorld()->GetTimeSeconds();

			if (UseTime - GetLastUseTime() > UseRechargeDuration)
			{
				//If the item is consumable like food, but it can't be removed from the inventory then disallow using the item 
				if (!CanBeRemoved() && bConsumeOnUse)
				{
					return false;
				}

				OnUse(OtherItem);
				Use(OtherItem);

				Inventory->OnItemUsed.Broadcast(this);

				if (!HasAuthority())
				{
					return true;
				}

				SetLastUseTime(UseTime);

				if (bToggleActiveOnUse)
				{
					SetActive(!bActive);
				}

				if (bConsumeOnUse)
				{
					Inventory->ConsumeItem(this, 1);
				}

				return true;
			}
		}
	}


	return false;
}

void UNarrativeItem::Use(UNarrativeItem* OtherItem/*=nullptr*/)
{
	
}

void UNarrativeItem::TickItem(const float DeltaTime)
{

}

void UNarrativeItem::AddedToInventory(class UNarrativeInventoryComponent* Inventory, const bool bFromLoad)
{
	if (!bFromLoad)
	{
		//TODO put in fragment
		for (auto& ActivityToGrant : ActivitiesToGrant)
		{
			if (IsValid(ActivityToGrant))
			{
				if (ANarrativeNPCCharacter* NPCChar = Cast<ANarrativeNPCCharacter>(Inventory->GetOwner()))
				{
					if (UNPCActivityComponent* ActivityComp = NPCChar->GetActivityComponent())
					{
						ActivityComp->AddActivity(ActivityToGrant, false);
					}
				}
			}
		}
	}

	if (Inventory && bWantsTickByDefault)
	{
		EnableItemTick(true);
	}
}

void UNarrativeItem::RemovedFromInventory(class UNarrativeInventoryComponent* Inventory)
{
	//TODO remove granted activities in here 
	if (bActive)
	{
		Deactivated();
	}

	if (Inventory)
	{
		EnableItemTick(false);
	}

	OwningInventory = nullptr; 
}

void UNarrativeItem::MarkDirtyForReplication()
{
	++CinematicStateRevision;
	//Mark this object for replication
	++RepKey;

	//Mark the array for replication
	if (OwningInventory)
	{
		++OwningInventory->ReplicatedItemsKey;
	}
}

void UNarrativeItem::EnableItemTick(const bool bEnable)
{
	if (OwningInventory)
	{
		if (bEnable)
		{
			OwningInventory->AddTickItem(this);
		}
		else
		{
			OwningInventory->RemoveTickItem(this);
		}
	}
}

FText UNarrativeItem::GetRawDescription_Implementation()
{
	return Description;
}

FText UNarrativeItem::GetParsedDescription()
{
	//Replace variables in dialogue line
	FString LineString = Description.ToString();

	int32 OpenBraceIdx = -1;
	int32 CloseBraceIdx = -1;
	bool bFoundOpenBrace = LineString.FindChar('{', OpenBraceIdx);
	bool bFoundCloseBrace = LineString.FindChar('}', CloseBraceIdx);
	uint32 Iters = 0; // More than 50 wildcard replaces and something has probably gone wrong, so safeguard against that

	while (bFoundOpenBrace && bFoundCloseBrace && OpenBraceIdx < CloseBraceIdx && Iters < 50)
	{
		const FString VariableName = LineString.Mid(OpenBraceIdx + 1, CloseBraceIdx - OpenBraceIdx - 1);
		const FString VariableVal = GetStringVariable(VariableName);

		//if (!VariableVal.IsEmpty()) //Not sure why this was even here, unimplemented variables should still be replaced, we'd never want to show {VariableName} to an end user 
		{
			LineString.RemoveAt(OpenBraceIdx, CloseBraceIdx - OpenBraceIdx + 1);
			LineString.InsertAt(OpenBraceIdx, VariableVal);
		}

		bFoundOpenBrace = LineString.FindChar('{', OpenBraceIdx);
		bFoundCloseBrace = LineString.FindChar('}', CloseBraceIdx);

		Iters++;
	}

	return FText::FromString(LineString);
}

FString UNarrativeItem::GetStringVariable_Implementation(const FString& VariableName)
{
	//Overriable in BP in case you want to add more 
	if (VariableName == ItemStat_DisplayName)
	{
		return DisplayName.ToString();
	}
	else if (VariableName == ItemStat_Weight)
	{
		return FString::SanitizeFloat(Weight);
	}
	else if (VariableName == ItemStat_RechargeDuration)
	{
		return FString::SanitizeFloat(UseRechargeDuration);
	}
	else if (VariableName == ItemStat_StackWeight)
	{
		return FString::SanitizeFloat(GetStackWeight());
	}
	else if (VariableName == ItemStat_Quantity)
	{
		return FString::FromInt(Quantity);
	}
	else if (VariableName == ItemStat_MaxStackSize)
	{
		return FString::FromInt(MaxStackSize);
	}
	else if (VariableName == ItemStat_BaseValue)
	{
		return FString::FromInt(BaseValue);
	}

	return FString();
}

void UNarrativeItem::Activated_Implementation() 
{

}

void UNarrativeItem::Deactivated_Implementation() 
{

}

bool UNarrativeItem::CanBeRemoved_Implementation() const
{
	return true;
}

bool UNarrativeItem::CanUse_Implementation() const
{
	return true;
}

bool UNarrativeItem::ShouldUseOnAdd_Implementation() const
{
	return false;
}

bool UNarrativeItem::ShouldShowInInventory_Implementation() const
{
	//By default, don't show vendors equipped items in their store  
	if (OwningInventory && OwningInventory->bIsVendor)
	{
		if (bActive)
		{
			return false; 
		}
	}

	return true;
}

UNarrativeItemFragment* UNarrativeItem::GetFragment(TSubclassOf<UNarrativeItemFragment> FragmentClass, bool& bSucceeded)
{
	bSucceeded = false;

	for (auto& Fragment : Fragments)
	{
		if (Fragment && Fragment->GetClass()->IsChildOf(FragmentClass))
		{
			bSucceeded = true; 
			return Fragment;
		}
	}

	return nullptr; 
}

TArray<UNarrativeItemFragment*> UNarrativeItem::GetFragments(TSubclassOf<UNarrativeItemFragment> FragmentClass, bool& bSucceeded)
{
	TArray<UNarrativeItemFragment*> RetFragments;

	bSucceeded = false; 

	for (auto& Fragment : Fragments)
	{
		if (Fragment && Fragment->GetClass()->IsChildOf(FragmentClass))
		{
			bSucceeded = true;
			RetFragments.Add(Fragment);
		}
	}

	return RetFragments;
}

void UNarrativeItem::PostInventoryLoaded()
{

}

FPickupMeshData UNarrativeItem::GetPickupMeshData_Implementation(int32 QuantityToGet) const
{
	//TODO account for quantities. 
	return PickupMeshData;
}

UNarrativeItemFragment::UNarrativeItemFragment(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

TArray<class UNarrativeItemUseAction*> UNarrativeItemFragment::GetItemUseActions_Implementation() const
{
	return {};
}

UNarrativeItemUseAction::UNarrativeItemUseAction(const FObjectInitializer& ObjectInitializer)
{

}

bool UNarrativeItemUseAction::IsEnabled_Implementation()
{
	return true; 
}

bool UNarrativeItemUseAction::OnUse_Implementation(class UNarrativeItem* Item, class UNarrativeItem* OtherItem)
{
	if (Item)
	{
		return Item->TryUse(OtherItem);
	}

	return false; 
}

bool UNarrativeItemUseAction::IsMultiItemUse_Implementation(class UNarrativeItem* Item)
{
	if (Item)
	{
		return Item->bUsedWithOtherItem;
	}

	return false; 
}

bool UNarrativeItemUseAction::GetItemsUsableWith_Implementation(UNarrativeItem* Item, TArray<UNarrativeItem*>& OutItems) const
{
	if (Item && Item->OwningInventory)
	{
		TArray<UNarrativeItem*> ItemsOfClass;

		for (auto& InvItem : Item->OwningInventory->GetItems())
		{
			if (InvItem && Item->CanUseItemWith(InvItem))
			{
				OutItems.Add(InvItem);
			}
		}
	}

	return OutItems.Num() > 0;
}

FText UNarrativeItemUseAction::GetActionDisplayName_Implementation(class UNarrativeItem* Item)
{
	return Item->UseActionText;
}

#undef LOCTEXT_NAMESPACE
