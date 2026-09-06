// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Components/SovDeflectionComponent.h"
#include "Components/SovEchoComponent.h"
#include "Components/SovSeleneEchoGenerationComponent.h"
#include "Components/SovTarrikEchoGenerationComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Script.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace SovHeroRewardLifeTests
{
struct FWorld
{
	UWorld* World = nullptr;
	FWorld()
	{
		const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
	}
	~FWorld()
	{ if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
	ASovCombatAdmissionTestCharacter* Character(int32 Team)
	{
		if (!World) { return nullptr; }
		FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Actor = World->SpawnActor<ASovCombatAdmissionTestCharacter>(ASovCombatAdmissionTestCharacter::StaticClass(),
			FVector(Team * 200.f, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
		if (Actor) { Actor->InitializeTestCombat(Team); }
		return Actor;
	}
};
void Hit(ASovAxiomRuntimeTestCharacter* Source, ASovAxiomRuntimeTestCharacter* Target, FName Bone = NAME_None)
{
	auto* ASC = Source->GetNarrativeAbilitySystemComponent();
	auto Context = ASC->MakeEffectContext(); Context.AddInstigator(Source, Source);
	if (!Bone.IsNone()) { FHitResult HitResult; HitResult.BoneName = Bone; Context.AddHitResult(HitResult); }
	FGameplayEffectSpec Spec(GetDefault<USovCombatRoutingTestEffect>(), Context, 1.f);
	Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, 10.f);
	Spec.AddDynamicAssetTag(FSovGameplayTags::Get().Damage_Channel_Kinetic);
	ASC->ApplyGameplayEffectSpecToTarget(Spec, Target->GetNarrativeAbilitySystemComponent());
}
void PrepareMarkedVictim(ASovAxiomRuntimeTestCharacter* Target)
{
	auto* ASC = Target->GetNarrativeAbilitySystemComponent();
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 0.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 1.f);
	ASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_Target_Marked);
	ASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_CommandTarget_Window);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHeroDamageRewardLifeTest,
	"ProjectVelkorran.Campaign.Echo.NativeRewardTargetLife", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovHeroDamageRewardLifeTest::RunTest(const FString& Parameters)
{
#if WITH_EDITOR
	FEditorScriptExecutionGuard AllowProductionHandlers;
#endif
	for (bool bTarrik : {false, true}) for (bool bRestoreInsideTeamPolicy : {false, true})
	{
		SovHeroRewardLifeTests::FWorld F; auto* Source = F.Character(0); auto* Target = F.Character(1);
		if (!Source || !Target) { return false; }
		auto* SourceASC = Source->GetNarrativeAbilitySystemComponent(); auto* TargetASC = Target->GetNarrativeAbilitySystemComponent();
		Source->TestEchoGeneration->InitializeWithAbilitySystem(nullptr);
		Source->TestEcho->RestoreEchoFromCheckpoint(0.f);
		if (bTarrik)
		{
			SourceASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Selene);
			SourceASC->AddLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Tarrik);
		}
		SovHeroRewardLifeTests::PrepareMarkedVictim(Target); SovHeroRewardLifeTests::Hit(Source, Target);
		const FSovDamageResult Retained = Target->LastDamageResult;
		if (!TestTrue(TEXT("Canonical damage minted a fatal current-life receipt"), Retained.bFatal && Retained.IsCurrentTargetLife())) { return false; }
		bool bRestored = false;
		const auto Restore = [&]() { bRestored = true; TargetASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f); };
		if (!bRestoreInsideTeamPolicy) { Restore(); }
		if (bTarrik)
		{
			auto* Generator = NewObject<USovTarrikEchoGenerationComponent>(Source);
			Source->AddInstanceComponent(Generator); Generator->RegisterComponent();
			if (!Generator->InitializeWithAbilitySystem(SourceASC)) { return false; }
		}
		else if (!Source->TestEchoGeneration->InitializeWithAbilitySystem(SourceASC)) { return false; }
		if (bRestoreInsideTeamPolicy) { Source->OnNextTeamQuery = Restore; }
		// This consumer has never seen this genuine packet. A GUID replay ledger
		// alone therefore cannot reject a late delivery from a retired target life.
		SourceASC->OnDamageResolvedAsSource.Broadcast(Retained);
		TestTrue(TEXT("Target restoration actually occurred at the intended boundary"), bRestored);
		TestFalse(TEXT("Retained native receipt belongs to the old life"), Retained.IsCurrentTargetLife());
		TestEqual(TEXT("Neither hero receives an old-life kill payoff"), Source->TestEcho->GetEcho(), 0.f);
		auto* Fresh = F.Character(1); if (!Fresh) { return false; }
		SovHeroRewardLifeTests::PrepareMarkedVictim(Fresh); SovHeroRewardLifeTests::Hit(Source, Fresh);
		TestEqual(TEXT("Fresh native damage retains the authored kill reward"), Source->TestEcho->GetEcho(), bTarrik ? 8.f : 6.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSeleneRewardContinuationLifeTest,
	"ProjectVelkorran.Campaign.Echo.RewardContinuationTargetLife", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSeleneRewardContinuationLifeTest::RunTest(const FString& Parameters)
{
#if WITH_EDITOR
	FEditorScriptExecutionGuard AllowProductionHandlers;
#endif
	for (bool bRestoreDuringReward : {false, true})
	{
		SovHeroRewardLifeTests::FWorld F; auto* Source = F.Character(0); auto* Target = F.Character(1);
		if (!Source || !Target) { return false; }
		auto* TargetASC = Target->GetNarrativeAbilitySystemComponent();
		auto* WeakPoints = NewObject<USovWeakPointRoutingTestComponent>(Target);
		Target->AddInstanceComponent(WeakPoints); WeakPoints->RegisterComponent(); WeakPoints->InitializeWithAbilitySystem(TargetASC);
		Source->TestEcho->RestoreEchoFromCheckpoint(0.f); SovHeroRewardLifeTests::PrepareMarkedVictim(Target);
		bool bRestored = false;
		auto& EchoChanged = Source->GetNarrativeAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetEchoAttribute());
		const FDelegateHandle Handle = EchoChanged.AddLambda([&](const FOnAttributeChangeData& Change)
		{
			if (!bRestoreDuringReward || bRestored || Change.NewValue <= Change.OldValue) { return; }
			bRestored = true; TargetASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
		});
		SovHeroRewardLifeTests::Hit(Source, Target, TEXT("weapon")); EchoChanged.Remove(Handle);
		TestEqual(TEXT("Control case grants kill6 plus weak-point8; restoration preserves only committed kill6"),
			Source->TestEcho->GetEcho(), bRestoreDuringReward ? 6.f : 14.f);
		TestEqual(TEXT("Callback restoration path was exercised"), bRestored, bRestoreDuringReward);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPerfectDeflectionRewardLifeTest,
	"ProjectVelkorran.Campaign.Echo.PerfectDeflectionTargetLife", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPerfectDeflectionRewardLifeTest::RunTest(const FString& Parameters)
{
#if WITH_EDITOR
	FEditorScriptExecutionGuard AllowProductionHandlers;
#endif
	SovHeroRewardLifeTests::FWorld F; auto* Player = F.Character(0); auto* Enemy = F.Character(1);
	if (!Player || !Enemy) { return false; }
	Player->TestEchoGeneration->InitializeWithAbilitySystem(nullptr); Player->TestEcho->RestoreEchoFromCheckpoint(0.f);
	if (!Player->TestDeflection->BeginDeflection()) { return false; }
	SovHeroRewardLifeTests::Hit(Enemy, Player);
	const FSovDamageResult Retained = Player->LastDamageResult;
	if (!TestTrue(TEXT("Real damage confirms native perfect deflection"), Retained.bPerfectDefense && Retained.IsCurrentTargetLife())) { return false; }
	auto* ASC = Player->GetNarrativeAbilitySystemComponent();
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	Player->TestEchoGeneration->InitializeWithAbilitySystem(ASC);
	Player->TestDeflection->OnPerfectDeflection.Broadcast(Retained);
	TestEqual(TEXT("A never-consumed pre-restore defense cannot grant Echo"), Player->TestEcho->GetEcho(), 0.f);
	Player->TestDeflection->EndDeflection();
	if (!Player->TestDeflection->BeginDeflection()) { return false; }
	SovHeroRewardLifeTests::Hit(Enemy, Player);
	TestEqual(TEXT("A fresh native perfect defense still grants10"), Player->TestEcho->GetEcho(), 10.f);
	return true;
}
#endif
