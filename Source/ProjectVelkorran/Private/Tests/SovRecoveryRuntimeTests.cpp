// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Tests/SovRecoveryRuntimeTestFixtures.h"
#include "Recovery/SovRecoveryExclusionVolume.h"
#include "TimerManager.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Character/PlayerDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Recovery/SovFatalRecoveryComponent.h"
#include "Sovereign/SovGameplayTags.h"

#if WITH_AUTOMATION_TESTS
struct FSovRecoveryTestAccess
{
	static void Resolve(USovFatalRecoveryComponent* Recovery) { Recovery->ResolveFatal(Recovery->Epoch); }
	static void Retry(USovFatalRecoveryComponent* Recovery) { Recovery->Retry(Recovery->Epoch); }
	static bool HasPendingDecision(USovFatalRecoveryComponent* Recovery) { return Recovery->GetWorld()->GetTimerManager().IsTimerActive(Recovery->DecisionTimer); }
	static void Damage(USovFatalRecoveryComponent* Recovery, const FSovDamageResult& Result) { Recovery->HandleDamage(Result); }
	static bool ExcludedFatal(const USovFatalRecoveryComponent* Recovery) { return Recovery->bExcludedFatal; }
	static void Detach(USovFatalRecoveryComponent* Recovery) { Recovery->InitializeWithAbilitySystem(nullptr); }
};
namespace
{
	struct FRecoveryWorld
	{
		UWorld* World = nullptr;
		ASovHandoffRuntimeTestPawn* Pawn = nullptr;
		ASovHandoffRuntimeTestController* PC = nullptr;
		UNarrativeAbilitySystemComponent* ASC = nullptr;
		FRecoveryWorld()
		{
			const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &WorldInitialization);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			PC = World->SpawnActor<ASovHandoffRuntimeTestController>();
			Pawn = World->SpawnActor<ASovHandoffRuntimeTestPawn>();
			auto* PS = World->SpawnActor<ASovPlayerState>();
			if (!PC || !Pawn || !PS) { return; }
			auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
			Pawn->PrepareCampaignInitialization(Definition); PC->SetTestPlayerState(PS); PC->Possess(Pawn);
			if (!Pawn->StageTestReadiness(PS, true) || !Pawn->CompleteCampaignDataInitialization(false)) { return; }
			ASC = Cast<UNarrativeAbilitySystemComponent>(PS->GetAbilitySystemComponent());
		}
		~FRecoveryWorld()
		{ if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
		AActor* Box(FVector Position, FVector Extent)
		{
			auto* Actor = World->SpawnActor<AActor>(); auto* Shape = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Shape); Actor->AddInstanceComponent(Shape); Shape->SetBoxExtent(Extent);
			Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Shape->SetCollisionObjectType(ECC_WorldStatic);
			Shape->SetCollisionResponseToAllChannels(ECR_Block); Shape->RegisterComponent(); Actor->SetActorLocation(Position); return Actor;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRecoveryProtectionTest, "ProjectVelkorran.Campaign.Recovery.OwnedProtectionAndClearance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovRecoveryProtectionTest::RunTest(const FString& Parameters)
{
	FRecoveryWorld F; if (!TestNotNull(TEXT("Real persistent ASC initialized"), F.ASC)) { return false; }
	auto* Recovery = F.Pawn->GetRecoveryComponent(); const auto& N = FNarrativeGameplayTags::Get();
	F.ASC->AddLooseGameplayTag(N.State_Invulnerable);
	TestTrue(TEXT("Checkpoint protection applies through a finite GE"), Recovery->ProtectRestoredCheckpoint());
	TestTrue(TEXT("Recovery protection has a semantic subtype"), F.ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Invulnerable_Respawn));
	FSovRecoveryTestAccess::Detach(Recovery);
	TestFalse(TEXT("Detach removes only its recovery GE"), F.ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Invulnerable_Respawn));
	TestTrue(TEXT("Unrelated invulnerability remains owned by its source"), F.ASC->HasMatchingGameplayTag(N.State_Invulnerable));
	const float Height = F.Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Point(0, 0, Height + 2.f); F.Pawn->SetActorLocation(Point);
	TestFalse(TEXT("A position over empty space cannot be a recovery point"), Recovery->IsSafeRecoveryPosition(F.Pawn, Point));
	F.Box(FVector(0, 0, -10), FVector(500, 500, 10));
	TestTrue(TEXT("Clear capsule with walkable floor is accepted"), Recovery->IsSafeRecoveryPosition(F.Pawn, Point));
	auto* Exclusion = F.World->SpawnActor<ASovRecoveryExclusionVolume>();
	Exclusion->SetActorLocation(Point); Exclusion->ExclusionBounds->SetBoxExtent(FVector(20.f));
	TestFalse(TEXT("Active authored hazard forbids an otherwise clear recovery point"), Recovery->IsSafeRecoveryPosition(F.Pawn, Point));
	Exclusion->SetExclusionActive(false);
	TestTrue(TEXT("Disabling the resolved hazard restores eligibility"), Recovery->IsSafeRecoveryPosition(F.Pawn, Point));
	Exclusion->SetExclusionActive(true); Exclusion->SetActorLocation(Point + FVector(F.Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius() + 10.f, 0, 0));
	TestFalse(TEXT("Exclusion includes the capsule edge, not only the center"), Recovery->IsSafeRecoveryPosition(F.Pawn, Point));
	Exclusion->Destroy();
	F.Box(Point, FVector(20, 20, 20));
	TestFalse(TEXT("Dynamic checkpoint obstruction is rejected"), Recovery->IsSafeRecoveryPosition(F.Pawn, Point));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRecoveryFatalTest, "ProjectVelkorran.Campaign.Recovery.FatalRoutingAndStaleOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovRecoveryFatalTest::RunTest(const FString& Parameters)
{
	FRecoveryWorld F; if (!TestNotNull(TEXT("Real persistent ASC initialized"), F.ASC)) { return false; }
	auto* Recovery = F.Pawn->GetRecoveryComponent();
	F.ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
	FGameplayEffectSpec Fatal(GetDefault<USovCombatRoutingTestEffect>(), F.ASC->MakeEffectContext(), 1.f);
	F.ASC->HandleOutOfHealth(nullptr, nullptr, Fatal, 100.f);
	TestTrue(TEXT("Actual ASC fatal state is owned"), Recovery->OwnsFatalRecovery());
	TestEqual(TEXT("Death schedules a bounded native decision"), Recovery->GetRecoveryState(), ESovRecoveryState::ResolvingFatal);
	TestTrue(TEXT("Death immediately blocks movement input"), F.PC->IsMoveInputIgnored());
	FSovDamageResult Canonical; Canonical.TransactionId = FGuid::NewGuid(); Canonical.TargetActor = F.Pawn;
	Canonical.bFatal = true; Canonical.bCanonicalFatal = true;
	FSovRecoveryTestAccess::Damage(Recovery, Canonical);
	TestTrue(TEXT("Typed canonical fatal cannot be converted to companion rescue"), FSovRecoveryTestAccess::ExcludedFatal(Recovery));
	Canonical.TransactionId = FGuid::NewGuid(); Canonical.bCanonicalFatal = false;
	FSovRecoveryTestAccess::Damage(Recovery, Canonical);
	TestTrue(TEXT("A later ordinary result cannot erase the fatal exclusion"), FSovRecoveryTestAccess::ExcludedFatal(Recovery));
	FSovRecoveryTestAccess::Resolve(Recovery);
	TestEqual(TEXT("Unverified fatal cannot mint a companion rescue"), Recovery->GetRecoveryState(), ESovRecoveryState::Retrying);
	FSovCombatResourceSnapshot Resources; USovEncounterSnapshotLibrary::CaptureResources(F.ASC, Resources); Resources.Health = 35.f;
	TestTrue(TEXT("Real resource restore revives without resetting the player's definition"), USovEncounterSnapshotLibrary::RestoreResources(F.ASC, Resources));
	TestEqual(TEXT("Restored Health remains the explicit fraction"), F.ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 35.f);
	TestEqual(TEXT("Revive retires the pending retry epoch"), Recovery->GetRecoveryState(), ESovRecoveryState::Ready);
	TestFalse(TEXT("Revive releases owned movement suppression"), F.PC->IsMoveInputIgnored());
	FSovRecoveryTestAccess::Retry(Recovery);
	TestTrue(TEXT("A late retry cannot affect the revived pawn"), F.Pawn->IsAlive());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRecoveryReentryTest, "ProjectVelkorran.Campaign.Recovery.CallbackOwnershipFences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovRecoveryReentryTest::RunTest(const FString& Parameters)
{
	FRecoveryWorld F; if (!TestNotNull(TEXT("Real persistent ASC initialized"), F.ASC)) { return false; }
	auto* Recovery = F.Pawn->GetRecoveryComponent();
	const auto& Tags = FSovGameplayTags::Get();
	TestTrue(TEXT("Initial finite recovery effect exists"), Recovery->ProtectRestoredCheckpoint());
	bool bReenterRemoval = true;
	const FDelegateHandle Removal = F.ASC->OnAnyGameplayEffectRemovedDelegate().AddLambda(
		[&](const FActiveGameplayEffect& Effect)
		{
			if (!bReenterRemoval) { return; }
			bReenterRemoval = false;
			Recovery->InitializeWithAbilitySystem(nullptr);
			Recovery->InitializeWithAbilitySystem(F.ASC);
		});
	TestFalse(TEXT("Effect-removal rebind aborts the old protection transaction"), Recovery->ProtectRestoredCheckpoint());
	TestFalse(TEXT("Old transaction cannot grant protection into the newer binding"), F.ASC->HasMatchingGameplayTag(Tags.State_Invulnerable_Respawn));
	F.ASC->OnAnyGameplayEffectRemovedDelegate().Remove(Removal);
	bool bReenterApply = true;
	const FDelegateHandle Addition = F.ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddLambda(
		[&](UAbilitySystemComponent* AppliedASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
		{
			if (!bReenterApply) { return; }
			bReenterApply = false;
			Recovery->InitializeWithAbilitySystem(nullptr);
			Recovery->InitializeWithAbilitySystem(F.ASC);
		});
	TestFalse(TEXT("Effect-application rebind aborts the old protection transaction"), Recovery->ProtectRestoredCheckpoint());
	TestFalse(TEXT("Aborted newly applied effect is removed by exact handle"), F.ASC->HasMatchingGameplayTag(Tags.State_Invulnerable_Respawn));
	F.ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(Addition);
	auto* Probe = NewObject<USovRecoveryReentryProbe>(Recovery); Probe->Recovery = Recovery; Probe->bDetachOnResolving = true;
	Recovery->OnRecoveryChanged.AddDynamic(Probe, &USovRecoveryReentryProbe::OnChanged);
	F.ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
	FGameplayEffectSpec Fatal(GetDefault<USovCombatRoutingTestEffect>(), F.ASC->MakeEffectContext(), 1.f);
	F.ASC->HandleOutOfHealth(nullptr, nullptr, Fatal, 100.f);
	TestEqual(TEXT("State callback detached the old recovery owner"), Recovery->GetRecoveryState(), ESovRecoveryState::Ready);
	TestFalse(TEXT("State callback cannot leave a late fatal decision timer"), FSovRecoveryTestAccess::HasPendingDecision(Recovery));
	TestFalse(TEXT("Reentrant detach releases only owned input suppression"), F.PC->IsMoveInputIgnored());
	return true;
}
#endif
