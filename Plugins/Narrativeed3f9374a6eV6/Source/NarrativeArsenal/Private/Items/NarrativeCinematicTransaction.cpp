// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Items/NarrativeCinematicTransaction.h"
#include "Items/InventoryComponent.h"
#include "Items/NarrativeItem.h"
#include "Items/EquippableItem.h"
#include "Items/NarrativeCinematicTransactionPolicy.h"
#include "Components/EquipmentComponent.h"

bool UNarrativeInventoryComponent::OwnsCinematicItem(const UNarrativeItem* Item) const
{
    return IsValid(Item) && Item->OwningInventory == this && Items.Contains(Item)
        && Item->ItemGUID.IsValid() && ItemGUIDMap.FindRef(Item->ItemGUID) == Item;
}

bool UNarrativeInventoryComponent::RemoveOwnedItemInternal(UNarrativeItem* Item)
{
    // Callers validate authority/permission and their exact write receipt immediately before this primitive.
    // There is no second overridable permission callback between validation and membership mutation.
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Item) || Item->OwningInventory != this || !Items.Contains(Item)) { return false; }
    const int32 RemovedQuantity = Item->GetQuantity();
    ++Item->InventoryMembershipRevision; Items.RemoveSingle(Item);
    if (ItemGUIDMap.FindRef(Item->ItemGUID) == Item) { ItemGUIDMap.Remove(Item->ItemGUID); }
    ReplicatedItems.Items.RemoveSingle(FNarrativeItemEntry(Item)); ReplicatedItems.MarkArrayDirty(); ++ReplicatedItemsKey;
    Item->RemovedFromInventory(this);
    if (RemovedQuantity != 0) { OnItemRemoved.Broadcast(Item, RemovedQuantity); }
    OnInventoryUpdated.Broadcast();
    return !Items.Contains(Item) && Item->OwningInventory != this;
}

bool UNarrativeInventoryComponent::PrepareCinematicItemChange(const FNarrativeCinematicItemMutation& Mutation,
    FNarrativeCinematicItemChange& Change, FString& OutError)
{
    const auto Fail = [&]() { OutError = TEXT("Cinematic item requires an approved exact class, unique owned stack, bounded quantity and authority."); return false; };
    if ((Change.bApplied && !Change.bRetired) || !GetOwner() || !GetOwner()->HasAuthority() || IsLoading() || !IsValid(Mutation.ItemClass)
        || Mutation.ItemClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
        || Mutation.MutationId.IsNone() || Mutation.Operation > ENarrativeCinematicItemOperation::Remove) { return Fail(); }
    const auto* Defaults = Mutation.ItemClass->GetDefaultObject<UNarrativeItem>();
    if (!Defaults || !NarrativeCinematicTransactionPolicy::ValidQuantity(Mutation.Quantity, Defaults->GetMaxStackSize())) { return Fail(); }
    Change = FNarrativeCinematicItemChange(); Change.Contract = Mutation; Change.Inventory = this;
    Change.InventoryLoadRevision = CinematicLoadRevision;
    UNarrativeItem* Item = nullptr;
    if (Mutation.Operation == ENarrativeCinematicItemOperation::Grant)
    {
        if (!Defaults->bAllowCampaignCinematicGrant || Mutation.ItemGUID.IsValid()) { return Fail(); }
        // Dedicated new stack: a rollback never subtracts from a player's pre-existing stack.
        Item = NewObject<UNarrativeItem>(GetOwner(), Mutation.ItemClass);
        Item->World = GetWorld(); Item->ItemGUID = FGuid::NewGuid(); Item->SetQuantity(Mutation.Quantity);
    }
    else
    {
        if (!Defaults->bAllowCampaignCinematicRemoval) { return Fail(); }
        for (UNarrativeItem* Candidate : Items)
        {
            if (!IsValid(Candidate) || Candidate->GetClass() != Mutation.ItemClass
                || (Mutation.ItemGUID.IsValid() && Candidate->ItemGUID != Mutation.ItemGUID)) { continue; }
            if (Item) { return Fail(); } Item = Candidate;
        }
        if (!OwnsCinematicItem(Item) || Item->GetQuantity() < Mutation.Quantity) { return Fail(); }
    }
    if (!IsValid(Item) || Item->IsActive() || Item->bIsBusy) { return Fail(); }
    Change.Item = Item; Change.ItemGUID = Item->ItemGUID;
    Change.OriginalQuantity = Item->GetQuantity(); Change.ExpectedQuantity = Change.OriginalQuantity;
    Change.OriginalMembershipRevision = Change.ExpectedMembershipRevision = Item->GetInventoryMembershipRevision();
    Change.OriginalQuantityRevision = Change.ExpectedQuantityRevision = Item->GetQuantityRevision();
    Change.ExpectedStateRevision = Item->GetCinematicStateRevision();
    OutError.Reset(); return true;
}

