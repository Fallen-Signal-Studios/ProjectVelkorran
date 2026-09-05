// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Cinematics/SovCampaignCinematicComponent.h"
#include "Items/InventoryComponent.h"
#include "Items/NarrativeItem.h"
#include "Items/EquippableItem.h"
#include "Items/WeaponItem.h"
#include "Items/NarrativeCinematicTransactionPolicy.h"
#include "Components/EquipmentComponent.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Weapons/WeaponVisual.h"
#include "Weapons/SovTransformingWeaponVisual.h"

namespace
{
    bool IsExactOwnedItem(UNarrativeInventoryComponent* Inventory, const UNarrativeItem* Item, const FGuid& GUID)
    {
        return IsValid(Inventory) && IsValid(Item) && GUID.IsValid() && Item->ItemGUID == GUID
            && Item->OwningInventory == Inventory && Inventory->GetItems().Contains(Item)
            && Inventory->FindItemByGUID(GUID) == Item;
    }
}

bool USovCampaignCinematicComponent::ResolveInventoryPostconditions(FString& OutError)
{
    InventoryChanges.Reset(); EquipmentSnapshots.Reset();
    const auto FindCharacter = [&](FName Binding) -> ANarrativeCharacter*
    {
        for (int32 Index = 0; Index < Participants.Num(); ++Index)
        { if (Participants[Index].BindingTag == Binding && Snapshot.IsValidIndex(Index)) { return Snapshot[Index].Character.Get(); } }
        return nullptr;
    };
    const auto Fail = [&]() { OutError = TEXT("Cinematic inventory/equipment manifest has missing, ambiguous, unapproved or conflicting owned items."); return false; };
    TSet<UNarrativeItem*> MutatedItems;
    for (const auto& Contract : InventoryPostconditions)
    {
        auto* Character = FindCharacter(Contract.ParticipantBinding);
        auto* Inventory = Character ? Character->GetInventoryComponent() : nullptr;
        FNarrativeCinematicItemChange Change;
        if (!Inventory || Inventory->GetOwningPawn() != Character
            || !Inventory->PrepareCinematicItemChange(Contract.Mutation, Change, OutError)) { return false; }
        if (MutatedItems.Contains(Change.Item.Get())) { return Fail(); }
        MutatedItems.Add(Change.Item.Get()); InventoryChanges.Add(Change);
    }
    struct FProjectedInventory { int32 Stacks = 0; double Weight = 0.; };
    TMap<UNarrativeInventoryComponent*, FProjectedInventory> Projected;
    for (const auto& Change : InventoryChanges)
    {
        auto* Inventory = Change.Inventory.Get();
        if (!Projected.Contains(Inventory))
        { FProjectedInventory Value; Value.Stacks = Inventory->GetItems().Num(); Value.Weight = Inventory->GetCurrentWeight(); Projected.Add(Inventory, Value); }
    }
    for (const auto Operation : {ENarrativeCinematicItemOperation::Remove, ENarrativeCinematicItemOperation::Grant})
    {
        for (const auto& Change : InventoryChanges)
        {
            if (Change.Contract.Operation != Operation) { continue; }
            auto* Inventory = Change.Inventory.Get(); auto& Value = Projected.FindChecked(Inventory); const auto* Item = Change.Item.Get();
            if (!FMath::IsFinite(Value.Weight) || Value.Weight < 0. || !FMath::IsFinite(Item->Weight) || Item->Weight < 0.f) { return Fail(); }
            if (Operation == ENarrativeCinematicItemOperation::Remove)
            { Value.Stacks -= Change.Contract.Quantity == Change.OriginalQuantity ? 1 : 0; Value.Weight = FMath::Max(0., Value.Weight - Item->Weight * Change.Contract.Quantity); }
            else
            {
                if (!NarrativeCinematicTransactionPolicy::DedicatedStackFits(Change.Contract.Quantity, Item->GetMaxStackSize(),
                    Value.Stacks, Inventory->GetCapacity(), Value.Weight, Item->Weight, Inventory->GetWeightCapacity()))
                { OutError = TEXT("Cinematic manifest cannot fit its complete dedicated grants after declared removals."); return false; }
                ++Value.Stacks; Value.Weight += Item->Weight * Change.Contract.Quantity;
            }
        }
    }
    TSet<UNarrativeItem*> EquipmentItems;
    for (const auto& Contract : EquipmentPostconditions)
    {
        auto* Character = FindCharacter(Contract.ParticipantBinding);
        auto* Inventory = Character ? Character->GetInventoryComponent() : nullptr;
        auto* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
        if (!Inventory || !Equipment || Inventory->GetOwningPawn() != Character) { return Fail(); }
        FSovCinematicEquipmentSnapshot Entry; Entry.Contract = Contract; Entry.Character = Character;
        Entry.Inventory = Inventory; Entry.Equipment = Equipment; Entry.InventoryLoadRevision = Inventory->GetCinematicLoadRevision();
        Entry.Previous = Equipment->GetEquippedItemAtSlot(Contract.EquipmentSlot);
        if (Entry.Previous)
        {
            if (!Contract.PreviousItemClass || Entry.Previous->GetClass() != Contract.PreviousItemClass
                || (Contract.PreviousItemGUID.IsValid() && Entry.Previous->ItemGUID != Contract.PreviousItemGUID)
                || !IsExactOwnedItem(Inventory, Entry.Previous, Entry.Previous->ItemGUID)
                || !Entry.Previous->GetClass()->GetDefaultObject<UNarrativeItem>()->bAllowCampaignCinematicEquipment
                || Entry.Previous->GetEquippedSlot() != Contract.EquipmentSlot || EquipmentItems.Contains(Entry.Previous)) { return Fail(); }
            Entry.PreviousGUID = Entry.Previous->ItemGUID;
            if (auto* Weapon = Cast<UWeaponItem>(Entry.Previous))
            {
                const auto* Visual = Character->GetWeaponVisual(Contract.EquipmentSlot);
                if (!Weapon->GetWeaponVisualClass().IsValid() || !IsValid(Visual) || Visual->IsActorBeingDestroyed() || Visual->WeaponOwner != Weapon) { return Fail(); }
            }
            Entry.ExpectedPreviousRevision = Entry.Previous->GetEquipmentRevision();
            Entry.PreviousMembershipRevision = Entry.Previous->GetInventoryMembershipRevision();
            Entry.ExpectedPreviousStateRevision = Entry.Previous->GetCinematicStateRevision(); EquipmentItems.Add(Entry.Previous);
        }
        else if (Contract.PreviousItemClass || Contract.PreviousItemGUID.IsValid()) { return Fail(); }
        if (Contract.ReplacementItemClass)
        {
            if (!Contract.ReplacementGrantId.IsNone())
            {
                for (const auto& Change : InventoryChanges)
                {
                    if (Change.Inventory.Get() == Inventory && Change.Contract.MutationId == Contract.ReplacementGrantId
                        && Change.Contract.Operation == ENarrativeCinematicItemOperation::Grant)
                    { Entry.Replacement = Cast<UEquippableItem>(Change.Item); }
                }
            }
            else
            {
                for (UNarrativeItem* Item : Inventory->GetItems())
                {
                    if (!IsValid(Item) || Item->GetClass() != Contract.ReplacementItemClass
                        || (Contract.ReplacementItemGUID.IsValid() && Item->ItemGUID != Contract.ReplacementItemGUID)) { continue; }
                    if (Entry.Replacement) { return Fail(); } Entry.Replacement = Cast<UEquippableItem>(Item);
                }
            }
            auto* Replacement = Entry.Replacement.Get();
            if (!Replacement || Replacement->GetClass() != Contract.ReplacementItemClass || Replacement->IsEquipped()
                || !Replacement->AllowsEquipmentSlot(Contract.EquipmentSlot) || Replacement->IsActive() || Replacement->bIsBusy
                || !Replacement->GetClass()->GetDefaultObject<UNarrativeItem>()->bAllowCampaignCinematicEquipment
                || EquipmentItems.Contains(Replacement)) { return Fail(); }
            for (const auto& Change : InventoryChanges)
            { if (Change.Item == Replacement && Change.Contract.Operation == ENarrativeCinematicItemOperation::Remove) { return Fail(); } }
            Entry.ReplacementGUID = Replacement->ItemGUID; Entry.ExpectedReplacementRevision = Replacement->GetEquipmentRevision();
            Entry.ReplacementMembershipRevision = Replacement->GetInventoryMembershipRevision();
            if (const auto* Weapon = Cast<UWeaponItem>(Replacement); Weapon && (!Weapon->GetWeaponVisualClass().IsValid() || !IsValid(Character->GetCharacterVisual()))) { return Fail(); }
            Entry.ExpectedReplacementStateRevision = Replacement->GetCinematicStateRevision(); EquipmentItems.Add(Replacement);
        }
        Entry.ExpectedSlotRevision = Equipment->GetEquipmentSlotRevision(Contract.EquipmentSlot);
        EquipmentSnapshots.Add(Entry);
    }
    for (const auto& Change : InventoryChanges)
    {
        if (Change.Contract.Operation != ENarrativeCinematicItemOperation::Remove) { continue; }
        if (const auto* Equipped = Cast<UEquippableItem>(Change.Item); Equipped && Equipped->IsEquipped())
        {
            bool bExplicitlyReplaced = false;
            for (const auto& Entry : EquipmentSnapshots) { bExplicitlyReplaced |= Entry.Previous == Equipped; }
            if (!bExplicitlyReplaced) { return Fail(); }
        }
    }
    for (int32 Index = 0; Index < Participants.Num(); ++Index)
    {
        for (const auto& Entry : EquipmentSnapshots)
        {
            if (Entry.Character != Snapshot[Index].Character) { continue; }
            // A replacement must declare an explicit final wield state; Keep cannot retain the removed identity.
            if (Participants[Index].ExitWield == ESovCinematicExitWield::Keep) { return Fail(); }
            if (Participants[Index].ExitWield == ESovCinematicExitWield::DrawRequiredWeapon
                && Entry.Contract.EquipmentSlot == Participants[Index].EquipmentSlot)
            {
                auto* Weapon = Cast<UWeaponItem>(Entry.Replacement);
                if (!Weapon || Weapon->GetWeaponWieldAttachConfig(Participants[Index].WieldSlot).SocketName.IsNone()
                    || !Weapon->GetWeaponWieldAttachConfig(Participants[Index].WieldSlot).Offset.IsValid()) { return Fail(); }
            }
        }
    }
    return ValidateInventoryPostconditions(false, OutError);
}

