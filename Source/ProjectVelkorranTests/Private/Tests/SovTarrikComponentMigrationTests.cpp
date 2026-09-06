// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Characters/SovTarrikCharacter.h"
#include "Components/SovGuardComponent.h"
#include "Components/SovTarrikEchoGenerationComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_AUTOMATION_TESTS
namespace
{
	struct FTarrikMigrationWorld
	{
		UWorld* World = nullptr;
		FTarrikMigrationWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World)
			{
				if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
				World->InitializeActorsForPlay(FURL());
			}
		}
		~FTarrikMigrationWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		ASovTarrikCharacter* DeferredCharacter()
		{
			return World ? World->SpawnActorDeferred<ASovTarrikCharacter>(ASovTarrikCharacter::StaticClass(),
				FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn) : nullptr;
		}
	};

	bool SetCompatibilitySlot(ASovTarrikCharacter* Character, FName Name, UObject* Value)
	{
		FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(ASovTarrikCharacter::StaticClass(), Name);
		if (!Character || !Property) { return false; }
		// Models the reflected property state restored from a legacy Blueprint's
		// serialized null/foreign override, before normal actor initialization.
		Property->SetObjectPropertyValue_InContainer(Character, Value);
		return true;
	}

	UNarrativeAbilitySystemComponent* CreateAttributeASC(ASovTarrikCharacter* Character)
	{
		auto* ASC = NewObject<UNarrativeAbilitySystemComponent>(Character, TEXT("MigrationTestASC"));
		Character->AddInstanceComponent(ASC);
		ASC->RegisterComponent();
		ASC->AddAttributeSetSubobject(NewObject<UNarrativeAttributeSetBase>(ASC));
		ASC->InitAbilityActorInfo(Character, Character);
		return ASC;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTarrikLegacyNullComponentTest,
	"ProjectVelkorran.Campaign.Readiness.TarrikLegacyNullComponentSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTarrikLegacyNullComponentTest::RunTest(const FString& Parameters)
{
	FTarrikMigrationWorld Fixture;
	auto* Character = Fixture.DeferredCharacter();
	if (!TestNotNull(TEXT("Deferred native Tarrik"), Character)) { return false; }
	auto* NativeGuard = Cast<USovGuardComponent>(Character->GetDefaultSubobjectByName(TEXT("SovGuardComponent")));
	auto* NativeGenerator = Cast<USovTarrikEchoGenerationComponent>(Character->GetDefaultSubobjectByName(TEXT("SovTarrikEchoGenerationComponent")));
	if (!TestNotNull(TEXT("Native Guard subobject"), NativeGuard)
		|| !TestNotNull(TEXT("Native Echo generator subobject"), NativeGenerator)) { return false; }
	TestTrue(TEXT("Restore serialized-null Guard slot"), SetCompatibilitySlot(Character, TEXT("GuardComponent"), nullptr));
	TestTrue(TEXT("Restore serialized-null generator slot"), SetCompatibilitySlot(Character, TEXT("TarrikEchoGenerationComponent"), nullptr));
	TestNull(TEXT("Legacy Guard getter is null before initialization"), Character->GetGuardComponent());
	TestNull(TEXT("Legacy generator getter is null before initialization"), Character->GetTarrikEchoGenerationComponent());
	Character->FinishSpawning(FTransform::Identity);
	TestTrue(TEXT("Normal deferred spawning ran actor initialization"), Character->IsActorInitialized());
	TestTrue(TEXT("Guard getter binds the exact existing native component"), Character->GetGuardComponent() == NativeGuard);
	TestTrue(TEXT("Generator getter binds the exact existing native component"), Character->GetTarrikEchoGenerationComponent() == NativeGenerator);
	TArray<USovGuardComponent*> Guards;
	Character->GetComponents(Guards);
	TestEqual(TEXT("Repair does not construct another Guard"), Guards.Num(), 1);
	auto* ASC = CreateAttributeASC(Character);
	TestTrue(TEXT("Repaired Guard passes production canonical initialization"), NativeGuard->InitializeWithAbilitySystem(ASC));
	TestTrue(TEXT("Repaired Guard is initialized"), NativeGuard->IsInitialized());

	auto* Duplicate = NewObject<USovGuardComponent>(Character, TEXT("DuplicateGuard"));
	Character->AddInstanceComponent(Duplicate);
	Duplicate->RegisterComponent();
	AddExpectedError(TEXT("non-canonical or duplicate Guard component"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Extra component still fails production canonical check"), Duplicate->InitializeWithAbilitySystem(ASC));
	TestFalse(TEXT("Duplicate remains uninitialized"), Duplicate->IsInitialized());
	TestTrue(TEXT("Canonical Guard remains initialized after duplicate rejection"), NativeGuard->IsInitialized());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTarrikMigrationRejectsMismatchTest,
	"ProjectVelkorran.Campaign.Readiness.TarrikMigrationPreservesComponentRejection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTarrikMigrationRejectsMismatchTest::RunTest(const FString& Parameters)
{
	FTarrikMigrationWorld Fixture;
	auto* Owner = Fixture.DeferredCharacter();
	auto* ForeignOwner = Fixture.DeferredCharacter();
	if (!TestNotNull(TEXT("Tarrik owner"), Owner) || !TestNotNull(TEXT("Foreign Tarrik owner"), ForeignOwner)) { return false; }
	auto* NativeGuard = Owner->GetGuardComponent();
	auto* ForeignGuard = ForeignOwner->GetGuardComponent();
	TestTrue(TEXT("Restore a nonnull mismatched reference"), SetCompatibilitySlot(Owner, TEXT("GuardComponent"), ForeignGuard));
	ForeignOwner->FinishSpawning(FTransform::Identity);
	Owner->FinishSpawning(FTransform::Identity);
	TestTrue(TEXT("Migration does not silently replace a nonnull mismatched reference"), Owner->GetGuardComponent() == ForeignGuard);
	auto* ASC = CreateAttributeASC(Owner);
	AddExpectedError(TEXT("non-canonical or duplicate Guard component"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("A native component with mismatched canonical slot remains rejected"), NativeGuard->InitializeWithAbilitySystem(ASC));

	auto* NonNativeOwner = Fixture.DeferredCharacter();
	if (!TestNotNull(TEXT("Non-native-creation rejection pawn"), NonNativeOwner)) { return false; }
	auto* NonNativeGuard = NonNativeOwner->GetGuardComponent();
	NonNativeGuard->CreationMethod = EComponentCreationMethod::Instance;
	TestTrue(TEXT("Restore null slot with an ineligible same-name component"), SetCompatibilitySlot(NonNativeOwner, TEXT("GuardComponent"), nullptr));
	NonNativeOwner->FinishSpawning(FTransform::Identity);
	TestNull(TEXT("A same-name non-native component cannot become canonical"), NonNativeOwner->GetGuardComponent());
	return true;
}
#endif
