// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovSelenePayloadTestFixtures.h"
#include "World/SovDestructibleCover.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovRuntimeActorTestFixtures.h"
#include "Combat/SovSelenePayload.h"
#include "Components/BoxComponent.h"
#include "Components/SovDeflectionComponent.h"
#include "Components/SovEchoComponent.h"
#include "Components/SovStatusComponent.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Projectiles/SovSeleneCombatProjectile.h"
#include "Sovereign/SovGameplayTags.h"

#if WITH_AUTOMATION_TESTS
struct FSovSelenePayloadTestAccess
{
	static void Field(ASovSeleneCombatProjectile& Projectile) { Projectile.ApplyField(); }
	static int32 OutboundCount(const ASovSeleneCombatProjectile& Projectile) { return Projectile.OutboundTargets.Num(); }
	static int32 ReturnCount(const ASovSeleneCombatProjectile& Projectile) { return Projectile.ReturnTargets.Num(); }
};
namespace SovSelenePayloadTests
{
	struct FSeleneTestWorld
	{
		UWorld* World = nullptr;
		FSeleneTestWorld()
		{
			const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &WorldInitialization);
			if (World)
			{
				if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			}
		}
		~FSeleneTestWorld()
		{
			if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		}
		ASovAxiomRuntimeTestCharacter* Character(FVector Location, int32 Team = 1)
		{
			FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Actor = World ? World->SpawnActor<ASovAxiomRuntimeTestCharacter>(
				ASovAxiomRuntimeTestCharacter::StaticClass(), Location, FRotator::ZeroRotator, Spawn) : nullptr;
			if (Actor) { Actor->InitializeTestCombat(Team); }
			return Actor;
		}
		void Wall(FVector Location, FVector Extent)
		{
			auto* Actor = World->SpawnActor<AActor>();
			auto* Box = NewObject<UBoxComponent>(Actor); Actor->AddInstanceComponent(Box); Actor->SetRootComponent(Box);
			Box->SetBoxExtent(Extent); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent(); Actor->SetActorLocation(Location);
		}
		ASovSeleneCombatProjectile* Projectile()
		{
			for (TActorIterator<ASovSeleneCombatProjectile> It(World); It; ++It)
			{
				if (!It->IsActorBeingDestroyed()) { return *It; }
			}
			return nullptr;
		}
	};
	FSovSelenePayloadContext Context(ASovAxiomRuntimeTestCharacter* Source, FGameplayTag Tag)
	{
		FSovSelenePayloadContext Result;
		Result.SourceAvatar = Source; Result.SourceASC = Source->GetNarrativeAbilitySystemComponent(); Result.AbilityTag = Tag;
		return Result;
	}
	template<class T> T* Activate(FAutomationTestBase& Test, ASovAxiomRuntimeTestCharacter* Source)
	{
		auto* ASC = Source->GetNarrativeAbilitySystemComponent();
		FGameplayAbilitySpec Spec(T::StaticClass(), 1, INDEX_NONE, Source->SetTestWeapon());
		Spec.InputPressed = true;
		const auto Handle = ASC->GiveAbility(Spec);
		if (!Test.TestTrue(TEXT("Real paid GAS activation"), ASC->TryActivateAbility(Handle))) { return nullptr; }
		const auto* Granted = ASC->FindAbilitySpecFromHandle(Handle);
		return Granted ? Cast<T>(Granted->GetPrimaryInstance()) : nullptr;
	}
	float Shield(ASovAxiomRuntimeTestCharacter* Target)
	{
		return Target->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSeleneZeroNativeTest, "ProjectVelkorran.Campaign.SelenePayload.ZeroPaidPrecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSeleneZeroNativeTest::RunTest(const FString& Parameters)
{
	SovSelenePayloadTests::FSeleneTestWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Target = Fixture.Character(FVector(1000.0f, 0.0f, 0.0f));
	if (!TestNotNull(TEXT("Source"), Source) || !TestNotNull(TEXT("Target"), Target)) { return false; }
	auto* Ability = SovSelenePayloadTests::Activate<USovZeroPayloadTestAbility>(*this, Source);
	if (!TestNotNull(TEXT("Native Zero instance"), Ability)) { return false; }
	TestEqual(TEXT("One Echo spend"), Source->TestEcho->GetEcho(), 70.0f);
	TestFalse(TEXT("Native shot closes lifecycle"), Ability->IsActive());
	TestEqual(TEXT("Exactly one direct damage transaction"), Target->ResolvedHitCount, 1);
	TestTrue(TEXT("Damage scalar is applied once"), FMath::IsNearlyEqual(Target->LastDamageResult.BaseDamage, 60.0f)
		&& FMath::IsNearlyEqual(Target->LastDamageResult.ResolvedDamage, 105.0f));
	TestTrue(TEXT("Accepted hit freezes deterministically"), SovSelenePayload::HasStatus(Target, FSovGameplayTags::Get().State_Status_Frozen));
	TestTrue(TEXT("Freeze owns a movement lock"), SovSelenePayload::HasStatus(Target, FNarrativeGameplayTags::Get().State_Movement_Lock));
	TestTrue(TEXT("Freeze owns refreeze protection"), SovSelenePayload::HasStatus(Target, FSovGameplayTags::Get().Status_Immunity_Freeze));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSeleneDefenseReceiptTest, "ProjectVelkorran.Campaign.SelenePayload.DefenseAndImmunity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSeleneDefenseReceiptTest::RunTest(const FString& Parameters)
{
	SovSelenePayloadTests::FSeleneTestWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Target = Fixture.Character(FVector(200.0f, 0.0f, 0.0f));
	if (!Source || !Target) { AddError(TEXT("Fixture creation failed")); return false; }
	const auto& Tags = FSovGameplayTags::Get();
	auto Ctx = SovSelenePayloadTests::Context(Source, Tags.Ability_Echo_Selene_StaccatoZero);
	auto* ASC = Target->GetNarrativeAbilitySystemComponent();
	ASC->AddLooseGameplayTag(Tags.Damage_Immunity_Thermal);
	TestTrue(TEXT("Thermal immunity alone preserves Echo vulnerability"), SovSelenePayload::EligibleTarget(Ctx, Target));
	ASC->AddLooseGameplayTag(Tags.Damage_Immunity_Echo);
	TestFalse(TEXT("All declared channels immune rejects payload"), SovSelenePayload::EligibleTarget(Ctx, Target));
	ASC->RemoveLooseGameplayTag(Tags.Damage_Immunity_Echo);
	ASC->AddLooseGameplayTag(Tags.Status_Immunity_Freeze);
	TestFalse(TEXT("Resistant target receives Chill instead of Freeze"), SovSelenePayload::Control(Ctx, Target, 3.0f, 3.0f, true));
	TestTrue(TEXT("Freeze immunity does not suppress Chill"), ASC->HasMatchingGameplayTag(Tags.State_Status_Chilled));
	Target->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
	ASC->AddLooseGameplayTag(Tags.State_Deflecting);
	const float ShieldBefore = SovSelenePayloadTests::Shield(Target);
	TestFalse(TEXT("Perfect defense receipt rejects control"), SovSelenePayload::Damage(Ctx, Target, nullptr, 60.0f, 10.0f, 1.75f, true));
	TestEqual(TEXT("Perfect deflection negates damage"), SovSelenePayloadTests::Shield(Target), ShieldBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSeleneStillpointFieldTest, "ProjectVelkorran.Campaign.SelenePayload.StillpointFieldAndLateEntrants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSeleneStillpointFieldTest::RunTest(const FString& Parameters)
{
	SovSelenePayloadTests::FSeleneTestWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Target = Fixture.Character(FVector(600.0f, 0.0f, 0.0f));
	auto* Late = Fixture.Character(FVector(1500.0f, 200.0f, 0.0f));
	if (!Source || !Target || !Late) { AddError(TEXT("Fixture creation failed")); return false; }
	FSovSeleneProjectileParameters Data;
	Data.Context = SovSelenePayloadTests::Context(Source, FSovGameplayTags::Get().Ability_Echo_Selene_StillpointGrenade);
	Data.Fuse = 0.0f; Data.DamagePerSecond = 12.0f;
	auto* Field = ASovSeleneCombatProjectile::SpawnNativePayload(nullptr, FVector(500.0f, 0.0f, 0.0f), Data);
	if (!TestNotNull(TEXT("Native grenade needs no Blueprint projectile"), Field)) { return false; }
	Field->Tick(0.05f);
	TestTrue(TEXT("Fuse opens native field"), Field->GetPayloadPhase() == ESovSeleneProjectilePhase::Field);
	TestTrue(TEXT("Nearby target Frozen"), SovSelenePayload::HasStatus(Target, FSovGameplayTags::Get().State_Status_Frozen));
	TestFalse(TEXT("Distant actor untouched"), SovSelenePayload::HasStatus(Late, FSovGameplayTags::Get().State_Status_Frozen));
	const int32 FreezeCount = Target->GetNarrativeAbilitySystemComponent()->GetGameplayTagCount(FSovGameplayTags::Get().State_Status_Frozen);
	Late->SetActorLocation(FVector(600.0f, 180.0f, 0.0f));
	Field->Tick(0.15f);
	TestTrue(TEXT("Late entrant receives remaining field control"), SovSelenePayload::HasStatus(Late, FSovGameplayTags::Get().State_Status_Frozen));
	TestEqual(TEXT("Persistent overlap never reapplies Freeze"), Target->GetNarrativeAbilitySystemComponent()->GetGameplayTagCount(FSovGameplayTags::Get().State_Status_Frozen), FreezeCount);
	TestEqual(TEXT("Canonical targets entered ledger once"), FSovSelenePayloadTestAccess::OutboundCount(*Field), 2);
	Source->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_IsDead);
	Field->Tick(0.1f);
	TestTrue(TEXT("Source death terminates pending field"), Field->IsActorBeingDestroyed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSeleneWakeLaneTest, "ProjectVelkorran.Campaign.SelenePayload.WakeLaneAndWall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSeleneWakeLaneTest::RunTest(const FString& Parameters)
{
	SovSelenePayloadTests::FSeleneTestWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Center = Fixture.Character(FVector(500.0f, 0.0f, 0.0f));
	auto* Flank = Fixture.Character(FVector(500.0f, 180.0f, 0.0f));
	auto* Hidden = Fixture.Character(FVector(1000.0f, 0.0f, 0.0f));
	if (!Source || !Center || !Flank || !Hidden) { AddError(TEXT("Fixture creation failed")); return false; }
	Fixture.Wall(FVector(750.0f, 0.0f, 0.0f), FVector(25.0f, 600.0f, 200.0f));
	FSovSeleneProjectileParameters Data;
	Data.Context = SovSelenePayloadTests::Context(Source, FSovGameplayTags::Get().Ability_Echo_Selene_VeritysWake);
	Data.Mode = ESovSeleneProjectileMode::Wake; Data.Radius = 250.0f; Data.Damage = 50.0f; Data.Speed = 2200.0f;
	auto* Wave = ASovSeleneCombatProjectile::SpawnNativePayload(nullptr, FVector::ZeroVector, Data);
	if (!Wave) { AddError(TEXT("Wave creation failed")); return false; }
	Wave->Tick(0.5f);
	TestEqual(TEXT("Center receives one hit"), Center->ResolvedHitCount, 1);
	TestEqual(TEXT("Flank receives one hit"), Flank->ResolvedHitCount, 1);
	TestTrue(TEXT("Centerline freezes"), SovSelenePayload::HasStatus(Center, FSovGameplayTags::Get().State_Status_Frozen));
	TestTrue(TEXT("Outer lane chills"), SovSelenePayload::HasStatus(Flank, FSovGameplayTags::Get().State_Status_Chilled));
	TestFalse(TEXT("Outer lane does not guarantee freeze"), SovSelenePayload::HasStatus(Flank, FSovGameplayTags::Get().State_Status_Frozen));
	TestEqual(TEXT("Wall shields downstream target"), Hidden->ResolvedHitCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSeleneDispatchLedgerTest, "ProjectVelkorran.Campaign.SelenePayload.DispatchRecallAndCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSeleneDispatchLedgerTest::RunTest(const FString& Parameters)
{
	SovSelenePayloadTests::FSeleneTestWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Target = Fixture.Character(FVector(350.0f, 0.0f, 0.0f));
	if (!Source || !Target) { AddError(TEXT("Fixture creation failed")); return false; }
	auto* Ability = SovSelenePayloadTests::Activate<USovDispatchPayloadTestAbility>(*this, Source);
	auto* Projectile = Fixture.Projectile();
	if (!TestNotNull(TEXT("Native Dispatch instance"), Ability) || !TestNotNull(TEXT("Returning Verity"), Projectile)) { return false; }
	TestEqual(TEXT("Signature spends 90 Echo"), Source->TestEcho->GetEcho(), 10.0f);
	TestTrue(TEXT("Active signature owns Verity absence"), Source->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Weapon_VerityAbsent));
	Projectile->Tick(0.3f);
	TestEqual(TEXT("Outbound path applies one target hit"), Target->ResolvedHitCount, 1);
	TestTrue(TEXT("Recall changes existing projectile"), Projectile->Recall());
	TestFalse(TEXT("Recall cannot start twice"), Projectile->Recall());
	Projectile->Tick(0.3f);
	TestEqual(TEXT("Same target can be hit once on return"), Target->ResolvedHitCount, 2);
	TestEqual(TEXT("Recall spends no Echo"), Source->TestEcho->GetEcho(), 10.0f);
	TestFalse(TEXT("Return closes ability"), Ability->IsActive());
	TestFalse(TEXT("Return clears only owned absence effect"), Source->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Weapon_VerityAbsent));
	Source->TestEcho->RestoreEchoFromCheckpoint(100.0f);
	auto* Second = SovSelenePayloadTests::Activate<USovDispatchPayloadTestAbility>(*this, Source);
	if (!Second) { return false; }
	Second->FinishEchoAbility(true);
	TestNull(TEXT("Cancellation destroys returning payload"), Fixture.Projectile());
	TestFalse(TEXT("Cancellation restores availability"), Source->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Weapon_VerityAbsent));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSeleneNativeLaunchTest, "ProjectVelkorran.Campaign.SelenePayload.NativeLaunchDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSeleneNativeLaunchTest::RunTest(const FString& Parameters)
{
	SovSelenePayloadTests::FSeleneTestWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	if (!Source) { AddError(TEXT("Fixture creation failed")); return false; }
	auto* Stillpoint = SovSelenePayloadTests::Activate<USovStillpointPayloadTestAbility>(*this, Source);
	if (!Stillpoint) { return false; }
	TestEqual(TEXT("Stillpoint spends 35 with native defaults"), Source->TestEcho->GetEcho(), 65.0f);
	TestFalse(TEXT("Throw closes action while grenade persists"), Stillpoint->IsActive());
	auto* Grenade = Fixture.Projectile();
	if (!TestNotNull(TEXT("Native initialized grenade exists"), Grenade)) { return false; }
	Grenade->Destroy();
	Source->TestEcho->RestoreEchoFromCheckpoint(100.0f);
	auto* Wake = SovSelenePayloadTests::Activate<USovWakePayloadTestAbility>(*this, Source);
	if (!Wake) { return false; }
	TestEqual(TEXT("Wake spends 30 with native defaults"), Source->TestEcho->GetEcho(), 70.0f);
	TestFalse(TEXT("Wave release closes action"), Wake->IsActive());
	TestNotNull(TEXT("Native initialized wave exists"), Fixture.Projectile());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSeleneDispatchQueuedInputTest, "ProjectVelkorran.Campaign.SelenePayload.DispatchQueuedGASRecall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSeleneDispatchQueuedInputTest::RunTest(const FString& Parameters)
{
	SovSelenePayloadTests::FSeleneTestWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	if (!Source) { AddError(TEXT("Fixture creation failed")); return false; }
	auto* ASC = Source->GetNarrativeAbilitySystemComponent();
	// WaitInputPress replays queued events only for an authority serving a remote
	// player. An unpossessed authority pawn is locally controlled in GAS.
	auto* RemotePC = Fixture.World->SpawnActor<ASovRuntimeTestPlayerController>();
	if (!TestNotNull(TEXT("Remote ownership controller"), RemotePC)) { return false; }
	RemotePC->Possess(Source);
	ASC->InitAbilityActorInfo(Source, Source);
	if (!TestTrue(TEXT("Authority exercises the actual queued remote-input branch"),
		ASC->AbilityActorInfo->IsNetAuthority() && !ASC->AbilityActorInfo->IsLocallyControlled())) { return false; }
	FGameplayAbilitySpec Spec(USovDispatchPayloadTestAbility::StaticClass(), 1, INDEX_NONE, Source->SetTestWeapon());
	Spec.InputPressed = true;
	const auto Handle = ASC->GiveAbility(Spec);
	// Remote input can reach GAS before the authority's WaitInputPress task binds.
	// This follows the same generic replicated-event channel used by Narrative input.
	FPredictionKey RemotePrediction; RemotePrediction.Current = 1; // received client activation key
	ASC->InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Handle, RemotePrediction);
	if (!TestTrue(TEXT("Paid activation consumes queued input safely"), ASC->InternalTryActivateAbility(Handle, RemotePrediction))) { return false; }
	auto* Projectile = Fixture.Projectile();
	if (!TestNotNull(TEXT("Exactly one initialized returning actor"), Projectile)) { return false; }
	TestTrue(TEXT("Early queued recall is applied after actor assignment"), Projectile->GetPayloadPhase() == ESovSeleneProjectilePhase::Recalling);
	TestEqual(TEXT("Queued recall still spends only once"), Source->TestEcho->GetEcho(), 10.0f);
	const auto* Granted = ASC->FindAbilitySpecFromHandle(Handle);
	auto* Ability = Granted ? Cast<USovDispatchPayloadTestAbility>(Granted->GetPrimaryInstance()) : nullptr;
	if (!Ability) { AddError(TEXT("Dispatch instance missing")); return false; }
	Ability->FinishEchoAbility(true);
	// A late event for the ended activation has no task or projectile to mutate.
	ASC->InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Handle,
		Ability->GetCurrentActivationInfoRef().GetActivationPredictionKey());
	TestNull(TEXT("Late ended recall cannot spawn another weapon"), Fixture.Projectile());
	TestEqual(TEXT("Late ended recall cannot spend Echo"), Source->TestEcho->GetEcho(), 10.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSeleneFrozenDOTTest, "ProjectVelkorran.Campaign.SelenePayload.FrozenDOTStopsOnThaw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSeleneFrozenDOTTest::RunTest(const FString& Parameters)
{
	SovSelenePayloadTests::FSeleneTestWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Frozen = Fixture.Character(FVector(300.0f, 0.0f, 0.0f));
	auto* Thawed = Fixture.Character(FVector(300.0f, 200.0f, 0.0f));
	if (!Source || !Frozen || !Thawed) { AddError(TEXT("Fixture creation failed")); return false; }
	const auto Ctx = SovSelenePayloadTests::Context(Source, FSovGameplayTags::Get().Ability_Echo_Selene_StillpointGrenade);
	SovSelenePayload::Control(Ctx, Frozen, 3.5f, 3.0f, true);
	SovSelenePayload::Control(Ctx, Thawed, 3.5f, 3.0f, true);
	SovSelenePayload::FrostDOT(Ctx, Frozen, 12.0f, 3.5f, true);
	SovSelenePayload::FrostDOT(Ctx, Thawed, 12.0f, 3.5f, true);
	FGameplayTagContainer Freeze(FSovGameplayTags::Get().State_Status_Frozen);
	Thawed->GetNarrativeAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(Freeze);
	TestFalse(TEXT("Test thaw removed Frozen"), SovSelenePayload::HasStatus(Thawed, FSovGameplayTags::Get().State_Status_Frozen));
	TestTrue(TEXT("Thaw preserves independently owned refreeze lockout"), SovSelenePayload::HasStatus(Thawed, FSovGameplayTags::Get().Status_Immunity_Freeze));
	// Automation runs synchronously inside one engine frame; advance this isolated
	// world's timer frames explicitly, then restore the outer frame counter.
	{
		TGuardValue<uint64> FrameGuard(GFrameCounter, GFrameCounter + 1);
		Fixture.World->Tick(LEVELTICK_All, 0.01f);
		const double PeriodStarted = Fixture.World->GetTimeSeconds();
		// WorldSettings clamps a single 1.1-second frame below the DOT period.
		for (int32 Frame = 0; Frame < 22; ++Frame)
		{
			++GFrameCounter; Fixture.World->Tick(LEVELTICK_All, .05f);
		}
		TestTrue(TEXT("Actual world time crosses the periodic damage interval"), Fixture.World->GetTimeSeconds() - PeriodStarted > 1.0);
	}
	TestTrue(TEXT("Frozen target receives actual periodic damage"), SovSelenePayloadTests::Shield(Frozen) < 100.0f);
	TestEqual(TEXT("Thawed target receives no frozen-only damage"), SovSelenePayloadTests::Shield(Thawed), 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNativeAndGenericStatusOwnershipTest,
	"ProjectVelkorran.Campaign.Status.NativeAndGenericOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNativeAndGenericStatusOwnershipTest::RunTest(const FString& Parameters)
{
	SovSelenePayloadTests::FSeleneTestWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Target = Fixture.Character(FVector(200.0f, 0.0f, 0.0f));
	if (!Source || !Target) { AddError(TEXT("Fixture creation failed")); return false; }
	auto* ASC = Target->GetNarrativeAbilitySystemComponent();
	auto* Status = NewObject<USovStatusComponent>(Target);
	Target->AddInstanceComponent(Status); Status->RegisterComponent();
	if (!TestTrue(TEXT("Generic status listener initialized"), Status->InitializeWithAbilitySystem(ASC))) { return false; }
	const auto& Tags = FSovGameplayTags::Get();
	auto Ctx = SovSelenePayloadTests::Context(Source, Tags.Ability_Echo_Selene_StaccatoZero);
	TestTrue(TEXT("Native damage accepts its control receipt"), SovSelenePayload::Damage(Ctx, Target, nullptr, 1.0f, 0.0f, 1.0f, true));
	TestFalse(TEXT("Receipt does not prematurely apply generic Freeze"), Status->HasActiveStatus(Tags.Status_Apply_Freeze));
	TestFalse(TEXT("Receipt does not also apply generic Chill"), Status->HasActiveStatus(Tags.Status_Apply_Chill));
	TestTrue(TEXT("Native payload can apply its own Freeze"), SovSelenePayload::Control(Ctx, Target, 2.0f, 1.0f, true));
	TestEqual(TEXT("Freeze tag is owned exactly once"), ASC->GetGameplayTagCount(Tags.State_Status_Frozen), 1);
	TestFalse(TEXT("Successful Freeze does not manufacture a Chill fallback"), ASC->HasMatchingGameplayTag(Tags.State_Status_Chilled));
	auto* SourceASC = Source->GetNarrativeAbilitySystemComponent();
	FGameplayEffectSpec Generic(GetDefault<USovCombatRoutingTestEffect>(), SourceASC->MakeEffectContext(), 1.0f);
	Generic.AddDynamicAssetTag(Tags.Damage_Channel_Disruption);
	Generic.AddDynamicAssetTag(Tags.Status_Apply_Chill);
	Generic.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, 1.0f);
	SourceASC->ApplyGameplayEffectSpecToTarget(Generic, ASC);
	TestTrue(TEXT("Ordinary damage still reaches the generic status listener"), Status->HasActiveStatus(Tags.Status_Apply_Chill));
	ASC->AddLooseGameplayTag(Tags.Status_Immunity_All);
	TestFalse(TEXT("Explicit all-status immunity rejects native control receipts"), SovSelenePayload::Damage(Ctx, Target, nullptr, 1.0f, 0.0f, 1.0f, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSeleneWakeDestructibleCoverTest, "ProjectVelkorran.World.Destruction.SeleneWakeBreaksCoverWithoutPassing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSeleneWakeDestructibleCoverTest::RunTest(const FString& Parameters)
{
	SovSelenePayloadTests::FSeleneTestWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Hidden = Fixture.Character(FVector(1000.0f, 0.0f, 0.0f));
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Cover = Fixture.World->SpawnActor<ASovDestructibleCover>(ASovDestructibleCover::StaticClass(), FVector(750.0f, 0.0f, 0.0f), FRotator::ZeroRotator, Spawn);
	if (!Source || !Hidden || !Cover) { AddError(TEXT("Fixture creation failed")); return false; }
	Cover->Obstruction->SetBoxExtent(FVector(25.0f, 600.0f, 200.0f)); Cover->PlacementGuid = FGuid(0x5E1E, 1, 2, 3);
	Cover->FracturedAsset = NewObject<UGeometryCollection>(Cover); Cover->bDestructionEnabled = true; Cover->RemainingHealth = 40.0f;
	FSovSeleneProjectileParameters Data;
	Data.Context = SovSelenePayloadTests::Context(Source, FSovGameplayTags::Get().Ability_Echo_Selene_VeritysWake);
	Data.Mode = ESovSeleneProjectileMode::Wake; Data.Radius = 250.0f; Data.Damage = 50.0f; Data.Speed = 2200.0f;
	auto* Wave = ASovSeleneCombatProjectile::SpawnNativePayload(nullptr, FVector::ZeroVector, Data);
	if (!Wave) { AddError(TEXT("Wave creation failed")); return false; }
	Wave->Tick(0.5f);
	TestTrue(TEXT("Wake base damage breaks the cover at its wall"), Cover->IsBroken());
	TestEqual(TEXT("The stopped segment does not pass through the new opening"), Hidden->ResolvedHitCount, 0);
	return true;
}

#endif
