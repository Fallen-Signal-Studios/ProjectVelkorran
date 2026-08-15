// Copyright Narrative Tools 2025.

#include "UnrealFramework/GameplayDebuggerCategory_NChar.h"

#if WITH_GAMEPLAY_DEBUGGER_MENU
#include "Engine/Canvas.h"
#include "GameplayDebuggerTypes.h"
#include "Engine/ActorChannel.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerController.h"

#endif 

#include "UnrealFramework/NarrativeCharacter.h"
#include "Components/EquipmentComponent.h"


#if WITH_GAMEPLAY_DEBUGGER_MENU

static TAutoConsoleVariable<int32> CGDTDrawLocalInfo(
	TEXT("n.gdt.DrawRoleLevel"),
	2,
	TEXT("When using the Narrative character debug category, what roles to draw info for? 0=Server 1=Client, 2 = Both"),
	ECVF_Default);

FGameplayDebuggerCategory_NarrativeCharacter::FGameplayDebuggerCategory_NarrativeCharacter()
{
	SetDataPackReplication<FRepData>(&DataPack);

	bAllowLocalDataCollection = true;
}

void FGameplayDebuggerCategory_NarrativeCharacter::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	if (ANarrativeCharacter* MyPawn = Cast<ANarrativeCharacter>(DebugActor))
	{

		//we dont want servers data if we're supposed to be drawing only client. This wont work actually since client sets this var not server 
		if (CGDTDrawLocalInfo.GetValueOnGameThread() == 1)
		{
			ResetReplicatedData();
		}
		
		const bool bIsAuth = OwnerPC ? OwnerPC->HasAuthority() : false;
		
		if (bIsAuth && CGDTDrawLocalInfo.GetValueOnGameThread() != 1)
		{
			MyPawn->DescribeSelfToGameplayDebugger(this);
		}
		else if (!bIsAuth && CGDTDrawLocalInfo.GetValueOnGameThread() > 0)
		{
			MyPawn->DescribeSelfToGameplayDebugger(this);
		}

		DataPack.EquippedItems = CollectEquipmentData(OwnerPC, MyPawn);
	}
}

void FGameplayDebuggerCategory_NarrativeCharacter::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	DrawEquippedItems(CanvasContext, OwnerPC);
}

