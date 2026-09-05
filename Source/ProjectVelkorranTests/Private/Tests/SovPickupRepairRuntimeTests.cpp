// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovPickupRepairTestFixtures.h"
#include "Components/SovEchoComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "UnrealFramework/NarrativePlayerController.h"

#if WITH_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPickupGrantReentryTest,
    "ProjectVelkorran.Campaign.Sustain.ReentrantEchoCollection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPickupGrantReentryTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!World) { return false; }
    if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
    World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
        .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
    auto* Player = World->SpawnActor<ASovPickupRepairTestCharacter>();
    auto* Controller = World->SpawnActor<ANarrativePlayerController>();
    auto* Pickup = World->SpawnActor<ASovPickupRepairTestPickup>();
    if (Player && Controller && Pickup)
    {
        Controller->Possess(Player); Player->InitializeExertion();
        auto* ASC = Player->GetNarrativeAbilitySystemComponent();
        Player->GetEchoComponent()->RestoreEchoFromCheckpoint(0.f); Pickup->InitializeEcho(10.f);
        bool bNested = false;
        auto& EchoChanged = ASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetEchoAttribute());
        const auto Handle = EchoChanged.AddLambda([&](const FOnAttributeChangeData& Change)
        {
            if (bNested || Change.NewValue <= Change.OldValue) { return; }
            bNested = true; Pickup->Touch(Player);
        });
        Pickup->Touch(Player); EchoChanged.Remove(Handle);
        TestTrue(TEXT("Real Echo attribute callback reentered overlap collection"), bNested);
        TestTrue(TEXT("Successful resource transaction consumes pickup"), Pickup->IsClaimed());
        TestEqual(TEXT("Same pack grants Echo once across nested overlaps"), Player->GetEchoComponent()->GetEcho(), 10.f);
        Pickup->Touch(Player);
        TestEqual(TEXT("Claimed pack remains consumed"), Player->GetEchoComponent()->GetEcho(), 10.f);
        auto* FullPickup = World->SpawnActor<ASovPickupRepairTestPickup>();
        if (FullPickup)
        {
            FullPickup->InitializeEcho(10.f); Player->GetEchoComponent()->RestoreEchoFromCheckpoint(100.f);
            FullPickup->Touch(Player); TestFalse(TEXT("Full recipient leaves pack available"), FullPickup->IsClaimed());
            Player->GetEchoComponent()->RestoreEchoFromCheckpoint(0.f); FullPickup->Touch(Player);
            TestTrue(TEXT("Failed grant released its reservation for a later overlap"), FullPickup->IsClaimed());
        }
        else { AddError(TEXT("Second pickup failed to spawn")); }
    }
    else { AddError(TEXT("Pickup fixture failed to spawn")); }
    World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); }
    return true;
}
#endif
