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
#include <limits>

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
#endif
