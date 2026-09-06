// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovWeaponPairingTestFixtures.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/MeleeWeaponItem.h"
#include "Items/RangedWeaponItem.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FWeaponPairingWorld
{
	UWorld* World = nullptr;
	FWeaponPairingWorld()
	{
		const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
	}
	~FWeaponPairingWorld()
	{
		if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWeaponPairingPolicyTest,
	"ProjectVelkorran.Campaign.Weapons.SymmetricOptInDualWieldPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWeaponPairingPolicyTest::RunTest(const FString& Parameters)
{
	FWeaponPairingWorld Fixture;
	if (!TestNotNull(TEXT("World"), Fixture.World)) { return false; }
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Character = Fixture.World->SpawnActor<ASovWeaponPairingTestCharacter>(
		ASovWeaponPairingTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	auto* Visual = Fixture.World->SpawnActor<ASovWeaponPairingTestVisual>();
	auto* FirstVisual = Fixture.World->SpawnActor<AWeaponVisual>();
	auto* SecondVisual = Fixture.World->SpawnActor<AWeaponVisual>();
	if (!TestTrue(TEXT("Content-free character and equipped visuals exist"), Character && Visual && FirstVisual && SecondVisual)) { return false; }
	Character->SetTestVisual(Visual);
	const FGameplayTag FirstSlot = FNarrativeGameplayTags::Get().Equipment_Slot_Weapon_HipLeft;
	const FGameplayTag SecondSlot = FNarrativeGameplayTags::Get().Equipment_Slot_Weapon_HipRight;
	Visual->SetTestWeapon(FirstSlot, FirstVisual);
	Visual->SetTestWeapon(SecondSlot, SecondVisual);
	TStrongObjectPtr<USovWeaponPairingTestMelee> Melee(NewObject<USovWeaponPairingTestMelee>(Character));
	TStrongObjectPtr<USovWeaponPairingTestRanged> Ranged(NewObject<USovWeaponPairingTestRanged>(Character));
	TStrongObjectPtr<USovWeaponPairingTestRanged> SameRanged(NewObject<USovWeaponPairingTestRanged>(Character));
	Melee->CurrentSlot = FirstSlot;
	Ranged->CurrentSlot = SecondSlot;
	SameRanged->CurrentSlot = FirstSlot;
	Melee->WeaponHand = Ranged->WeaponHand = SameRanged->WeaponHand = EWeaponHandRule::WHR_Either;
	TestFalse(TEXT("Pair restriction defaults off"), Ranged->bRequireSameClassForDualWield);
	TestTrue(TEXT("Existing melee-first mixed pairing is permissive without opt-in"), Melee->CanDualWieldWith(Ranged.Get()));
	TestFalse(TEXT("Existing ranged override still rejects melee"), Ranged->CanDualWieldWith(Melee.Get()));
	Ranged->bRequireSameClassForDualWield = true;
	TestFalse(TEXT("Opted-in partner rejects the formerly permissive mixed order"), Melee->CanDualWieldWith(Ranged.Get()));
	TestFalse(TEXT("Opted-in caller rejects the reverse mixed order"), Ranged->CanDualWieldWith(Melee.Get()));
	// Exercise the base implementation in both directions as well as the public ranged override above.
	TestFalse(TEXT("Base policy is symmetric with an opted-in caller"), Ranged->CanPairUsingBase(Melee.Get()));
	TestTrue(TEXT("Same exact ranged class remains eligible"), Ranged->CanDualWieldWith(SameRanged.Get()));
	TestTrue(TEXT("Same class remains eligible in reverse order"), SameRanged->CanDualWieldWith(Ranged.Get()));
	TestFalse(TEXT("A weapon cannot pair with itself"), Ranged->CanDualWieldWith(Ranged.Get()));
	TestFalse(TEXT("Null partner still fails"), Ranged->CanDualWieldWith(nullptr));
	SameRanged->WeaponHand = EWeaponHandRule::WHR_Both;
	TestFalse(TEXT("Matching class does not bypass two-hand restriction"), Ranged->CanDualWieldWith(SameRanged.Get()));
	SameRanged->WeaponHand = Ranged->WeaponHand = EWeaponHandRule::WHR_Mainhand;
	TestFalse(TEXT("Matching class does not bypass conflicting hands"), Ranged->CanDualWieldWith(SameRanged.Get()));
	SameRanged->WeaponHand = EWeaponHandRule::WHR_Offhand;
	TestTrue(TEXT("Complementary hands retain their existing valid pair"), Ranged->CanDualWieldWith(SameRanged.Get()));
	SameRanged->CurrentSlot = FGameplayTag();
	TestFalse(TEXT("Matching class must be equipped"), Ranged->CanDualWieldWith(SameRanged.Get()));
	SameRanged->CurrentSlot = FirstSlot;
	Visual->SetTestWeapon(FirstSlot, nullptr);
	TestFalse(TEXT("Matching class must have an equipped visual"), Ranged->CanDualWieldWith(SameRanged.Get()));
	return true;
}
#endif
