// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Abilities/SovGameplayAbility_Finisher.h"
#include "Combat/SovFinisherTargetComponent.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "UObject/Script.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Projectiles/SovReformationDroneRocketProjectile.h"
#include "Sovereign/SovGameplayTags.h"
#include "TimerManager.h"
#if WITH_AUTOMATION_TESTS
struct FSovFinisherRuntimeTestAccess
{
    // Isolate phase delivery from authored alignment geometry. Keep the real
    // GAS activation and target reservation for the durable commit boundary.
    static void AlignedStrike(USovGameplayAbility_Finisher* Ability)
    {
#if WITH_EDITOR
        FEditorScriptExecutionGuard AllowNativeReceipt;
#endif
        Ability->bAligned=true; Ability->Strike();
    }
};
struct FSovProjectileDefenseTestAccess
{
    static void Impact(ASovReformationDroneRocketProjectile* Rocket,AActor* Target)
    {
#if WITH_EDITOR
        FEditorScriptExecutionGuard AllowNativeDefenseReceipt;
#endif
        FHitResult Hit(Target,Cast<UPrimitiveComponent>(Target->GetRootComponent()),Rocket->GetActorLocation(),FVector::ForwardVector);
        Hit.bBlockingHit=true; Rocket->ResolveImpact(Hit);
    }
    static void Resume(ASovReformationDroneRocketProjectile* Rocket) { Rocket->ResumeReflectedFlight(); }
    static void Expire(ASovReformationDroneRocketProjectile* Rocket) { Rocket->ExpireProjectile(); }
    static AActor* Source(const ASovReformationDroneRocketProjectile* Rocket) { return Rocket->SourceAvatar.Get(); }
};
namespace
{
struct FFinisherWorld
{
    UWorld* World=nullptr;
    FFinisherWorld()
    {
        const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World=UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
        if (World)
        {
            if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
            World->GetTimerManager().Tick(0.f);
        }
    }
    ~FFinisherWorld() { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
    ASovAxiomRuntimeTestCharacter* Character(float X,int32 Team)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Actor=World?World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(),FVector(X,0,100),FRotator::ZeroRotator,Spawn):nullptr;
        if (Actor) { Actor->InitializeTestCombat(Team); }
        return Actor;
    }
    USovFinisherTargetComponent* Eligible(ASovAxiomRuntimeTestCharacter* Target)
    {
        auto* Component=NewObject<USovFinisherTargetComponent>(Target); Target->AddInstanceComponent(Component); Component->RegisterComponent();
        Target->GetNarrativeAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(),20.f);
        return Component;
    }
    ASovReformationDroneRocketProjectile* Rocket(ASovAxiomRuntimeTestCharacter* Source)
    {
        const auto& T=FSovGameplayTags::Get();
        auto* Result=World->SpawnActorDeferred<ASovReformationDroneRocketProjectile>(ASovReformationDroneRocketProjectile::StaticClass(),
            FTransform(FRotator::ZeroRotator,FVector(50,0,100)),Source,Source,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Result) { return nullptr; }
        Result->InitializeRocket(Source->GetNarrativeAbilitySystemComponent(),Source,Source,USovCombatRoutingTestEffect::StaticClass(),
            T.Ability_NPC_ReformationDrone_RocketLauncher,FGameplayTagContainer(T.Damage_Channel_Kinetic),FGameplayTagContainer(T.Damage_GuardClass_Standard),
            1.f,FVector(-1000,0,0),0.f,10.f,5.f,200.f,55.f,10.f,.3f,false,nullptr,0.f);
        Result->FinishSpawning(FTransform(FRotator::ZeroRotator,FVector(50,0,100))); return Result;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFinisherFallbackRuntimeTest,
    "ProjectVelkorran.Campaign.Finisher.FallbackAndOwnership",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovFinisherFallbackRuntimeTest::RunTest(const FString& Parameters)
{
    FFinisherWorld F; auto* Player=F.Character(0.f,0); auto* Enemy=F.Character(150.f,1);
    if (!Player || !Enemy) { return false; }
    auto* Component=F.Eligible(Enemy); auto* ASC=Player->GetNarrativeAbilitySystemComponent(); auto* EnemyASC=Enemy->GetNarrativeAbilitySystemComponent();
    const auto& T=FSovGameplayTags::Get(); const auto& N=FNarrativeGameplayTags::Get();
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovGameplayAbility_Finisher::StaticClass(),1));
    TestTrue(TEXT("Earned low-health state is available"),Component->IsAvailableFor(Player));
    TestTrue(TEXT("Native finisher activates without animation or nav assets"),ASC->TryActivateAbility(Handle,false));
    TestTrue(TEXT("Target has finite interruption protection"),EnemyASC->HasMatchingGameplayTag(T.State_InterruptProtected));
    TestFalse(TEXT("Reservation excludes another finisher"),Component->IsAvailableFor(Player));
    ASC->AddLooseGameplayTag(N.State_Busy); ASC->CancelAbilityHandle(Handle);
    TestTrue(TEXT("Cancellation releases target lease"),Component->IsAvailableFor(Player));
    TestFalse(TEXT("Cancellation removes its target protection"),EnemyASC->HasMatchingGameplayTag(T.State_InterruptProtected));
    TestEqual(TEXT("External Busy owner survives cleanup"),ASC->GetTagCount(N.State_Busy),1);
    ASC->RemoveLooseGameplayTag(N.State_Busy);
    Component->TargetKind=ESovFinisherTargetKind::Elite;
    TestFalse(TEXT("Unconfigured elite cannot be executed"),Component->IsAvailableFor(Player));
    Component->RequiredPhaseTag=T.State_Target_Exposed; EnemyASC->AddLooseGameplayTag(T.State_Target_Exposed);
    TestTrue(TEXT("Authored elite phase plus vulnerability allows a finisher"),Component->IsAvailableFor(Player));
    TestTrue(TEXT("Finite strike fallback activates"),ASC->TryActivateAbility(Handle,false));
    { TGuardValue<uint64> Frame(GFrameCounter,GFrameCounter+1); F.World->GetTimerManager().Tick(.4f); }
    TestEqual(TEXT("Fallback is nonlethal for an elite"),EnemyASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()),1.f);
    TestFalse(TEXT("Missing alignment cannot consume cinematic phase outcome"),Component->HasResolvedPhase(T.State_Target_Exposed));
    TestFalse(TEXT("Fallback returns action control within bounded duration"),ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFinisherCommittedOutcomeTest,
    "ProjectVelkorran.Campaign.Finisher.CommittedOutcomeSurvivesCancellation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovFinisherCommittedOutcomeTest::RunTest(const FString& Parameters)
{
    FFinisherWorld F; auto* Player=F.Character(0.f,0); auto* Enemy=F.Character(150.f,1);
    if (!TestNotNull(TEXT("Player fixture"),Player) || !TestNotNull(TEXT("Enemy fixture"),Enemy)) { return false; }
    auto* Component=F.Eligible(Enemy); auto* ASC=Player->GetNarrativeAbilitySystemComponent();
    auto* EnemyASC=Enemy->GetNarrativeAbilitySystemComponent(); const auto& T=FSovGameplayTags::Get();
    Component->TargetKind=ESovFinisherTargetKind::Elite; Component->RequiredPhaseTag=T.State_Target_Exposed;
    EnemyASC->AddLooseGameplayTag(T.State_Target_Exposed);
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovGameplayAbility_Finisher::StaticClass(),1));
    if (!TestTrue(TEXT("Real phase finisher activates"),ASC->TryActivateAbility(Handle,false))) { return false; }
    auto* Ability=Cast<USovGameplayAbility_Finisher>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    if (!TestNotNull(TEXT("Native finisher instance"),Ability)) { return false; }
    int32 OutcomeCount=0;
    bool bPreservedSource=false, bPreservedPhase=false;
    const auto EventHandle=EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(T.Event_Finisher_PhaseResolved)
        .AddLambda([&](const FGameplayEventData* Payload)
        {
            ++OutcomeCount;
            bPreservedSource=Payload && Payload->Instigator.Get()==Player && Payload->Target.Get()==Enemy;
            bPreservedPhase=Payload && Payload->TargetTags.HasTagExact(T.State_Target_Exposed);
        });
    const auto HealthHandle=EnemyASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute())
        .AddLambda([&](const FOnAttributeChangeData&) { ASC->CancelAbilityHandle(Handle); });
    FSovFinisherRuntimeTestAccess::AlignedStrike(Ability);
    EnemyASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute()).Remove(HealthHandle);
    TestFalse(TEXT("Health callback cancelled the action"),ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
    TestTrue(TEXT("Phase remains durably committed"),Component->HasResolvedPhase(T.State_Target_Exposed));
    TestEqual(TEXT("Committed phase still delivers one outcome"),OutcomeCount,1);
    TestTrue(TEXT("Outcome retains the source and target cleared during cancellation"),bPreservedSource);
    TestTrue(TEXT("Outcome retains the committed phase"),bPreservedPhase);
    TestEqual(TEXT("Elite strike remains nonlethal"),EnemyASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()),1.f);
    FSovFinisherRuntimeTestAccess::AlignedStrike(Ability);
    TestEqual(TEXT("A duplicate strike cannot replay the outcome"),OutcomeCount,1);
    TestFalse(TEXT("Committed phase cannot be reserved for repeated damage"),Component->IsAvailableFor(Player));
    EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(T.Event_Finisher_PhaseResolved).Remove(EventHandle);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFinisherRejectedDamageOutcomeTest,
    "ProjectVelkorran.Campaign.Finisher.RejectedDamageCannotCommitPhase",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovFinisherRejectedDamageOutcomeTest::RunTest(const FString& Parameters)
{
    FFinisherWorld F; auto* Player=F.Character(0.f,0); auto* Enemy=F.Character(150.f,1);
    if (!Player || !Enemy) { return false; }
    auto* Component=F.Eligible(Enemy); auto* ASC=Player->GetNarrativeAbilitySystemComponent();
    auto* EnemyASC=Enemy->GetNarrativeAbilitySystemComponent(); const auto& T=FSovGameplayTags::Get();
    Component->TargetKind=ESovFinisherTargetKind::Elite; Component->RequiredPhaseTag=T.State_Target_Exposed;
    EnemyASC->AddLooseGameplayTag(T.State_Target_Exposed);
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovGameplayAbility_Finisher::StaticClass(),1));
    if (!TestTrue(TEXT("Finisher activates"),ASC->TryActivateAbility(Handle,false))) { return false; }
    auto* Ability=Cast<USovGameplayAbility_Finisher>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    bool bVetoed=false, bPendingSaveRejected=false, bReplacementRejected=false;
    const int32 QueryIndex=EnemyASC->GameplayEffectApplicationQueries.Add(FGameplayEffectApplicationQuery::CreateLambda(
        [&](const FActiveGameplayEffectsContainer&,const FGameplayEffectSpec& Spec)
        {
            if (!Spec.Def || !Spec.Def->IsA<USovGameplayEffect_FinisherDamage>()) { return true; }
            bVetoed=true;
            FNarrativeSaveComponent Record;
            bPendingSaveRejected=!USovEncounterSnapshotLibrary::CaptureComponent(Component,Record);
            ASC->CancelAbilityHandle(Handle);
            ASC->TryActivateAbility(Handle,false);
            bReplacementRejected=!ASC->FindAbilitySpecFromHandle(Handle)->IsActive() && !Component->IsAvailableFor(Player);
            return false;
        }));
    FSovFinisherRuntimeTestAccess::AlignedStrike(Ability);
    EnemyASC->GameplayEffectApplicationQueries.RemoveAt(QueryIndex);
    TestTrue(TEXT("GAS veto occurs after native preflight"),bVetoed);
    TestTrue(TEXT("No partial damage/outcome save admitted"),bPendingSaveRejected);
    TestTrue(TEXT("Cancellation cannot reserve target during pending damage"),bReplacementRejected);
    TestFalse(TEXT("Rejected damage cannot consume phase"),Component->HasResolvedPhase(T.State_Target_Exposed));
    TestEqual(TEXT("Rejected damage leaves Health intact"),EnemyASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()),20.f);
    TestTrue(TEXT("Rejected transaction releases pending ownership"),Component->IsAvailableFor(Player));
    FNarrativeSaveComponent Record;
    TestTrue(TEXT("Save capture is available again after outcome settles"),USovEncounterSnapshotLibrary::CaptureComponent(Component,Record));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFinisherZeroDamageOutcomeTest,
    "ProjectVelkorran.Campaign.Finisher.ZeroDamageCannotCommitPhase",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovFinisherZeroDamageOutcomeTest::RunTest(const FString& Parameters)
{
    FFinisherWorld F; auto* Player=F.Character(0.f,0); auto* Enemy=F.Character(150.f,1);
    if (!Player || !Enemy) { return false; }
    auto* Component=F.Eligible(Enemy); auto* ASC=Player->GetNarrativeAbilitySystemComponent();
    auto* EnemyASC=Enemy->GetNarrativeAbilitySystemComponent(); const auto& T=FSovGameplayTags::Get();
    Component->TargetKind=ESovFinisherTargetKind::Elite; Component->RequiredPhaseTag=T.State_Target_Exposed;
    EnemyASC->AddLooseGameplayTag(T.State_Target_Exposed);
    EnemyASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(),1.f);
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovGameplayAbility_Finisher::StaticClass(),1));
    if (!TestTrue(TEXT("Earned one-Health phase activates"),ASC->TryActivateAbility(Handle,false))) { return false; }
    auto* Ability=Cast<USovGameplayAbility_Finisher>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    FSovFinisherRuntimeTestAccess::AlignedStrike(Ability);
    TestFalse(TEXT("Nonlethal zero payload does not award a phase"),Component->HasResolvedPhase(T.State_Target_Exposed));
    TestTrue(TEXT("Zero strike releases target ownership"),Component->IsAvailableFor(Player));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFinisherReboundTargetOutcomeTest,
    "ProjectVelkorran.Campaign.Finisher.TargetGenerationOwnsOutcome",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovFinisherReboundTargetOutcomeTest::RunTest(const FString& Parameters)
{
    FFinisherWorld F; auto* Player=F.Character(0.f,0); auto* Enemy=F.Character(150.f,1);
    if (!Player || !Enemy) { return false; }
    auto* Component=F.Eligible(Enemy); auto* ASC=Player->GetNarrativeAbilitySystemComponent();
    auto* EnemyASC=Enemy->GetNarrativeAbilitySystemComponent(); const auto& T=FSovGameplayTags::Get();
    Component->TargetKind=ESovFinisherTargetKind::Elite; Component->RequiredPhaseTag=T.State_Target_Exposed;
    EnemyASC->AddLooseGameplayTag(T.State_Target_Exposed);
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovGameplayAbility_Finisher::StaticClass(),1));
    if (!ASC->TryActivateAbility(Handle,false)) { return false; }
    auto* Ability=Cast<USovGameplayAbility_Finisher>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    const auto HealthHandle=EnemyASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute())
        .AddLambda([&](const FOnAttributeChangeData&)
        {
            EnemyASC->ClearActorInfo(); EnemyASC->InitAbilityActorInfo(Enemy,Enemy);
        });
    FSovFinisherRuntimeTestAccess::AlignedStrike(Ability);
    EnemyASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute()).Remove(HealthHandle);
    TestFalse(TEXT("Same-pointer rebind cannot receive the old phase"),Component->HasResolvedPhase(T.State_Target_Exposed));
    ASC->CancelAbilityHandle(Handle);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRocketReflectionRuntimeTest,
    "ProjectVelkorran.Campaign.Projectile.PhysicalReflection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovRocketReflectionRuntimeTest::RunTest(const FString& Parameters)
{
    FFinisherWorld F; auto* Shooter=F.Character(100.f,1); auto* Defender=F.Character(0.f,0);
    if (!Shooter || !Defender) { return false; }
    auto* Rocket=F.Rocket(Shooter); if (!Rocket) { return false; }
    Defender->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FSovGameplayTags::Get().State_Deflecting);
    FSovProjectileDefenseTestAccess::Impact(Rocket,Defender);
    TestFalse(TEXT("Perfect deflection prevents the radial explosion"),Rocket->HasResolved());
    TestTrue(TEXT("Collision projectile ownership transfers to actual defender"),FSovProjectileDefenseTestAccess::Source(Rocket)==Defender);
    TestTrue(TEXT("Typed damage result proves real defense"),Defender->LastDamageResult.bDeflected);
    TestEqual(TEXT("Defender does not take blast damage"),Defender->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()),100.f);
    FSovProjectileDefenseTestAccess::Resume(Rocket);
    FSovProjectileDefenseTestAccess::Impact(Rocket,Shooter);
    TestTrue(TEXT("Reflected projectile still resolves on a subsequent physical hit"),Rocket->HasResolved());
    TestEqual(TEXT("Original shooter receives exactly one direct payload"),Shooter->LastDamageResult.AppliedShieldDamage,55.f);
    TestEqual(TEXT("Direct target is excluded from radial duplicate damage"),Shooter->ResolvedHitCount,1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFinisherReentrantProtectionTest,
    "ProjectVelkorran.Campaign.Finisher.ReentrantProtectionOwnership",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovFinisherReentrantProtectionTest::RunTest(const FString& Parameters)
{
    FFinisherWorld F; auto* Player=F.Character(0.f,0); auto* Enemy=F.Character(150.f,1);
    if (!Player||!Enemy) { return false; }
    F.Eligible(Enemy); auto* ASC=Player->GetNarrativeAbilitySystemComponent();
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovGameplayAbility_Finisher::StaticClass(),1));
    bool bReentered=false, bRestarted=false;
    const auto Delegate=ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddLambda(
        [&](UAbilitySystemComponent*,const FGameplayEffectSpec& Spec,FActiveGameplayEffectHandle)
        {
            if (bReentered || !Spec.Def || !Spec.Def->IsA<USovGameplayEffect_FinisherProtection>()) { return; }
            bReentered=true; ASC->CancelAbilityHandle(Handle); bRestarted=ASC->TryActivateAbility(Handle,false);
        });
    ASC->TryActivateAbility(Handle,false);
    ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(Delegate);
    TestTrue(TEXT("A protection callback can replace the action"),bReentered&&bRestarted);
    TestTrue(TEXT("The old activation does not end the replacement"),ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
    const auto& T=FSovGameplayTags::Get();
    TestEqual(TEXT("Only the replacement owns player interruption protection"),ASC->GetTagCount(T.State_InterruptProtected),1);
    ASC->CancelAbilityHandle(Handle);
    TestFalse(TEXT("Cancelled protection callbacks do not leak a player effect"),ASC->HasMatchingGameplayTag(T.State_InterruptProtected));
    TestFalse(TEXT("Replacement cleanup also releases target protection"),Enemy->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(T.State_InterruptProtected));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRocketAbsorptionRuntimeTest,
    "ProjectVelkorran.Campaign.Projectile.PerfectGuardAbsorption",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovRocketAbsorptionRuntimeTest::RunTest(const FString& Parameters)
{
    FFinisherWorld F; auto* Shooter=F.Character(100.f,1); auto* Defender=F.Character(0.f,0);
    if (!Shooter || !Defender) { return false; }
    auto* Rocket=F.Rocket(Shooter); if (!Rocket) { return false; }
    auto* ASC=Defender->GetNarrativeAbilitySystemComponent(); const auto& T=FSovGameplayTags::Get();
    ASC->AddLooseGameplayTag(T.State_Guarding); ASC->AddLooseGameplayTag(T.State_PerfectGuard);
    FSovProjectileDefenseTestAccess::Impact(Rocket,Defender);
    TestTrue(TEXT("Perfect Guard dissipates physical projectile"),Rocket->GetResolution().bAbsorbed);
    TestTrue(TEXT("Absorption is a final resolution"),Rocket->HasResolved());
    TestFalse(TEXT("Absorption does not execute explosion damage"),Rocket->GetResolution().bDamagedAnyTarget);
    TestEqual(TEXT("Defender shield remains intact"),ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()),100.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRocketReflectionExpiryTest,
    "ProjectVelkorran.Campaign.Projectile.ReflectionRetainsLaunchExpiry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovRocketReflectionExpiryTest::RunTest(const FString& Parameters)
{
    FFinisherWorld F; auto* Shooter=F.Character(100.f,1); auto* Defender=F.Character(0.f,0);
    if (!Shooter||!Defender) { return false; }
    auto* Rocket=F.Rocket(Shooter); if (!Rocket) { return false; }
    Defender->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FSovGameplayTags::Get().State_Deflecting);
    FSovProjectileDefenseTestAccess::Impact(Rocket,Defender);
    FSovProjectileDefenseTestAccess::Expire(Rocket);
    FSovProjectileDefenseTestAccess::Resume(Rocket);
    TestTrue(TEXT("An expiry during deferred reflection is consumed before flight resumes"),Rocket->HasResolved()&&Rocket->GetResolution().bExpired);
    TestFalse(TEXT("Expired reflected rocket does not explode"),Rocket->GetResolution().bDamagedAnyTarget);
    return true;
}
#endif