bool USovCampaignCinematicComponent::ValidateInventoryPostconditions(bool bFinal, FString& OutError) const
{
    const auto Fail = [&]() { OutError = TEXT("Cinematic inventory/equipment postcondition or write ownership changed."); return false; };
    if (InventoryChanges.Num() != InventoryPostconditions.Num() || EquipmentSnapshots.Num() != EquipmentPostconditions.Num()) { return Fail(); }
    for (const auto& Change : InventoryChanges)
    {
        auto* Inventory = Change.Inventory.Get();
        if (!Inventory || (bFinal && !Change.bApplied) || !Inventory->ValidateCinematicItemChange(Change, Change.bApplied, OutError)) { return false; }
        bool bOwnerMatches = false;
        for (const auto& Participant : Snapshot)
        { bOwnerMatches |= Participant.Character.IsValid() && Participant.Character->GetInventoryComponent() == Inventory && Inventory->GetOwningPawn() == Participant.Character.Get(); }
        if (!bOwnerMatches) { return Fail(); }
    }
    const auto ExpectedState = [&](UNarrativeItem* Item, uint64 Fallback)
    { for (const auto& Change : InventoryChanges) { if (Change.Item == Item) { return Change.ExpectedStateRevision; } } return Fallback; };
    for (const auto& Entry : EquipmentSnapshots)
    {
        auto* Character = Entry.Character.Get(); auto* Inventory = Entry.Inventory.Get(); auto* Equipment = Entry.Equipment.Get();
        if (Entry.bRetired || !IsValid(Character) || Character->IsActorBeingDestroyed() || !Character->HasAuthority()
            || Character->GetWorld() != GetWorld() || !Inventory || !Equipment || Character->GetInventoryComponent() != Inventory
            || Character->GetEquipmentComponent() != Equipment || Inventory->GetOwningPawn() != Character
            || Inventory->IsLoading() || Inventory->GetCinematicLoadRevision() != Entry.InventoryLoadRevision
            || Equipment->GetEquipmentSlotRevision(Entry.Contract.EquipmentSlot) != Entry.ExpectedSlotRevision) { return Fail(); }
        UEquippableItem* Expected = Entry.bReplacementEquipped ? Entry.Replacement.Get() : Entry.bPreviousUnequipped ? nullptr : Entry.Previous.Get();
        if (Equipment->GetEquippedItemAtSlot(Entry.Contract.EquipmentSlot) != Expected
            || (bFinal && ((Entry.Previous && !Entry.bPreviousUnequipped) || (Entry.Replacement && !Entry.bReplacementEquipped)))) { return Fail(); }
        if (Entry.Previous && (!IsValid(Entry.Previous) || Entry.Previous->ItemGUID != Entry.PreviousGUID
            || Entry.Previous->GetEquipmentRevision() != Entry.ExpectedPreviousRevision
            || Entry.Previous->GetCinematicStateRevision() != ExpectedState(Entry.Previous, Entry.ExpectedPreviousStateRevision)
            || Entry.Previous->GetEquippedSlot() != (Entry.bPreviousUnequipped ? FGameplayTag() : Entry.Contract.EquipmentSlot))) { return Fail(); }
        if (Entry.Replacement && (!IsValid(Entry.Replacement) || Entry.Replacement->ItemGUID != Entry.ReplacementGUID
            || Entry.Replacement->GetEquipmentRevision() != Entry.ExpectedReplacementRevision
            || Entry.Replacement->GetCinematicStateRevision() != ExpectedState(Entry.Replacement, Entry.ExpectedReplacementStateRevision)
            || Entry.Replacement->GetEquippedSlot() != (Entry.bReplacementEquipped ? Entry.Contract.EquipmentSlot : FGameplayTag()))) { return Fail(); }
        for (UEquippableItem* Item : {Entry.Previous.Get(), Entry.Replacement.Get()})
        {
            if (!Item) { continue; }
            bool bHasInventoryJournal = false;
            for (const auto& Change : InventoryChanges) { bHasInventoryJournal |= Change.Item == Item; }
            if (!bHasInventoryJournal && (!IsExactOwnedItem(Inventory, Item, Item == Entry.Previous ? Entry.PreviousGUID : Entry.ReplacementGUID)
                || Item->GetInventoryMembershipRevision() != (Item == Entry.Previous ? Entry.PreviousMembershipRevision : Entry.ReplacementMembershipRevision))) { return Fail(); }
        }
        if (Expected && !IsExactOwnedItem(Inventory, Expected, Expected == Entry.Previous ? Entry.PreviousGUID : Entry.ReplacementGUID)) { return Fail(); }
        if (bFinal && Entry.Replacement && Cast<UWeaponItem>(Entry.Replacement))
        {
            const auto* Visual = Character->GetWeaponVisual(Entry.Contract.EquipmentSlot);
            const auto Wield = Character->GetWeaponWieldState(); FGameplayTag WieldSlot;
            for (int32 Index = 0; Index < Wield.EquipSlots.Num(); ++Index)
            { if (Wield.EquipSlots.GetByIndex(Index) == Entry.Contract.EquipmentSlot && Index < Wield.WieldSlots.Num()) { WieldSlot = Wield.WieldSlots.GetByIndex(Index); } }
            if (!IsValid(Visual) || Visual->IsActorBeingDestroyed() || Visual->WeaponOwner != Entry.Replacement
                || Visual->AttachState.CharOwner != Character || Visual->VisualOwner != Character->GetCharacterVisual()
                || !Visual->HasCommittedAttachment(Entry.Contract.EquipmentSlot, WieldSlot)) { return Fail(); }
        }
    }
    if (!InventoryPostconditions.IsEmpty() || !EquipmentPostconditions.IsEmpty())
    {
        for (const auto& Entry : Snapshot)
        {
            if (!Entry.Character.IsValid() || Entry.Character->GetWeaponWieldRevision()
                != (Entry.bWieldApplied ? Entry.AppliedWieldRevision : Entry.OriginalWieldRevision)) { return Fail(); }
        }
    }
    return true;
}