bool UNarrativeInventoryComponent::ValidateCinematicItemChange(const FNarrativeCinematicItemChange& Change,
    bool bApplied, FString& OutError) const
{
    const auto Fail = [&]() { OutError = TEXT("A cinematic inventory identity, resource, quantity or native write owner changed."); return false; };
    const auto* Item = Change.Item.Get();
    if (Change.bRetired || Change.bApplied != bApplied || Change.Inventory.Get() != this || !GetOwner()
        || !GetOwner()->HasAuthority() || GetOwner()->IsActorBeingDestroyed() || IsLoading()
        || CinematicLoadRevision != Change.InventoryLoadRevision || !IsValid(Item)
        || Item->GetClass() != Change.Contract.ItemClass || Item->ItemGUID != Change.ItemGUID
        || Item->GetQuantity() != Change.ExpectedQuantity || Item->IsActive() || Item->bIsBusy
        || !NarrativeCinematicTransactionPolicy::OwnsWrite(Item->GetQuantityRevision(), Change.ExpectedQuantityRevision)
        || Item->GetInventoryMembershipRevision() != Change.ExpectedMembershipRevision
        || Item->GetCinematicStateRevision() != Change.ExpectedStateRevision) { return Fail(); }
    const bool bExpectedOwned = Change.Contract.Operation == ENarrativeCinematicItemOperation::Grant ? bApplied
        : !(bApplied && Change.Contract.Quantity == Change.OriginalQuantity);
    if (bExpectedOwned ? !OwnsCinematicItem(Item)
        : (Item->OwningInventory != nullptr || Items.Contains(Item) || ItemGUIDMap.Contains(Change.ItemGUID))) { return Fail(); }
    OutError.Reset(); return true;
}

bool UNarrativeInventoryComponent::InsertCinematicItem(UNarrativeItem* Item, bool bRestoreExisting)
{
    if (!IsValid(Item) || Item->OwningInventory || Items.Contains(Item) || !Item->ItemGUID.IsValid()
        || ItemGUIDMap.Contains(Item->ItemGUID)) { return false; }
    ++Item->InventoryMembershipRevision;
    Item->OwningInventory = this; Item->World = GetWorld(); Items.Add(Item); ItemGUIDMap.Add(Item->ItemGUID, Item);
    FNarrativeItemEntry& RepEntry = ReplicatedItems.Items.Add_GetRef(Item); ReplicatedItems.MarkItemDirty(RepEntry);
    Item->MarkDirtyForReplication();
    // Restore-mode initialization preserves an existing instance's clip, recharge time and fragments.
    // It also suppresses auto-use, auto-equip, ammo reload and NPC activity grants for new cinematic items.
    if (bRestoreExisting) { Item->UNarrativeItem::AddedToInventory(this, true); }
    else { Item->AddedToInventory(this, true); }
    if (!OwnsCinematicItem(Item)) { return false; }
    OnInventoryUpdated.Broadcast();
    return OwnsCinematicItem(Item);
}

bool UNarrativeInventoryComponent::ApplyCinematicItemChange(FNarrativeCinematicItemChange& Change, FString& OutError)
{
    if (!ValidateCinematicItemChange(Change, false, OutError)) { return false; }
    auto* Item = Change.Item.Get();
    if (Change.Contract.Operation == ENarrativeCinematicItemOperation::Grant)
    {
        if (!NarrativeCinematicTransactionPolicy::DedicatedStackFits(Change.Contract.Quantity, Item->GetMaxStackSize(),
            Items.Num(), Capacity, GetCurrentWeight(), Item->Weight, WeightCapacity))
        { OutError = TEXT("Cinematic grant requires capacity for a complete dedicated stack; partial grants are forbidden."); return false; }
        Change.bApplied = true; ++Change.ExpectedMembershipRevision; ++Change.ExpectedStateRevision;
        if (!InsertCinematicItem(Item)) { OutError = TEXT("Cinematic grant initialization was interrupted."); return false; }
        FItemAddResult Added = FItemAddResult::AddedAll({Item}, Change.Contract.Quantity); Added.ItemClass = Change.Contract.ItemClass;
        OnItemAdded.Broadcast(Added);
    }
    else
    {
        const auto* Equipped = Cast<UEquippableItem>(Item);
        if ((Equipped && Equipped->IsEquipped()) || !Item->CanBeRemoved()
            || !ValidateCinematicItemChange(Change, false, OutError))
        { OutError = TEXT("Cinematic removal must follow its explicit equipment replacement and remain removable."); return false; }
        Change.bApplied = true;
        if (Change.Contract.Quantity == Change.OriginalQuantity)
        {
            ++Change.ExpectedMembershipRevision;
            if (!RemoveOwnedItemInternal(Item)) { OutError = TEXT("Cinematic removal failed."); return false; }
        }
        else
        {
            Change.ExpectedQuantity -= Change.Contract.Quantity;
            ++Change.ExpectedQuantityRevision; ++Change.ExpectedStateRevision;
            Item->SetQuantity(Change.ExpectedQuantity);
            OnItemRemoved.Broadcast(Item, Change.Contract.Quantity);
            OnInventoryUpdated.Broadcast();
        }
    }
    return ValidateCinematicItemChange(Change, true, OutError);
}

