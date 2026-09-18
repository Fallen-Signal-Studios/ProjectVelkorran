// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovMeleeRuntimeTestFixtures.h"
#include "World/SovDestructibleCover.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Melee/SovAbilityTask_MeleeSweep.h"
#include "Melee/SovMeleeAttackDefinition.h"
#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Character/PlayerDefinition.h"
#include "Framework/SovPlayerState.h"
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
    static bool ArmSuperArmor(USovGameplayAbility_Melee* Ability,float Open,float Close)
    {
        if (!Ability||!Ability->AttackDefinition||Ability->AttackDefinition->Nodes.IsEmpty()) { return false; }
        for (FSovMeleeAttackNode& Node : Ability->AttackDefinition->Nodes)
        { Node.SuperArmorOpen=Open; Node.SuperArmorClose=Close; }
        FString Error; return Ability->AttackDefinition->Validate(Error);
    }
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
        const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World=UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
        if (World)
        {
            if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
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
    ASovMeleeRuntimeTestPlayer* ReadyPlayer(FVector Location)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Player = World->SpawnActor<ASovMeleeRuntimeTestPlayer>(ASovMeleeRuntimeTestPlayer::StaticClass(), Location, FRotator::ZeroRotator, Spawn);
        auto* Controller = World->SpawnActor<ASovHandoffRuntimeTestController>();
        auto* State = World->SpawnActor<ASovPlayerState>();
        if (!Player || !Controller || !State) { return nullptr; }
        auto* Definition = NewObject<UPlayerDefinition>(Controller); Controller->KeepAlive.Add(Definition);
        Player->PrepareCampaignInitialization(Definition); Controller->SetTestPlayerState(State); Controller->Possess(Player);
        return Player->StageTestReadiness(State, true) && Player->CompleteCampaignDataInitialization(false) ? Player : nullptr;
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
    FMeleeWorld F; auto* Source=F.ReadyPlayer(FVector(0,0,100)); auto* Target=F.Character(FVector(90,0,100),1);
    if (!Source||!Target) { return false; }
    auto* Mesh=NewObject<USovMeleeRuntimeTestMesh>(Source); Source->AddInstanceComponent(Mesh);
    Mesh->SetupAttachment(Source->GetRootComponent()); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->RegisterComponent();
    auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovMeleeRuntimeTestAbility::StaticClass(),1));
    TestTrue(TEXT("Native definition admits attack"),ASC->TryActivateAbility(Handle,false));
    auto* Ability=Cast<USovMeleeRuntimeTestAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    if (!TestNotNull(TEXT("Ability instance"),Ability)) { return false; }
    TestTrue(TEXT("Combo input belongs to a ready possessed campaign player"), Source->IsCharacterReady() && Source->GetController() != nullptr);
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovMeleeTraceSegmentsRuntimeTest,
    "ProjectVelkorran.Campaign.Melee.OffsetAndAdditionalEdgesShareOneLedger",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovMeleeTraceSegmentsRuntimeTest::RunTest(const FString& Parameters)
{
    // Targets are capsules of radius 34 on one line the blade crosses. Near (Y=-50) touches only the
    // offset primary edge, Far (Y=+50) only the added edge, and Middle (Y=0) is reached by both edges.
    FMeleeWorld F; auto* Source=F.ReadyPlayer(FVector(0,0,100));
    auto* Near=F.Character(FVector(90,-50,100),1); auto* Middle=F.Character(FVector(90,0,100),1); auto* Far=F.Character(FVector(90,50,100),1);
    if (!Source||!Near||!Middle||!Far) { return false; }
    auto* Mesh=NewObject<USovMeleeRuntimeTestMesh>(Source); Source->AddInstanceComponent(Mesh);
    Mesh->SetupAttachment(Source->GetRootComponent()); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->RegisterComponent();
    auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovMeleeRuntimeTestSegmentAbility::StaticClass(),1));
    TestTrue(TEXT("A node with offset and added edges admits the attack"),ASC->TryActivateAbility(Handle,false));
    auto* Ability=Cast<USovMeleeRuntimeTestSegmentAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    if (!TestNotNull(TEXT("Ability instance"),Ability)) { return false; }
    F.Advance(Ability,.15f); Mesh->SetWorldLocation(FVector(180,0,100)); F.Advance(Ability,.1f);
    TestEqual(TEXT("The offset primary edge reaches a target beyond its bare socket"),Near->ResolvedHitCount,1);
    TestEqual(TEXT("The added edge is swept as well"),Far->ResolvedHitCount,1);
    TestEqual(TEXT("A target reached by both edges is hit once"),Middle->ResolvedHitCount,1);
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
    Definition->Nodes[0]=FSovMeleeAttackNode(); Definition->Nodes[0].EndOffset=FVector(0,0,400);
    TestFalse(TEXT("A socket offset cannot reach beyond the weapon"),Definition->Validate(Error));
    Definition->Nodes[0]=FSovMeleeAttackNode(); Definition->Nodes[0].StartOffset=FVector(std::numeric_limits<double>::quiet_NaN(),0,0);
    TestFalse(TEXT("A non-finite socket offset rejects the definition"),Definition->Validate(Error));
    Definition->Nodes[0]=FSovMeleeAttackNode(); Definition->Nodes[0].AdditionalSegments.AddDefaulted();
    TestFalse(TEXT("An added edge must name both of its sockets"),Definition->Validate(Error));
    Definition->Nodes[0]=FSovMeleeAttackNode();
    FSovMeleeTraceSegment Edge; Edge.StartSocket=TEXT("blade_root"); Edge.EndSocket=TEXT("blade_tip");
    Definition->Nodes[0].AdditionalSegments.Init(Edge,3);
    TestTrue(TEXT("Up to three added edges validate"),Definition->Validate(Error));
    Definition->Nodes[0].AdditionalSegments.Add(Edge);
    TestFalse(TEXT("Added edges are bounded"),Definition->Validate(Error));

    // A super-armour window is optional, and absent has to stay valid or every node authored before
    // the field existed would fail the day it was added (audit PC2-13).
    Definition->Nodes[0]=FSovMeleeAttackNode();
    TestTrue(TEXT("A node with no super-armour window is still a valid node"),Definition->Validate(Error));
    Definition->Nodes[0].SuperArmorOpen=.05f; Definition->Nodes[0].SuperArmorClose=.3f;
    TestTrue(TEXT("A window inside the node validates"),Definition->Validate(Error));
    Definition->Nodes[0].SuperArmorClose=99.f;
    TestFalse(TEXT("A window outliving its node is refused, since the ability that owns the tag ends first"),
        Definition->Validate(Error));
    Definition->Nodes[0].SuperArmorOpen=.4f; Definition->Nodes[0].SuperArmorClose=.2f;
    TestFalse(TEXT("A window that closes before it opens is refused"),Definition->Validate(Error));
    Definition->Nodes[0].SuperArmorOpen=-1.f; Definition->Nodes[0].SuperArmorClose=.2f;
    TestFalse(TEXT("A window opening before the node does is refused"),Definition->Validate(Error));

    // The predicate the ability consults, checked at its edges: half-open on close, so two adjacent
    // windows can never both be active on the same frame.
    FSovMeleeAttackNode Window; Window.SuperArmorOpen=.1f; Window.SuperArmorClose=.25f;
    TestFalse(TEXT("Before the window opens there is no armour"),Window.IsSuperArmored(.099f));
    TestTrue(TEXT("The opening edge is armoured"),Window.IsSuperArmored(.1f));
    TestTrue(TEXT("The middle is armoured"),Window.IsSuperArmored(.2f));
    TestFalse(TEXT("The closing edge is already unarmoured"),Window.IsSuperArmored(.25f));
    FSovMeleeAttackNode None;
    TestFalse(TEXT("A node with no window is never armoured"),None.IsSuperArmored(0.f));
    TestFalse(TEXT("...at any time"),None.IsSuperArmored(10.f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovMeleeSuperArmorTest,
    "ProjectVelkorran.Campaign.Melee.CommittedAttacksHoldSuperArmor",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovMeleeSuperArmorTest::RunTest(const FString& Parameters)
{
    FMeleeWorld F;
    auto* Source=F.ReadyPlayer(FVector(0,0,100));
    if (!TestNotNull(TEXT("Ready melee player"),Source)) { return false; }
    auto* Mesh=NewObject<USovMeleeRuntimeTestMesh>(Source); Source->AddInstanceComponent(Mesh);
    Mesh->SetupAttachment(Source->GetRootComponent()); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->RegisterComponent();
    auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    const FGameplayTag Armor=FSovGameplayTags::Get().State_Poise_SuperArmor;

    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovMeleeRuntimeTestAbility::StaticClass(),1));
    if (!TestTrue(TEXT("The attack activates"),ASC->TryActivateAbility(Handle,false))) { return false; }
    auto* Ability=Cast<USovMeleeRuntimeTestAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    if (!TestNotNull(TEXT("Ability instance"),Ability)) { return false; }

    // Armour through the wind-up and the swing, released before recovery: the committed part of a
    // committed action, which is what TDD 6.6 asks for and what nothing produced before (PC2-13).
    if (!TestTrue(TEXT("The node arms with a window inside itself"),
        FSovMeleeRuntimeTestAccess::ArmSuperArmor(Ability,.05f,.35f))) { return false; }
    TestFalse(TEXT("An attack does not begin armoured"),ASC->HasMatchingGameplayTag(Armor));

    // The failure that would matter most first, on its own activation: a tag left behind by a
    // cancelled attack makes the protagonist unstaggerable for the rest of the run.
    F.Advance(Ability,.1f);
    TestTrue(TEXT("The wind-up is armoured once the window opens"),ASC->HasMatchingGameplayTag(Armor));
    ASC->CancelAbilityHandle(Handle);
    TestFalse(TEXT("Cancelling mid-window releases the armour rather than leaking it"),
        ASC->HasMatchingGameplayTag(Armor));

    // A fresh attack, run all the way through, for the shape of the window itself.
    if (!TestTrue(TEXT("A later attack activates once the first has ended"),ASC->TryActivateAbility(Handle,false)))
    { return false; }
    auto* Second=Cast<USovMeleeRuntimeTestAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    if (!TestNotNull(TEXT("Second ability instance"),Second)) { return false; }
    if (!TestTrue(TEXT("...and arms"),FSovMeleeRuntimeTestAccess::ArmSuperArmor(Second,.05f,.35f))) { return false; }
    F.Advance(Second,.1f);
    TestTrue(TEXT("The wind-up is armoured"),ASC->HasMatchingGameplayTag(Armor));
    F.Advance(Second,.15f);
    TestTrue(TEXT("The swing stays armoured"),ASC->HasMatchingGameplayTag(Armor));
    F.Advance(Second,.15f);
    TestFalse(TEXT("Recovery is unarmoured, so a committed attack stays punishable afterwards"),
        ASC->HasMatchingGameplayTag(Armor));
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovMeleeDestructibleCoverRuntimeTest,
    "ProjectVelkorran.World.Destruction.MeleeBladeBreaksCoverWithoutReachingThrough",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovMeleeDestructibleCoverRuntimeTest::RunTest(const FString& Parameters)
{
    FMeleeWorld F; auto* Source=F.Character(FVector(0,0,100),0); auto* Target=F.Character(FVector(180,0,100),1);
    if (!Source||!Target) { return false; }
    auto* Mesh=NewObject<USovMeleeRuntimeTestMesh>(Source); Source->AddInstanceComponent(Mesh);
    Mesh->SetupAttachment(Source->GetRootComponent()); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->RegisterComponent();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Cover=F.World->SpawnActor<ASovDestructibleCover>(ASovDestructibleCover::StaticClass(),FVector(90,0,100),FRotator::ZeroRotator,Spawn);
    if (!Cover) { return false; }
    Cover->Obstruction->SetBoxExtent(FVector(10,300,150)); Cover->PlacementGuid=FGuid(0x3E1EE,1,2,3);
    Cover->FracturedAsset=NewObject<UGeometryCollection>(Cover); Cover->bDestructionEnabled=true; Cover->RemainingHealth=15.f;
    auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovMeleeRuntimeTestAbility::StaticClass(),1));
    TestTrue(TEXT("Attack begins on the source side of cover"),ASC->TryActivateAbility(Handle,false));
    auto* Ability=Cast<USovMeleeRuntimeTestAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    if (!Ability) { return false; }
    Mesh->SetWorldLocation(FVector(180,0,100)); F.Advance(Ability,.1f); F.Advance(Ability,.15f);
    TestTrue(TEXT("Authored node damage breaks the blocking cover"),Cover->IsBroken());
    TestEqual(TEXT("The blocked step does not reach the target through the new opening"),Target->ResolvedHitCount,0);
    ASC->CancelAbilityHandle(Handle);
    return true;
}
#endif
