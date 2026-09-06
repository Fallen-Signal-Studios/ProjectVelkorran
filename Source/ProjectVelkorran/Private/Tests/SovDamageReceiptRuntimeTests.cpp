// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Components/SovSeleneEchoGenerationComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "UObject/Script.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS
namespace SovDamageReceiptTests
{
	struct FWorld
	{
		UWorld* World = nullptr;
		FWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
		}
		~FWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		ASovAxiomRuntimeTestCharacter* Character(const int32 Team)
		{
			if (!World) { return nullptr; }
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Character = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(
				ASovAxiomRuntimeTestCharacter::StaticClass(), FVector(Team * 150.f, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
			if (Character) { Character->InitializeTestCombat(Team); }
			return Character;
		}
	};

	FSovDamageResult Damage(ASovAxiomRuntimeTestCharacter* Source, ASovAxiomRuntimeTestCharacter* Target)
	{
		auto* SourceASC = Source->GetNarrativeAbilitySystemComponent();
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(Source, Source);
		FHitResult Hit;
		Hit.BoneName = TEXT("weapon");
		Context.AddHitResult(Hit);
		FGameplayEffectSpec Spec(GetDefault<USovCombatRoutingTestEffect>(), Context, 1.f);
		Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, 5.f);
#if WITH_EDITOR
		// Exercise real component delegates without beginning the content-free world.
		FEditorScriptExecutionGuard AllowNativeComponentReceivers;
#endif
		SourceASC->ApplyGameplayEffectSpecToTarget(Spec, Target->GetNarrativeAbilitySystemComponent());
		return Target->LastDamageResult;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNativeDamageReceiptCopiesTest,
	"ProjectVelkorran.Campaign.Defense.NativeReceiptCopiesAndIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNativeDamageReceiptCopiesTest::RunTest(const FString& Parameters)
{
	SovDamageReceiptTests::FWorld Fixture;
	auto* Source = Fixture.Character(0);
	auto* Target = Fixture.Character(1);
	if (!TestNotNull(TEXT("Source"), Source) || !TestNotNull(TEXT("Target"), Target)) { return false; }
	FSovDamageResult Result = SovDamageReceiptTests::Damage(Source, Target);
	TestTrue(TEXT("Real damage publisher mints native proof"), Result.HasNativeReceipt());
	TestTrue(TEXT("Receipt owns current target life"), Result.IsCurrentTargetLife());
	FSovDamageResult Copy;
	FSovDamageResult::StaticStruct()->CopyScriptStruct(&Copy, &Result);
	TStrongObjectPtr<UObject> Consumer(NewObject<UObject>());
	TestTrue(TEXT("Reflected copy retains receipt"), Copy.HasNativeReceipt());
	TestTrue(TEXT("First consumer channel is accepted"), Copy.ConsumeNativeReceipt(Consumer.Get()));
	TestFalse(TEXT("Original shares consumed state with reflected copy"), Result.ConsumeNativeReceipt(Consumer.Get()));
	TestTrue(TEXT("A separate consumer channel is independent"), Result.ConsumeNativeReceipt(Consumer.Get(), 1));
	TestFalse(TEXT("Invalid channel fails closed"), Result.ConsumeNativeReceipt(Consumer.Get(), 8));
	FSovDamageResult Forged;
	Forged.TransactionId = Result.TransactionId;
	Forged.TargetActor = Target;
	TestFalse(TEXT("Matching reflected fields do not forge native proof"), Forged.HasNativeReceipt());
	Copy.TransactionId = FGuid::NewGuid();
	TestFalse(TEXT("Changing transaction identity invalidates copied proof"), Copy.HasNativeReceipt());
	Copy = Result;
	Copy.TargetActor = Source;
	TestFalse(TEXT("Changing the recipient cannot redirect the receipt"), Copy.IsCurrentTargetLife());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNativeDamageReceiptBoundTest,
	"ProjectVelkorran.Campaign.Defense.NativeReceiptBoundedConsumers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNativeDamageReceiptBoundTest::RunTest(const FString& Parameters)
{
	SovDamageReceiptTests::FWorld Fixture;
	auto* Source = Fixture.Character(0);
	auto* Target = Fixture.Character(1);
	if (!Source || !Target) { AddError(TEXT("Receipt fixture actors failed")); return false; }
	// Isolate the capacity assertion from source reward observers, including
	// listeners that adopt native receipts in later combat integrations.
	Source->GetNarrativeAbilitySystemComponent()->OnDamageResolvedAsSource.RemoveAll(Source->TestEchoGeneration.Get());
	const FSovDamageResult Unobserved = SovDamageReceiptTests::Damage(Source, Target);
	TArray<TStrongObjectPtr<UObject>> Consumers;
	for (int32 Index = 0; Index < 33; ++Index)
	{
		Consumers.Emplace(NewObject<UObject>());
		const bool bConsumed = Unobserved.ConsumeNativeReceipt(Consumers.Last().Get());
		TestEqual(FString::Printf(TEXT("Consumer %d respects the per-packet cap"), Index), bConsumed, Index < 32);
	}
	TestTrue(TEXT("Existing consumer can use another channel at capacity"), Unobserved.ConsumeNativeReceipt(Consumers[0].Get(), 7));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNativeDamageReceiptRestoredLifeTest,
	"ProjectVelkorran.Campaign.Defense.NativeReceiptRetiredLife",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNativeDamageReceiptRestoredLifeTest::RunTest(const FString& Parameters)
{
	SovDamageReceiptTests::FWorld Fixture;
	auto* Source = Fixture.Character(0);
	auto* Target = Fixture.Character(1);
	if (!Source || !Target) { AddError(TEXT("Receipt fixture actors failed")); return false; }
	const FSovDamageResult OldResult = SovDamageReceiptTests::Damage(Source, Target);
	auto* ASC = Target->GetNarrativeAbilitySystemComponent();
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	TStrongObjectPtr<UObject> Consumer(NewObject<UObject>());
	TestTrue(TEXT("Old receipt remains historical native proof"), OldResult.HasNativeReceipt());
	TestFalse(TEXT("Same-avatar Health restoration retires old life"), OldResult.IsCurrentTargetLife());
	TestFalse(TEXT("Previously unconsumed old receipt cannot affect restored life"), OldResult.ConsumeNativeReceipt(Consumer.Get()));
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
	TestFalse(TEXT("A second zero Health cannot revive the old life receipt"), OldResult.IsCurrentTargetLife());
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	const FSovDamageResult Fresh = SovDamageReceiptTests::Damage(Source, Target);
	TestTrue(TEXT("A fresh packet owns restored life"), Fresh.ConsumeNativeReceipt(Consumer.Get()));
	ASC->InitAbilityActorInfo(Target, Source);
	TestFalse(TEXT("Reassigned ASC avatar cannot consume former avatar's result"), Fresh.IsCurrentTargetLife());
	ASC->InitAbilityActorInfo(Target, Target);
	TestFalse(TEXT("Avatar A to B to A cannot revive a former actor-info receipt"), Fresh.IsCurrentTargetLife());
	TestFalse(TEXT("Another channel cannot consume after avatar ABA"), Fresh.ConsumeNativeReceipt(Consumer.Get(), 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNativeDamageReceiptLateWeakPointTest,
	"ProjectVelkorran.Campaign.WeakPoint.RetiredReceiptLateListener",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNativeDamageReceiptLateWeakPointTest::RunTest(const FString& Parameters)
{
	SovDamageReceiptTests::FWorld Fixture;
	auto* Source = Fixture.Character(0);
	auto* Target = Fixture.Character(1);
	if (!Source || !Target) { AddError(TEXT("Receipt fixture actors failed")); return false; }
	const FSovDamageResult Result = SovDamageReceiptTests::Damage(Source, Target);
	auto* ASC = Target->GetNarrativeAbilitySystemComponent();
	auto* WeakPoints = NewObject<USovWeakPointRoutingTestComponent>(Target);
	Target->AddInstanceComponent(WeakPoints);
	WeakPoints->RegisterComponent();
	WeakPoints->InitializeWithAbilitySystem(ASC);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	FName Zone;
	TestTrue(TEXT("Late subscriber rejects an unconsumed result from before restoration"),
		WeakPoints->ResolveWeakPointHit(Result, Zone) == ESovWeakPointHitResolution::NotWeakPoint);
	TestFalse(TEXT("Restored weapon zone is not broken by historical packet"), WeakPoints->IsWeakPointBroken(TEXT("Weapon")));
	SovDamageReceiptTests::Damage(Source, Target);
	TestTrue(TEXT("Fresh damage still breaks the zone"), WeakPoints->IsWeakPointBroken(TEXT("Weapon")));
	return true;
}
#endif
