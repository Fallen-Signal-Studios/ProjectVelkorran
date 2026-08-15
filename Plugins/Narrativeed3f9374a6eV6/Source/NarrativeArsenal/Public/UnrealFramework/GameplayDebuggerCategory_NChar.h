// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"

#if WITH_GAMEPLAY_DEBUGGER_MENU

#include "GameplayDebuggerCategory.h"
#include "GameplayTagContainer.h"
#include "NarrativeCharacter.h"

class AActor;
class APlayerController;

class FGameplayDebuggerCategory_NarrativeCharacter : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_NarrativeCharacter();

	//Copy technique from GAS debugger. 
	enum class ENetworkStatus : uint8
	{
		ServerOnly, LocalOnly, Networked, Detached, MAX
	};

	// Unary operator + for quick conversion from enum class to int32
	friend constexpr int32 operator+(const ENetworkStatus& value) { return static_cast<int32>(value); }

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

	void DrawEquippedItems(FGameplayDebuggerCanvasContext& CanvasContext, const APlayerController* OwnerPC) const;

	struct FRepData
	{

		struct FGameplayEquippedItemDebug
		{
			FString EquippedItem;
			FGameplayTag EquippedSlot;
			ENetworkStatus NetworkStatus = ENetworkStatus::ServerOnly;
		};
		TArray<FGameplayEquippedItemDebug> EquippedItems;


		void Serialize(FArchive& Ar);

	};
	FRepData DataPack;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

private:
	TArray<FRepData::FGameplayEquippedItemDebug> CollectEquipmentData(const APlayerController* OwnerPC, const ANarrativeCharacter* Character) const;

};

#endif // WITH_GAMEPLAY_DEBUGGER
