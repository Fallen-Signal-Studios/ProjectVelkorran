// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Components/SovDismembermentComponent.h"
#include "Components/SovEchoComponent.h"
#include "Components/SovStatusComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Class.h"
#include "UObject/Script.h"
#include <limits>

#if WITH_AUTOMATION_TESTS
namespace
{
	struct FCombatRoutingWorld
	{
		UWorld* World = nullptr;
		FCombatRoutingWorld()
		{
			const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (World)
			{
				if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDamageExplicitCallbackReentryTest,
    "ProjectVelkorran.Campaign.Defense.CommittedBreakNestedDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDamageExplicitCallbackReentryTest::RunTest(const FString& Parameters)
{
    FCombatRoutingWorld F; auto* Source = F.Character(100.f, 0); auto* Target = F.Character(0.f, 1);
    if (!Source || !Target) { return false; }
    auto* ASC = Target->GetNarrativeAbilitySystemComponent();
    auto* Attributes = const_cast<UNarrativeAttributeSetBase*>(ASC->GetSet<UNarrativeAttributeSetBase>());
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 10.f);
    bool bReentered = false; float HealthAtBreak = -1.f;
    const FDelegateHandle Handle = Attributes->OnShieldBroken.AddLambda(
        [&](AActor*, AActor*, const FGameplayEffectSpec&, float)
        {
            if (bReentered) { return; }
            bReentered = true; HealthAtBreak = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
            Attack(Source, Target, 30.f, 0.f);
        });
    Attack(Source, Target, 20.f, 0.f);
    Attributes->OnShieldBroken.Remove(Handle);
    TestTrue(TEXT("Real shield-break callback reenters the production damage resolver"), bReentered);
    TestEqual(TEXT("Health debit commits before shield-break gameplay notification"), HealthAtBreak, 90.f);
    TestEqual(TEXT("Nested body hit is preserved without stale outer overwrite"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 60.f);
    TestEqual(TEXT("Outer result excludes the nested health debit"), Target->LastDamageResult.AppliedHealthDamage, 10.f);
    TestEqual(TEXT("Both synchronous packets publish a result"), Target->ResolvedHitCount, 2);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDamageAttributeCallbackReentryTest,
    "ProjectVelkorran.Campaign.Defense.AttributeDelegateNestedDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDamageAttributeCallbackReentryTest::RunTest(const FString& Parameters)
{
    for (const bool bLethal : {false, true})
    {
        FCombatRoutingWorld F; auto* Source = F.Character(100.f, 0); auto* Target = F.Character(0.f, 1);
        if (!Source || !Target) { return false; }
        auto* ASC = Target->GetNarrativeAbilitySystemComponent();
        auto* Attributes = const_cast<UNarrativeAttributeSetBase*>(ASC->GetSet<UNarrativeAttributeSetBase>());
        ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 10.f);
        bool bReentered = false; int32 Deaths = 0;
        const auto DeathHandle = Attributes->OnOutOfHealth.AddLambda([&](AActor*, AActor*, const FGameplayEffectSpec&, float) { ++Deaths; });
        auto& ShieldDelegate = ASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetShieldAttribute());
        const auto Handle = ShieldDelegate.AddLambda([&](const FOnAttributeChangeData& Change)
        {
            if (bReentered || Change.NewValue > 0.f) { return; }
            bReentered = true; Attack(Source, Target, bLethal ? 200.f : 30.f, 40.f);
        });
        Attack(Source, Target, 20.f, 50.f);
        ShieldDelegate.Remove(Handle); Attributes->OnOutOfHealth.Remove(DeathHandle);
        TestTrue(TEXT("GAS attribute delegate exercised synchronous reentry"), bReentered);
        TestEqual(TEXT("Later Health write preserves nested damage, including death"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), bLethal ? 0.f : 60.f);
        TestEqual(TEXT("Later Poise write preserves nested debit and skips dead targets"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetPoiseAttribute()), bLethal ? 100.f : 10.f);
        TestEqual(TEXT("Only the packet crossing zero owns the death event"), Deaths, bLethal ? 1 : 0);
        TestFalse(TEXT("Outer packet cannot steal nested fatal result"), Target->LastDamageResult.bFatal);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWeakPointReceiptLifetimeTest,
    "ProjectVelkorran.Campaign.WeakPoint.ReceiptReplayAfterReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWeakPointReceiptLifetimeTest::RunTest(const FString& Parameters)
{
    FCombatRoutingWorld F; auto* Source = F.Character(100.f, 0); auto* Target = F.Character(0.f, 1);
    if (!Source || !Target) { return false; }
    auto* WeakPoints = AddWeakPoints(Target);
    Attack(Source, Target, 5.f, 0.f, false, TEXT("weapon"));
    const FSovDamageResult RetainedCopy = Target->LastDamageResult;
    TestTrue(TEXT("Authoritative resolver minted a shared native receipt"), RetainedCopy.HasNativeReceipt());
    TestTrue(TEXT("Initial hit consumed the weak point"), WeakPoints->IsWeakPointBroken(TEXT("Weapon")));
    Attack(Source, Target, 5.f, 0.f, false, TEXT("weapon"));
    const FSovDamageResult HitWhileBroken = Target->LastDamageResult;
    WeakPoints->ResetWeakPoints(); FName Zone;
    TestEqual(TEXT("Retained hit remains consumed after component reset"), WeakPoints->ResolveWeakPointHit(RetainedCopy, Zone), ESovWeakPointHitResolution::NotWeakPoint);
    TestEqual(TEXT("A hit received while broken cannot replay after reset"), WeakPoints->ResolveWeakPointHit(HitWhileBroken, Zone), ESovWeakPointHitResolution::NotWeakPoint);
    auto Forged = RetainedCopy; Forged.TransactionId = FGuid::NewGuid();
    TestFalse(TEXT("Changing the GUID cannot mint a new receipt"), Forged.HasNativeReceipt());
    TestFalse(TEXT("Reset target remains intact after replay"), WeakPoints->IsWeakPointBroken(TEXT("Weapon")));
    Attack(Source, Target, 5.f, 0.f, false, TEXT("weapon"));
    TestTrue(TEXT("A fresh committed hit works after reset"), WeakPoints->IsWeakPointBroken(TEXT("Weapon")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDamageRetiredAvatarCallbackTest,
    "ProjectVelkorran.Campaign.Defense.RetiredAvatarNotifications", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDamageRetiredAvatarCallbackTest::RunTest(const FString& Parameters)
{
    FCombatRoutingWorld F; auto* Source = F.Character(100.f, 0); auto* Target = F.Character(0.f, 1); auto* Replacement = F.Character(400.f, 1);
    if (!Source || !Target || !Replacement) { return false; }
    auto* ASC = Target->GetNarrativeAbilitySystemComponent();
    auto* Attributes = const_cast<UNarrativeAttributeSetBase*>(ASC->GetSet<UNarrativeAttributeSetBase>());
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 10.f);
    int32 PoiseEvents = 0, ShieldGameplayEvents = 0;
    const auto PoiseHandle = Attributes->OnPoiseBroken.AddLambda([&](AActor*, AActor*, const FGameplayEffectSpec&, float) { ++PoiseEvents; });
    const auto ShieldHandle = Attributes->OnShieldBroken.AddLambda([&](AActor*, AActor*, const FGameplayEffectSpec&, float)
    { ASC->InitAbilityActorInfo(Target, Replacement); });
    const auto EventHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(FSovGameplayTags::Get().Event_Shield_Broken).AddLambda(
        [&](const FGameplayEventData*) { ++ShieldGameplayEvents; });
    Attack(Source, Target, 20.f, 100.f);
    TestEqual(TEXT("Break callback changed the canonical avatar"), ASC->GetAvatarActor(), static_cast<AActor*>(Replacement));
    TestEqual(TEXT("Later native break notification does not act on replaced ownership"), PoiseEvents, 0);
    TestEqual(TEXT("Old actor gameplay lookup cannot route event to shared ASC's new avatar"), ShieldGameplayEvents, 0);
    Attributes->OnShieldBroken.Remove(ShieldHandle); Attributes->OnPoiseBroken.Remove(PoiseHandle);
    ASC->GenericGameplayEventCallbacks.FindChecked(FSovGameplayTags::Get().Event_Shield_Broken).Remove(EventHandle);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDamageExplicitRestoreCallbackTest,
    "ProjectVelkorran.Campaign.Defense.RestoreBeforeDeathNotification", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDamageExplicitRestoreCallbackTest::RunTest(const FString& Parameters)
{
    FCombatRoutingWorld F; auto* Source = F.Character(100.f, 0); auto* Target = F.Character(0.f, 1);
    if (!Source || !Target) { return false; }
    auto* ASC = Target->GetNarrativeAbilitySystemComponent();
    auto* Attributes = const_cast<UNarrativeAttributeSetBase*>(ASC->GetSet<UNarrativeAttributeSetBase>());
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 0.f);
    int32 Deaths = 0; bool bRestored = false;
    const auto DeathHandle = Attributes->OnOutOfHealth.AddLambda([&](AActor*, AActor*, const FGameplayEffectSpec&, float) { ++Deaths; });
    auto& HealthChanged = ASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute());
    const auto HealthHandle = HealthChanged.AddLambda([&](const FOnAttributeChangeData& Change)
    {
        if (bRestored || Change.NewValue > 0.f) { return; }
        bRestored = true; ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 50.f);
    });
    Attack(Source, Target, 200.f, 0.f);
    HealthChanged.Remove(HealthHandle); Attributes->OnOutOfHealth.Remove(DeathHandle);
    TestTrue(TEXT("Explicit recovery callback restored Health"), bRestored);
    TestEqual(TEXT("A pending death does not kill the explicitly restored target"), Deaths, 0);
    TestEqual(TEXT("Restored Health is retained"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 50.f);
    TestFalse(TEXT("Restored target cannot generate a stale fatal reward"), Target->LastDamageResult.bFatal);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDamageRestoreNestedFatalTest,
    "ProjectVelkorran.Campaign.Defense.RestoreThenNestedFatalHasOneOwner", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDamageRestoreNestedFatalTest::RunTest(const FString& Parameters)
{
    FCombatRoutingWorld F; auto* Source = F.Character(100.f, 0); auto* Target = F.Character(0.f, 1);
    if (!Source || !Target) { return false; }
    auto* ASC = Target->GetNarrativeAbilitySystemComponent();
    auto* Attributes = const_cast<UNarrativeAttributeSetBase*>(ASC->GetSet<UNarrativeAttributeSetBase>());
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 0.f);
    int32 Deaths = 0; bool bRestored = false;
    const auto DeathHandle = Attributes->OnOutOfHealth.AddLambda([&](AActor*, AActor*, const FGameplayEffectSpec&, float) { ++Deaths; });
    auto& HealthChanged = ASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute());
    const auto HealthHandle = HealthChanged.AddLambda([&](const FOnAttributeChangeData& Change)
    {
        if (bRestored || Change.NewValue > 0.f) { return; }
        bRestored = true; ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
        Attack(Source, Target, 300.f, 0.f);
    });
    Attack(Source, Target, 200.f, 0.f);
    HealthChanged.Remove(HealthHandle); Attributes->OnOutOfHealth.Remove(DeathHandle);
    TestTrue(TEXT("Health callback restored then applied a new-life lethal hit"), bRestored);
    TestEqual(TEXT("Retired life cannot claim the new life's death"), Deaths, 1);
    TestEqual(TEXT("Only the current life publishes a fatal receipt"), Target->ResolvedHitCount, 1);
    TestTrue(TEXT("New-life nested packet retains fatal attribution"), Target->LastDamageResult.bFatal);
    TestEqual(TEXT("Nested packet owns the last immutable result"), Target->LastDamageResult.BaseDamage, 300.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDamageCoefficientAdmissionTest,
    "ProjectVelkorran.Campaign.Defense.FiniteCoefficientAdmission", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDamageCoefficientAdmissionTest::RunTest(const FString& Parameters)
{
    FCombatRoutingWorld F; auto* Source = F.Character(100.f, 0); auto* Target = F.Character(0.f, 1);
    if (!Source || !Target) { return false; }
    auto* ASC = Target->GetNarrativeAbilitySystemComponent(); auto* SourceASC = Source->GetNarrativeAbilitySystemComponent();
    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext(); Context.AddInstigator(Source, Source);
    FGameplayEffectSpec Spec(GetDefault<USovCombatRoutingTestEffect>(), Context, 1.f);
    const auto& T = FSovGameplayTags::Get();
    Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, 20.f);
    Spec.AddDynamicAssetTag(T.Damage_AlreadyResolved); Spec.AddDynamicAssetTag(T.Damage_Channel_Kinetic);
    Spec.SetSetByCallerMagnitude(T.SetByCaller_Damage_ShieldCoefficient, std::numeric_limits<float>::infinity());
    SourceASC->ApplyGameplayEffectSpecToTarget(Spec, ASC);
    TestEqual(TEXT("Nonfinite coefficient is rejected before any attribute debit"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()), 100.f);
    TestEqual(TEXT("Rejected coefficient does not publish a usable hit"), Target->ResolvedHitCount, 0);
    Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, 1e30f);
    Spec.SetSetByCallerMagnitude(T.SetByCaller_Damage_ShieldCoefficient, 1e30f);
    Spec.SetSetByCallerMagnitude(T.SetByCaller_Damage_HealthCoefficient, 1e30f);
    SourceASC->ApplyGameplayEffectSpecToTarget(Spec, ASC);
    TestTrue(TEXT("Finite large coefficients remain finite in published damage"), FMath::IsFinite(Target->LastDamageResult.RequestedShieldDamage)
        && FMath::IsFinite(Target->LastDamageResult.AppliedHealthDamage) && FMath::IsFinite(Target->LastDamageResult.HealthOverkillDamage));
    TestEqual(TEXT("Large valid routing remains bounded by available resources"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 0.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDamagePublicationRestoreTest,
    "ProjectVelkorran.Campaign.Defense.RestoreDuringDeathAndResultPublication", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDamagePublicationRestoreTest::RunTest(const FString& Parameters)
{
    for (const bool bRestoreAtDeath : {false, true})
    {
        FCombatRoutingWorld F; auto* Source = F.Character(100.f, 0); auto* Target = F.Character(0.f, 1);
        if (!Source || !Target) { return false; }
        auto* ASC = Target->GetNarrativeAbilitySystemComponent(); auto* SourceASC = Source->GetNarrativeAbilitySystemComponent();
        auto* Attributes = const_cast<UNarrativeAttributeSetBase*>(ASC->GetSet<UNarrativeAttributeSetBase>());
        ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 0.f);
        auto* Observer = NewObject<USovDamagePublicationRepairObserver>(Target);
        Observer->TargetASC = ASC; Observer->bRestoreOnTargetResult = !bRestoreAtDeath;
        ASC->OnDamageResolvedAsTarget.AddDynamic(Observer, &USovDamagePublicationRepairObserver::OnTargetResult);
        SourceASC->OnDamageResolvedAsSource.AddDynamic(Observer, &USovDamagePublicationRepairObserver::OnSourceResult);
        int32 Deaths = 0, Kills = 0;
        const auto DeathHandle = Attributes->OnOutOfHealth.AddLambda([&](AActor*, AActor*, const FGameplayEffectSpec&, float)
        {
            ++Deaths;
            if (bRestoreAtDeath) { ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 50.f); }
        });
        auto& KillEvents = SourceASC->GenericGameplayEventCallbacks.FindOrAdd(FNarrativeGameplayTags::Get().GameplayEvent_KilledEnemy);
        const auto KillHandle = KillEvents.AddLambda([&](const FGameplayEventData*) { ++Kills; });
        Attack(Source, Target, 200.f, 0.f);
        Attributes->OnOutOfHealth.Remove(DeathHandle); KillEvents.Remove(KillHandle);
        ASC->OnDamageResolvedAsTarget.RemoveDynamic(Observer, &USovDamagePublicationRepairObserver::OnTargetResult);
        SourceASC->OnDamageResolvedAsSource.RemoveDynamic(Observer, &USovDamagePublicationRepairObserver::OnSourceResult);
        TestEqual(TEXT("The original death callback executes once"), Deaths, 1);
        TestEqual(TEXT("Kill notification cannot outlive restoration in the death callback"), Kills, bRestoreAtDeath ? 0 : 1);
        TestEqual(TEXT("Committed source damage still has one result"), Observer->SourceResults, 1);
        TestFalse(TEXT("Later source reward consumers cannot claim the restored life"), Observer->bLastSourceFatal);
        TestEqual(TEXT("Publication callback restoration survives"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 50.f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTypedStatusOwnershipRoutingTest,
    "ProjectVelkorran.Campaign.Defense.TypedStatusOwnershipAndMetadata", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTypedStatusOwnershipRoutingTest::RunTest(const FString& Parameters)
{
    // Normal, restored life, replaced avatar, native-owned and ABA delivery each use
    // both control-only and damage-backed specs. No status assets are required.
    for (const bool bDamageBacked : {false, true})
    {
        for (int32 Mode = 0; Mode != 5; ++Mode)
        {
            FCombatRoutingWorld Fixture;
            auto* Source = Fixture.Character(100.f, 0);
            auto* Target = Fixture.Character(0.f, 1);
            auto* Replacement = Fixture.Character(400.f, 1);
            if (!Source || !Target || !Replacement) { AddError(TEXT("Typed status fixture failed")); return false; }
            auto* SourceASC = Source->GetNarrativeAbilitySystemComponent();
            auto* TargetASC = Target->GetNarrativeAbilitySystemComponent();
            auto* Observer = NewObject<USovDamagePublicationRepairObserver>(Target);
            Observer->TargetASC = TargetASC;
            Observer->bRestoreOnFirstStatus = Mode == 1;
            Observer->ReplacementAvatar = Mode == 2 || Mode == 4 ? Replacement : nullptr;
            Observer->bRestoreAvatarAfterReplacement = Mode == 4;
            TargetASC->OnStatusApplicationRequested.AddDynamic(Observer, &USovDamagePublicationRepairObserver::OnStatusRequest);
            SourceASC->OnDamageResolvedAsSource.AddDynamic(Observer, &USovDamagePublicationRepairObserver::OnSourceResult);
            const auto& Tags = FSovGameplayTags::Get();
            int32 AggregateEvents = 0;
            const auto EventHandle = TargetASC->GenericGameplayEventCallbacks.FindOrAdd(Tags.Event_Status_ApplicationRequested)
                .AddLambda([&](const FGameplayEventData*) { ++AggregateEvents; });

            FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
            Context.AddInstigator(Source, Source);
            Context.AddSourceObject(Observer);
            FGameplayEffectSpec Spec(GetDefault<USovCombatRoutingTestEffect>(), Context, 3.f);
            Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, bDamageBacked ? 5.f : 0.f);
            Spec.SetSetByCallerMagnitude(Tags.SetByCaller_Status_Magnitude, 2.5f);
            Spec.SetSetByCallerMagnitude(Tags.SetByCaller_Status_Duration, 7.25f);
            Spec.AddDynamicAssetTag(Tags.Damage_Channel_Disruption);
            Spec.AddDynamicAssetTag(Tags.Status_Apply_Chill);
            Spec.AddDynamicAssetTag(Tags.Status_Apply_Exposed);
            Spec.AddDynamicAssetTag(Tags.Ability_Echo_Selene_AxiomNullPulse);
            if (Mode == 3) { Spec.AddDynamicAssetTag(Tags.Status_Application_NativeOwned); }
            SourceASC->ApplyGameplayEffectSpecToTarget(Spec, TargetASC);

            TargetASC->OnStatusApplicationRequested.RemoveDynamic(Observer, &USovDamagePublicationRepairObserver::OnStatusRequest);
            SourceASC->OnDamageResolvedAsSource.RemoveDynamic(Observer, &USovDamagePublicationRepairObserver::OnSourceResult);
            TargetASC->GenericGameplayEventCallbacks.FindChecked(Tags.Event_Status_ApplicationRequested).Remove(EventHandle);
            const bool bRetired = Mode == 1 || Mode == 2 || Mode == 4;
            const FString Case = FString::Printf(TEXT("Status mode %d, damage-backed %d"), Mode, bDamageBacked ? 1 : 0);
            TestEqual(Case + TEXT(": no later typed leaf crosses a life/avatar retirement"), Observer->StatusRequests.Num(), Mode == 3 ? 0 : bRetired ? 1 : 2);
            TestEqual(Case + TEXT(": legacy aggregate is preserved only for the current owner"), AggregateEvents, bRetired ? 0 : 1);
            TestEqual(Case + TEXT(": committed source transaction is still published once"), Observer->SourceResults, 1);
            TestFalse(Case + TEXT(": status callback cannot create a stale source fatal result"), Observer->bLastSourceFatal);
            TestEqual(Case + TEXT(": retired target does not receive the old result"), Target->ResolvedHitCount, bRetired ? 0 : 1);
            FGameplayTagContainer DeliveredLeaves;
            for (const FSovStatusApplicationRequest& Request : Observer->StatusRequests)
            {
                TestTrue(Case + TEXT(": request shares the damage transaction identity"), Request.RequestId.IsValid()
                    && Request.RequestId == Observer->LastSourceResult.TransactionId);
                TestEqual(Case + TEXT(": source identity survives"), Request.SourceActor.Get(), static_cast<AActor*>(Source));
                TestEqual(Case + TEXT(": request retains the admitted target, not its replacement"), Request.TargetActor.Get(), static_cast<AActor*>(Target));
                TestEqual(Case + TEXT(": magnitude survives"), Request.Magnitude, 2.5f);
                TestEqual(Case + TEXT(": duration survives"), Request.Duration, 7.25f);
                TestEqual(Case + TEXT(": effect level survives"), Request.EffectLevel, 3.f);
                TestTrue(Case + TEXT(": original effect context survives"), Request.Context.GetSourceObject() == Observer);
                TestEqual(Case + TEXT(": damage requirement distinguishes control from damage"), Request.bRequiresAppliedDamage, bDamageBacked);
                TestTrue(Case + TEXT(": ability metadata survives"), Request.SourceAbilityTags.HasTagExact(Tags.Ability_Echo_Selene_AxiomNullPulse));
                TestEqual(Case + TEXT(": damage/status tags do not leak into ability metadata"), Request.SourceAbilityTags.Num(), 1);
                DeliveredLeaves.AddTag(Request.StatusTag);
            }
            if (Mode == 0)
            {
                TestTrue(Case + TEXT(": both exact authored leaves are delivered"), DeliveredLeaves.HasTagExact(Tags.Status_Apply_Chill)
                    && DeliveredLeaves.HasTagExact(Tags.Status_Apply_Exposed));
            }
            if (Mode == 1)
            {
                TestEqual(Case + TEXT(": new-life restoration is not overwritten"), TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 50.f);
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDamageTeamAdmissionRetirementTest,
    "ProjectVelkorran.Campaign.Defense.TeamAdmissionRetirement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDamageTeamAdmissionRetirementTest::RunTest(const FString& Parameters)
{
    // A virtual admission callback may replace either participant or complete a
    // round trip to the same actor. Identity equality alone cannot admit ABA.
    for (int32 Mode = 0; Mode != 7; ++Mode)
    {
        FCombatRoutingWorld Fixture;
        if (!Fixture.World) { AddError(TEXT("Admission world failed")); return false; }
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Source = Fixture.World->SpawnActor<ASovCombatAdmissionTestCharacter>(ASovCombatAdmissionTestCharacter::StaticClass(),
            FVector(100.f, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
        auto* Target = Fixture.Character(0.f, 1);
        auto* Replacement = Fixture.Character(400.f, 1);
        if (!Source || !Target || !Replacement) { AddError(TEXT("Admission fixture failed")); return false; }
        Source->InitializeTestCombat(0);
        auto* ASC = Target->GetNarrativeAbilitySystemComponent();
        auto* SourceASC = Source->GetNarrativeAbilitySystemComponent();
        bool bCallbackInvoked = false;
        Source->OnNextTeamQuery = [&]()
        {
            bCallbackInvoked = true;
            if (Mode == 0)
            {
                ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
                ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 50.f);
            }
            else if (Mode == 1 || Mode == 3)
            {
                ASC->InitAbilityActorInfo(Target, Replacement);
                if (Mode == 3) { ASC->InitAbilityActorInfo(Target, Target); }
            }
            else if (Mode == 2)
            {
                SourceASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
                SourceASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 50.f);
            }
            else if (Mode == 4 || Mode == 5)
            {
                SourceASC->InitAbilityActorInfo(Source, Replacement);
                if (Mode == 4) { SourceASC->InitAbilityActorInfo(Source, Source); }
            }
            else
            {
                // A dead projectile owner has not been restored or rebound:
                // already in-flight damage may still resolve for this life.
                SourceASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
            }
        };
        Attack(Source, Target, 20.f, 30.f, true);
        Source->OnNextTeamQuery = TFunction<void()>();
        TestTrue(TEXT("Production friendly-fire admission invokes the adversarial team query"), bCallbackInvoked);
        TestEqual(TEXT("Only the unchanged source life may commit a Shield debit"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()), Mode == 6 ? 80.f : 100.f);
        TestEqual(TEXT("Only the unchanged source life may commit a Poise debit"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetPoiseAttribute()), Mode == 6 ? 70.f : 100.f);
        TestEqual(TEXT("Rejected admission preserves the callback's Health state"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), Mode == 0 ? 50.f : 100.f);
        TestEqual(TEXT("Admission preserves the source callback's life state"), SourceASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), Mode == 2 ? 50.f : Mode == 6 ? 0.f : 100.f);
        TestEqual(TEXT("Retired admission cannot publish, but an in-flight hit from a dead source can"), Target->ResolvedHitCount, Mode == 6 ? 1 : 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDamageReceiptLifeCopyTest,
    "ProjectVelkorran.Campaign.WeakPoint.ReceiptTargetLifeAndReflectedCopy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDamageReceiptLifeCopyTest::RunTest(const FString& Parameters)
{
    FCombatRoutingWorld Fixture;
    auto* Source = Fixture.Character(100.f, 0);
    auto* Target = Fixture.Character(0.f, 1);
    auto* Replacement = Fixture.Character(400.f, 1);
    if (!Source || !Target || !Replacement) { AddError(TEXT("Receipt fixture failed")); return false; }
    auto* ASC = Target->GetNarrativeAbilitySystemComponent();
    Attack(Source, Target, 5.f, 0.f, false, TEXT("weapon"));
    const FSovDamageResult Original = Target->LastDamageResult;
    FSovDamageResult ReflectedCopy;
    FSovDamageResult::StaticStruct()->CopyScriptStruct(&ReflectedCopy, &Original);
    auto* Consumer = NewObject<UObject>(Target);
    TestTrue(TEXT("Fresh resolver result belongs to the current target life"), Original.IsCurrentTargetLife());
    TestTrue(TEXT("Native result can be claimed once on a dedicated channel"), Original.ConsumeNativeReceipt(Consumer, 5));
    TestTrue(TEXT("Reflected copies preserve native receipt identity"), ReflectedCopy.HasNativeReceipt());
    TestFalse(TEXT("Reflected copies cannot replay an already consumed channel"), ReflectedCopy.ConsumeNativeReceipt(Consumer, 5));
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
    TestFalse(TEXT("Retained native result cannot act on the restored life"), Original.IsCurrentTargetLife());
    TestFalse(TEXT("Even an unused channel cannot revive a retired-life result"), ReflectedCopy.ConsumeNativeReceipt(Consumer, 6));
    Attack(Source, Target, 5.f, 0.f);
    const FSovDamageResult Fresh = Target->LastDamageResult;
    TestTrue(TEXT("A new committed hit belongs to the restored life"), Fresh.IsCurrentTargetLife());
    ASC->InitAbilityActorInfo(Target, Replacement);
    TestFalse(TEXT("Avatar replacement retires the fresh receipt too"), Fresh.IsCurrentTargetLife());
    TestFalse(TEXT("Replaced-avatar receipt cannot be claimed by a new consumer"), Fresh.ConsumeNativeReceipt(NewObject<UObject>(Target)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDamageSourcePublicationRestoreTest,
    "ProjectVelkorran.Campaign.Defense.SourceRestoreDuringTargetPublication", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDamageSourcePublicationRestoreTest::RunTest(const FString& Parameters)
{
    FCombatRoutingWorld Fixture;
    auto* Source = Fixture.Character(100.f, 0);
    auto* Target = Fixture.Character(0.f, 1);
    if (!Source || !Target) { AddError(TEXT("Source publication fixture failed")); return false; }
    auto* SourceASC = Source->GetNarrativeAbilitySystemComponent();
    auto* TargetASC = Target->GetNarrativeAbilitySystemComponent();
    TargetASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 0.f);
    auto* Observer = NewObject<USovDamagePublicationRepairObserver>(Target);
    Observer->SourceASC = SourceASC;
    Observer->bRestoreSourceOnTargetResult = true;
    TargetASC->OnDamageResolvedAsTarget.AddDynamic(Observer, &USovDamagePublicationRepairObserver::OnTargetResult);
    SourceASC->OnDamageResolvedAsSource.AddDynamic(Observer, &USovDamagePublicationRepairObserver::OnSourceResult);
    Attack(Source, Target, 20.f, 0.f);
    TargetASC->OnDamageResolvedAsTarget.RemoveDynamic(Observer, &USovDamagePublicationRepairObserver::OnTargetResult);
    SourceASC->OnDamageResolvedAsSource.RemoveDynamic(Observer, &USovDamagePublicationRepairObserver::OnSourceResult);
    TestFalse(TEXT("The target callback actually performed source restoration"), Observer->bRestoreSourceOnTargetResult);
    TestEqual(TEXT("Target's already committed damage is not rolled back"), TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 80.f);
    TestEqual(TEXT("Target still owns its single result"), Target->ResolvedHitCount, 1);
    TestEqual(TEXT("Source restoration remains intact"), SourceASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 50.f);
    TestEqual(TEXT("Old-life source rewards cannot cross restoration"), Observer->SourceResults, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovLateStatusSubscriberLifeTest,
    "ProjectVelkorran.Campaign.Defense.LateStatusSubscriberRetiredLife", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovLateStatusSubscriberLifeTest::RunTest(const FString& Parameters)
{
    int32 RestorationBeforeStatusCases = 0;
    // Delegate invocation order is not a gameplay contract. Exercise both
    // registration permutations and explicitly prove the adversarial ordering.
    for (int32 Mode = 0; Mode != 3; ++Mode)
    {
        FCombatRoutingWorld Fixture;
        auto* Source = Fixture.Character(100.f, 0);
        auto* Target = Fixture.Character(0.f, 1);
        if (!Source || !Target) { AddError(TEXT("Late status fixture failed")); return false; }
        auto* SourceASC = Source->GetNarrativeAbilitySystemComponent();
        auto* TargetASC = Target->GetNarrativeAbilitySystemComponent();
        auto* Status = NewObject<USovStatusComponent>(Target);
        Target->AddInstanceComponent(Status);
        Status->RegisterComponent();
        auto* Observer = NewObject<USovDamagePublicationRepairObserver>(Target);
        Observer->TargetASC = TargetASC;
        Observer->StatusComponent = Status;
        Observer->bRestoreOnFirstStatus = Mode != 0;
        if (Mode == 1)
        {
            TargetASC->OnStatusApplicationRequested.AddDynamic(Observer, &USovDamagePublicationRepairObserver::OnStatusRequest);
        }
        if (!TestTrue(TEXT("Real status component initializes without content"), Status->InitializeWithAbilitySystem(TargetASC))) { return false; }
        if (Mode != 1)
        {
            TargetASC->OnStatusApplicationRequested.AddDynamic(Observer, &USovDamagePublicationRepairObserver::OnStatusRequest);
        }
        const auto& Tags = FSovGameplayTags::Get();
        FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
        Context.AddInstigator(Source, Source);
        FGameplayEffectSpec Spec(GetDefault<USovCombatRoutingTestEffect>(), Context, 1.f);
        Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, 0.f);
        Spec.SetSetByCallerMagnitude(Tags.SetByCaller_Status_Magnitude, 1.f);
        Spec.AddDynamicAssetTag(Tags.Status_Apply_Chill);
        {
#if WITH_EDITOR
            // Run the actual non-CallInEditor component handler without starting
            // gameplay or reverting the Mac-safe one-time world initialization.
            FEditorScriptExecutionGuard AllowStatusHandler;
#endif
            SourceASC->ApplyGameplayEffectSpecToTarget(Spec, TargetASC);
        }
        TargetASC->OnStatusApplicationRequested.RemoveDynamic(Observer, &USovDamagePublicationRepairObserver::OnStatusRequest);
        TestEqual(TEXT("The observer receives exactly one real damage-origin status leaf"), Observer->StatusRequests.Num(), 1);
        if (Mode == 0)
        {
            TestTrue(TEXT("Baseline proves the real late-bound status handler applies Chill"), Status->HasActiveStatus(Tags.Status_Apply_Chill));
        }
        else if (!Observer->bStatusPresentBeforeFirstRequest)
        {
            ++RestorationBeforeStatusCases;
            TestFalse(TEXT("Later listener cannot apply the old request to restored Health"), Status->HasActiveStatus(Tags.Status_Apply_Chill));
            TestFalse(TEXT("Retired request does not grant the gameplay state tag"), TargetASC->HasMatchingGameplayTag(Tags.State_Status_Chilled));
            TestEqual(TEXT("Same-multicast retirement preserves the new-life Health"), TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 50.f);
            if (Observer->StatusRequests.Num() == 1)
            {
                FSovStatusApplicationRequest RetainedCopy;
                FSovStatusApplicationRequest::StaticStruct()->CopyScriptStruct(&RetainedCopy, &Observer->StatusRequests[0]);
                TestFalse(TEXT("Reflected status copy retains the retired native origin"), RetainedCopy.IsCurrentDamageOrigin());
                TestEqual(TEXT("Direct replay of the old copied request also fails closed"), Status->ApplyStatus(RetainedCopy), ESovStatusApplicationResult::RejectedInvalidRequest);
            }
            TestEqual(TEXT("Fresh direct native requests remain supported after recovery"), Status->ApplyStatusByTag(Tags.Status_Apply_Chill, Source), ESovStatusApplicationResult::Applied);
        }
    }
    TestTrue(TEXT("At least one registration order exercised restore-before-handler, not a vacuous rejection"), RestorationBeforeStatusCases > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovApprovedDamageBudgetHealingTest,
    "ProjectVelkorran.Campaign.Defense.ApprovedPolicyBudgetSurvivesHealing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovApprovedDamageBudgetHealingTest::RunTest(const FString& Parameters)
{
    FCombatRoutingWorld Fixture;
    auto* Source = Fixture.Character(100.f, 0);
    auto* Target = Fixture.Character(0.f, 1);
    if (!Source || !Target) { AddError(TEXT("Damage policy fixture failed")); return false; }
    auto* Policy = NewObject<USovIdentityDamagePolicyTestComponent>(Source);
    Source->AddInstanceComponent(Policy);
    Policy->RegisterComponent();
    auto* ASC = Target->GetNarrativeAbilitySystemComponent();
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 10.f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 10.f);
    bool bHealedDuringShieldDebit = false;
    auto& ShieldChanged = ASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetShieldAttribute());
    const FDelegateHandle Handle = ShieldChanged.AddLambda([&](const FOnAttributeChangeData& Change)
    {
        if (bHealedDuringShieldDebit || Change.NewValue > 0.f) { return; }
        bHealedDuringShieldDebit = true;
        ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
    });
    Attack(Source, Target, 50.f, 0.f);
    ShieldChanged.Remove(Handle);
    TestEqual(TEXT("The source policy approved exactly one transaction"), Policy->PolicyCalls, 1);
    TestEqual(TEXT("Policy sees the currently capped Shield budget"), Policy->ApprovedShieldDamage, 10.f);
    TestEqual(TEXT("Policy sees the currently capped Health budget"), Policy->ApprovedHealthDamage, 10.f);
    TestTrue(TEXT("The real Shield setter callback healed the target before Health commit"), bHealedDuringShieldDebit);
    TestEqual(TEXT("Approved unchanged Health limit remains binding after healing"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 90.f);
    TestEqual(TEXT("Outer result claims only the approved Health budget"), Target->LastDamageResult.AppliedHealthDamage, 10.f);
    TestEqual(TEXT("Approved policy does not invent overkill after healing"), Target->LastDamageResult.HealthOverkillDamage, 0.f);
    TestFalse(TEXT("The approved nonlethal transaction cannot become a kill"), Target->LastDamageResult.bFatal);
    return true;
}
#endif