void USovCampaignCinematicComponent::AdvanceOwnedItemState(UNarrativeItem* Item)
{
    for (auto& Change : InventoryChanges) { if (Change.Item == Item) { ++Change.ExpectedStateRevision; } }
    for (auto& Entry : EquipmentSnapshots)
    {
        if (Entry.Previous == Item) { ++Entry.ExpectedPreviousStateRevision; }
        if (Entry.Replacement == Item) { ++Entry.ExpectedReplacementStateRevision; }
    }
}

bool USovCampaignCinematicComponent::SetOwnedWield(int32 ParticipantIndex, const FWeaponWieldState& Wield, FString& OutError)
{
    if (!Snapshot.IsValidIndex(ParticipantIndex)) { return false; }
    auto& Entry = Snapshot[ParticipantIndex]; auto* Character = Entry.Character.Get();
    if (!Character || Character->GetWeaponWieldRevision() != (Entry.bWieldApplied ? Entry.AppliedWieldRevision : Entry.OriginalWieldRevision))
    { OutError = TEXT("A later native wield writer owns this participant."); return false; }
    Entry.bWieldApplied = true; Entry.AppliedWieldRevision = Character->GetWeaponWieldRevision() + 1;
    const FWeaponWieldState Previous = Character->GetWeaponWieldState();
    Character->SetWieldState(Wield);
    if (!IsValid(Character) || Character->IsActorBeingDestroyed() || Character->GetWeaponWieldRevision() != Entry.AppliedWieldRevision
        || Character->GetWeaponWieldState().EquipSlots != Wield.EquipSlots || Character->GetWeaponWieldState().WieldSlots != Wield.WieldSlots)
    { OutError = TEXT("A cinematic wield callback changed its native postcondition."); return false; }
    const FWeaponWieldState Applied = Character->GetWeaponWieldState();
    TArray<TObjectPtr<UWeaponItem>> Presentations = Previous.EquipWeapons;
    for (UWeaponItem* Weapon : Applied.EquipWeapons) { Presentations.AddUnique(Weapon); }
    for (UWeaponItem* Weapon : Presentations)
    {
        if (!IsValid(Weapon) || !Weapon->IsEquipped() || Weapon->OwningInventory != Character->GetInventoryComponent())
        { OutError = TEXT("Cinematic wield presentation lost its equipped inventory identity."); return false; }
        FGameplayTag Target;
        for (int32 Index = 0; Index < Applied.EquipWeapons.Num(); ++Index)
        { if (Applied.EquipWeapons[Index] == Weapon && Index < Applied.WieldSlots.Num()) { Target = Applied.WieldSlots.GetByIndex(Index); } }
        auto* Visual = Character->GetWeaponVisual(Weapon->GetEquippedSlot());
        if (auto* Transforming = Cast<ASovTransformingWeaponVisual>(Visual))
        { if (!Transforming->CompleteCinematicHandoff(Target)) { OutError = TEXT("Cinematic transforming-weapon handoff was superseded or could not settle."); return false; } }
        if (!IsValid(Visual) || Visual->IsActorBeingDestroyed() || Visual->WeaponOwner != Weapon
            || Visual->AttachState.CharOwner != Character || Visual->VisualOwner != Character->GetCharacterVisual()
            || !Visual->HasCommittedAttachment(Weapon->GetEquippedSlot(), Target)
            || !IsValid(Character) || Character->IsActorBeingDestroyed() || Character->GetWeaponWieldRevision() != Entry.AppliedWieldRevision)
        { OutError = TEXT("Cinematic final weapon presentation does not match its native wield state."); return false; }
    }
    return true;
}

