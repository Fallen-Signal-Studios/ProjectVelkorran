// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCinematicInventoryRuntimeTestFixtures.h"
#include "Tests/SovMeleeRuntimeTestFixtures.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    struct FInventoryWorld
    {
        UWorld* World = nullptr;
        UNarrativeInventoryComponent* Inventory = nullptr;
        FInventoryWorld()
        {
            World = UWorld::CreateWorld(EWorldType::Game, false);
            if (!World) { return; }
            if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
            World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false));
            AActor* Owner = World->SpawnActor<AActor>(); if (!Owner) { return; }
            Inventory = NewObject<UNarrativeInventoryComponent>(Owner); Owner->AddInstanceComponent(Inventory); Inventory->RegisterComponent();
        }
        ~FInventoryWorld()
        { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
    };
    FNarrativeCinematicItemMutation Mutation(ENarrativeCinematicItemOperation Operation, int32 Quantity,
        TSubclassOf<UNarrativeItem> Class = USovCinematicTestStack::StaticClass())
    {
        FNarrativeCinematicItemMutation Result; Result.MutationId = TEXT("ItemWrite");
        Result.Operation = Operation; Result.Quantity = Quantity; Result.ItemClass = Class; return Result;
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicDedicatedGrantTest, "ProjectVelkorran.Campaign.Cinematic.Inventory.DedicatedGrantAndExactlyOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicDedicatedGrantTest::RunTest(const FString& Parameters)
{
    FInventoryWorld F; if (!TestNotNull(TEXT("Inventory"), F.Inventory)) { return false; }
    auto* Existing = F.Inventory->TryAddItemFromClass(USovCinematicTestStack::StaticClass(), 7, false).Stacks[0];
    auto* Probe = NewObject<USovCinematicInventoryProbe>(F.Inventory);
    F.Inventory->OnItemAdded.AddDynamic(Probe, &USovCinematicInventoryProbe::Added);
    F.Inventory->OnItemRemoved.AddDynamic(Probe, &USovCinematicInventoryProbe::Removed);
    FNarrativeCinematicItemChange Change; FString Error;
    TestTrue(TEXT("Prepare does not mutate inventory"), F.Inventory->PrepareCinematicItemChange(Mutation(ENarrativeCinematicItemOperation::Grant, 3), Change, Error));
    TestEqual(TEXT("Preflight keeps original stack count"), F.Inventory->GetItems().Num(), 1);
    TestTrue(TEXT("Complete grant"), F.Inventory->ApplyCinematicItemChange(Change, Error));
    TestEqual(TEXT("Grant owns a dedicated stack"), F.Inventory->GetItems().Num(), 2);
    TestEqual(TEXT("Pre-existing quantity unchanged"), Existing->GetQuantity(), 7);
    TestFalse(TEXT("Same journal cannot apply twice"), F.Inventory->ApplyCinematicItemChange(Change, Error));
    TestEqual(TEXT("One grant notification"), Probe->AddedCount, 1);
    TestTrue(TEXT("Owned grant rolls back"), F.Inventory->RollbackCinematicItemChange(Change));
    TestTrue(TEXT("Rollback is idempotent"), F.Inventory->RollbackCinematicItemChange(Change));
    TestEqual(TEXT("One removal notification"), Probe->RemovedCount, 1);
    TestNull(TEXT("Removed grant no longer resolves by GUID"), F.Inventory->FindItemByGUID(Change.ItemGUID));
    TestTrue(TEXT("Unrelated identity survives"), F.Inventory->FindItemByGUID(Existing->ItemGUID) == Existing);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicQuantityBoundaryTest, "ProjectVelkorran.Campaign.Cinematic.Inventory.QuantityCapacityAndPermissionBoundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicQuantityBoundaryTest::RunTest(const FString& Parameters)
{
    FInventoryWorld F; if (!TestNotNull(TEXT("Inventory"), F.Inventory)) { return false; }
    FNarrativeCinematicItemChange Change; FString Error;
    TestFalse(TEXT("Unapproved class denied"), F.Inventory->PrepareCinematicItemChange(Mutation(ENarrativeCinematicItemOperation::Grant, 1, UNarrativeItem::StaticClass()), Change, Error));
    for (int32 Quantity : {0, -1, 101, MAX_int32})
    { TestFalse(TEXT("Out-of-bound quantities rejected"), F.Inventory->PrepareCinematicItemChange(Mutation(ENarrativeCinematicItemOperation::Grant, Quantity), Change, Error)); }
    F.Inventory->SetCapacity(0);
    TestTrue(TEXT("Prepare remains non-mutating at full capacity"), F.Inventory->PrepareCinematicItemChange(Mutation(ENarrativeCinematicItemOperation::Grant, 1), Change, Error));
    TestFalse(TEXT("Full capacity cannot partially grant"), F.Inventory->ApplyCinematicItemChange(Change, Error));
    TestEqual(TEXT("No partial stack leaked"), F.Inventory->GetItems().Num(), 0);
    F.Inventory->SetCapacity(3); F.Inventory->SetWeightCapacity(.5f);
    TestFalse(TEXT("Weight limit cannot partially grant"), F.Inventory->ApplyCinematicItemChange(Change, Error));
    F.Inventory->SetWeightCapacity(3.f);
    TestTrue(TEXT("Exact fitting grant"), F.Inventory->ApplyCinematicItemChange(Change, Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicRemovalResourceTest, "ProjectVelkorran.Campaign.Cinematic.Inventory.RemovalRestoresIdentityAndResources",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicRemovalResourceTest::RunTest(const FString& Parameters)
{
    FInventoryWorld F; if (!TestNotNull(TEXT("Inventory"), F.Inventory)) { return false; }
    auto* Weapon = Cast<USovCinematicTestResourceWeapon>(F.Inventory->TryAddItemFromClass(USovCinematicTestResourceWeapon::StaticClass(), 1, false).Stacks[0]);
    if (!TestNotNull(TEXT("Real resource weapon"), Weapon)) { return false; }
    Weapon->SetTestClip(7); Weapon->SetLastUseTime(42.f); const FGuid GUID = Weapon->ItemGUID;
    auto Remove = Mutation(ENarrativeCinematicItemOperation::Remove, 1, Weapon->GetClass()); Remove.ItemGUID = GUID;
    FNarrativeCinematicItemChange Change; FString Error;
    TestTrue(TEXT("Capture owned resource instance"), F.Inventory->PrepareCinematicItemChange(Remove, Change, Error));
    TestTrue(TEXT("Remove complete stack"), F.Inventory->ApplyCinematicItemChange(Change, Error));
    TestNull(TEXT("Removed GUID is no longer an inventory member"), F.Inventory->FindItemByGUID(GUID));
    TestTrue(TEXT("Restore same object"), F.Inventory->RollbackCinematicItemChange(Change));
    TestTrue(TEXT("GUID maps to original object"), F.Inventory->FindItemByGUID(GUID) == Weapon);
    TestEqual(TEXT("Clip was not reconstructed/refilled"), Weapon->GetTestClip(), 7);
    TestEqual(TEXT("Recharge timestamp was not reset"), Weapon->GetLastUseTime(), 42.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicLaterWriteTest, "ProjectVelkorran.Campaign.Cinematic.Inventory.PreserveLaterQuantityBusyAndResourceWrites",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicLaterWriteTest::RunTest(const FString& Parameters)
{
    for (int32 Kind = 0; Kind < 3; ++Kind)
    {
        FInventoryWorld F; if (!TestNotNull(TEXT("Inventory"), F.Inventory)) { return false; }
        FNarrativeCinematicItemChange Change; FString Error;
        auto Grant = Mutation(ENarrativeCinematicItemOperation::Grant, 1, USovCinematicTestResourceWeapon::StaticClass());
        TestTrue(TEXT("Prepare grant"), F.Inventory->PrepareCinematicItemChange(Grant, Change, Error));
        TestTrue(TEXT("Apply grant"), F.Inventory->ApplyCinematicItemChange(Change, Error));
        auto* Item = CastChecked<USovCinematicTestResourceWeapon>(Change.Item);
        if (Kind == 0) { Item->SetQuantity(Item->GetQuantity()); }
        else if (Kind == 1) { Item->SetBusy(true); Item->SetBusy(false); }
        else { Item->SetTestClip(2); }
        TestFalse(TEXT("Later native write invalidates receipt"), F.Inventory->ValidateCinematicItemChange(Change, true, Error));
        TestFalse(TEXT("Rollback will not delete a later owner's item"), F.Inventory->RollbackCinematicItemChange(Change));
        TestTrue(TEXT("Later item remains authoritative member"), F.Inventory->FindItemByGUID(Item->ItemGUID) == Item);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicPermissionReentryTest, "ProjectVelkorran.Campaign.Cinematic.Inventory.PermissionCallbackCannotInvalidateThenRemove",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicPermissionReentryTest::RunTest(const FString& Parameters)
{
    FInventoryWorld F; if (!TestNotNull(TEXT("Inventory"), F.Inventory)) { return false; }
    auto* Item = Cast<USovCinematicTestStack>(F.Inventory->TryAddItemFromClass(USovCinematicTestStack::StaticClass(), 5, false).Stacks[0]);
    if (!TestNotNull(TEXT("Stack"), Item)) { return false; }
    FNarrativeCinematicItemChange Change; FString Error;
    auto Remove = Mutation(ENarrativeCinematicItemOperation::Remove, 5); Remove.ItemGUID = Item->ItemGUID;
    TestTrue(TEXT("Prepare full removal"), F.Inventory->PrepareCinematicItemChange(Remove, Change, Error));
    Item->bRewriteDuringPermission = true;
    TestFalse(TEXT("Permission callback write prevents removal"), F.Inventory->ApplyCinematicItemChange(Change, Error));
    TestTrue(TEXT("Later owner's item was never detached"), F.Inventory->FindItemByGUID(Item->ItemGUID) == Item);
    FNarrativeCinematicItemChange Grant;
    TestTrue(TEXT("Prepare own grant"), F.Inventory->PrepareCinematicItemChange(Mutation(ENarrativeCinematicItemOperation::Grant, 1), Grant, Error));
    TestTrue(TEXT("Apply own grant"), F.Inventory->ApplyCinematicItemChange(Grant, Error));
    CastChecked<USovCinematicTestStack>(Grant.Item)->bRewriteDuringPermission = true;
    TestFalse(TEXT("Rollback permission callback write also preserves item"), F.Inventory->RollbackCinematicItemChange(Grant));
    TestTrue(TEXT("Conflicting grant remains a member"), F.Inventory->FindItemByGUID(Grant.ItemGUID) == Grant.Item);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicPartialRemovalAndLoadTest, "ProjectVelkorran.Campaign.Cinematic.Inventory.PartialRemovalForeignOwnerAndLoadEpoch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicPartialRemovalAndLoadTest::RunTest(const FString& Parameters)
{
    FInventoryWorld F; FInventoryWorld Other; if (!TestNotNull(TEXT("Inventory"), F.Inventory) || !TestNotNull(TEXT("Other inventory"), Other.Inventory)) { return false; }
    auto* Item = F.Inventory->TryAddItemFromClass(USovCinematicTestStack::StaticClass(), 8, false).Stacks[0];
    TestFalse(TEXT("Foreign inventory cannot remove this object"), Other.Inventory->RemoveItem(Item));
    TestEqual(TEXT("Foreign consume rejects this object"), Other.Inventory->ConsumeItem(Item, 1), 0);
    FNarrativeCinematicItemChange Change; FString Error;
    TestTrue(TEXT("Prepare exact partial removal"), F.Inventory->PrepareCinematicItemChange(Mutation(ENarrativeCinematicItemOperation::Remove, 3), Change, Error));
    TestTrue(TEXT("Apply partial removal"), F.Inventory->ApplyCinematicItemChange(Change, Error));
    Item->SetLastUseTime(11.f);
    TestTrue(TEXT("Partial rollback preserves unrelated resource write"), F.Inventory->RollbackCinematicItemChange(Change));
    TestEqual(TEXT("Original quantity restored"), Item->GetQuantity(), 8);
    TestEqual(TEXT("Later recharge timestamp preserved"), Item->GetLastUseTime(), 11.f);
    FNarrativeCinematicItemChange BeforeLoad;
    TestTrue(TEXT("Prepare before inventory load"), F.Inventory->PrepareCinematicItemChange(Mutation(ENarrativeCinematicItemOperation::Remove, 1), BeforeLoad, Error));
    F.Inventory->PrepareForSave_Implementation(); F.Inventory->Load_Implementation();
    TestFalse(TEXT("Loading invalidates old transaction epoch"), F.Inventory->ApplyCinematicItemChange(BeforeLoad, Error));
    TestTrue(TEXT("Loaded GUID resolves a replacement instance"), F.Inventory->FindItemByGUID(Item->ItemGUID) != Item);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinematicPhysicalAttachmentTest, "ProjectVelkorran.Campaign.Cinematic.Inventory.PhysicalAttachmentNotCachedFlags",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinematicPhysicalAttachmentTest::RunTest(const FString& Parameters)
{
    FInventoryWorld F; if (!TestNotNull(TEXT("World"), F.World)) { return false; }
    auto* Character = F.World->SpawnActor<ASovCinematicPresentationTestCharacter>();
    auto* Visual = F.World->SpawnActor<ASovCinematicPresentationTestVisual>();
    auto* Weapon = F.World->SpawnActor<ASovCinematicPresentationTestWeapon>();
    if (!TestNotNull(TEXT("Character"), Character) || !TestNotNull(TEXT("Character visual"), Visual) || !TestNotNull(TEXT("Weapon visual"), Weapon)) { return false; }
    auto* Mesh = NewObject<USovMeleeRuntimeTestMesh>(Visual); Visual->AddInstanceComponent(Mesh); Mesh->RegisterComponent();
    Visual->Configure(Character, Mesh); Character->SetTestVisual(Visual);
    const auto Slot = FNarrativeGameplayTags::Get().Equipment_Slot_Ammo;
    auto* Item = NewObject<USovCinematicPresentationTestWeaponItem>(Character); Item->Configure(Slot);
    Weapon->PrimeCachedAttachment(Item, Character, Visual, Slot);
    TestFalse(TEXT("Cached success cannot certify an unattached weapon"), Weapon->HasCommittedAttachment(Slot, FGameplayTag()));
    TestTrue(TEXT("Native mesh attachment succeeds"), Weapon->WeaponMesh->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform, TEXT("blade_root")));
    Weapon->WeaponMesh->SetRelativeTransform(FTransform::Identity);
    TestTrue(TEXT("Actual expected parent/socket/offset certifies attachment"), Weapon->HasCommittedAttachment(Slot, FGameplayTag()));
    Weapon->WeaponMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    TestFalse(TEXT("A later detach invalidates cached success"), Weapon->HasCommittedAttachment(Slot, FGameplayTag()));
    Weapon->WeaponMesh->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform, TEXT("blade_tip"));
    TestFalse(TEXT("A different valid socket is not the committed socket"), Weapon->HasCommittedAttachment(Slot, FGameplayTag()));
    Weapon->WeaponMesh->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform, TEXT("blade_root"));
    Weapon->WeaponMesh->SetRelativeTransform(FTransform(FVector(5.f, 0.f, 0.f)));
    TestFalse(TEXT("A later offset change also fails postconditions"), Weapon->HasCommittedAttachment(Slot, FGameplayTag()));
    return true;
}
#endif
