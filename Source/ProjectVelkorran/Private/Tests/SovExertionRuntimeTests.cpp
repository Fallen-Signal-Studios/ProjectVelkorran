// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovExertionRuntimeTestFixtures.h"
#include "Character/NarrativeCharacterMovement.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SovEchoComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Exertion/SovExertionComponent.h"
#include "Exertion/SovGameplayAbility_Exertion.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

#if WITH_AUTOMATION_TESTS
struct FSovExertionTestAccess
{
	static void Step(USovExertionComponent* Exertion, float Delta) { Exertion->UpdateExertion(Delta); }
};
namespace
{
	struct FExertionWorld
	{
		UWorld* World;
		FExertionWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
		}
		~FExertionWorld()
		{
			World->DestroyWorld(false);
			if (GEngine) { GEngine->DestroyWorldContext(World); }
		}
		ASovExertionRuntimeTestCharacter* Character(bool bSelene = false)
		{
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Result = World->SpawnActor<ASovExertionRuntimeTestCharacter>(
				ASovExertionRuntimeTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
			Result->InitializeExertion(bSelene);
			return Result;
		}
		AActor* Wall(float X)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Box); Actor->AddInstanceComponent(Box);
			Box->SetBoxExtent(FVector(10.f, 300.f, 300.f));
			Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent(); Actor->SetActorLocation(FVector(X, 0.f, 0.f));
			return Actor;
		}
	};
	float Stamina(ASovExertionRuntimeTestCharacter* Character) { return Character->GetExertionComponent()->GetStamina(); }
	void SetStamina(ASovExertionRuntimeTestCharacter* Character, float Value)
	{
		Character->GetNarrativeAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), Value);
	}
	FGameplayAbilitySpecHandle Grant(ASovExertionRuntimeTestCharacter* Character, TSubclassOf<UGameplayAbility> Class)
	{
		FGameplayAbilitySpec Spec(Class, 1); Spec.InputPressed = true;
		return Character->GetNarrativeAbilitySystemComponent()->GiveAbility(Spec);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovExertionRuntimeResourceTest,
	"ProjectVelkorran.Campaign.Exertion.RegenSprintAndCheckpoint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovExertionRuntimeResourceTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FExertionWorld Test; auto* Character = Test.Character(); auto* Exertion = Character->GetExertionComponent();
	SetStamina(Character, 40.f); Exertion->ResetForCheckpoint();
	FSovExertionTestAccess::Step(Exertion, 0.65f);
	TestEqual(TEXT("Checkpoint/external damage restart full delay"), Stamina(Character), 40.f);
	FSovExertionTestAccess::Step(Exertion, 1.f);
	TestEqual(TEXT("Idle regen uses existing 32/s attribute"), Stamina(Character), 72.f);
	Character->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FSovGameplayTags::Get().State_Guarding);
	FSovExertionTestAccess::Step(Exertion, 1.f);
	TestEqual(TEXT("Guarding regen remains slower"), Stamina(Character), 88.f);
	Character->GetNarrativeAbilitySystemComponent()->RemoveLooseGameplayTag(FSovGameplayTags::Get().State_Guarding);
	auto* Movement = Cast<UNarrativeCharacterMovement>(Character->GetCharacterMovement());
	Movement->bWantsSprint = true; Movement->Velocity = FVector(600.f, 0.f, 0.f);
	SetStamina(Character, 50.f);
	FSovExertionTestAccess::Step(Exertion, 0.5f);
	TestEqual(TEXT("Out of combat sprint costs zero"), Stamina(Character), 50.f);
	Character->GetEchoComponent()->BeginEncounter();
	FSovExertionTestAccess::Step(Exertion, 0.5f);
	TestEqual(TEXT("Registered combat sprint drains by elapsed time"), Stamina(Character), 42.f);
	SetStamina(Character, 1.f);
	FSovExertionTestAccess::Step(Exertion, 1.f);
	TestEqual(TEXT("Continuous sprint stops exactly at zero"), Stamina(Character), 0.f);
	TestFalse(TEXT("Exhaustion clears sprint request"), bool(Movement->bWantsSprint));
	TestTrue(TEXT("Resource state is readable"), Character->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Exertion_Exhausted));
	TestFalse(TEXT("Unpayable discrete action fails"), Exertion->TrySpendExertion(24.f));
	Character->GetEchoComponent()->EndEncounter();
	TestTrue(TEXT("Zero stamina still permits out of combat sprint"), Exertion->CanSprint());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovExertionRuntimeChargeTest,
	"ProjectVelkorran.Campaign.Exertion.ChargedReleaseAndCancelTransactions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovExertionRuntimeChargeTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FExertionWorld Test; auto* Character = Test.Character(); auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	const auto Handle = Grant(Character, USovExertionRuntimeChargedAbility::StaticClass());
	TestTrue(TEXT("Attack can charge"), ASC->TryActivateAbility(Handle));
	auto* Ability = Cast<USovExertionRuntimeChargedAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
	Ability->DispatchTestHit(); TestEqual(TEXT("No payload before release commits"), Ability->Dispatches, 0);
	SetStamina(Character, 19.f);
	TestFalse(TEXT("Unpayable charged tier rejects release"), Ability->TryCommitChargedRelease(2));
	TestEqual(TEXT("Failure pays nothing"), Stamina(Character), 19.f);
	SetStamina(Character, 20.f);
	TestTrue(TEXT("Exact cost releases"), Ability->TryCommitChargedRelease(2));
	TestEqual(TEXT("Exact cost clamps to zero"), Stamina(Character), 0.f);
	TestFalse(TEXT("Release transaction cannot replay"), Ability->TryCommitChargedRelease(2));
	Ability->DispatchTestHit(); TestEqual(TEXT("Committed release dispatches"), Ability->Dispatches, 1);
	ASC->CancelAllAbilities(); TestTrue(TEXT("New activation accepted"), ASC->TryActivateAbility(Handle));
	TestTrue(TEXT("Below threshold tier costs zero"), Ability->TryCommitChargedRelease(1));
	SetStamina(Character, 10.f);
	TestTrue(TEXT("Admitted defensive cancel commits exact configured cost"), Ability->TryCommitDefensiveCancel(10.f));
	TestFalse(TEXT("Cancel cannot pay twice"), Ability->TryCommitDefensiveCancel(0.f));
	Ability->DispatchTestHit(); TestEqual(TEXT("Cancelled node no longer dispatches damage"), Ability->Dispatches, 1);
	ASC->CancelAllAbilities(); ASC->TryActivateAbility(Handle); SetStamina(Character, 20.f);
	Character->bCancelOnSpend = true;
	TestFalse(TEXT("Synchronous cancellation prevents release continuation"), Ability->TryCommitChargedRelease(2));
	TestFalse(TEXT("Cancelled receipt is not committed"), Ability->IsChargedReleaseCommitted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovExertionRuntimeEvadeTest,
	"ProjectVelkorran.Campaign.Exertion.EvadeAdmissionAndOwnedCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovExertionRuntimeEvadeTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FExertionWorld Test; auto* Character = Test.Character(); auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	const auto Handle = Grant(Character, USovGameplayAbility_Evade::StaticClass());
	SetStamina(Character, 23.f);
	TestFalse(TEXT("Exhausted evade never activates"), ASC->TryActivateAbility(Handle));
	TestFalse(TEXT("Rejected evade grants no immunity"), ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Invulnerable));
	SetStamina(Character, 24.f);
	AActor* Wall = Test.Wall(Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + 15.f);
	TestFalse(TEXT("No capsule clearance rejects before payment"), ASC->TryActivateAbility(Handle));
	TestEqual(TEXT("Blocked evade retains all stamina"), Stamina(Character), 24.f);
	Wall->Destroy();
	ASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_Invulnerable);
	TestTrue(TEXT("Clear path exact-cost evade starts"), ASC->TryActivateAbility(Handle));
	TestEqual(TEXT("Evade pays prototype cost once"), Stamina(Character), 0.f);
	TestTrue(TEXT("Movement uses existing CMC root-motion source"), Character->GetCharacterMovement()->HasRootMotionSources());
	TestTrue(TEXT("Native evade state visible"), ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Evading));
	const auto Attack = Grant(Character, USovExertionRuntimeChargedAbility::StaticClass());
	TestFalse(TEXT("Committed evade blocks offensive activation"), ASC->TryActivateAbility(Attack));
	ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled);
	TestFalse(TEXT("Cinematic interrupts evade"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
	TestFalse(TEXT("Evade owns and clears only its Busy state"), ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy));
	TestFalse(TEXT("Evade state clears on interruption"), ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Evading));
	TestEqual(TEXT("Unrelated immunity survives cleanup"), ASC->GetGameplayTagCount(FSovGameplayTags::Get().State_Invulnerable), 1);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatInputRuntimeTest,
	"ProjectVelkorran.Campaign.Exertion.SemanticInputBufferOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCombatInputRuntimeTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FExertionWorld Test; auto* Character = Test.Character(); auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	const auto Handle = Grant(Character, USovExertionRuntimeChargedAbility::StaticClass());
	ASC->TryActivateAbility(Handle);
	auto* Ability = Cast<USovExertionRuntimeChargedAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
	const FGameplayTag Attack = FNarrativeGameplayTags::Get().Narrative_Input_Attack;
	const FGameplayTag Evade = FSovGameplayTags::Get().Input_Evade;
	FGameplayTagContainer Allowed(Attack); Allowed.AddTag(Evade);
	FGuid Window = ASC->RegisterCombatInputWindow(Ability, Allowed, .1f, .8f);
	TestTrue(TEXT("Live attack owns an authored scope"), Window.IsValid());
	TestFalse(TEXT("Live scope cannot be silently replaced"), ASC->RegisterCombatInputWindow(Ability, Allowed, 0.f, 1.f).IsValid());
	FGameplayTag Press; bool bHeld = false;
	ASC->AbilityInputTagPressed(Attack);
	TestFalse(TEXT("Buffer never opens a premature cancel"), ASC->ConsumeCombatInputWindow(Ability, Window, Press, bHeld));
	ASC->AbilityInputTagReleased(Attack);
	Test.World->Tick(LEVELTICK_All, .11f);
	TestTrue(TEXT("Tap is consumed once after authored opening"), ASC->ConsumeCombatInputWindow(Ability, Window, Press, bHeld));
	TestEqual(TEXT("Semantic tag is preserved"), Press, Attack);
	TestFalse(TEXT("Released tap is not turned into a held attack"), bHeld);
	TestFalse(TEXT("One press cannot be consumed twice"), ASC->ConsumeCombatInputWindow(Ability, Window, Press, bHeld));
	ASC->AbilityInputTagPressed(Attack); ASC->AbilityInputTagPressed(Evade);
	TestTrue(TEXT("Newest intent wins within scope"), ASC->ConsumeCombatInputWindow(Ability, Window, Press, bHeld));
	TestEqual(TEXT("Newest semantic tag is retained"), Press, Evade);
	ASC->AbilityInputTagPressed(Attack);
	TestTrue(TEXT("Finite next node receives new attack identity"), Ability->AdvanceTestNode());
	TestFalse(TEXT("Old node cannot consume after GUID renewal"), ASC->ConsumeCombatInputWindow(Ability, Window, Press, bHeld));
	Window = ASC->RegisterCombatInputWindow(Ability, Allowed, 0.f, 1.f);
	ASC->AbilityInputTagPressed(Attack); ASC->ClearCombatInputBuffer();
	TestFalse(TEXT("Focus/modal suppression drops pending intent"), ASC->ConsumeCombatInputWindow(Ability, Window, Press, bHeld));
	Window = ASC->RegisterCombatInputWindow(Ability, Allowed, 0.f, 1.f);
	ASC->AbilityInputTagPressed(Attack); ASC->CancelAllAbilities();
	TestFalse(TEXT("Cancelled activation cannot consume"), ASC->ConsumeCombatInputWindow(Ability, Window, Press, bHeld));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInputReentryReleaseTest,
	"ProjectVelkorran.Campaign.Exertion.StaleReleaseDoesNotReachNewActivation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovInputReentryReleaseTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FExertionWorld Test; auto* Character = Test.Character(); auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	const FGameplayAbilitySpecHandle Handle = Grant(Character, USovInputReentryTestAbility::StaticClass());
	ASC->FindAbilitySpecFromHandle(Handle)->GetDynamicSpecSourceTags().AddTag(FNarrativeGameplayTags::Get().Narrative_Input_Attack);
	TestTrue(TEXT("Initial activation succeeds"), ASC->TryActivateAbility(Handle, false));
	auto* Ability = Cast<USovInputReentryTestAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
	ASC->AbilityInputTagReleased(FNarrativeGameplayTags::Get().Narrative_Input_Attack);
	TestTrue(TEXT("Release callback may create a fresh same-spec activation"), Ability->IsActive());
	TestTrue(TEXT("Fresh activation keeps its new pressed state"), bool(ASC->FindAbilitySpecFromHandle(Handle)->InputPressed));
	TestEqual(TEXT("Old replicated release is not delivered to the new activation"), Ability->ReplicatedReleases, 0);
	return true;
}
#endif