namespace UE::NarrativeCharacter::Debug
{
	// This is the longest name we can use for the UI (string format truncate with %.35s).  We use a variety of letters because MeasureString depends on kerning.
	const FString LongestDebugObjectName{ TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ_ ABCDEFGH") };

	// Let's define some names for consistent look across all of the categories
	const TCHAR* BothColor = TEXT("{yellow}");
	const TCHAR* ServerColor = TEXT("{cyan}");
	const TCHAR* LocalColor = TEXT("{green}");
	const TCHAR* NonReplicatedColor = TEXT("{violetred}");

	/** Given a string, print it out using the color legend based on the NetworkStatus */
	inline FString ColorNetworkString(FGameplayDebuggerCategory_NarrativeCharacter::ENetworkStatus NetworkStatus, const FString DisplayString)
	{
		const TCHAR* Colors[+FGameplayDebuggerCategory_NarrativeCharacter::ENetworkStatus::MAX] =
		{
			ServerColor, LocalColor, BothColor, NonReplicatedColor
		};

		return FString::Printf(TEXT("%s%.*s"), Colors[+NetworkStatus], DisplayString.Len(), *DisplayString);
	}
}

void FGameplayDebuggerCategory_NarrativeCharacter::DrawEquippedItems(FGameplayDebuggerCanvasContext& CanvasContext, const APlayerController* OwnerPC) const
{
	using namespace UE::NarrativeCharacter::Debug;

	struct FDebugEquippedItemData
	{
		FString EquippedItemName;
		FGameplayTag EquippedSlot;

		ENetworkStatus NetworkStatus = ENetworkStatus::LocalOnly;
	};

	const bool bConsiderNetworkStatus = !OwnerPC->IsNetMode(ENetMode::NM_Standalone);
	const ANarrativeCharacter* LocalChar = bConsiderNetworkStatus ? Cast<ANarrativeCharacter>(FindLocalDebugActor()) : nullptr;
	const TArray<FRepData::FGameplayEquippedItemDebug> LocalEquippedItems = LocalChar ? CollectEquipmentData(OwnerPC, LocalChar) : TArray<FRepData::FGameplayEquippedItemDebug>{};
	TArray<FRepData::FGameplayEquippedItemDebug> ServerEquippedItems = DataPack.EquippedItems;

	int NumEquippedItemCounts[+ENetworkStatus::MAX] = { 0 };

	// Reverse the order of iteration so RemoveAt has a better chance at removing near the end
	TArray<FDebugEquippedItemData> EquippedItemDebugData;
	for (int32 Index = LocalEquippedItems.Num() - 1; Index >= 0; --Index)
	{
		const FRepData::FGameplayEquippedItemDebug& LocalEquippedItem = LocalEquippedItems[Index];

		if (LocalEquippedItem.EquippedItem.Len())
		{
			FDebugEquippedItemData& DebugDatum = EquippedItemDebugData.Add_GetRef(
				FDebugEquippedItemData{
					.EquippedItemName = LocalEquippedItem.EquippedItem,
					.EquippedSlot = LocalEquippedItem.EquippedSlot,
					.NetworkStatus = ENetworkStatus::LocalOnly
				});

			int32 ServerIndex = ServerEquippedItems.FindLastByPredicate([FindItem = LocalEquippedItem.EquippedItem](const FRepData::FGameplayEquippedItemDebug& Item) { return FindItem == Item.EquippedItem; });
			if (ServerIndex != INDEX_NONE)
			{
				const FRepData::FGameplayEquippedItemDebug& ServerEquippedItem = ServerEquippedItems[ServerIndex];

				if (ServerEquippedItem.EquippedItem.Len())
				{
					DebugDatum.EquippedItemName = ServerEquippedItem.EquippedItem;
					DebugDatum.EquippedSlot = ServerEquippedItem.EquippedSlot;
					DebugDatum.NetworkStatus = (ServerEquippedItem.NetworkStatus == ENetworkStatus::Networked) ? ENetworkStatus::Networked : ENetworkStatus::Detached;

					// Remove them from server-only array
					ServerEquippedItems.RemoveAtSwap(ServerIndex, EAllowShrinking::No);
				}
			}
			++NumEquippedItemCounts[+DebugDatum.NetworkStatus];
		}

	}

	// If any entries are still in this array, they are server only and mark them as such
	for (const FRepData::FGameplayEquippedItemDebug& ServerEquippedItem : ServerEquippedItems)
	{
		FDebugEquippedItemData& DebugDatum = EquippedItemDebugData.AddDefaulted_GetRef();
		DebugDatum.EquippedItemName = ServerEquippedItem.EquippedItem;
		DebugDatum.EquippedSlot = ServerEquippedItem.EquippedSlot;
		DebugDatum.NetworkStatus = ENetworkStatus::ServerOnly;
		++NumEquippedItemCounts[+DebugDatum.NetworkStatus];
	}


	// Measure a large string once and save the value, so that we can size the columns properly and not continually resize it
	static float MaxLineSize = 0.0f;
	if (MaxLineSize <= 0.0f)
	{
		float TempSizeY;
		const FString MaxLineString = FString::Printf(TEXT("%.45s"), *UE::NarrativeCharacter::Debug::LongestDebugObjectName);
		CanvasContext.MeasureString(MaxLineString, MaxLineSize, TempSizeY);
	}

	constexpr float Padding = 10.0f;
	const float ColumnWidth = MaxLineSize + Padding;
	const float CanvasWidth = CanvasContext.Canvas->SizeX;
	const int NumColumns = FMath::Max(1, FMath::FloorToInt(CanvasWidth / ColumnWidth));

	// Figure out the colors we're going to be using and print the legend
	FString LegendText;
	const bool bAllEquippedItemsNetworked = (0 == (NumEquippedItemCounts[+ENetworkStatus::ServerOnly] + NumEquippedItemCounts[+ENetworkStatus::LocalOnly] + NumEquippedItemCounts[+ENetworkStatus::Detached]));
	if (!bConsiderNetworkStatus)// || bAllEquippedItemsNetworked)
	{
		// Normal case: no legend required
	}
	else
	{
		// Not all EquippedItems are networked; we should display all cases for easier debugging
		LegendText = FString::Printf(TEXT("Legend:  %sBoth [%d]    %sServer [%d]    %sLocal [%d]    %sNonReplicated [%d]"),
			BothColor, NumEquippedItemCounts[+ENetworkStatus::Networked],
			ServerColor, NumEquippedItemCounts[+ENetworkStatus::ServerOnly],
			LocalColor, NumEquippedItemCounts[+ENetworkStatus::LocalOnly],
			NonReplicatedColor, NumEquippedItemCounts[+ENetworkStatus::Detached]);
	}

	CanvasContext.Printf(TEXT("EquippedItems [%d]:"), EquippedItemDebugData.Num());
	CanvasContext.CursorX += 200.0f;
	CanvasContext.CursorY -= CanvasContext.GetLineHeight();
	CanvasContext.Print(LegendText);

	CanvasContext.CursorX += Padding;
	for (const FDebugEquippedItemData& EquippedItemData : EquippedItemDebugData)
	{
		const bool bServerValueMatch = true;
		const bool bLocalValueMatch = true;
		const bool bModified = !bServerValueMatch || !bLocalValueMatch;
		const bool bNetworkValueMatch = true;

		// Let's build up the EquippedItem value string which is just trying to represent the four states: srv cur [srv base] local cur [local base] in as little text as possible
		TStringBuilder<64> EquippedItemValueStr;
		if (bNetworkValueMatch && bServerValueMatch && bLocalValueMatch)
		{
			// Everything matches, let's choose white and any one of the values (since they match)
			EquippedItemValueStr.Appendf(TEXT("{white}%s"), *EquippedItemData.EquippedSlot.GetTagLeafName().ToString());
		}

		// Print positions manually to align things properly
		const float CursorX = CanvasContext.CursorX;
		const float CursorY = CanvasContext.CursorY;

		const ENetworkStatus NetworkStatus = bConsiderNetworkStatus ? EquippedItemData.NetworkStatus : ENetworkStatus::Networked;
		const FString ColoredEquippedItemName = UE::NarrativeCharacter::Debug::ColorNetworkString(NetworkStatus, EquippedItemData.EquippedItemName.Left(LongestDebugObjectName.Len()));
		const FString EquippedItemDebugText = FString::Printf(TEXT("%s%s: %.*s"), bModified ? TEXT("*") : TEXT(""), *ColoredEquippedItemName, EquippedItemValueStr.Len(), EquippedItemValueStr.GetData());
		CanvasContext.PrintAt(CursorX, CursorY, EquippedItemDebugText);

		// PrintAt would have reset these values, restore them.
		CanvasContext.CursorX = CursorX + (CanvasWidth / NumColumns);
		CanvasContext.CursorY = CursorY;

		// If we're going to overflow, go to the next line...
		if (CanvasContext.CursorX + ColumnWidth >= CanvasWidth)
		{
			CanvasContext.MoveToNewLine();
			CanvasContext.CursorX += Padding;
		}
	}

	// End the row with a newline
	if (CanvasContext.CursorX != CanvasContext.DefaultX)
	{
		CanvasContext.MoveToNewLine();
	}

	// End the category with a newline to separate from the other categories
	CanvasContext.MoveToNewLine();

}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_NarrativeCharacter::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_NarrativeCharacter());
}