UWeaponItem* USovCampaignCinematicComponent::GetExitWeapon(int32 ParticipantIndex) const
{
    if (!Snapshot.IsValidIndex(ParticipantIndex) || !Participants.IsValidIndex(ParticipantIndex)) { return nullptr; }
    if (bInventoryPostconditionsApplied)
    {
        for (const auto& Entry : EquipmentSnapshots)
        { if (Entry.Character == Snapshot[ParticipantIndex].Character && Entry.Contract.EquipmentSlot == Participants[ParticipantIndex].EquipmentSlot) { return Cast<UWeaponItem>(Entry.Replacement); } }
    }
    return Snapshot[ParticipantIndex].RequiredWeapon.Get();
}

bool USovCampaignCinematicComponent::ApplyInventoryPostconditions(FString& OutError)
{
    if (bInventoryPostconditionsApplied || !ValidateInventoryPostconditions(false, OutError)) { return false; }
    const uint64 Epoch = RequestEpoch;
    const auto Current = [&]() { return RequestEpoch == Epoch && OwnsPlaybackGeneration() && IsContextCurrent(); };
    // Holster first, before any slot is cleared, so Narrative does not retain a wield pointer to a removed item.
    for (int32 Index = 0; Index < Snapshot.Num(); ++Index)
    {
        bool bChangesEquipment = false;
        for (const auto& Entry : EquipmentSnapshots) { bChangesEquipment |= Entry.Character == Snapshot[Index].Character; }
        if (bChangesEquipment && !SetOwnedWield(Index, FWeaponWieldState(), OutError)) { return false; }
        if (!Current() || !ValidateInventoryPostconditions(false, OutError)) { return false; }
    }
    for (auto& Entry : EquipmentSnapshots)
    {
        if (!Entry.Previous) { continue; }
        Entry.bPreviousUnequipped = true; ++Entry.ExpectedPreviousRevision; ++Entry.ExpectedSlotRevision;
        AdvanceOwnedItemState(Entry.Previous);
        if (!Entry.Inventory->SetCinematicEquipment(Entry.Previous, FGameplayTag()) || !Current()
            || !ValidateInventoryPostconditions(false, OutError)) { return false; }
    }
    // Remove before grant: exact replacements can succeed at full capacity without allowing partial grants.
    for (const auto Operation : {ENarrativeCinematicItemOperation::Remove, ENarrativeCinematicItemOperation::Grant})
    {
        for (auto& Change : InventoryChanges)
        {
            if (Change.Contract.Operation != Operation) { continue; }
            if (!Change.Inventory.IsValid() || !Change.Inventory->ApplyCinematicItemChange(Change, OutError) || !Current()
                || !ValidateInventoryPostconditions(false, OutError)) { return false; }
        }
    }
    for (auto& Entry : EquipmentSnapshots)
    {
        if (!Entry.Replacement) { continue; }
        Entry.bReplacementEquipped = true; ++Entry.ExpectedReplacementRevision; ++Entry.ExpectedSlotRevision;
        AdvanceOwnedItemState(Entry.Replacement);
        if (!Entry.Inventory->SetCinematicEquipment(Entry.Replacement, Entry.Contract.EquipmentSlot) || !Current()) { return false; }
        if (auto* Weapon = Cast<UWeaponItem>(Entry.Replacement))
        {
            auto* Visual = Entry.Character.IsValid() ? Entry.Character->GetCharacterVisual() : nullptr;
            if (!IsValid(Visual) || !Visual->CompletePreloadedWeaponVisual(Weapon) || !Current())
            { OutError = TEXT("Preloaded replacement weapon could not create its owned presentation."); return false; }
        }
        if (!ValidateInventoryPostconditions(false, OutError)) { return false; }
    }
    bInventoryPostconditionsApplied = true;
    return ValidateInventoryPostconditions(true, OutError);
}

