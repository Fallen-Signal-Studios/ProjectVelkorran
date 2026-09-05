// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAxiomRuntimeTestFixtures.h"

#include "Abilities/SovGameplayAbility_SeleneDeflection.h"
#include "Components/SovDeflectionComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "TimerManager.h"

#if WITH_AUTOMATION_TESTS
namespace SovDeflectionRuntimeTests
{
struct FWorld
{
	UWorld* World = nullptr;
	FWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (World)
		{
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false)
				.ShouldSimulatePhysics(false).SetTransactional(false));
			World->GetTimerManager().Tick(0.f);
		}
	}
	~FWorld()
	{
		if (World)
		{
			World->DestroyWorld(false);
			if (GEngine) { GEngine->DestroyWorldContext(World); }
		}
	}
	ASovAxiomRuntimeTestCharacter* Character()
	{
		auto* Result = World ? World->SpawnActor<ASovAxiomRuntimeTestCharacter>() : nullptr;
		if (Result) { Result->InitializeTestCombat(0); }
		return Result;
	}
	void Advance(float Seconds)
	{
		TGuardValue<uint64> Frame(GFrameCounter, GFrameCounter + 1);
		World->GetTimerManager().Tick(Seconds);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDeflectionReentrantWindowTest,
	"ProjectVelkorran.Campaign.Deflection.ReentrantWindowOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDeflectionReentrantWindowTest::RunTest(const FString& Parameters)
{
	SovDeflectionRuntimeTests::FWorld Fixture;
	auto* Character = Fixture.Character();
	if (!TestNotNull(TEXT("Selene fixture"), Character)) { return false; }
	auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	auto* Deflection = Character->TestDeflection.Get();
	const auto Tag = FSovGameplayTags::Get().State_Deflecting;
	const auto Listener = ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
		.AddLambda([Deflection](FGameplayTag, int32 Count)
		{
			if (Count > 0) { Deflection->EndDeflection(); }
		});
	TestFalse(TEXT("Tag callback retires the window before Begin returns"), Deflection->BeginDeflection());
	TestEqual(TEXT("Reentrant tag cleanup leaves no contribution"), ASC->GetTagCount(Tag), 0);
	ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).Remove(Listener);
	Deflection->OnDeflectionStarted.AddDynamic(Deflection, &USovDeflectionComponent::EndDeflection);
	TestFalse(TEXT("Start event cancellation also rejects Begin"), Deflection->BeginDeflection());
	Deflection->OnDeflectionStarted.RemoveDynamic(Deflection, &USovDeflectionComponent::EndDeflection);
	TestTrue(TEXT("A later clean window can start"), Deflection->BeginDeflection());
	ASC->AddLooseGameplayTag(Tag);
	Deflection->EndDeflection();
	TestEqual(TEXT("Cleanup preserves another owner's contribution"), ASC->GetTagCount(Tag), 1);
	ASC->RemoveLooseGameplayTag(Tag);
	Fixture.Advance(1.f);
	TestFalse(TEXT("Retired timers cannot reopen the window"), Deflection->IsDeflectionWindowOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDeflectionReentrantActivationTest,
	"ProjectVelkorran.Campaign.Deflection.ReentrantActivationOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDeflectionReentrantActivationTest::RunTest(const FString& Parameters)
{
	SovDeflectionRuntimeTests::FWorld Fixture;
	auto* Character = Fixture.Character();
	if (!TestNotNull(TEXT("Selene fixture"), Character)) { return false; }
	auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(USovGameplayAbility_SeleneDeflection::StaticClass(), 1));
	const auto Tag = FSovGameplayTags::Get().State_Deflecting;
	const auto Busy = FNarrativeGameplayTags::Get().State_Busy;
	bool bReplace = false;
	bool bCancelled = false;
	bool bRestarted = false;
	const auto Listener = ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
		.AddLambda([&](FGameplayTag, int32 Count)
		{
			if (Count <= 0 || bCancelled) { return; }
			bCancelled = true;
			ASC->CancelAbilityHandle(Handle);
			if (bReplace) { bRestarted = ASC->TryActivateAbility(Handle, false); }
		});
	ASC->TryActivateAbility(Handle, false);
	TestTrue(TEXT("Callback reached actual GAS activation"), bCancelled);
	TestFalse(TEXT("Cancelled startup cannot continue the ability"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
	TestEqual(TEXT("Cancelled startup clears its defense contribution"), ASC->GetTagCount(Tag), 0);
	TestEqual(TEXT("Cancelled startup clears Busy"), ASC->GetTagCount(Busy), 0);
	bCancelled = false;
	bReplace = true;
	ASC->TryActivateAbility(Handle, false);
	ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).Remove(Listener);
	TestTrue(TEXT("A cancellation callback can start a fresh activation"), bRestarted);
	TestTrue(TEXT("Old startup does not end its replacement"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
	TestEqual(TEXT("Only the replacement owns the window"), ASC->GetTagCount(Tag), 1);
	Fixture.Advance(1.f);
	TestFalse(TEXT("Replacement recovery finishes normally"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
	TestEqual(TEXT("No old or new defense contribution remains"), ASC->GetTagCount(Tag), 0);
	TestEqual(TEXT("No recovery leaves Busy behind"), ASC->GetTagCount(Busy), 0);
	return true;
}
#endif