bool UNarrativeInventoryComponent::RollbackCinematicItemChange(FNarrativeCinematicItemChange& Change)
{
    if (Change.bRetired) { return !Change.bApplied; }
    Change.bRetired = true;
    if (!Change.bApplied) { return true; }
    auto* Item = Change.Item.Get();
    if (Change.Inventory.Get() != this || !GetOwner() || !GetOwner()->HasAuthority() || GetOwner()->IsActorBeingDestroyed()
        || IsLoading() || CinematicLoadRevision != Change.InventoryLoadRevision || !IsValid(Item) || Item->ItemGUID != Change.ItemGUID
        || Item->GetClass() != Change.Contract.ItemClass || Item->GetQuantity() != Change.ExpectedQuantity
        || Item->GetInventoryMembershipRevision() != Change.ExpectedMembershipRevision
        || Item->GetQuantityRevision() != Change.ExpectedQuantityRevision) { return false; }
    if (Change.Contract.Operation == ENarrativeCinematicItemOperation::Grant)
    {
        // Never delete a new item another owner has since consumed, equipped, modified or used.
        const auto* Equipped = Cast<UEquippableItem>(Item);
        if (!OwnsCinematicItem(Item) || Item->IsActive() || Item->bIsBusy || (Equipped && Equipped->IsEquipped())
            || Item->GetCinematicStateRevision() != Change.ExpectedStateRevision || !Item->CanBeRemoved()
            || Item->GetCinematicStateRevision() != Change.ExpectedStateRevision) { return false; }
        if (!OwnsCinematicItem(Item) || Item->ItemGUID != Change.ItemGUID || Item->GetQuantity() != Change.ExpectedQuantity
            || Item->GetQuantityRevision() != Change.ExpectedQuantityRevision
            || Item->GetInventoryMembershipRevision() != Change.ExpectedMembershipRevision
            || Item->IsActive() || Item->bIsBusy || (Equipped && Equipped->IsEquipped())) { return false; }
        Change.bApplied = false; return RemoveOwnedItemInternal(Item);
    }
    if (Change.Contract.Quantity == Change.OriginalQuantity)
    {
        // Retain this object, including GUID, clip, cooldown, fragments and any later resource writes.
        // Never overwrite another member or exceed capacity that later legitimate inventory writes used.
        if (Item->OwningInventory || Items.Contains(Item) || ItemGUIDMap.Contains(Change.ItemGUID)
            || !NarrativeCinematicTransactionPolicy::DedicatedStackFits(Item->GetQuantity(), Item->GetMaxStackSize(),
                Items.Num(), Capacity, GetCurrentWeight(), Item->Weight, WeightCapacity)) { return false; }
        Change.bApplied = false; return InsertCinematicItem(Item, true);
    }
    if (!OwnsCinematicItem(Item) || !FMath::IsFinite(Item->Weight) || Item->Weight < 0.f
        || !FMath::IsFinite(GetCurrentWeight()) || !FMath::IsFinite(WeightCapacity) || WeightCapacity < 0.f
        || Change.OriginalQuantity > Item->GetMaxStackSize()
        || GetCurrentWeight() + Item->Weight * Change.Contract.Quantity > WeightCapacity) { return false; }
    Change.bApplied = false; Item->SetQuantity(Change.OriginalQuantity); OnInventoryUpdated.Broadcast();
    return OwnsCinematicItem(Item) && Item->GetQuantity() == Change.OriginalQuantity;
}

bool UNarrativeInventoryComponent::SetCinematicEquipment(UEquippableItem* Item, FGameplayTag Slot)
{
    return GetOwner() && GetOwner()->HasAuthority() && !IsLoading() && OwnsCinematicItem(Item)
        && Item->GetClass()->GetDefaultObject<UNarrativeItem>()->bAllowCampaignCinematicEquipment
        && !Item->IsActive() && !Item->bIsBusy && (!Slot.IsValid() || Item->AllowsEquipmentSlot(Slot))
        && Item->EquipItem(Slot);
}
