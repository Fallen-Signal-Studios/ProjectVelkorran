// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Abilities/SovGameplayAbility_Finisher.h"
#include "Combat/SovFinisherTargetComponent.h"
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
struct FSovFinisherOutcomeTestAccess
{
    static void StrikeAligned(USovGameplayAbility_Finisher* Ability) { Ability->bAligned = true; Ability->Strike(); }
    static bool Pending(const USovFinisherTargetComponent* Target, FGameplayTag Phase) { return Target->PendingPhaseOutcomes.HasTagExact(Phase); }
    static void Publish(USovFinisherTargetComponent* Target, FGameplayTag Phase) { Target->PublishCommittedPhase(Phase, nullptr, FGameplayEffectContextHandle()); }
    static void RestorePending(USovFinisherTargetComponent* Target, FGameplayTag Phase) { Target->PendingPhaseOutcomes.AddTag(Phase); Target->Load_Implementation(); }
};
struct FSovProjectileDefenseTestAccess
{
    static void Impact(ASovReformationDroneRocketProjectile* Rocket,AActor* Target)
    {
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
        World=UWorld::CreateWorld(EWorldType::Game,false);
        if (World)
        {
            if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
            World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
                .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFinisherCommittedPhaseCancellationTest,
    "ProjectVelkorran.Campaign.Finisher.CommittedPhaseCancellationAndRestore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovFinisherCommittedPhaseCancellationTest::RunTest(const FString& Parameters)
{
    FFinisherWorld F; auto* Player = F.Character(0.f, 0); auto* Enemy = F.Character(150.f, 1);
    if (!Player || !Enemy) { return false; }
    auto* Component = F.Eligible(Enemy); auto* ASC = Player->GetNarrativeAbilitySystemComponent(); auto* TargetASC = Enemy->GetNarrativeAbilitySystemComponent();
    const auto& T = FSovGameplayTags::Get();
    Component->TargetKind = ESovFinisherTargetKind::Elite; Component->RequiredPhaseTag = T.State_Target_Exposed;
    Component->PhaseDamage = 5.f; TargetASC->AddLooseGameplayTag(T.State_Target_Exposed);
    const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(USovGameplayAbility_Finisher::StaticClass(), 1));
    if (!ASC->TryActivateAbility(Handle, false)) { AddError(TEXT("Real finisher reservation failed")); return false; }
    auto* Ability = Cast<USovGameplayAbility_Finisher>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    if (!Ability) { return false; }
    int32 PhaseEvents = 0; bool bPendingDuringDamage = false;
    const auto EventHandle = TargetASC->GenericGameplayEventCallbacks.FindOrAdd(T.Event_Finisher_PhaseResolved).AddLambda(
        [&](const FGameplayEventData* Event)
        {
            if (Event && Event->TargetTags.HasTagExact(T.State_Target_Exposed)) { ++PhaseEvents; }
            FSovFinisherOutcomeTestAccess::Publish(Component, T.State_Target_Exposed);
        });
    auto& HealthDelegate = TargetASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute());
    const auto DamageHandle = HealthDelegate.AddLambda([&](const FOnAttributeChangeData& Change)
    {
        if (Change.NewValue >= Change.OldValue) { return; }
        bPendingDuringDamage = FSovFinisherOutcomeTestAccess::Pending(Component, T.State_Target_Exposed);
        ASC->CancelAbilityHandle(Handle);
    });
    // Seed only the authored alignment prerequisite. Strike, damage, cancellation,
    // target reservation and phase dispatch all execute their real production paths.
    FSovFinisherOutcomeTestAccess::StrikeAligned(Ability);
    HealthDelegate.Remove(DamageHandle);
    TestTrue(TEXT("Durable pending outcome exists during damage callbacks"), bPendingDuringDamage);
    TestFalse(TEXT("Damage callback cancels the actual ability"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
    TestTrue(TEXT("Committed phase is retained despite animation cancellation"), Component->HasResolvedPhase(T.State_Target_Exposed));
    TestEqual(TEXT("Committed phase dispatch occurs once after cancellation"), PhaseEvents, 1);
    TestFalse(TEXT("Delivery consumes its pending outbox"), FSovFinisherOutcomeTestAccess::Pending(Component, T.State_Target_Exposed));
    Component->Load_Implementation();
    { TGuardValue<uint64> Frame(GFrameCounter, GFrameCounter + 1); F.World->GetTimerManager().Tick(.1f); }
    TestEqual(TEXT("Loading a delivered phase does not replay it"), PhaseEvents, 1);
    // This is the state a checkpoint taken inside the damage delegate serializes.
    FSovFinisherOutcomeTestAccess::RestorePending(Component, T.State_Target_Exposed);
    { TGuardValue<uint64> Frame(GFrameCounter, GFrameCounter + 1); F.World->GetTimerManager().Tick(.1f); }
    TestEqual(TEXT("Restoring an undelivered committed phase completes delivery"), PhaseEvents, 2);
    TargetASC->InitAbilityActorInfo(Enemy, nullptr);
    FSovFinisherOutcomeTestAccess::RestorePending(Component, T.State_Target_Exposed);
    { TGuardValue<uint64> Frame(GFrameCounter, GFrameCounter + 1); F.World->GetTimerManager().Tick(.1f); }
    TestEqual(TEXT("An uninitialized ASC cannot consume pending outcome delivery"), PhaseEvents, 2);
    TestTrue(TEXT("Pending outcome is retained until Narrative ASC readiness"), FSovFinisherOutcomeTestAccess::Pending(Component, T.State_Target_Exposed));
    TargetASC->InitAbilityActorInfo(Enemy, Enemy); Enemy->OnASCInitialized.Broadcast();
    { TGuardValue<uint64> Frame(GFrameCounter, GFrameCounter + 1); F.World->GetTimerManager().Tick(.1f); }
    TestEqual(TEXT("Late Narrative readiness completes retained phase outcome"), PhaseEvents, 3);
    TargetASC->GenericGameplayEventCallbacks.FindChecked(T.Event_Finisher_PhaseResolved).Remove(EventHandle);
    return true;
}
#endif