void USovCampaignCinematicComponent::RestoreInventoryPostconditions()
{
    if (bInventoryTransactionCommitted) { return; }
    const uint64 Epoch = RequestEpoch;
    const auto Current = [&]() { return RequestEpoch == Epoch && OwnsPlaybackGeneration(); };
    const auto EquipmentCurrent = [&](const FSovCinematicEquipmentSnapshot& Entry)
    {
        return Current() && Entry.Character.IsValid() && !Entry.Character->IsActorBeingDestroyed()
            && Entry.Character->HasAuthority() && Entry.Character->GetWorld() == GetWorld()
            && Entry.Inventory.IsValid() && !Entry.Inventory->IsLoading() && Entry.Equipment.IsValid()
            && Entry.Inventory->GetCinematicLoadRevision() == Entry.InventoryLoadRevision
            && Entry.Character->GetInventoryComponent() == Entry.Inventory.Get()
            && Entry.Character->GetEquipmentComponent() == Entry.Equipment.Get()
            && Entry.Inventory->GetOwningPawn() == Entry.Character.Get()
            && Entry.Equipment->GetEquipmentSlotRevision(Entry.Contract.EquipmentSlot) == Entry.ExpectedSlotRevision;
    };
    // Clear only our final wield before changing equipped identities. Preserve a later writer, even same-value writes.
    for (int32 Index = 0; Index < Snapshot.Num(); ++Index)
    {
        auto& Participant = Snapshot[Index];
        if (!Current()) { bInventoryRollbackIncomplete = true; return; }
        if (!Participant.bWieldApplied || !Participant.Character.IsValid()
            || Participant.Character->GetWeaponWieldRevision() != Participant.AppliedWieldRevision) { continue; }
        FString Ignored; if (!SetOwnedWield(Index, FWeaponWieldState(), Ignored)) { bInventoryRollbackIncomplete = true; }
    }
    for (int32 Index = EquipmentSnapshots.Num() - 1; Index >= 0; --Index)
    {
        auto& Entry = EquipmentSnapshots[Index];
        if (Entry.bRetired || !Entry.bReplacementEquipped) { continue; }
        if (!EquipmentCurrent(Entry) || !IsExactOwnedItem(Entry.Inventory.Get(), Entry.Replacement, Entry.ReplacementGUID)
            || Entry.Replacement->GetEquipmentRevision() != Entry.ExpectedReplacementRevision
            || Entry.Equipment->GetEquippedItemAtSlot(Entry.Contract.EquipmentSlot) != Entry.Replacement)
        { bInventoryRollbackIncomplete = true; Entry.bRetired = true; continue; }
        // If another owner still wields the replacement, its equip state is no longer ours to undo.
        bool bExternallyWielded = false;
        for (const auto& Participant : Snapshot)
        {
            bExternallyWielded |= Participant.Character == Entry.Character && Participant.Character.IsValid()
                && Participant.Character->GetWeaponWieldState().EquipWeapons.Contains(Cast<UWeaponItem>(Entry.Replacement));
        }
        if (bExternallyWielded) { bInventoryRollbackIncomplete = true; Entry.bRetired = true; continue; }
        Entry.bReplacementEquipped = false; ++Entry.ExpectedReplacementRevision; ++Entry.ExpectedSlotRevision;
        AdvanceOwnedItemState(Entry.Replacement);
        if (!Entry.Inventory->SetCinematicEquipment(Entry.Replacement, FGameplayTag()) || !EquipmentCurrent(Entry)
            || !IsValid(Entry.Replacement) || Entry.Replacement->GetEquipmentRevision() != Entry.ExpectedReplacementRevision
            || Entry.Replacement->IsEquipped() || Entry.Equipment->GetEquippedItemAtSlot(Entry.Contract.EquipmentSlot))
        { bInventoryRollbackIncomplete = true; Entry.bRetired = true; }
    }
    // Free dedicated grant stacks before restoring removed original objects. Other stacks are never overwritten.
    for (const auto Operation : {ENarrativeCinematicItemOperation::Grant, ENarrativeCinematicItemOperation::Remove})
    {
        for (int32 Index = InventoryChanges.Num() - 1; Index >= 0; --Index)
        {
            auto& Change = InventoryChanges[Index];
            if (!Current()) { bInventoryRollbackIncomplete = true; return; }
            if (Change.Contract.Operation != Operation) { continue; }
            if (!Change.Inventory.IsValid() || !Change.Inventory->RollbackCinematicItemChange(Change)) { bInventoryRollbackIncomplete = true; }
        }
    }
    for (int32 Index = EquipmentSnapshots.Num() - 1; Index >= 0; --Index)
    {
        auto& Entry = EquipmentSnapshots[Index];
        if (Entry.bRetired) { continue; }
        Entry.bRetired = true;
        if (!Entry.bPreviousUnequipped) { continue; }
        if (!EquipmentCurrent(Entry) || !IsExactOwnedItem(Entry.Inventory.Get(), Entry.Previous, Entry.PreviousGUID)
            || Entry.Previous->GetEquipmentRevision() != Entry.ExpectedPreviousRevision || Entry.Previous->IsEquipped()
            || Entry.Equipment->GetEquippedItemAtSlot(Entry.Contract.EquipmentSlot)) { bInventoryRollbackIncomplete = true; continue; }
        Entry.bPreviousUnequipped = false;
        ++Entry.ExpectedPreviousRevision; ++Entry.ExpectedSlotRevision;
        if (!Entry.Inventory->SetCinematicEquipment(Entry.Previous, Entry.Contract.EquipmentSlot) || !EquipmentCurrent(Entry)
            || !IsValid(Entry.Previous) || Entry.Previous->GetEquipmentRevision() != Entry.ExpectedPreviousRevision
            || Entry.Equipment->GetEquippedItemAtSlot(Entry.Contract.EquipmentSlot) != Entry.Previous)
        { bInventoryRollbackIncomplete = true; continue; }
        if (auto* Weapon = Cast<UWeaponItem>(Entry.Previous))
        {
            auto* Visual = Entry.Character->GetCharacterVisual();
            if (!IsValid(Visual) || !Visual->CompletePreloadedWeaponVisual(Weapon) || !EquipmentCurrent(Entry)) { bInventoryRollbackIncomplete = true; }
        }
    }
    bInventoryPostconditionsApplied = false;
}
