// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCinderJudgementGeometryFixtures.h"
#include "World/SovDestructibleCover.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SovEchoComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Presentation/SovCinderJudgementPresentation.h"
#include "Sovereign/SovGameplayTags.h"
#include "Weapons/WeaponVisual.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS
namespace
{
struct FJudgementGeometryWorld
{
	UWorld* World = nullptr;
	FJudgementGeometryWorld()
	{
		// Preserve the UE 5.7 Mac single-initialization fix.
		const UWorld::InitializationValues Values = UWorld::InitializationValues()
			.AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
			.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
			.SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
			ERHIFeatureLevel::Num, &Values);
		if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
	}
	~FJudgementGeometryWorld()
	{
		if (World)
		{
			World->DestroyWorld(false);
			if (GEngine) { GEngine->DestroyWorldContext(World); }
		}
	}
	ASovCinderJudgementGeometryCharacter* Character(FVector Location, int32 Team)
	{
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Actor = World ? World->SpawnActor<ASovCinderJudgementGeometryCharacter>(
			ASovCinderJudgementGeometryCharacter::StaticClass(), Location, FRotator::ZeroRotator, Spawn) : nullptr;
		if (Actor)
		{
			Actor->InitializeTestCombat(Team);
			Actor->GetCapsuleComponent()->SetCollisionResponseToChannel(
				UArsenalStatics::GetNarrativeProSettings()->WeaponTraceChannel, ECR_Block);
			if (Team == 0)
			{
				auto* ASC = Actor->GetNarrativeAbilitySystemComponent();
				ASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Selene);
				ASC->AddLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Tarrik);
			}
		}
		return Actor;
	}
	AActor* Wall(FVector Location, FVector Extent)
	{
		AActor* Actor = World ? World->SpawnActor<AActor>() : nullptr;
		if (!Actor) { return nullptr; }
		auto* Box = NewObject<UBoxComponent>(Actor);
		Actor->AddInstanceComponent(Box);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent();
		Actor->SetActorLocation(Location);
		return Actor;
	}
	ASovCinderJudgementPresentation* Presentation() const
	{
		if (!World) { return nullptr; }
		for (TActorIterator<ASovCinderJudgementPresentation> It(World); It; ++It)
		{
			if (IsValid(*It) && !It->IsActorBeingDestroyed()) { return *It; }
		}
		return nullptr;
	}
};

