// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovCombatHUDQuiet.h"
#include "UI/SovCombatVitalsWidget.h"
#include "Tests/SovCombatReadinessTestFixtures.h"
#include "Tests/SovReadinessRuntimeTestFixtures.h"
#include "Tests/SovBotAttackTestFixtures.h"
#include "AI/NarrativeNPCController.h"
#include "Character/PlayerDefinition.h"
#include "Components/SovEchoComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "UObject/Script.h"

struct FSovCombatHUDQuietTestAccess
{
    static void Bind(USovCombatVitalsWidget* Widget,ASovPlayerController* PC) { Widget->BindQuietSources(PC); }
    static float Sample(USovCombatVitalsWidget* Widget,ASovReadinessRuntimeTestPawn* Pawn)
    { return Widget->QuietState.Update(Pawn,Pawn->GetNarrativeAbilitySystemComponent()->GetCombatActorInfoEpoch(),
        Pawn->GetWorld()->GetTimeSeconds(),Pawn->GetEchoComponent()->GetSecondsSinceCombatActivity(),false,false); }
    static void Destruct(USovCombatVitalsWidget* Widget) { Widget->NativeDestruct(); }
};

#if WITH_AUTOMATION_TESTS
namespace
{
struct FQuietWorld
{
    FEditorScriptExecutionGuard ScriptGuard;
    UWorld* World=nullptr;
    ASovHUDReadinessTestController* PC=nullptr;
    ASovPlayerState* PS=nullptr;
    FQuietWorld()
    {
        const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
            .CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
        World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
        if (!World) { return; }
        if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
        PC=World->SpawnActor<ASovHUDReadinessTestController>(); PS=World->SpawnActor<ASovPlayerState>();
        if (PC) { World->AddController(PC); }
        if (PC && PS) { PC->SetTestPlayerState(PS); }
    }
    ~FQuietWorld()
    { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
    ASovReadinessRuntimeTestPawn* Stage()
    {
        if (!PC || !PS) { return nullptr; }
        auto* Pawn=World->SpawnActor<ASovReadinessRuntimeTestPawn>();
        auto* Definition=NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
        if (!Pawn || !Pawn->PrepareCampaignInitialization(Definition)) { return nullptr; }
        PC->Possess(Pawn);
        if (!Pawn->StageTestReadiness(PS,true)) { return nullptr; }
        Pawn->BindProductionReadiness();
        return Pawn->CompleteCampaignDataInitialization(false) ? Pawn : nullptr;
    }
    void Advance(double Seconds)
    {
        const double Until=World->GetTimeSeconds()+Seconds;
        uint64 FrameNumber=GFrameCounter;
        while (World->GetTimeSeconds()+UE_DOUBLE_SMALL_NUMBER<Until)
        {
            TGuardValue<uint64> Frame(GFrameCounter,++FrameNumber);
            World->Tick(LEVELTICK_TimeOnly,FMath::Min(.05,Until-World->GetTimeSeconds()));
        }
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovQuietParticipation,
    "ProjectVelkorran.UI.QuietHUD.ParticipationClockAndSixSecondFade",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSovQuietParticipation::RunTest(const FString& Parameters)
{
    FQuietWorld F; auto* Pawn=F.Stage();
    if (!TestNotNull(TEXT("Production-ready current pawn"),Pawn)) { return false; }
    auto* Echo=Pawn->GetEchoComponent(); const float Meter=Echo->GetEcho();
    SovCombatHUDQuiet::FState State;
    const auto Sample=[&](bool Threat=false,bool Action=false)
    { return State.Update(Pawn,Pawn->GetNarrativeAbilitySystemComponent()->GetCombatActorInfoEpoch(),F.World->GetTimeSeconds(),
        Echo->GetSecondsSinceCombatActivity(),Threat,Action); };
    TestEqual(TEXT("New owner starts visible"),Sample(),1.f);
    F.Advance(5.95); TestEqual(TEXT("No early fade before six actual seconds"),Sample(),1.f);
    F.Advance(.2); TestTrue(TEXT("Opacity transitions after six seconds"),Sample()>0.f && Sample()<1.f);
    F.Advance(.3); TestEqual(TEXT("Quiet fade completes"),Sample(),0.f);
    const float Age=Echo->GetSecondsSinceCombatActivity();
    for (int32 I=0; I<10; ++I) { TestEqual(TEXT("Reading the clock has no timing side effect"),Echo->GetSecondsSinceCombatActivity(),Age); }
    TestEqual(TEXT("Reading/fading never changes Echo"),Echo->GetEcho(),Meter);
    Echo->RecordCombatActivity(FNarrativeGameplayTags::Get().Narrative_Input_Attack);
    TestEqual(TEXT("Native participation wakes immediately without a resource award"),Sample(),1.f);
    F.Advance(6.5); TestEqual(TEXT("Participation expires once, not after a second six-second timer"),Sample(),0.f);
    TestEqual(TEXT("Native valid threat keeps the HUD awake"),Sample(true),1.f);
    F.Advance(5.9); TestEqual(TEXT("Threat retirement starts a fresh quiet interval"),Sample(),1.f);
    F.Advance(.6); TestEqual(TEXT("No remaining threat or participation fades"),Sample(),0.f);
    TestEqual(TEXT("Current action restores visibility"),Sample(false,true),1.f);
    F.Advance(6.5); TestEqual(TEXT("Action completion can quiet again"),Sample(),0.f);
    const uint64 Epoch=Pawn->GetNarrativeAbilitySystemComponent()->GetCombatActorInfoEpoch();
    TestEqual(TEXT("Same pawn with a new ASC epoch retires old quiet state"),
        State.Update(Pawn,Epoch+1,F.World->GetTimeSeconds(),100.,false,false),1.f);
    TestEqual(TEXT("Clock rollback starts visible"),State.Update(Pawn,Epoch+1,0.,100.,false,false),1.f);
    auto* ASC=Pawn->GetNarrativeAbilitySystemComponent();
    const uint64 BeforeRetirement=ASC->GetCombatActorInfoEpoch();
    // Narrative intentionally ignores a null replacement while an avatar is bound.
    // Campaign handoff actually retires GAS actor info through this native method.
    ASC->ClearActorInfo();
    TestNull(TEXT("Native campaign retirement clears the actual avatar"),ASC->GetAvatarActor());
    TestTrue(TEXT("Native retirement advances the actor-info epoch"),ASC->GetCombatActorInfoEpoch()>BeforeRetirement);
    TestEqual(TEXT("Retired avatar cannot publish an old participation age"),Echo->GetSecondsSinceCombatActivity(),0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovQuietThreatOwnership,
    "ProjectVelkorran.UI.QuietHUD.NativeThreatExpiryAndOwnerRetirement",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSovQuietThreatOwnership::RunTest(const FString& Parameters)
{
    FQuietWorld F; auto* Pawn=F.Stage();
    if (!TestNotNull(TEXT("Current player"),Pawn)) { return false; }
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Source=F.World->SpawnActor<ASovBotTestCharacter>(ASovBotTestCharacter::StaticClass(),FVector(500,0,0),FRotator::ZeroRotator,Spawn);
    auto* AI=F.World->SpawnActor<ANarrativeNPCController>();
    if (!Source || !AI) { AddError(TEXT("Native threat fixtures failed")); return false; }
    Source->InitializeTestCombat(0); AI->Possess(Source); AI->bShareThreatsWithFaction=false;
    TestFalse(TEXT("An unobserved enemy is not invented as a current threat"),SovCombatHUDQuiet::HasNativeThreat(Pawn));
    const auto Observe=[&]() { return AI->ReportThreatObservation(Pawn,ENarrativeThreatSource::Hearing,Pawn->GetActorLocation(),1.f,1.f,2.f); };
    TestTrue(TEXT("Native hearing observation accepted"),Observe());
    const int32 Count=AI->GetThreatDebugSnapshot().Num();
    TestTrue(TEXT("Finite native investigation conservatively keeps readout visible"),SovCombatHUDQuiet::HasNativeThreat(Pawn));
    TestEqual(TEXT("HUD query does not refresh or add threat observations"),AI->GetThreatDebugSnapshot().Num(),Count);
    TestFalse(TEXT("Keep-visible does not grant direct-fire admission"),AI->CanDirectlyTargetThreat(Pawn));
    {
        FQuietWorld Other; auto* OtherPawn=Other.Stage();
        if (!TestNotNull(TEXT("Independent current pawn in another native world"),OtherPawn)) { return false; }
        TestFalse(TEXT("Another world's threat cannot keep this owner's HUD awake"),SovCombatHUDQuiet::HasNativeThreat(OtherPawn));
    }
    F.Advance(2.1); TestFalse(TEXT("Native confidence expiry retires the signal"),SovCombatHUDQuiet::HasNativeThreat(Pawn));
    TestTrue(TEXT("New observation is native-owned"),Observe());
    auto* Suspension=F.PC; AI->SetThreatMemorySuspended(Suspension,true);
    TestFalse(TEXT("Held/restoring sources do not keep HUD awake"),SovCombatHUDQuiet::HasNativeThreat(Pawn));
    AI->SetThreatMemorySuspended(Suspension,false); TestTrue(TEXT("Actual new observation after resume"),Observe());
    Source->SetActorHiddenInGame(true); TestFalse(TEXT("Hidden future presentation excluded"),SovCombatHUDQuiet::HasNativeThreat(Pawn));
    Source->SetActorHiddenInGame(false);
    Source->GetNarrativeAbilitySystemComponent()->InitAbilityActorInfo(Source,Pawn);
    TestFalse(TEXT("Wrong source ASC avatar excluded"),SovCombatHUDQuiet::HasNativeThreat(Pawn));
    Source->GetNarrativeAbilitySystemComponent()->InitAbilityActorInfo(Source,Source);
    TestTrue(TEXT("Restored actual source can observe again"),Observe());
    AI->UnPossess(); TestFalse(TEXT("Retired controller/pawn memory excluded"),SovCombatHUDQuiet::HasNativeThreat(Pawn));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovQuietInputRetirement,
    "ProjectVelkorran.UI.QuietHUD.InputBindingsResetAndDestruct",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSovQuietInputRetirement::RunTest(const FString& Parameters)
{
    FQuietWorld F; auto* Pawn=F.Stage();
    if (!TestNotNull(TEXT("Current ready input owner"),Pawn)) { return false; }
    auto* LP=NewObject<ULocalPlayer>(GEngine); F.PC->Player=LP; LP->PlayerController=F.PC; F.PC->SetAsLocalPlayerController();
    auto* Widget=NewObject<USovCombatVitalsWidget>(F.PC); F.PC->KeepAlive.Add(Widget); Widget->SetOwningPlayer(F.PC);
    FSovCombatHUDQuietTestAccess::Bind(Widget,F.PC);
    TestEqual(TEXT("Bound view starts visible"),FSovCombatHUDQuietTestAccess::Sample(Widget,Pawn),1.f);
    F.Advance(6.5); TestEqual(TEXT("Readout can quiet"),FSovCombatHUDQuietTestAccess::Sample(Widget,Pawn),0.f);
    const auto Attack=FNarrativeGameplayTags::Get().Narrative_Input_Attack;
    F.PC->AbilityInputPressed(Attack);
    TestEqual(TEXT("Actual native semantic input wakes the bound view"),FSovCombatHUDQuietTestAccess::Sample(Widget,Pawn),1.f);
    F.PC->AbilityInputReleased(Attack);
    F.Advance(6.5); TestEqual(TEXT("Release starts its normal quiet interval"),FSovCombatHUDQuietTestAccess::Sample(Widget,Pawn),0.f);
    const float Echo=Pawn->GetEchoComponent()->GetEcho();
    FSovCombatHUDQuietTestAccess::Destruct(Widget);
    FSovCombatHUDQuietTestAccess::Sample(Widget,Pawn); F.Advance(6.5);
    F.PC->AbilityInputPressed(Attack);
    TestEqual(TEXT("Destructed view no longer receives native input callbacks"),FSovCombatHUDQuietTestAccess::Sample(Widget,Pawn),0.f);
    F.PC->AbilityInputReleased(Attack);
    TestEqual(TEXT("HUD input listener spends no Echo"),Pawn->GetEchoComponent()->GetEcho(),Echo);
    FSovCombatHUDQuietTestAccess::Bind(Widget,F.PC);
    TestEqual(TEXT("Fresh binding restores a full quiet interval"),FSovCombatHUDQuietTestAccess::Sample(Widget,Pawn),1.f);
    FSovCombatHUDQuietTestAccess::Destruct(Widget);
    return true;
}
#endif