TArray<FGameplayDebuggerCategory_NarrativeCharacter::FRepData::FGameplayEquippedItemDebug> FGameplayDebuggerCategory_NarrativeCharacter::CollectEquipmentData(const APlayerController* OwnerPC, const ANarrativeCharacter* Character) const
{
	TArray<FRepData::FGameplayEquippedItemDebug> DebugEquippedItems;

	if (Character)
	{
		if (const ANarrativeCharacter* MyPawn = Character)
		{
			if (UEquipmentComponent* EquipmentComp = MyPawn->GetEquipmentComponent())
			{
				//FString RoleStr = HasAuthority() ? "Server" : "Client";
				//DebuggerCategory->AddTextLine(FString::Printf(TEXT("%s Equipped items: (%d)"), *RoleStr, EquipmentComp->EquippedItems.Num()));

					// We need to do a lot of work to detect if the AttributeSet is replicated which only occurs of AbilitySystemComponent is replicated
				const ENetRole LocalRole = MyPawn->GetLocalRole();
				const ENetworkStatus NetStatus = LocalRole == ROLE_Authority ? ENetworkStatus::Networked : ENetworkStatus::LocalOnly;

				for (auto& Item : EquipmentComp->EquippedItems)
				{
					if (UEquippableItem* Equippable = Item.Value)
					{
						FRepData::FGameplayEquippedItemDebug& DebugEquippedItem = DebugEquippedItems.Add_GetRef(
							{
								.EquippedItem = Equippable->DisplayName.ToString(),
								.EquippedSlot = Item.Key,
								.NetworkStatus = NetStatus
							});

						//DebugEquippedItems.Add(DebugEquippedItem);
						//DebuggerCategory->AddTextLine(FString::Printf(TEXT("Slot %s,  %s"), *Item.Key.ToString(), *GetNameSafe(Equippable)));
					}
				}
			}
		}
	}

	return DebugEquippedItems;
}

void FGameplayDebuggerCategory_NarrativeCharacter::FRepData::Serialize(FArchive& Ar)
{

	int32 Num = EquippedItems.Num();
	Ar << Num;
	if (Ar.IsLoading())
	{
		EquippedItems.SetNum(Num);
	}

	for (int32 Idx = 0; Idx < Num; ++Idx)
	{
		Ar << EquippedItems[Idx].EquippedItem;
		Ar << EquippedItems[Idx].EquippedSlot;
		Ar << EquippedItems[Idx].NetworkStatus;
	}
}

#endif