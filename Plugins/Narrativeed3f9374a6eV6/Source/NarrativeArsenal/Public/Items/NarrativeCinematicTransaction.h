// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "NarrativeCinematicTransaction.generated.h"
class UNarrativeItem;
class UNarrativeInventoryComponent;

UENUM(BlueprintType)
enum class ENarrativeCinematicItemOperation : uint8 { Grant, Remove };

/** One exact item stack. Omitted removal GUID is accepted only when the exact class resolves uniquely. */
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FNarrativeCinematicItemMutation
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName MutationId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ENarrativeCinematicItemOperation Operation = ENarrativeCinematicItemOperation::Grant;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<UNarrativeItem> ItemClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGuid ItemGUID;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1",ClampMax="100000")) int32 Quantity = 1;
};

/** Journal on the existing inventory, not another inventory. Strong references retain removed identities/resources. */
USTRUCT()
struct NARRATIVEARSENAL_API FNarrativeCinematicItemChange
{
    GENERATED_BODY()
    UPROPERTY() FNarrativeCinematicItemMutation Contract;
    UPROPERTY() TWeakObjectPtr<UNarrativeInventoryComponent> Inventory;
    UPROPERTY() TObjectPtr<UNarrativeItem> Item;
    UPROPERTY() FGuid ItemGUID;
    int32 OriginalQuantity = 0;
    int32 ExpectedQuantity = 0;
    uint64 InventoryLoadRevision = 0;
    uint64 OriginalMembershipRevision = 0;
    uint64 ExpectedMembershipRevision = 0;
    uint64 OriginalQuantityRevision = 0;
    uint64 ExpectedQuantityRevision = 0;
    uint64 ExpectedStateRevision = 0;
    bool bApplied = false;
    bool bRetired = false;
};
