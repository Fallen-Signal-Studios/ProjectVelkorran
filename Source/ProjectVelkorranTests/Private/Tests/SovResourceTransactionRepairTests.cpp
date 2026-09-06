// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovRuntimeActorTestFixtures.h"
#include "Tests/SovResourceTransactionRepairFixtures.h"
#include "Components/SovEchoComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Items/AmmoItem.h"
#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FResourceRepairWorld
{
	UWorld* World = nullptr;
	FResourceRepairWorld()
	{
		// Preserve the Mac-verified single initialization path. Do not InitializeNewWorld again.
		const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
	}
	~FResourceRepairWorld()
	{ if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
	UNarrativeInventoryComponent* Inventory()
	{
		APawn* Owner = World ? World->SpawnActor<APawn>() : nullptr;
		if (!Owner) { return nullptr; }
		auto* Result = NewObject<UNarrativeInventoryComponent>(Owner);
		Owner->AddInstanceComponent(Result); Result->RegisterComponent(); return Result;
	}
	ASovResourceRepairPlayer* Player()
	{
		if (!World) { return nullptr; }
		FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Result = World->SpawnActor<ASovResourceRepairPlayer>(ASovResourceRepairPlayer::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
		auto* Controller = World->SpawnActor<ASovRuntimeTestPlayerController>();
		if (!Result || !Controller) { return nullptr; }
		Controller->Possess(Result); Result->InitializeExertion();
		Result->GetNarrativeAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxEchoAttribute(), 100.f);
		return Result;
	}
};
bool AddResourceWeapon(UNarrativeInventoryComponent* Inventory, USovResourceRepairAmmo*& Ammo, USovResourceRepairWeapon*& Weapon, int32 Quantity = 20)
{
	if (!Inventory) { return false; }
	Inventory->TryAddItemFromClass(USovResourceRepairAmmo::StaticClass(), Quantity, false);
	Inventory->TryAddItemFromClass(USovResourceRepairWeapon::StaticClass(), 1, false);
	Ammo = Cast<USovResourceRepairAmmo>(Inventory->FindItemOfClass(USovResourceRepairAmmo::StaticClass()));
	Weapon = Cast<USovResourceRepairWeapon>(Inventory->FindItemOfClass(USovResourceRepairWeapon::StaticClass()));
	return Ammo && Weapon;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAmmoExactPaymentRepairTest,
	"ProjectVelkorran.Campaign.Repairs.Resources.ExactAmmoPayment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAmmoExactPaymentRepairTest::RunTest(const FString& Parameters)
{
	FResourceRepairWorld F; auto* Inventory = F.Inventory();
	USovResourceRepairAmmo* Ammo = nullptr; USovResourceRepairWeapon* Weapon = nullptr;
	if (!TestTrue(TEXT("Native pawn inventory has real ammo and weapon"), AddResourceWeapon(Inventory, Ammo, Weapon))) { return false; }
	Weapon->SetLoaded(1);
	for (int32 Invalid : {0, -1, MIN_int32}) { TestFalse(TEXT("Nonpositive request never authorizes a shot"), Weapon->ConsumeAmmo(Invalid)); }
	TestFalse(TEXT("More than the loaded clip is rejected"), Weapon->ConsumeAmmo(2));
	TestEqual(TEXT("Rejected requests preserve clip"), Weapon->RawLoaded(), 1);
	TestEqual(TEXT("Rejected requests preserve reserve"), Ammo->GetQuantity(), 20);
	Ammo->bAllowRemoval = false;
	TestFalse(TEXT("Removal veto cannot authorize unpaid shot"), Weapon->ConsumeAmmo(1));
	TestEqual(TEXT("Unchanged failed transaction refunds its reservation"), Weapon->RawLoaded(), 1);
	Ammo->bAllowRemoval = true;
	TStrongObjectPtr<USovResourceRepairProbe> Probe(NewObject<USovResourceRepairProbe>()); Probe->Weapon = Weapon;
	Ammo->OnItemModified.AddDynamic(Probe.Get(), &USovResourceRepairProbe::DuringAmmoMutation);
	TestTrue(TEXT("One exact payment succeeds"), Weapon->ConsumeAmmo(1));
	TestFalse(TEXT("Nested shot cannot reuse the reserved clip"), Probe->bNestedConsumeAccepted);
	TestFalse(TEXT("Nested reload cannot reuse the reserved clip"), Probe->bNestedReloadAccepted);
	TestEqual(TEXT("Exactly one round removed"), Ammo->GetQuantity(), 19);
	TestEqual(TEXT("Clip never goes negative"), Weapon->RawLoaded(), 0);
	TestTrue(TEXT("Reload is available after the transaction ends"), Weapon->Reload());
	TestEqual(TEXT("Reload does not mint reserve"), Ammo->GetQuantity(), 19);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAmmoPermissionRevisionRepairTest,
	"ProjectVelkorran.Campaign.Repairs.Resources.AmmoPermissionAndReloadRevision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAmmoPermissionRevisionRepairTest::RunTest(const FString& Parameters)
{
	FResourceRepairWorld F; auto* Inventory = F.Inventory();
	USovResourceRepairAmmo* Ammo = nullptr; USovResourceRepairWeapon* Weapon = nullptr;
	if (!AddResourceWeapon(Inventory, Ammo, Weapon)) { return false; }
	Weapon->SetLoaded(1); Ammo->bRewriteOnPermission = true;
	TestFalse(TEXT("Even a same-value permission write retires the old payment"), Weapon->ConsumeAmmo(1));
	TestEqual(TEXT("Rejected stale payment cannot debit reserve"), Ammo->GetQuantity(), 20);
	TestEqual(TEXT("Old rollback does not overwrite the replacement resource revision"), Weapon->RawLoaded(), 0);
	Weapon->bRewriteReserveOnSpareRead = true;
	TestFalse(TEXT("Reload cannot commit a stale overridable reserve read"), Weapon->Reload());
	TestEqual(TEXT("Rejected reload leaves clip unchanged"), Weapon->RawLoaded(), 0);
	TestTrue(TEXT("A fresh reload works"), Weapon->Reload());
	UNarrativeInventoryComponent* OtherInventory = F.Inventory(); if (!OtherInventory) { return false; }
	OtherInventory->TryAddItemFromClass(USovResourceRepairAmmo::StaticClass(), 5, false);
	Weapon->SetAmmoSourceForTest(OtherInventory->FindItemOfClass(USovResourceRepairAmmo::StaticClass()));
	Weapon->SetLoaded(0);
	TestTrue(TEXT("Transferred weapon replaces a foreign ammo-source cache"), Weapon->Reload());
	TestTrue(TEXT("Reload binds only the current inventory's ammunition"), Weapon->GetAmmoSource() == Ammo);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAmmoEmptyStackRepairTest,
	"ProjectVelkorran.Campaign.Repairs.Resources.EmptyStackCleanupOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAmmoEmptyStackRepairTest::RunTest(const FString& Parameters)
{
	for (bool bRefillDuringDebit : {false, true})
	{
		FResourceRepairWorld F; auto* Inventory = F.Inventory();
		USovResourceRepairAmmo* Ammo = nullptr; USovResourceRepairWeapon* Weapon = nullptr;
		if (!AddResourceWeapon(Inventory, Ammo, Weapon, 1)) { return false; }
		TStrongObjectPtr<USovResourceRepairAmmo> AmmoLifetime(Ammo);
		Weapon->SetLoaded(1); Ammo->PermissionCalls = 0; Ammo->RefillOnPermissionCall = 2;
		TStrongObjectPtr<USovResourceRepairProbe> Probe(NewObject<USovResourceRepairProbe>());
		if (bRefillDuringDebit)
		{
			Probe->Ammo = Ammo; Probe->bRefillAfterDebit = true;
			Ammo->OnItemModified.AddDynamic(Probe.Get(), &USovResourceRepairProbe::DuringAmmoMutation);
		}
		TestTrue(TEXT("Last round is paid exactly once"), Weapon->ConsumeAmmo(1));
		TestEqual(TEXT("Cleanup cannot invoke a second overridable removal permission"), Ammo->PermissionCalls, 1);
		if (bRefillDuringDebit)
		{
			TestTrue(TEXT("A callback-refilled stack retains membership"), Inventory->GetItems().Contains(Ammo));
			TestEqual(TEXT("Old empty-stack cleanup cannot delete new rounds"), Ammo->GetQuantity(), 5);
		}
		else { TestFalse(TEXT("Unchanged empty stack is removed"), Inventory->GetItems().Contains(Ammo)); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInventoryCommittedGrantRepairTest,
	"ProjectVelkorran.Campaign.Repairs.Resources.CommittedGrantAccounting", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovInventoryCommittedGrantRepairTest::RunTest(const FString& Parameters)
{
	for (bool bNestedGrant : {false, true})
	{
		FResourceRepairWorld F; auto* Inventory = F.Inventory(); if (!Inventory) { return false; }
		Inventory->TryAddItemFromClass(USovResourceRepairAmmo::StaticClass(), 5, false);
		auto* Ammo = Inventory->FindItemOfClass(USovResourceRepairAmmo::StaticClass()); if (!Ammo) { return false; }
		TStrongObjectPtr<USovResourceRepairProbe> Probe(NewObject<USovResourceRepairProbe>());
		Probe->Inventory = Inventory; Probe->Ammo = Ammo; Probe->bAddInsteadOfConsume = bNestedGrant; Probe->Quantity = bNestedGrant ? 1 : 3;
		Ammo->OnItemModified.AddDynamic(Probe.Get(), &USovResourceRepairProbe::DuringAmmoMutation);
		const FItemAddResult Result = Inventory->TryAddItemFromClass(USovResourceRepairAmmo::StaticClass(), 3, false);
		TestFalse(TEXT("Grant callback executed"), Probe->bArmed);
		TestEqual(TEXT("Receipt reports only this invocation's committed grant"), Result.AmountGiven, 3);
		TestEqual(TEXT("Callbacks cannot duplicate or erase this grant's accounting"), Inventory->GetTotalQuantityOfItem(USovResourceRepairAmmo::StaticClass(), false), bNestedGrant ? 9 : 5);
		TestEqual(TEXT("Spent callback grant does not produce an extra stack"), Inventory->GetItems().Num(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRemovedResourceOwnerRepairTest,
	"ProjectVelkorran.Campaign.Repairs.Resources.RemovedOwnerNotificationFence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovRemovedResourceOwnerRepairTest::RunTest(const FString& Parameters)
{
	FResourceRepairWorld F; auto* Inventory = F.Inventory(); if (!Inventory) { return false; }
	Inventory->TryAddItemFromClass(USovResourceRepairAmmo::StaticClass(), 1, false);
	auto* Ammo = Cast<USovResourceRepairAmmo>(Inventory->FindItemOfClass(USovResourceRepairAmmo::StaticClass()));
	if (!Ammo) { return false; }
	TStrongObjectPtr<USovResourceRepairProbe> Probe(NewObject<USovResourceRepairProbe>());
	Inventory->OnInventoryUpdated.AddDynamic(Probe.Get(), &USovResourceRepairProbe::ObserveInventoryUpdate);
	Inventory->OnItemRemoved.AddDynamic(Probe.Get(), &USovResourceRepairProbe::ObserveRemoval);
	Ammo->bDestroyOwnerOnRemoval = true;
	const int32 Paid = Inventory->ConsumeItemExact(Ammo, 1, Ammo->GetQuantityRevision(), []() { return true; });
	TestEqual(TEXT("Retiring owner does not undo a committed debit"), Paid, 1);
	TestEqual(TEXT("RemovedFromInventory owner retirement suppresses later update notification"), Probe->InventoryNotifications, 0);
	TestEqual(TEXT("Old debit cannot notify the retired owner"), Probe->RemovalNotifications, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEchoPickupReservationRepairTest,
	"ProjectVelkorran.Campaign.Repairs.Resources.EchoPickupReservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEchoPickupReservationRepairTest::RunTest(const FString& Parameters)
{
	FResourceRepairWorld F; auto* Player = F.Player(); if (!Player) { return false; }
	auto* Pickup = F.World->SpawnActor<ASovResourceRepairEchoPickup>(); if (!Pickup) { return false; }
	auto* Echo = Player->GetEchoComponent(); auto* ASC = Player->GetNarrativeAbilitySystemComponent();
	Echo->RestoreEchoFromCheckpoint(0.f); Pickup->InitializeEcho(10.f);
	bool bNested = false;
	auto& Changed = ASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetEchoAttribute());
	const FDelegateHandle Handle = Changed.AddLambda([&](const FOnAttributeChangeData& Change)
	{
		if (bNested || Change.NewValue <= Change.OldValue) { return; }
		bNested = true; Pickup->Touch(Player); Pickup->InitializeEcho(100.f);
		Echo->TrySpendEcho(10.f, FSovGameplayTags::Get().Ability_Echo_Tarrik_CinderJudgement);
	});
	Pickup->Touch(Player); Changed.Remove(Handle);
	TestTrue(TEXT("Real Echo delegate reenters overlap"), bNested);
	TestTrue(TEXT("Committed grant stays claimed even if immediately spent"), Pickup->IsClaimed());
	TestEqual(TEXT("Nested collection cannot duplicate Echo"), Echo->GetEcho(), 0.f);
	Pickup->Touch(Player); TestEqual(TEXT("Claim remains permanent"), Echo->GetEcho(), 0.f);
	auto* Retry = F.World->SpawnActor<ASovResourceRepairEchoPickup>(); if (!Retry) { return false; }
	Retry->InitializeEcho(10.f); Echo->RestoreEchoFromCheckpoint(100.f); Retry->Touch(Player);
	TestFalse(TEXT("Full resource leaves pickup unclaimed"), Retry->IsClaimed());
	Echo->RestoreEchoFromCheckpoint(0.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f); Retry->Touch(Player);
	TestFalse(TEXT("Zero Health admission is rejected before death flag catches up"), Retry->IsClaimed());
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f); Retry->Touch(Player);
	TestTrue(TEXT("Failed grants release reservation for a later living recipient"), Retry->IsClaimed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAmmoPickupRemainderRepairTest,
	"ProjectVelkorran.Campaign.Repairs.Resources.AmmoPickupRemainder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAmmoPickupRemainderRepairTest::RunTest(const FString& Parameters)
{
	FResourceRepairWorld F; auto* Player = F.Player(); if (!Player) { return false; }
	auto* Inventory = Player->GetInventoryComponent(); if (!Inventory) { return false; }
	const int32 Cap = GetDefault<UAmmoItem>()->GetMaxStackSize();
	if (!TestTrue(TEXT("Native ammo class has a positive stack cap"), Cap > 0)) { return false; }
	auto* Pickup = F.World->SpawnActor<ASovResourceRepairAmmoPickup>(); if (!Pickup) { return false; }
	Pickup->InitializeAmmo(UAmmoItem::StaticClass(), Cap + 1); Pickup->Touch(Player);
	TestFalse(TEXT("Partial pickup retains remainder"), Pickup->IsClaimed());
	TestEqual(TEXT("Only native carried cap was granted"), Inventory->GetTotalQuantityOfItem(UAmmoItem::StaticClass(), false), Cap);
	TestEqual(TEXT("Exactly one remaining round stays in the world"), Pickup->GetAmmoQuantity(), 1);
	auto* Ammo = Inventory->FindItemOfClass(UAmmoItem::StaticClass()); if (!Ammo) { return false; }
	Inventory->ConsumeItem(Ammo, 1);
	TStrongObjectPtr<USovResourceRepairProbe> Probe(NewObject<USovResourceRepairProbe>());
	Probe->Inventory = Inventory; Probe->Ammo = Ammo; Probe->AmmoPickup = Pickup; Probe->Player = Player;
	Ammo->OnItemModified.AddDynamic(Probe.Get(), &USovResourceRepairProbe::DuringAmmoMutation);
	Pickup->Touch(Player);
	TestFalse(TEXT("Ammo delegate performed nested overlap and spend"), Probe->bArmed);
	TestTrue(TEXT("Remainder is consumed exactly once despite nested callbacks"), Pickup->IsClaimed());
	TestEqual(TEXT("Fully granted pack has zero payload"), Pickup->GetAmmoQuantity(), 0);
	TestEqual(TEXT("Immediate spend cannot cause the grant to be minted again"), Inventory->GetTotalQuantityOfItem(UAmmoItem::StaticClass(), false), Cap - 1);
	auto* Empty = F.World->SpawnActor<ASovResourceRepairAmmoPickup>(); if (!Empty) { return false; }
	Empty->InitializeAmmo(UAmmoItem::StaticClass(), 0); Empty->Touch(Player);
	TestFalse(TEXT("Zero-quantity initialization cannot mint a round"), Empty->IsClaimed());
	TestEqual(TEXT("Empty payload preserves reserve"), Inventory->GetTotalQuantityOfItem(UAmmoItem::StaticClass(), false), Cap - 1);
	return true;
}
#endif