USovCinderJudgementGeometryAbility* ActivateJudgement(
	FAutomationTestBase& Test, ASovAxiomRuntimeTestCharacter* Source)
{
	if (!Test.TestNotNull(TEXT("Tarrik fixture"), Source)) { return nullptr; }
	auto* ASC = Source->GetNarrativeAbilitySystemComponent();
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(
		USovCinderJudgementGeometryAbility::StaticClass(), 1, INDEX_NONE, Source->SetTestWeapon()));
	if (!Test.TestTrue(TEXT("Real GAS Judgement activation"), ASC->TryActivateAbility(Handle, false))) { return nullptr; }
	const auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	return Spec ? Cast<USovCinderJudgementGeometryAbility>(Spec->GetPrimaryInstance()) : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinderJudgementThinWallTest,
	"ProjectVelkorran.Campaign.CinderJudgement.Geometry.ThinWall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinderJudgementThinWallTest::RunTest(const FString& Parameters)
{
	for (float Radius : {0.0f, 4.0f})
	{
		FJudgementGeometryWorld F;
		auto* Source = F.Character(FVector::ZeroVector, 0);
		auto* Target = F.Character(FVector(220.0, 0.0, 0.0), 1);
		auto* Wall = F.Wall(FVector(50.0, 0.0, 0.0), FVector(1.0, 300.0, 300.0));
		if (!Target || !Wall) { return false; }
		auto* Ability = ActivateJudgement(*this, Source);
		if (!Ability) { return false; }
		Ability->SetRadiusForTest(Radius);
		TestTrue(TEXT("Obstructed paid shot resolves against near-side wall"), Ability->ReleaseCinderJudgementFromAim());
		auto* Packet = F.Presentation();
		if (!TestNotNull(TEXT("Native presentation packet"), Packet)) { return false; }
		TestTrue(TEXT("Clipped muzzle resets to the trusted eye"), Packet->GetTraceStart().Equals(FVector::ZeroVector, 0.1));
		TestTrue(TEXT("Impact remains on the wall's near side"), Packet->GetTraceEnd().X <= 49.1);
		TestEqual(TEXT("World wall is the resolved hit"), Packet->GetHitActor(), Wall);
		TestEqual(TEXT("No direct or radial damage leaks through the wall"), Target->ResolvedHitCount, 0);
		TestEqual(TEXT("One existing Echo payment"), Source->TestEcho->GetEcho(), 50.0f);
		TestFalse(TEXT("Release remains exactly once"), Ability->ReleaseCinderJudgementFromAim());
		Ability->FinishEchoAbility(true);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinderJudgementReverseConvergenceTest,
	"ProjectVelkorran.Campaign.CinderJudgement.Geometry.ReverseConvergence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinderJudgementReverseConvergenceTest::RunTest(const FString& Parameters)
{
	FJudgementGeometryWorld F;
	auto* Source = F.Character(FVector::ZeroVector, 0);
	auto* Target = F.Character(FVector(500.0, 100.0, 0.0), 1);
	// Eye aim hits this close obstacle. The offset muzzle's bridge clears it,
	// leaving the aim point behind the muzzle: the previous shot ran backwards.
	auto* Wall = F.Wall(FVector(50.0, 0.0, 0.0), FVector(2.0, 5.0, 100.0));
	if (!Target || !Wall) { return false; }
	auto* Ability = ActivateJudgement(*this, Source);
	if (!Ability) { return false; }
	Ability->SetMuzzleForTest(FVector(100.0, 100.0, 0.0));
	TestTrue(TEXT("Close convergence produces a forward shot"), Ability->ReleaseCinderJudgementFromAim());
	auto* Packet = F.Presentation();
	if (!TestNotNull(TEXT("Forward shot packet"), Packet)) { return false; }
	TestTrue(TEXT("Clear muzzle remains the origin"), Packet->GetTraceStart().Equals(FVector(100.0, 100.0, 0.0), 0.1));
	TestTrue(TEXT("Shot cannot reverse relative to authority aim"), Packet->GetTraceEnd().X > Packet->GetTraceStart().X);
	TestEqual(TEXT("Forward target receives the hit"), Packet->GetHitActor(), static_cast<AActor*>(Target));
	Ability->FinishEchoAbility(true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinderJudgementUnobstructedTest,
	"ProjectVelkorran.Campaign.CinderJudgement.Geometry.UnobstructedRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinderJudgementUnobstructedTest::RunTest(const FString& Parameters)
{
	FJudgementGeometryWorld F;
	auto* Source = F.Character(FVector::ZeroVector, 0);
	auto* Target = F.Character(FVector(500.0, 0.0, 0.0), 1);
	if (!Target) { return false; }
	auto* Ability = ActivateJudgement(*this, Source);
	if (!Ability) { return false; }
	TestTrue(TEXT("Legitimate release succeeds"), Ability->ReleaseCinderJudgementFromAim());
	auto* Packet = F.Presentation();
	if (!TestNotNull(TEXT("Legitimate packet"), Packet)) { return false; }
	TestTrue(TEXT("Unclipped muzzle remains the origin"), Packet->GetTraceStart().Equals(FVector(100.0, 0.0, 0.0), 0.1));
	TestTrue(TEXT("Native direct damage still resolves"), Packet->DirectDamageResolved());
	TestTrue(TEXT("Blocking hit still triggers the controlled blast"), Packet->BlastTriggered());
	TestEqual(TEXT("Existing direct plus radial payloads each execute once"), Target->ResolvedHitCount, 2);
	TestEqual(TEXT("Existing 50 Echo price preserved"), Source->TestEcho->GetEcho(), 50.0f);
	Ability->FinishEchoAbility(true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinderJudgementRangePolicyTest,
	"ProjectVelkorran.Campaign.CinderJudgement.Geometry.MaximumRangePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinderJudgementRangePolicyTest::RunTest(const FString& Parameters)
{
	for (bool bBlast : {false, true})
	{
		FJudgementGeometryWorld F;
		auto* Source = F.Character(FVector::ZeroVector, 0);
		auto* Ability = ActivateJudgement(*this, Source);
		if (!Ability) { return false; }
		Ability->SetMaximumRangeBlastForTest(bBlast);
		TestTrue(TEXT("Unobstructed maximum-range release succeeds"), Ability->ReleaseCinderJudgementFromAim());
		auto* Packet = F.Presentation();
		if (!Packet) { return false; }
		TestTrue(TEXT("Range stays measured from the resolved origin"),
			FMath::IsNearlyEqual(FVector::Distance(Packet->GetTraceStart(), Packet->GetTraceEnd()), 1000.0, 0.1));
		TestFalse(TEXT("Miss does not invent a blocking hit"), Packet->HasBlockingHit());
		TestEqual(TEXT("Authored maximum-range blast policy preserved"), Packet->BlastTriggered(), bBlast);
		Ability->FinishEchoAbility(true);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinderJudgementInvalidGeometryTest,
	"ProjectVelkorran.Campaign.CinderJudgement.Geometry.InvalidEyeAndFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinderJudgementInvalidGeometryTest::RunTest(const FString& Parameters)
{
	for (bool bInvalidEye : {false, true})
	{
		FJudgementGeometryWorld F;
		auto* Source = F.Character(FVector::ZeroVector, 0);
		auto* Ability = ActivateJudgement(*this, Source);
		if (!Ability) { return false; }
		if (bInvalidEye)
		{
			Source->bInvalidEye = true;
			AddExpectedError(TEXT("invalid Cinder Judgement release geometry"), EAutomationExpectedErrorFlags::Contains, 1);
		}
		else
		{
			Ability->SetMuzzleForTest(FVector(501.0, 0.0, 0.0));
			AddExpectedError(TEXT("Cinder Judgement could not resolve its authoritative release context"),
				EAutomationExpectedErrorFlags::Contains, 1);
		}
		TestFalse(TEXT("Invalid geometry fails closed"), Ability->ReleaseCinderJudgementFromAim());
		TestFalse(TEXT("Failed release ends the paid action"), Ability->IsActive());
		TestNull(TEXT("Failed release cannot publish a shot"), F.Presentation());
		TestEqual(TEXT("Committed payment is neither refunded nor duplicated"), Source->TestEcho->GetEcho(), 50.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinderJudgementSocketValidationTest,
	"ProjectVelkorran.Campaign.CinderJudgement.Geometry.SocketValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinderJudgementSocketValidationTest::RunTest(const FString& Parameters)
{
	for (int32 Mode = 0; Mode < 3; ++Mode)
	{
		FJudgementGeometryWorld F;
		auto* Source = F.Character(FVector::ZeroVector, 0);
		auto* Ability = ActivateJudgement(*this, Source);
		if (!Ability) { return false; }
		auto* Visual = F.World->SpawnActor<AWeaponVisual>();
		if (!Visual) { return false; }
		Visual->SetOwner(Source);
		auto* Mesh = NewObject<USovCinderJudgementGeometryMesh>(Visual);
		Visual->AddInstanceComponent(Mesh);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->RegisterComponent();
		Visual->WeaponMesh = Mesh;
		Ability->TestVisual = Visual;
		Mesh->TestSocket = FTransform(FVector(Mode == 1 ? 501.0 : 150.0, 0.0, 0.0));
		if (Mode == 2) { Mesh->TestSocket.SetRotation(FQuat(0.0, 0.0, 0.0, 0.0)); }
		TestTrue(TEXT("Socket or safe fallback resolves the shot"), Ability->ReleaseCinderJudgementFromAim());
		auto* Packet = F.Presentation();
		if (!Packet) { return false; }
		TestTrue(TEXT("Only a bounded, normalized socket may replace the fallback"),
			Packet->GetTraceStart().Equals(FVector(Mode == 0 ? 150.0 : 100.0, 0.0, 0.0), 0.1));
		Ability->FinishEchoAbility(true);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinderJudgementDisplacedEyeTest,
	"ProjectVelkorran.Campaign.CinderJudgement.Geometry.DisplacedEye",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinderJudgementDisplacedEyeTest::RunTest(const FString& Parameters)
{
	FJudgementGeometryWorld F;
	auto* Source = F.Character(FVector::ZeroVector, 0);
	auto* Wall = F.Wall(FVector(50.0, 0.0, 0.0), FVector(1.0, 300.0, 300.0));
	if (!Source || !Wall) { return false; }
	Source->TestEyeOffset = FVector(900.0, 0.0, 0.0);
	auto* Ability = ActivateJudgement(*this, Source);
	if (!Ability) { return false; }
	TestTrue(TEXT("Out-of-envelope camera safely falls back"), Ability->ReleaseCinderJudgementFromAim());
	auto* Packet = F.Presentation();
	if (!Packet) { return false; }
	TestTrue(TEXT("Displaced eye cannot bypass near geometry"), Packet->GetTraceStart().Equals(FVector::ZeroVector, 0.1));
	TestEqual(TEXT("Near wall remains authoritative"), Packet->GetHitActor(), Wall);
	Ability->FinishEchoAbility(true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinderJudgementDirectContinuationTest,
	"ProjectVelkorran.Campaign.CinderJudgement.Ownership.DirectHitRetiresOldContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinderJudgementDirectContinuationTest::RunTest(const FString& Parameters)
{
	for (const auto Mutation : {ESovJudgementDamageMutation::Cancel, ESovJudgementDamageMutation::Restart,
		ESovJudgementDamageMutation::AvatarABA, ESovJudgementDamageMutation::LifeABA})
	{
		FJudgementGeometryWorld F;
		auto* Source = F.Character(FVector::ZeroVector, 0);
		auto* Target = F.Character(FVector(500.0, 0.0, 0.0), 1);
		auto* Bystander = F.Character(FVector(500.0, 200.0, 0.0), 1);
		auto* Replacement = F.Character(FVector(3000.0, 0.0, 0.0), 0);
		if (!Target || !Bystander || !Replacement) { return false; }
		auto* Ability = ActivateJudgement(*this, Source);
		if (!Ability) { return false; }
		TStrongObjectPtr<USovCinderJudgementDamageProbe> Probe(NewObject<USovCinderJudgementDamageProbe>());
		Probe->SourceASC = Source->GetNarrativeAbilitySystemComponent();
		Probe->Replacement = Replacement; Probe->Handle = Ability->SpecForTest(); Probe->Mutation = Mutation;
		Target->GetNarrativeAbilitySystemComponent()->OnDamageResolvedAsTarget.AddDynamic(
			Probe.Get(), &USovCinderJudgementDamageProbe::DuringDamage);
		TestTrue(TEXT("Already committed direct hit stays successful"), Ability->ReleaseCinderJudgementFromAim());
		TestEqual(TEXT("Retired shot never delivers its radial packet to direct target"), Target->ResolvedHitCount, 1);
		TestEqual(TEXT("Retired shot cannot splash another target"), Bystander->ResolvedHitCount, 0);
		TestTrue(TEXT("Direct damage is not rolled back"), Target->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetShieldAttribute()) < 100.0f);
		TestNull(TEXT("Retired deferred presentation never publishes"), F.Presentation());
		if (Mutation == ESovJudgementDamageMutation::Restart)
		{
			TestTrue(TEXT("Damage callback creates a legitimate replacement action"), Probe->bRestartAccepted);
			TestTrue(TEXT("Old continuation cannot cancel the replacement"), Ability->IsActive());
			TestEqual(TEXT("Each activation paid exactly once"), Source->TestEcho->GetEcho(), 0.0f);
			F.World->Tick(LEVELTICK_All, 0.6f);
			TestTrue(TEXT("Old shot cannot arm recovery that ends the replacement"), Ability->IsActive());
			TestTrue(TEXT("Replacement owns its own fresh release"), Ability->ReleaseCinderJudgementFromAim());
			TestNotNull(TEXT("Only replacement may publish a packet"), F.Presentation());
			Ability->FinishEchoAbility(true);
		}
		else
		{
			TestFalse(TEXT("Retired original action releases its active state"), Ability->IsActive());
			TestEqual(TEXT("Cancellation/rebind never refunds or duplicates debit"), Source->TestEcho->GetEcho(), 50.0f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinderJudgementRadialContinuationTest,
	"ProjectVelkorran.Campaign.CinderJudgement.Ownership.RadialCallbackStopsLaterTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinderJudgementRadialContinuationTest::RunTest(const FString& Parameters)
{
	FJudgementGeometryWorld F;
	auto* Source = F.Character(FVector::ZeroVector, 0);
	auto* Target = F.Character(FVector(500.0, 0.0, 0.0), 1);
	auto* Bystander = F.Character(FVector(500.0, 200.0, 0.0), 1);
	if (!Target || !Bystander) { return false; }
	auto* Ability = ActivateJudgement(*this, Source);
	if (!Ability) { return false; }
	TStrongObjectPtr<USovCinderJudgementDamageProbe> Probe(NewObject<USovCinderJudgementDamageProbe>());
	Probe->SourceASC = Source->GetNarrativeAbilitySystemComponent(); Probe->Handle = Ability->SpecForTest();
	Probe->TriggerAfterHit = 2; // Direct target is first in the production radial candidate list.
	Target->GetNarrativeAbilitySystemComponent()->OnDamageResolvedAsTarget.AddDynamic(
		Probe.Get(), &USovCinderJudgementDamageProbe::DuringDamage);
	TestTrue(TEXT("Paid direct and first radial hit remain committed"), Ability->ReleaseCinderJudgementFromAim());
	TestEqual(TEXT("Direct target received its two legitimate packets"), Target->ResolvedHitCount, 2);
	TestEqual(TEXT("Canceled radial iteration cannot reach another candidate"), Bystander->ResolvedHitCount, 0);
	TestNull(TEXT("Partial retired blast cannot publish a complete-shot packet"), F.Presentation());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCinderJudgementDestructibleCoverTest,
	"ProjectVelkorran.World.Destruction.CinderJudgementDirectHitBreaksCover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCinderJudgementDestructibleCoverTest::RunTest(const FString& Parameters)
{
	FJudgementGeometryWorld F;
	auto* Source = F.Character(FVector::ZeroVector, 0);
	auto* Target = F.Character(FVector(220.0, 0.0, 0.0), 1);
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Cover = F.World ? F.World->SpawnActor<ASovDestructibleCover>(ASovDestructibleCover::StaticClass(), FVector(50.0, 0.0, 0.0), FRotator::ZeroRotator, Spawn) : nullptr;
	if (!Target || !Cover) { return false; }
	Cover->Obstruction->SetBoxExtent(FVector(1.0, 300.0, 300.0)); Cover->PlacementGuid = FGuid(0xC1DE, 1, 2, 3);
	Cover->FracturedAsset = NewObject<UGeometryCollection>(Cover); Cover->bDestructionEnabled = true; Cover->RemainingHealth = 1.0f;
	auto* Ability = ActivateJudgement(*this, Source);
	if (!Ability) { return false; }
	TestTrue(TEXT("Paid shot resolves against the cover"), Ability->ReleaseCinderJudgementFromAim());
	auto* Packet = F.Presentation();
	if (!TestNotNull(TEXT("Native presentation packet"), Packet)) { return false; }
	TestEqual(TEXT("Cover is the resolved hit"), Packet->GetHitActor(), static_cast<AActor*>(Cover));
	TestTrue(TEXT("Direct Judgement damage breaks the cover"), Cover->IsBroken());
	TestEqual(TEXT("Characters resolved before the break receive nothing through it"), Target->ResolvedHitCount, 0);
	TestEqual(TEXT("One existing Echo payment"), Source->TestEcho->GetEcho(), 50.0f);
	Ability->FinishEchoAbility(true);
	return true;
}

#endif
