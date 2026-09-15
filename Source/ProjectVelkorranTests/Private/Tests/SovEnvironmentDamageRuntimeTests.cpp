// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "World/SovDestructibleCover.h"
#include "Sovereign/SovEnvironmentDamage.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "NarrativeGameplayTags.h"
#include <limits>

#if WITH_AUTOMATION_TESTS
namespace
{
struct FEnvironmentDamageWorld
{
    UWorld* World=nullptr;
    uint32 NextPlacement=1;
    FEnvironmentDamageWorld()
    {
        const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
        if (World&&GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
    }
    ~FEnvironmentDamageWorld() { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
    ASovAxiomRuntimeTestCharacter* Character(FVector Location,int32 Team)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Actor=World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(),Location,FRotator::ZeroRotator,Spawn);
        if (Actor) { Actor->InitializeTestCombat(Team); }
        return Actor;
    }
    ASovDestructibleCover* Cover(FVector Location,FVector Extent,float Health,bool bEnabled=true)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Actor=World->SpawnActor<ASovDestructibleCover>(ASovDestructibleCover::StaticClass(),Location,FRotator::ZeroRotator,Spawn);
        if (!Actor) { return nullptr; }
        Actor->Obstruction->SetBoxExtent(Extent);
        Actor->PlacementGuid=FGuid(0x5E0E0001,0,0,NextPlacement++);
        Actor->FracturedAsset=NewObject<UGeometryCollection>(Actor);
        Actor->bDestructionEnabled=bEnabled; Actor->RemainingHealth=Health;
        return Actor;
    }
    AActor* Wall(FVector Location,FVector Extent)
    {
        auto* Actor=World->SpawnActor<AActor>();
        auto* Box=NewObject<UBoxComponent>(Actor); Actor->AddInstanceComponent(Box); Actor->SetRootComponent(Box);
        Box->SetBoxExtent(Extent); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->RegisterComponent(); Actor->SetActorLocation(Location);
        return Actor;
    }
};
FHitResult HitOn(AActor* Actor,UPrimitiveComponent* Component,FVector From)
{
    FHitResult Hit(Actor,Component,Actor->GetActorLocation(),(From-Actor->GetActorLocation()).GetSafeNormal());
    Hit.TraceStart=From; Hit.TraceEnd=Actor->GetActorLocation(); Hit.bBlockingHit=true;
    return Hit;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEnvironmentDamageAdmissionTest,
    "ProjectVelkorran.World.Destruction.EnvironmentPointAndRadialAdmission",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSovEnvironmentDamageAdmissionTest::RunTest(const FString& Parameters)
{
    FEnvironmentDamageWorld F;
    if (!TestNotNull(TEXT("Test world"),F.World)) { return false; }
    auto* Source=F.Character(FVector::ZeroVector,0);
    auto* Hostile=F.Character(FVector(3000,-400,0),1);
    auto* Point=F.Cover(FVector(3000,0,0),FVector(20,50,100),120);
    auto* Protected=F.Cover(FVector(3000,400,0),FVector(20,50,100),120,false);
    AActor* Plain=F.Wall(FVector(3000,800,0),FVector(20,50,100));
    if (!Source||!Hostile||!Point||!Protected||!Plain) { AddError(TEXT("Fixture creation failed")); return false; }
    const FVector From(2900,0,0);
    TestEqual(TEXT("Authored owner accepts point damage"),SovEnvironmentDamage::ApplyPoint(Source,HitOn(Point,Point->Obstruction,From),30.f),30.f);
    TestEqual(TEXT("Missing source is rejected"),SovEnvironmentDamage::ApplyPoint(nullptr,HitOn(Point,Point->Obstruction,From),30.f),0.f);
    TestEqual(TEXT("Non-finite damage is rejected"),SovEnvironmentDamage::ApplyPoint(Source,HitOn(Point,Point->Obstruction,From),std::numeric_limits<float>::quiet_NaN()),0.f);
    TestEqual(TEXT("Opt-in remains the owner's decision"),SovEnvironmentDamage::ApplyPoint(Source,HitOn(Protected,Protected->Obstruction,From),30.f),0.f);
    TestEqual(TEXT("Ordinary geometry is not scenery damage"),SovEnvironmentDamage::ApplyPoint(Source,HitOn(Plain,Cast<UPrimitiveComponent>(Plain->GetRootComponent()),From),30.f),0.f);
    TestEqual(TEXT("Ability-system characters are never scenery"),SovEnvironmentDamage::ApplyPoint(Source,HitOn(Hostile,Hostile->GetCapsuleComponent(),From),30.f),0.f);
    TestEqual(TEXT("Only accepted damage was retained"),Point->RemainingHealth,90.f);

    auto* Near=F.Cover(FVector(200,0,0),FVector(20,50,100),120);
    auto* Hidden=F.Cover(FVector(0,350,0),FVector(50,50,100),120);
    F.Wall(FVector(0,200,0),FVector(200,10,200));
    auto* Far=F.Cover(FVector(1000,0,0),FVector(20,50,100),120);
    if (!Near||!Hidden||!Far) { AddError(TEXT("Radial fixture creation failed")); return false; }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SovEnvironmentDamageTest),false,Source);
    TArray<AActor*> Damaged;
    TestEqual(TEXT("Only the visible in-range owner is damaged"),SovEnvironmentDamage::ApplyRadial(Source,FVector::ZeroVector,400,100,.5f,true,Query,&Damaged),1);
    TestTrue(TEXT("Damaged owner is reported"),Damaged.Num()==1&&Damaged[0]==Near);
    TestTrue(TEXT("Falloff is measured to the nearest face"),FMath::IsNearlyEqual(Near->RemainingHealth,42.5f,.01f));
    TestEqual(TEXT("Blocked sightline receives nothing"),Hidden->RemainingHealth,120.f);
    TestEqual(TEXT("Out-of-range owner receives nothing"),Far->RemainingHealth,120.f);
    TestEqual(TEXT("Second blast breaks the weakened owner"),SovEnvironmentDamage::ApplyRadial(Source,FVector::ZeroVector,400,100,.5f,true,Query),1);
    TestTrue(TEXT("Owner broke"),Near->IsBroken());
    TestEqual(TEXT("Retired collision is not found again"),SovEnvironmentDamage::ApplyRadial(Source,FVector::ZeroVector,400,100,.5f,true,Query),0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEnvironmentTargetDataTest,
    "ProjectVelkorran.World.Destruction.NarrativeTargetDataAdmitsScenery",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSovEnvironmentTargetDataTest::RunTest(const FString& Parameters)
{
    FEnvironmentDamageWorld F;
    if (!TestNotNull(TEXT("Test world"),F.World)) { return false; }
    auto* Source=F.Character(FVector::ZeroVector,0);
    auto* Cover=F.Cover(FVector(300,0,0),FVector(20,50,100),120);
    if (!Source||!Cover) { AddError(TEXT("Fixture creation failed")); return false; }
    auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    const auto Apply=[&](float Damage)
    {
        FGameplayEffectSpecHandle Spec=ASC->MakeOutgoingSpec(UGameplayEffect::StaticClass(),1,ASC->MakeEffectContext());
        if (Spec.IsValid()) { Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage,Damage); }
        ASC->ApplyGameplayEffectSpecToTargetData(Spec,UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(HitOn(Cover,Cover->Obstruction,FVector::ZeroVector)));
    };
    Apply(25.f);
    TestEqual(TEXT("Hit-result target data carries its authored damage to scenery"),Cover->RemainingHealth,95.f);
    Apply(0.f);
    TestEqual(TEXT("A spec without authored damage leaves scenery unchanged"),Cover->RemainingHealth,95.f);
    Apply(500.f);
    TestTrue(TEXT("Hitscan-sized damage breaks the owner"),Cover->IsBroken());
    return true;
}
#endif
