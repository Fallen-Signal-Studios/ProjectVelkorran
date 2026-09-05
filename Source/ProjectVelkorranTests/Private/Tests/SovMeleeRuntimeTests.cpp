// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovMeleeRuntimeTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Components/SovPoiseComponent.h"
#include "Melee/SovAbilityTask_MeleeSweep.h"
#include "Melee/SovMeleeAttackDefinition.h"
#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#if WITH_AUTOMATION_TESTS
struct FSovMeleeRuntimeTestAccess
{
    static void Tick(USovGameplayAbility_Melee* Ability,float Delta) { if (Ability->SweepTask) { Ability->SweepTask->TickTask(Delta); } }
    static int32 Node(USovGameplayAbility_Melee* Ability) { return Ability->NodeIndex; }
    static USovAbilityTask_MeleeSweep* AdditionalTask(USovGameplayAbility_Melee* Ability,USkeletalMeshComponent* Mesh)
    {
        auto* Task=USovAbilityTask_MeleeSweep::SweepMeleeSockets(Ability,Mesh,Ability->AttackDefinition->Nodes[Ability->NodeIndex]);
        Task->OnInvalidated.AddDynamic(Ability,&USovGameplayAbility_Melee::OnTaskInvalidated);
        Task->ReadyForActivation(); return Task;
    }
};
namespace
{
struct FMeleeWorld
{
    UWorld* World=nullptr;
    FMeleeWorld()
    {
        World=UWorld::CreateWorld(EWorldType::Game,false);
        if (World)
        {
            if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
            World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
                .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
        }
    }
    ~FMeleeWorld() { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
    ASovAxiomRuntimeTestCharacter* Character(FVector Location,int32 Team)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Actor=World?World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(),Location,FRotator::ZeroRotator,Spawn):nullptr;
        if (Actor) { Actor->InitializeTestCombat(Team); Actor->GetCapsuleComponent()->SetCollisionResponseToChannel(UArsenalStatics::GetNarrativeProSettings()->WeaponTraceChannel,ECR_Block); }
        return Actor;
    }
    void Advance(USovGameplayAbility_Melee* Ability,float Seconds)
    {
        TGuardValue<uint64> Frame(GFrameCounter,GFrameCounter+1); World->Tick(LEVELTICK_TimeOnly,Seconds);
        FSovMeleeRuntimeTestAccess::Tick(Ability,Seconds);
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovMeleeSweepAndLedgerRuntimeTest,
    "ProjectVelkorran.Campaign.Melee.SocketSweepLedgerAndFiniteBranch",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovMeleeSweepAndLedgerRuntimeTest::RunTest(const FString& Parameters)
{
    FMeleeWorld F; auto* Source=F.Character(FVector(0,0,100),0); auto* Target=F.Character(FVector(90,0,100),1);
    if (!Source||!Target) { return false; }
    auto* Mesh=NewObject<USovMeleeRuntimeTestMesh>(Source); Source->AddInstanceComponent(Mesh);
    Mesh->SetupAttachment(Source->GetRootComponent()); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->RegisterComponent();
    auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovMeleeRuntimeTestAbility::StaticClass(),1));
    TestTrue(TEXT("Native definition admits attack"),ASC->TryActivateAbility(Handle,false));
    auto* Ability=Cast<USovMeleeRuntimeTestAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    if (!TestNotNull(TEXT("Ability instance"),Ability)) { return false; }
    auto* StaleTask=FSovMeleeRuntimeTestAccess::AdditionalTask(Ability,Mesh);
    F.Advance(Ability,.15f); Mesh->SetWorldLocation(FVector(180,0,100)); F.Advance(Ability,.1f);
    TestEqual(TEXT("A fast socket sweep catches crossed target"),Target->ResolvedHitCount,1);
    TestEqual(TEXT("Actual native packet reaches existing Shield routing"),Target->LastDamageResult.AppliedShieldDamage,20.f);
    const FGuid FirstAttack=Target->LastDamageResult.AttackId;
    TestTrue(TEXT("Native melee publishes active attack identity"),FirstAttack.IsValid());
    Mesh->SetWorldLocation(FVector(0,0,100)); F.Advance(Ability,.05f);
    TestEqual(TEXT("Returning blade cannot hit target twice in one attack"),Target->ResolvedHitCount,1);
    ASC->AbilityInputTagPressed(FNarrativeGameplayTags::Get().Narrative_Input_Attack); F.Advance(Ability,.051f);
    TestEqual(TEXT("A fresh authored branch advances one node"),FSovMeleeRuntimeTestAccess::Node(Ability),1);
    StaleTask->TickTask(.001f);
    TestTrue(TEXT("A stale task cannot invalidate the renewed attack on the same instance"),ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
    F.Advance(Ability,.15f); Mesh->SetWorldLocation(FVector(180,0,100)); F.Advance(Ability,.1f);
    TestEqual(TEXT("A new attack owns a fresh hit ledger"),Target->ResolvedHitCount,2);
    TestTrue(TEXT("New node renews attack identity"),Target->LastDamageResult.AttackId!=FirstAttack);
    ASC->AbilityInputTagPressed(FNarrativeGameplayTags::Get().Narrative_Input_Attack); F.Advance(Ability,.5f);
    TestFalse(TEXT("Last node cannot become an infinite held-input chain"),ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovMeleeDefinitionValidationTest,
    "ProjectVelkorran.Campaign.Melee.DefinitionValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovMeleeDefinitionValidationTest::RunTest(const FString& Parameters)
{
    auto* Definition=NewObject<USovMeleeAttackDefinition>(); FString Error;
    Definition->Nodes.Add(FSovMeleeAttackNode()); TestTrue(TEXT("Prototype node windows validate"),Definition->Validate(Error));
    Definition->Nodes[0].NextNode=0; Definition->Nodes[0].FollowUpInput=FNarrativeGameplayTags::Get().Narrative_Input_Attack;
    TestFalse(TEXT("Self-loop cannot create an infinite light chain"),Definition->Validate(Error));
    Definition->Nodes[0].NextNode=INDEX_NONE; Definition->Nodes[0].Active=-1;
    TestFalse(TEXT("Negative collision window rejects definition"),Definition->Validate(Error));
    Definition->Nodes[0]=FSovMeleeAttackNode(); Definition->Nodes[0].MaximumAimCorrection=90.f;
    TestFalse(TEXT("Target correction is bounded by the native contract"),Definition->Validate(Error));
    Definition->Nodes[0]=FSovMeleeAttackNode(); Definition->Nodes[0].AttackClassifications.AddTag(FSovGameplayTags::Get().Damage_Fatal);
    TestFalse(TEXT("An ordinary melee definition cannot turn into an explicit fatal packet"),Definition->Validate(Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovMeleeCoverRuntimeTest,
    "ProjectVelkorran.Campaign.Melee.AnimatedBladeCannotReachThroughCover",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovMeleeCoverRuntimeTest::RunTest(const FString& Parameters)
{
    FMeleeWorld F; auto* Source=F.Character(FVector(0,0,100),0); auto* Target=F.Character(FVector(180,0,100),1);
    if (!Source||!Target) { return false; }
    auto* Mesh=NewObject<USovMeleeRuntimeTestMesh>(Source); Source->AddInstanceComponent(Mesh);
    Mesh->SetupAttachment(Source->GetRootComponent()); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->RegisterComponent();
    AActor* Wall=F.World->SpawnActor<AActor>(); if (!Wall) { return false; }
    auto* Box=NewObject<UBoxComponent>(Wall); Wall->AddInstanceComponent(Box); Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(10,300,150)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Ignore);
    Box->SetCollisionResponseToChannel(UArsenalStatics::GetNarrativeProSettings()->WeaponTraceChannel,ECR_Block);
    Box->RegisterComponent(); Wall->SetActorLocation(FVector(90,0,100));
    auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovMeleeRuntimeTestAbility::StaticClass(),1));
    TestTrue(TEXT("Attack begins on the source side of cover"),ASC->TryActivateAbility(Handle,false));
    auto* Ability=Cast<USovMeleeRuntimeTestAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    if (!Ability) { return false; }
    // During startup the authored pose places the weapon beyond the blocking wall.
    Mesh->SetWorldLocation(FVector(180,0,100)); F.Advance(Ability,.1f); F.Advance(Ability,.15f);
    TestEqual(TEXT("Independent source-to-contact geometry rejects the occluded contact"),Target->ResolvedHitCount,0);
    ASC->CancelAbilityHandle(Handle);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovMeleePoiseInterruptionRepairTest,
    "ProjectVelkorran.Campaign.Repairs.Player.MeleePoiseInterruptionPhases",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovMeleePoiseInterruptionRepairTest::RunTest(const FString& Parameters)
{
    for (float BeforeBreak : {0.f,.18f,.36f})
    {
        FMeleeWorld F; auto* Source=F.Character(FVector(0,0,100),0); auto* Target=F.Character(FVector(90,0,100),1);
        if (!Source||!Target) { return false; }
        auto* Mesh=NewObject<USovMeleeRuntimeTestMesh>(Source); Source->AddInstanceComponent(Mesh);
        Mesh->SetupAttachment(Source->GetRootComponent()); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->RegisterComponent();
        auto* ASC=Source->GetNarrativeAbilitySystemComponent();
        auto* Poise=NewObject<USovPoiseComponent>(Source); Source->AddInstanceComponent(Poise); Poise->RegisterComponent();
        if (!TestTrue(TEXT("Real poise component binds source ASC"),Poise->InitializeWithAbilitySystem(ASC))) { return false; }
        const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovMeleeRuntimeTestAbility::StaticClass(),1));
        TestTrue(TEXT("Attack starts before incoming poise packet"),ASC->TryActivateAbility(Handle,false));
        auto* Ability=Cast<USovMeleeRuntimeTestAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
        if (!Ability) { return false; }
        if (BeforeBreak>0.f) { F.Advance(Ability,BeforeBreak); }
        const int32 PriorHits=Target->ResolvedHitCount;
        auto* EnemyASC=Target->GetNarrativeAbilitySystemComponent(); auto Context=EnemyASC->MakeEffectContext(); Context.AddInstigator(Target,Target);
        FGameplayEffectSpec Spec(GetDefault<USovCombatRoutingTestEffect>(),Context,1.f);
        Spec.AddDynamicAssetTag(FSovGameplayTags::Get().Damage_Channel_Kinetic);
        Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage,1.f);
        Spec.SetSetByCallerMagnitude(FSovGameplayTags::Get().SetByCaller_Damage_PoiseDamage,200.f);
        EnemyASC->ApplyGameplayEffectSpecToTarget(Spec,ASC);
        TestTrue(TEXT("Real damage packet reports a poise break"),Source->LastDamageResult.bPoiseBroken);
        TestFalse(TEXT("Poise break interrupts startup, active or recovery immediately"),Ability->IsActive());
        TestFalse(TEXT("Interrupted melee releases owned Busy tag"),ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy));
        Mesh->SetWorldLocation(FVector(180,0,100)); F.Advance(Ability,.25f);
        TestEqual(TEXT("Old sweep cannot damage after interruption"),Target->ResolvedHitCount,PriorHits);
    }
    return true;
}
#endif
