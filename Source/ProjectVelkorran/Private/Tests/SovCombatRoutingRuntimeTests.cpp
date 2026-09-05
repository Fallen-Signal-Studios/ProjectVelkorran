// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Components/SovDismembermentComponent.h"
#include "Components/SovEchoComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

#if WITH_AUTOMATION_TESTS
namespace
{
	struct FCombatRoutingWorld
	{
		UWorld* World = nullptr;
		FCombatRoutingWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (World)
			{
				if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
				World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false)
					.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
					.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
			}
		}
		~FCombatRoutingWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		ASovAxiomRuntimeTestCharacter* Character(float X, int32 Team)
		{
			if (!World) { return nullptr; }
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Actor = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(
				ASovAxiomRuntimeTestCharacter::StaticClass(), FVector(X, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
			if (Actor) { Actor->InitializeTestCombat(Team); }
			return Actor;
		}
	};
	void Attack(ASovAxiomRuntimeTestCharacter* Source, ASovAxiomRuntimeTestCharacter* Target,
		float Body, float Poise, bool bStatus = false, FName Bone = NAME_None)
	{
		auto* SourceASC = Source->GetNarrativeAbilitySystemComponent();
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(Source, Source);
		if (!Bone.IsNone())
		{
			FHitResult Hit; Hit.BoneName = Bone; Context.AddHitResult(Hit);
		}
		FGameplayEffectSpec Spec(GetDefault<USovCombatRoutingTestEffect>(), Context, 1.f);
		Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, Body);
		Spec.SetSetByCallerMagnitude(FSovGameplayTags::Get().SetByCaller_Damage_PoiseDamage, Poise);
		Spec.AddDynamicAssetTag(FSovGameplayTags::Get().Damage_Channel_Disruption);
		if (bStatus) { Spec.AddDynamicAssetTag(FSovGameplayTags::Get().Status_Apply_Freeze); }
		SourceASC->ApplyGameplayEffectSpecToTarget(Spec, Target->GetNarrativeAbilitySystemComponent());
	}
	USovWeakPointRoutingTestComponent* AddWeakPoints(ASovAxiomRuntimeTestCharacter* Target)
	{
		auto* WeakPoints = NewObject<USovWeakPointRoutingTestComponent>(Target);
		Target->AddInstanceComponent(WeakPoints);
		WeakPoints->RegisterComponent();
		WeakPoints->InitializeWithAbilitySystem(Target->GetNarrativeAbilitySystemComponent());
		WeakPoints->Observe();
		return WeakPoints;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPurePoiseRoutingTest,
	"ProjectVelkorran.Campaign.Defense.PurePoise", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPurePoiseRoutingTest::RunTest(const FString& Parameters)
{
	FCombatRoutingWorld Fixture;
	auto* Source = Fixture.Character(100.f, 0);
	auto* Target = Fixture.Character(0.f, 1);
	if (!TestNotNull(TEXT("Source"), Source) || !TestNotNull(TEXT("Target"), Target)) { return false; }
	auto* ASC = Target->GetNarrativeAbilitySystemComponent();
	const auto& Tags = FSovGameplayTags::Get();
	Attack(Source, Target, 0.f, 40.f);
	TestEqual(TEXT("Zero-body attack delivers Poise through one typed transaction"), Target->ResolvedHitCount, 1);
	TestEqual(TEXT("Requested Poise is recorded"), Target->LastDamageResult.RequestedPoiseDamage, 40.f);
	TestEqual(TEXT("Actual Poise drains"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetPoiseAttribute()), 60.f);
	TestEqual(TEXT("Shield untouched"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()), 100.f);
	TestEqual(TEXT("Health untouched"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 100.f);
	ASC->AddLooseGameplayTag(Tags.State_Guarding);
	Attack(Source, Target, 0.f, 40.f);
	TestTrue(TEXT("Pure Poise honors Guard"), Target->LastDamageResult.bGuarded);
	TestEqual(TEXT("Guard mitigates Poise by existing multiplier"), Target->LastDamageResult.AppliedPoiseDamage, 10.f);
	ASC->AddLooseGameplayTag(Tags.State_PerfectGuard);
	Attack(Source, Target, 0.f, 40.f);
	TestTrue(TEXT("Perfect Guard rejects pure Poise"), Target->LastDamageResult.bPerfectDefense);
	TestEqual(TEXT("Perfect defense prevents Poise"), Target->LastDamageResult.AppliedPoiseDamage, 0.f);
	ASC->RemoveLooseGameplayTag(Tags.State_Guarding);
	ASC->RemoveLooseGameplayTag(Tags.State_PerfectGuard);
	ASC->AddLooseGameplayTag(Tags.State_Deflecting);
	Attack(Source, Target, 0.f, 40.f);
	TestTrue(TEXT("Pure Poise honors Deflection"), Target->LastDamageResult.bDeflected);
	ASC->RemoveLooseGameplayTag(Tags.State_Deflecting);
	const int32 BeforeImmune = Target->ResolvedHitCount;
	ASC->AddLooseGameplayTag(Tags.Damage_Immunity_Disruption);
	Attack(Source, Target, 0.f, 40.f);
	TestEqual(TEXT("Channel immunity rejects pure Poise before telemetry"), Target->ResolvedHitCount, BeforeImmune);
	FGameplayEffectSpec Direct(GetDefault<USovCombatDirectPoiseTestEffect>(), Source->GetNarrativeAbilitySystemComponent()->MakeEffectContext(), 1.f);
	Direct.AddDynamicAssetTag(Tags.Damage_Channel_Disruption);
	Source->GetNarrativeAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(Direct, ASC);
	TestEqual(TEXT("Direct Poise meta effects use identical channel immunity gate"), Target->ResolvedHitCount, BeforeImmune);
	ASC->RemoveLooseGameplayTag(Tags.Damage_Immunity_Disruption);
	Target->TestTeam = 0;
	Attack(Source, Target, 0.f, 40.f);
	TestEqual(TEXT("Friendly policy applies to pure Poise"), Target->ResolvedHitCount, BeforeImmune);
	Target->TestTeam = 1;
	ASC->AddLooseGameplayTag(Tags.State_Poise_Recovering);
	Attack(Source, Target, 0.f, 80.f);
	TestFalse(TEXT("Recovery immunity prevents repeated break"), Target->LastDamageResult.bPoiseBroken);
	TestEqual(TEXT("Existing recovery floor preserved"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetPoiseAttribute()), 1.f);
	TestTrue(TEXT("Result captures target state at transaction entry"), Target->LastDamageResult.TargetTagsBeforeDamage.HasTagExact(Tags.State_Poise_Recovering));
	ASC->RemoveLooseGameplayTag(Tags.State_Poise_Recovering);
	Attack(Source, Target, 0.f, 80.f);
	TestTrue(TEXT("Pure Poise emits normal break result after recovery ends"), Target->LastDamageResult.bPoiseBroken);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovControlRoutingTest,
	"ProjectVelkorran.Campaign.Defense.ControlOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovControlRoutingTest::RunTest(const FString& Parameters)
{
	FCombatRoutingWorld Fixture;
	auto* Source = Fixture.Character(100.f, 0);
	auto* Target = Fixture.Character(0.f, 1);
	if (!Source || !Target) { AddError(TEXT("Fixture actors failed")); return false; }
	auto* ASC = Target->GetNarrativeAbilitySystemComponent();
	const auto& Tags = FSovGameplayTags::Get();
	Attack(Source, Target, 0.f, 0.f, true);
	TestTrue(TEXT("Control request survives without artificial body/Poise damage"), Target->LastDamageResult.bStatusApplicationRequested);
	TestEqual(TEXT("Control produces one typed result"), Target->ResolvedHitCount, 1);
	TestEqual(TEXT("Health unchanged"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 100.f);
	TestEqual(TEXT("Shield unchanged"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()), 100.f);
	TestEqual(TEXT("Poise unchanged"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetPoiseAttribute()), 100.f);
	ASC->AddLooseGameplayTag(Tags.State_Guarding);
	Attack(Source, Target, 0.f, 0.f, true);
	TestTrue(TEXT("Guard registers control defense"), Target->LastDamageResult.bGuarded);
	TestFalse(TEXT("Guard blocks status-only request"), Target->LastDamageResult.bStatusApplicationRequested);
	ASC->RemoveLooseGameplayTag(Tags.State_Guarding);
	ASC->AddLooseGameplayTag(Tags.State_Deflecting);
	Attack(Source, Target, 0.f, 0.f, true);
	TestTrue(TEXT("Deflection registers control defense"), Target->LastDamageResult.bDeflected);
	TestFalse(TEXT("Deflection blocks status-only request"), Target->LastDamageResult.bStatusApplicationRequested);
	ASC->RemoveLooseGameplayTag(Tags.State_Deflecting);
	ASC->AddLooseGameplayTag(Tags.State_Invulnerable);
	const int32 Before = Target->ResolvedHitCount;
	Attack(Source, Target, 0.f, 0.f, true);
	TestEqual(TEXT("Invulnerability rejects control before transaction"), Target->ResolvedHitCount, Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWeakPointConsequenceTest,
	"ProjectVelkorran.Campaign.WeakPoint.ConsequenceOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWeakPointConsequenceTest::RunTest(const FString& Parameters)
{
	FCombatRoutingWorld Fixture;
	auto* Source = Fixture.Character(100.f, 0);
	auto* Target = Fixture.Character(0.f, 1);
	if (!Source || !Target) { AddError(TEXT("Fixture actors failed")); return false; }
	auto* ASC = Target->GetNarrativeAbilitySystemComponent();
	auto* WeakPoints = AddWeakPoints(Target);
	const auto Fire = ASC->GiveAbility(FGameplayAbilitySpec(USovWeakPointFireTestAbility::StaticClass(), 1));
	const auto Melee = ASC->GiveAbility(FGameplayAbilitySpec(USovWeakPointMeleeTestAbility::StaticClass(), 1));
	TestTrue(TEXT("Fire begins before break"), ASC->TryActivateAbility(Fire));
	Source->TestEcho->RestoreEchoFromCheckpoint(0.f);
	Attack(Source, Target, 5.f, 0.f, false, TEXT("weapon"));
	TestTrue(TEXT("Real hit breaks authored weapon zone"), WeakPoints->IsWeakPointBroken(TEXT("Weapon")));
	TestFalse(TEXT("Broken capability cancels active fire"), ASC->FindAbilitySpecFromHandle(Fire)->IsActive());
	TestFalse(TEXT("Broken capability blocks fire reactivation"), ASC->TryActivateAbility(Fire));
	TestTrue(TEXT("Unrelated melee remains usable"), ASC->TryActivateAbility(Melee));
	TestTrue(TEXT("Native consequence grants equipment state"), ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Status_DeviceDisabled));
	FName Consumed;
	TestEqual(TEXT("Native source callback awards one weak-point break"), Source->TestEcho->GetEcho(), 8.f);
	TestFalse(TEXT("Source-consumed transaction cannot be claimed again"), WeakPoints->ConsumeWeakPointBreak(Target->LastDamageResult, Consumed));
	Attack(Source, Target, 5.f, 0.f, false, TEXT("weapon"));
	TestEqual(TEXT("Already broken zone gives no repeat Echo"), Source->TestEcho->GetEcho(), 8.f);
	WeakPoints->BreakWeakPointWithoutReward(TEXT("Sensor"));
	const auto Saved = WeakPoints->CaptureWeakPointState();
	TestEqual(TEXT("Two authored zones own separate consequences"), Saved.Consequences.Num(), 2);
	// Exercise the same removal delegate used by expiry/dispels without depending on a content clock.
	for (const FActiveGameplayEffectHandle Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
	{
		const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle);
		if (Effect && Effect->Spec.GetContext().GetSourceObject() == WeakPoints && Effect->Spec.GetDuration() > 0.f)
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}
	TestFalse(TEXT("Removing first zone preserves second zone ability block"), ASC->TryActivateAbility(Fire));
	ASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_Status_DeviceDisabled);
	WeakPoints->ResetWeakPoints();
	TestTrue(TEXT("Reset releases all owned ability block counts"), ASC->TryActivateAbility(Fire));
	TestTrue(TEXT("Reset preserves unrelated state contribution"), ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Status_DeviceDisabled));
	TestTrue(TEXT("Snapshot restores capability state"), WeakPoints->RestoreWeakPointState(Saved));
	TestEqual(TEXT("Restore never replays detailed break/reward event"), WeakPoints->DetailedBreakCount, 1);
	TestTrue(TEXT("Restore never cancels an already active ability"), ASC->FindAbilitySpecFromHandle(Fire)->IsActive());
	TestFalse(TEXT("Restore clears old reward ledger"), WeakPoints->ConsumeWeakPointBreak(Target->LastDamageResult, Consumed));
	auto Invalid = Saved; Invalid.BrokenZoneIds.Add(TEXT("MissingZone"));
	TestFalse(TEXT("Unknown saved zone fails atomically"), WeakPoints->RestoreWeakPointState(Invalid));
	TestEqual(TEXT("Failed restore preserves both broken zones"), WeakPoints->GetBrokenWeakPointIds().Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWeakPointResetReentryTest,
	"ProjectVelkorran.Campaign.WeakPoint.ReentrantResetAndSeverRestore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWeakPointResetReentryTest::RunTest(const FString& Parameters)
{
	FCombatRoutingWorld Fixture;
	auto* Source = Fixture.Character(100.f, 0);
	auto* Target = Fixture.Character(0.f, 1);
	if (!Source || !Target) { AddError(TEXT("Fixture actors failed")); return false; }
	auto* WeakPoints = AddWeakPoints(Target);
	WeakPoints->bResetOnBreakNotification = true;
	Attack(Source, Target, 5.f, 0.f, false, TEXT("weapon"));
	TestFalse(TEXT("Reentrant reset wins over in-flight break"), WeakPoints->IsWeakPointBroken(TEXT("Weapon")));
	TestEqual(TEXT("No stale detailed break is replayed after reset"), WeakPoints->DetailedBreakCount, 0);
	TestTrue(TEXT("Reentrant reset removes pending effects"), WeakPoints->CaptureWeakPointState().Consequences.IsEmpty());
	FName Id;
	TestFalse(TEXT("No stale reward survives reset"), WeakPoints->ConsumeWeakPointBreak(Target->LastDamageResult, Id));
	auto* Dismember = NewObject<USovDismembermentComponent>(Target);
	Target->AddInstanceComponent(Dismember);
	Dismember->RegisterComponent();
	const int32 ArmMask = 1 << static_cast<uint8>(ESovDismembermentRegion::LeftUpperArm);
	TestTrue(TEXT("Fresh participant accepts saved sever mask"), Dismember->RestoreSeveredRegionMask(ArmMask));
	TestEqual(TEXT("Saved mask preserved"), Dismember->GetSeveredRegionMask(), ArmMask);
	TestFalse(TEXT("Live rollback requires respawn"), Dismember->CanRestoreSeveredRegionMask(0));
	TestFalse(TEXT("Live rollback is refused without mutation"), Dismember->RestoreSeveredRegionMask(0));
	TestEqual(TEXT("Refused rollback preserves mask"), Dismember->GetSeveredRegionMask(), ArmMask);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWeightedChannelRoutingTest,
    "ProjectVelkorran.Campaign.Defense.WeightedChannels", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWeightedChannelRoutingTest::RunTest(const FString& Parameters)
{
    FCombatRoutingWorld Fixture; auto* Source=Fixture.Character(100.f,0); auto* Target=Fixture.Character(0.f,1);
    if (!Source || !Target) { return false; }
    auto* ASC=Target->GetNarrativeAbilitySystemComponent(); auto* SourceASC=Source->GetNarrativeAbilitySystemComponent();
    const auto& T=FSovGameplayTags::Get();
    ASC->AddLooseGameplayTag(T.Damage_Immunity_Kinetic);
    FGameplayEffectSpec Spec(GetDefault<USovCombatRoutingTestEffect>(),SourceASC->MakeEffectContext(),1.f);
    Spec.AddDynamicAssetTag(T.Damage_Channel_Kinetic); Spec.AddDynamicAssetTag(T.Damage_Channel_Thermal);
    Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage,80.f);
    Spec.SetSetByCallerMagnitude(T.SetByCaller_Damage_PoiseDamage,40.f);
    SourceASC->ApplyGameplayEffectSpecToTarget(Spec,ASC);
    TestEqual(TEXT("Equal semantic channels remove only the immune half"),Target->LastDamageResult.AppliedShieldDamage,40.f);
    TestEqual(TEXT("Independent Poise removes the same immune half exactly once"),Target->LastDamageResult.AppliedPoiseDamage,20.f);
    TestTrue(TEXT("Rejected channel available to status consumers"),Target->LastDamageResult.RejectedDamageChannels.HasTagExact(T.Damage_Channel_Kinetic));
    Spec.SetSetByCallerMagnitude(T.Damage_Channel_Kinetic,3.f); Spec.SetSetByCallerMagnitude(T.Damage_Channel_Thermal,1.f);
    SourceASC->ApplyGameplayEffectSpecToTarget(Spec,ASC);
    TestEqual(TEXT("Explicit 3:1 weights retain one quarter"),Target->LastDamageResult.AppliedShieldDamage,20.f);
    TestEqual(TEXT("Explicit Poise weight is not applied twice"),Target->LastDamageResult.AppliedPoiseDamage,10.f);
    ASC->AddLooseGameplayTag(T.Damage_Immunity_Thermal); const int32 Before=Target->ResolvedHitCount;
    SourceASC->ApplyGameplayEffectSpecToTarget(Spec,ASC);
    TestEqual(TEXT("All declared portions immune emits no accepted transaction"),Target->ResolvedHitCount,Before);
    FGameplayEffectSpec Untagged(GetDefault<USovCombatRoutingTestEffect>(),SourceASC->MakeEffectContext(),1.f);
    Untagged.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage,10.f);
    ASC->AddLooseGameplayTag(T.Damage_Immunity_All);
    SourceASC->ApplyGameplayEffectSpecToTarget(Untagged,ASC);
    TestEqual(TEXT("Global immunity also rejects untagged legacy damage"),Target->ResolvedHitCount,Before);
    ASC->RemoveLooseGameplayTag(T.Damage_Immunity_All);
    SourceASC->ApplyGameplayEffectSpecToTarget(Untagged,ASC);
    TestEqual(TEXT("Untagged legacy packet keeps ordinary behavior without global immunity"),Target->ResolvedHitCount,Before+1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInterruptionProtectionRoutingTest,
    "ProjectVelkorran.Campaign.Defense.InterruptionProtection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovInterruptionProtectionRoutingTest::RunTest(const FString& Parameters)
{
    FCombatRoutingWorld Fixture; auto* Source=Fixture.Character(100.f,0); auto* Target=Fixture.Character(0.f,1);
    if (!Source || !Target) { return false; }
    auto* ASC=Target->GetNarrativeAbilitySystemComponent(); const auto& T=FSovGameplayTags::Get();
    ASC->AddLooseGameplayTag(T.State_InterruptProtected);
    Attack(Source,Target,40.f,300.f,true);
    TestEqual(TEXT("Finisher protection does not grant damage immunity"),Target->LastDamageResult.AppliedShieldDamage,40.f);
    TestFalse(TEXT("Finisher protection prevents an interruption break"),Target->LastDamageResult.bPoiseBroken);
    TestFalse(TEXT("Hard freeze request suppressed during owned action protection"),Target->LastDamageResult.bStatusApplicationRequested);
    ASC->RemoveLooseGameplayTag(T.State_InterruptProtected);
    Attack(Source,Target,0.f,0.f,true);
    TestTrue(TEXT("Control is available again when protection ends"),Target->LastDamageResult.bStatusApplicationRequested);
    return true;
}
#endif
