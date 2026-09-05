// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovSaveRuntimeTestFixtures.h"
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
            World=UWorld::CreateWorld(EWorldType::Game,false); if(!World) { return; }
            if(GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
            World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
                .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
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
