// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovSaveRuntimeTestFixtures.h"
#include "Tests/SovCompanionApproachTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Companions/SovCompanionComponent.h"
#include "Companions/SovConvergenceCompanionState.h"
#include "AI/NarrativeNPCController.h"
#include "AI/NPCDefinition.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Character/PlayerDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Sovereign/SovGameplayTags.h"
#include "World/SovTraversalAnchor.h"
#include "World/SovWorldTransitActor.h"
#include "World/SovCarryTargetComponent.h"
#include "World/SovRescueDestination.h"

#if WITH_AUTOMATION_TESTS
struct FSovWorldTraversalTestAccess
{ static void Tick(ASovTraversalAnchor* Anchor, float Delta) { Anchor->Tick(Delta); } };
struct FSovCarryTestAccess
{
    static void Tick(USovCarryTargetComponent* Carry) { Carry->TickComponent(.016f,LEVELTICK_All,nullptr); }
    static void Cancel(USovCarryTargetComponent* Carry) { Carry->CancelCarry(); }
    static bool Save(USovCarryTargetComponent* Carry)
    {
        TArray<uint8> Bytes; FMemoryWriter Writer(Bytes); FObjectAndNameAsStringProxyArchive Ar(Writer,false); Ar.ArIsSaveGame=true;
        Carry->Serialize(Ar); return !Ar.IsError();
    }
};
struct FSovWorldTransitTestAccess
{
    static void SetState(ASovWorldTransitActor* Transit, ESovWorldTransitState State) { Transit->State=State; }
    static bool Save(ASovWorldTransitActor* Transit)
    {
        TArray<uint8> Bytes; FMemoryWriter Writer(Bytes); FObjectAndNameAsStringProxyArchive Ar(Writer,false);
        Ar.ArIsSaveGame=true; Transit->Serialize(Ar); return !Ar.IsError();
    }
};
namespace
{
    struct FWorldFixture
    {
        UWorld* World=nullptr;
        ASovHandoffRuntimeTestController* PC=nullptr;
        ASovHandoffRuntimeTestPawn* Player=nullptr;
        UNarrativeAbilitySystemComponent* ASC=nullptr;
        FVector Origin;
        FWorldFixture()
        {
            const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
                .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            World=UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
                ERHIFeatureLevel::Num, &WorldInitialization); if(!World) { return; }
            if(GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
            PC=World->SpawnActor<ASovHandoffRuntimeTestController>(); Player=World->SpawnActor<ASovHandoffRuntimeTestPawn>();
            auto* PS=World->SpawnActor<ASovPlayerState>(); if(!PC||!Player||!PS) { return; }
            auto* Definition=NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
            Player->PrepareCampaignInitialization(Definition); PC->SetTestPlayerState(PS); PC->Possess(Player);
            if(!Player->StageTestReadiness(PS,true)||!Player->CompleteCampaignDataInitialization(false)) { return; }
            ASC=Cast<UNarrativeAbilitySystemComponent>(PS->GetAbilitySystemComponent());
            Origin=FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f);
            Box(FVector(0,0,-10),FVector(1000,1000,10)); ResetPlayer();
        }
        ~FWorldFixture() { if(World) { World->DestroyWorld(false); if(GEngine) { GEngine->DestroyWorldContext(World); } } }
        AActor* Box(FVector Position,FVector Extent)
        {
            auto* Actor=World->SpawnActor<AActor>(); auto* Shape=NewObject<UBoxComponent>(Actor);
            Actor->SetRootComponent(Shape); Actor->AddInstanceComponent(Shape); Shape->SetBoxExtent(Extent);
            Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Shape->SetCollisionObjectType(ECC_WorldStatic);
            Shape->SetCollisionResponseToAllChannels(ECR_Block); Shape->RegisterComponent(); Actor->SetActorLocation(Position); return Actor;
        }
        void ResetPlayer()
        { Player->SetActorLocation(Origin,false,nullptr,ETeleportType::TeleportPhysics); Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking); }
        USovCarryTargetComponent* CarryTarget()
        {
            auto* Actor=World->SpawnActor<ASovSaveRuntimeActor>(); auto* Body=NewObject<UCapsuleComponent>(Actor,TEXT("CarryBody"));
            Actor->AddInstanceComponent(Body); Actor->SetRootComponent(Body); Body->SetCapsuleSize(20,40); Body->SetMobility(EComponentMobility::Movable);
            Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Body->SetCollisionObjectType(ECC_WorldDynamic);
            Body->SetCollisionResponseToAllChannels(ECR_Block); Body->RegisterComponent(); Actor->SetActorLocation(FVector(150,0,42));
            auto* Carry=NewObject<USovCarryTargetComponent>(Actor,TEXT("CarryTarget")); Actor->AddInstanceComponent(Carry); Carry->RegisterComponent();
            Carry->CarryTargetId=TEXT("TestCivilian"); return Carry;
        }
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTraversalRepeatedStartTest,"ProjectVelkorran.World.Traversal.RepeatedEntry30And60",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovTraversalRepeatedStartTest::RunTest(const FString& Parameters)
{
    FWorldFixture F; if(!TestNotNull(TEXT("Native player initialized"),F.ASC)) { return false; }
    auto* Anchor=F.World->SpawnActor<ASovTraversalAnchor>(); Anchor->TraversalId=TEXT("TraversalTest");
    Anchor->PathPoints={F.Origin,F.Origin+FVector(150,0,0),F.Origin+FVector(150,150,0)}; Anchor->DurationSeconds=.7f;
    const float Stamina=F.ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute());
    for(int Rate : {30,60})
    {
        for(int Start=0;Start<100;++Start)
        {
            F.ResetPlayer(); FText Error;
            if(!TestTrue(TEXT("Validated entry starts"),Anchor->RequestTraverse(F.Player,Error))) { AddError(Error.ToString()); return false; }
            for(int Frame=0;Frame<Rate*2&&Anchor->IsTraversalActive();++Frame) { FSovWorldTraversalTestAccess::Tick(Anchor,1.f/Rate); }
            if(!TestFalse(TEXT("Traversal terminates within authored duration"),Anchor->IsTraversalActive())) { return false; }
            TestTrue(TEXT("Arrives at authored safe endpoint"),F.Player->GetActorLocation().Equals(Anchor->PathPoints.Last(),.5f));
            TestFalse(TEXT("Traversal releases its input lock"),F.PC->IsMoveInputIgnored());
            TestFalse(TEXT("Traversal releases its own semantic window"),F.ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Traversal));
        }
    }
    TestEqual(TEXT("Alignment movement spends no combat resource"),F.ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute()),Stamina);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTraversalObstructionTest,"ProjectVelkorran.World.Traversal.ObstructionAndSlowFrame",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovTraversalObstructionTest::RunTest(const FString& Parameters)
{
    FWorldFixture F; if(!TestNotNull(TEXT("Native player initialized"),F.ASC)) { return false; }
    auto* Anchor=F.World->SpawnActor<ASovTraversalAnchor>(); Anchor->TraversalId=TEXT("ObstructionTest");
    Anchor->PathPoints={F.Origin,F.Origin+FVector(200,0,0),F.Origin+FVector(200,200,0)};
    // The corner route is clear; a shortcut chord would collide with this central block.
    auto* ChordBlock=F.Box(F.Origin+FVector(100,100,0),FVector(10,10,20)); FText Error;
    TestTrue(TEXT("Complete authored route passes preflight"),Anchor->RequestTraverse(F.Player,Error));
    FSovWorldTraversalTestAccess::Tick(Anchor,2.f);
    TestTrue(TEXT("One slow frame still visits corners instead of cutting through geometry"),F.Player->GetActorLocation().Equals(Anchor->PathPoints.Last(),.5f));
    ChordBlock->Destroy(); F.ResetPlayer();
    auto* Block=F.Box(F.Origin+FVector(100,0,0),FVector(10,70,70));
    TestFalse(TEXT("Blocked alignment does not start or consume state"),Anchor->RequestTraverse(F.Player,Error));
    TestFalse(TEXT("Rejected alignment has no traversal tag"),F.ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Traversal));
    Block->Destroy();
    TestTrue(TEXT("Clear route can start"),Anchor->RequestTraverse(F.Player,Error));
    F.Box(F.Origin+FVector(100,0,0),FVector(10,70,70));
    FSovWorldTraversalTestAccess::Tick(Anchor,1.f);
    TestFalse(TEXT("New blocker terminates movement"),Anchor->IsTraversalActive());
    TestFalse(TEXT("Interrupted traversal releases input"),F.PC->IsMoveInputIgnored());
    TestTrue(TEXT("Player did not tunnel through blocker"),F.Player->GetActorLocation().X<100.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTransitSaveTest,"ProjectVelkorran.World.Transit.StableEndpointSave",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovTransitSaveTest::RunTest(const FString& Parameters)
{
    FWorldFixture F; if(!TestNotNull(TEXT("World initialized"),F.World)) { return false; }
    auto* Transit=F.World->SpawnActor<ASovWorldTransitActor>();
    TestTrue(TEXT("Stationary origin can save"),FSovWorldTransitTestAccess::Save(Transit));
    FSovWorldTransitTestAccess::SetState(Transit,ESovWorldTransitState::WaitingForDestination);
    TestFalse(TEXT("Unresolved destination does not save"),FSovWorldTransitTestAccess::Save(Transit));
    Transit->MovingBody->SetRelativeLocation(Transit->DestinationOffset*.5f);
    Transit->StructuralHealth=0.f; FSovWorldTransitTestAccess::SetState(Transit,ESovWorldTransitState::Broken);
    TestFalse(TEXT("Broken midpoint cannot masquerade as a committed endpoint"),FSovWorldTransitTestAccess::Save(Transit));
    Transit->MovingBody->SetRelativeLocation(FVector::ZeroVector);
    TestTrue(TEXT("Broken stable endpoint is checkpointable"),FSovWorldTransitTestAccess::Save(Transit));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTransitCompanionBoardingTest,"ProjectVelkorran.World.Transit.RequiredMissionCompanionBoardsBeforeDeparture",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovTransitCompanionBoardingTest::RunTest(const FString& Parameters)
{
#if WITH_EDITOR
    FEditorScriptExecutionGuard ScriptGuard;
#endif
    FWorldFixture F; if (!TestNotNull(TEXT("Native player initialized"), F.ASC)) { return false; }
    auto* Transit = F.World->SpawnActor<ASovWorldTransitActor>(); if (!Transit) { return false; }
    Transit->TransitId = TEXT("CompanionBoardingTest"); Transit->Kind = ESovWorldTransitKind::Lift;
    Transit->MovingBody->SetBoxExtent(FVector(300,300,30)); Transit->SetActorLocation(FVector(0,0,500));
    F.Player->SetActorLocation(FVector(-100,0,530+F.Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f));
    F.Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking); F.Player->SetBase(Transit->MovingBody);
    FText Error;
    TestTrue(TEXT("Default lift keeps the existing solo boarding contract"), Transit->CanUse(F.Player, Error));
    Transit->bRequireMissionCompanionAboard = true;
    TestFalse(TEXT("Opt-in lift rejects absent active companion"), Transit->RequestUse(F.Player, Error));
    TestEqual(TEXT("Player-facing rejection explains actual boarding"), Error.ToString(), FString(TEXT("Wait for your companion to board")));
    TestFalse(TEXT("Rejected boarding leaves input unlocked"), F.PC->IsMoveInputIgnored());
    TestFalse(TEXT("Rejected boarding applies no traversal window"), F.ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Traversal));

    auto* Mission = NewObject<USovCampaignDefinition>(F.PC); F.PC->KeepAlive.Add(Mission);
    Mission->MissionId = TEXT("TransitCompanionFixture"); Mission->Protagonist = F.Player->GetProtagonistIdentityTag();
    Mission->PawnClass = ASovHandoffRuntimeTestPawn::StaticClass(); Mission->PlayerDefinition = F.Player->GetPlayerDefinition();
    Mission->AllowedCompanionIds = {TEXT("Selene")};
    FSovCampaignBeatDefinition Beat; Beat.BeatId = TEXT("Exit"); Mission->Beats.Add(Beat);
    if (!TestEqual(TEXT("Ordinary fixture mission starts without seeded progression"),
        F.PC->GetCampaignState()->BeginMission(Mission), ESovCampaignResult::Applied)) { return false; }
    auto* Source = F.World->SpawnActor<ASovAxiomRuntimeTestCharacter>(); if (!Source) { return false; }
    Source->InitializeTestCombat(0); Source->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Selene);
    const FTransform SpawnTransform(FVector(150,0,530+F.Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f));
    auto* Companion = F.World->SpawnActorDeferred<ASovCompanionApproachTestProxy>(ASovCompanionApproachTestProxy::StaticClass(),
        SpawnTransform, F.PC, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    FString Reason;
    if (!Companion || !TestTrue(TEXT("Real native proxy reads an actual matching source ASC"),
        Companion->PrepareProxy(FSovGameplayTags::Get().Character_Player_Selene, TEXT("Selene"), Source->GetNarrativeAbilitySystemComponent(), {}, Reason))) { return false; }
    auto* Definition = NewObject<UNPCDefinition>(F.PC); F.PC->KeepAlive.Add(Definition);
    Definition->NPCClassPath = ASovCompanionApproachTestProxy::StaticClass();
    Companion->SetNPCDefinition(Definition); Companion->FinishSpawning(SpawnTransform); Companion->EnsureEncounterController();
    auto* AI = Cast<ANarrativeNPCController>(Companion->GetController());
    if (!TestNotNull(TEXT("Actual Narrative AI possesses the proxy"), AI)) { return false; }
    if (!AI->HasActorBegunPlay()) { AI->DispatchBeginPlay(); }
    if (!TestTrue(TEXT("Native proxy applies its copied resources"), Companion->CompleteProxyInitialization())) { return false; }
    Companion->SetProxyStaged(false); Source->Destroy();
    auto* Commands = Companion->GetCompanionComponent();
    if (!TestTrue(TEXT("Real command owner accepts the actual player leader"), Commands->SetLeader(F.Player, Reason))) { AddError(Reason); return false; }
    auto* State = F.PC->GetConvergenceCompanionState();
    auto* Active = FindFProperty<FObjectPropertyBase>(USovConvergenceCompanionState::StaticClass(), TEXT("Active"));
    if (!TestNotNull(TEXT("Existing companion publication property"), Active)) { return false; }
    auto* Leader = FindFProperty<FObjectPropertyBase>(USovCompanionComponent::StaticClass(), TEXT("Leader"));
    if (!TestNotNull(TEXT("Existing command leader property"), Leader)) { return false; }
    // Supply external completed companion publication, not any mission beat or transit outcome.
    Active->SetObjectPropertyValue_InContainer(State, Companion);
    Companion->GetCharacterMovement()->SetMovementMode(MOVE_Walking); Companion->SetBase(nullptr);
    TestFalse(TEXT("Ready owned companion off the platform cannot authorize departure"), Transit->CanUse(F.Player, Error));
    Companion->SetBase(Transit->MovingBody);
    TestTrue(TEXT("Current ready living companion on this exact platform admits departure"), Transit->CanUse(F.Player, Error));
    Companion->SetProxyStaged(true);
    TestFalse(TEXT("Staged proxy is not an available passenger"), Transit->CanUse(F.Player, Error)); Companion->SetProxyStaged(false);
    Companion->SetOwner(F.Player);
    TestFalse(TEXT("A foreign-owned proxy cannot stand in for campaign ownership"), Transit->CanUse(F.Player, Error)); Companion->SetOwner(F.PC);
    Leader->SetObjectPropertyValue_InContainer(Commands, nullptr);
    TestFalse(TEXT("A retired command leader is not the current player companion"), Transit->CanUse(F.Player, Error));
    Leader->SetObjectPropertyValue_InContainer(Commands, F.Player);
    auto* ProxyASC = Companion->GetNarrativeAbilitySystemComponent();
    ProxyASC->InitAbilityActorInfo(Companion, F.Player);
    TestFalse(TEXT("A retired avatar binding is not a ready passenger"), Transit->CanUse(F.Player, Error)); ProxyASC->InitAbilityActorInfo(Companion, Companion);
    ProxyASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
    TestFalse(TEXT("Zero-health companion cannot admit departure before its death latch publishes"), Transit->CanUse(F.Player, Error));
    ProxyASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
    TestTrue(TEXT("Restored actual passenger can be checked again"), Transit->CanUse(F.Player, Error));
    Active->SetObjectPropertyValue_InContainer(State, nullptr);
    TestFalse(TEXT("Old living actor on the lift is rejected after membership retires"), Transit->CanUse(F.Player, Error));
    Transit->Kind = ESovWorldTransitKind::Door;
    TestTrue(TEXT("Door behavior is unchanged even when opt-in property is set"), Transit->CanUse(F.Player, Error)); Transit->Kind = ESovWorldTransitKind::Lift;
    Active->SetObjectPropertyValue_InContainer(State, Companion);
    const auto Busy = FNarrativeGameplayTags::Get().State_Busy;
    const FDelegateHandle Listener = F.ASC->RegisterGameplayTagEvent(Busy, EGameplayTagEventType::NewOrRemoved).AddLambda(
        [Active, State](FGameplayTag, int32 Count) { if (Count > 0) { Active->SetObjectPropertyValue_InContainer(State, nullptr); } });
    TestFalse(TEXT("Synchronous retirement while applying transit window rejects departure"), Transit->RequestUse(F.Player, Error));
    TestEqual(TEXT("Reentrant rejection leaves the real mechanism at origin"), Transit->GetTransitState(), ESovWorldTransitState::AtOrigin);
    TestFalse(TEXT("Reentrant rejection removes only the new transit window"), F.ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Traversal));
    TestFalse(TEXT("Reentrant rejection never acquires an input lock"), F.PC->IsMoveInputIgnored());
    F.ASC->RegisterGameplayTagEvent(Busy, EGameplayTagEventType::NewOrRemoved).Remove(Listener);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCarryOwnershipTest,"ProjectVelkorran.World.Carry.OwnershipReleaseAndSave",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovCarryOwnershipTest::RunTest(const FString& Parameters)
{
    FWorldFixture F; if(!TestNotNull(TEXT("Native player initialized"),F.ASC)) { return false; }
    auto* Carry=F.CarryTarget(); FText Error; const auto& N=FNarrativeGameplayTags::Get();
    TestTrue(TEXT("Grounded target can be picked up"),Carry->RequestCarry(F.Player,Error));
    TestTrue(TEXT("Real actor root attaches to player"),Carry->GetOwner()->GetAttachParentActor()==F.Player);
    TestTrue(TEXT("Existing slow-walk state constrains carrier"),F.ASC->HasMatchingGameplayTag(N.State_Movement_SlowWalking));
    TestFalse(TEXT("Unstable carried state cannot serialize"),FSovCarryTestAccess::Save(Carry));
    F.ASC->AddLooseGameplayTag(N.State_Busy);
    TestTrue(TEXT("Native release finds actual safe floor"),Carry->RequestRelease(F.Player,Error));
    TestFalse(TEXT("Released root has no carrier"),Carry->GetOwner()->GetAttachParentActor()!=nullptr);
    TestEqual(TEXT("Release preserves unrelated Busy contribution"),F.ASC->GetTagCount(N.State_Busy),1);
    TestTrue(TEXT("Safely released actor is serializable"),FSovCarryTestAccess::Save(Carry)); F.ASC->RemoveLooseGameplayTag(N.State_Busy);
    Carry->GetOwner()->SetActorLocation(FVector(150,0,42));
    TestTrue(TEXT("Released unresolved target may be carried again"),Carry->RequestCarry(F.Player,Error));
    F.PC->UnPossess(); FSovCarryTestAccess::Tick(Carry);
    TestFalse(TEXT("Avatar loss detaches target"),Carry->IsCarried());
    TestFalse(TEXT("Avatar loss clears only carrier window"),F.ASC->HasMatchingGameplayTag(N.State_Busy));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRescueCommitTest,"ProjectVelkorran.World.Carry.RescueRequiresDestination",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovRescueCommitTest::RunTest(const FString& Parameters)
{
    FWorldFixture F; if(!TestNotNull(TEXT("Native player initialized"),F.ASC)) { return false; }
    auto* Mission=NewObject<USovCampaignDefinition>(F.PC); F.PC->KeepAlive.Add(Mission);
    auto* Definition=NewObject<UPlayerDefinition>(F.PC); F.PC->KeepAlive.Add(Definition);
    Mission->MissionId=TEXT("RescueTest"); Mission->Protagonist=F.Player->GetProtagonistIdentityTag();
    Mission->PawnClass=ASovHandoffRuntimeTestPawn::StaticClass(); Mission->PlayerDefinition=Definition;
    FSovCampaignBeatDefinition Beat; Beat.BeatId=TEXT("CivilianSafe"); Mission->Beats.Add(Beat);
    TestEqual(TEXT("Mission begins through production owner"),F.PC->GetCampaignState()->BeginMission(Mission),ESovCampaignResult::Applied);
    auto* Carry=F.CarryTarget(); auto* Destination=F.World->SpawnActor<ASovRescueDestination>();
    Destination->DestinationId=TEXT("Shelter"); Destination->MissionId=Mission->MissionId;
    Destination->RequiredCarryTargetId=Carry->CarryTargetId; Destination->CompletionBeat=Beat.BeatId; Destination->SetActorLocation(FVector(500,0,0));
    FText Error; TestTrue(TEXT("Pickup accepted"),Carry->RequestCarry(F.Player,Error));
    TestFalse(TEXT("Rescue cannot commit outside destination"),Destination->RequestRescue(F.Player,Error));
    TestFalse(TEXT("Failed spatial proof writes no beat"),F.PC->GetCampaignState()->IsBeatComplete(Mission->MissionId,Beat.BeatId));
    F.Player->SetActorLocation(FVector(500,0,F.Origin.Z)); FSovCarryTestAccess::Tick(Carry);
    TestTrue(TEXT("Verified carried target at shelter commits rescue"),Destination->RequestRescue(F.Player,Error));
    TestTrue(TEXT("Actual campaign journal owns rescue beat"),F.PC->GetCampaignState()->IsBeatComplete(Mission->MissionId,Beat.BeatId));
    TestTrue(TEXT("Rescued flag is durable target state"),Carry->IsRescued());
    TestFalse(TEXT("A rescued target cannot be carried or rewarded again"),Carry->RequestCarry(F.Player,Error));
    TestFalse(TEXT("Repeated destination delivery is rejected"),Destination->RequestRescue(F.Player,Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCarryReentrantLeaseTest,"ProjectVelkorran.World.Carry.ReentrantLeaseRetirement",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovCarryReentrantLeaseTest::RunTest(const FString& Parameters)
{
    FWorldFixture F; if(!TestNotNull(TEXT("Native player initialized"),F.ASC)) { return false; }
    auto* Carry=F.CarryTarget(); FText Error; const auto Busy=FNarrativeGameplayTags::Get().State_Busy;
    const FDelegateHandle Listener=F.ASC->RegisterGameplayTagEvent(Busy,EGameplayTagEventType::NewOrRemoved).AddLambda(
        [Carry](FGameplayTag,int32) { FSovCarryTestAccess::Cancel(Carry); });
    TestFalse(TEXT("Synchronous cancellation during GE apply rejects the pickup"),Carry->RequestCarry(F.Player,Error));
    TestFalse(TEXT("Late GE handle is removed after its lease retired"),F.ASC->HasMatchingGameplayTag(Busy));
    TestFalse(TEXT("Cancellation cannot leave an attached actor"),Carry->GetOwner()->GetAttachParentActor()!=nullptr);
    TestFalse(TEXT("Lease is retired exactly once"),Carry->IsCarried());
    TestTrue(TEXT("Canceled pickup preserves a safe serializable target"),FSovCarryTestAccess::Save(Carry));
    F.ASC->RegisterGameplayTagEvent(Busy,EGameplayTagEventType::NewOrRemoved).Remove(Listener);
    return true;
}
#endif
