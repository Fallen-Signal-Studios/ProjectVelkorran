// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAurelionThermalTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovCampaignMassRoundTripFixtures.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Tests/SovCoActionRuntimeTestFixtures.h"
#include "Companions/SovCompanionCommandActivity.h"
#include "Components/SovAurelionThermalFractureComponent.h"
#include "Components/SovPoiseComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SceneComponent.h"
#include "Companions/SovCompanionComponent.h"
#include "Companions/SovConvergenceCompanionState.h"
#include "Combat/SovSelenePayload.h"
#include "Effects/SovGameplayEffect_CinderGrenade.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Character/PlayerDefinition.h"
#include "Framework/SovPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"

#if WITH_AUTOMATION_TESTS
namespace
{
struct FThermalWorld
{
    FEditorScriptExecutionGuard ScriptGuard;
    UWorld* World = nullptr;
    ASovHandoffRuntimeTestController* PC = nullptr;
    ASovAurelionThermalTestPlayer* Player = nullptr;
    ASovAurelionThermalTestCompanion* Selene = nullptr;
    ASovAurelionThermalTestDirector* Director = nullptr;
    ASovCampaignMassRoundTripNPC* Elite = nullptr;
    USovAurelionThermalFractureComponent* Fracture = nullptr;
    USovPoiseComponent* Poise = nullptr;
    USovWeakPointRoutingTestComponent* Weak = nullptr;
    bool bReady = false;
    uint64 Frame = GFrameCounter;
    FThermalWorld()
    {
        const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
        if (!World) { return; }
        if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
        World->InitializeActorsForPlay(FURL()); World->GetTimerManager().Tick(0.f);
        PC = World->SpawnActor<ASovHandoffRuntimeTestController>();
        Player = World->SpawnActor<ASovAurelionThermalTestPlayer>();
        auto* PS = World->SpawnActor<ASovPlayerState>();
        if (!PC || !Player || !PS) { return; }
        World->AddController(PC); PC->SetTestPlayerState(PS);
        auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
        Player->PrepareCampaignInitialization(Definition); PC->Possess(Player);
        if (!Player->StageTestReadiness(PS, true) || !Player->CompleteCampaignDataInitialization(false)) { return; }
        auto* Mission = NewObject<USovCampaignDefinition>(PC); PC->KeepAlive.Add(Mission);
        Mission->MissionId = TEXT("M12_ThermalFixture"); Mission->Protagonist = Player->GetProtagonistIdentityTag();
        Mission->PawnClass = ASovAurelionThermalTestPlayer::StaticClass(); Mission->PlayerDefinition = Definition;
        Mission->AllowedCompanionIds.Add(TEXT("Selene"));
        FSovCampaignBeatDefinition Beat; Beat.BeatId = TEXT("ObserveThermalFracture");
        Beat.ObjectiveText = FText::FromString(TEXT("Fracture the elite, then defeat it")); Mission->Beats.Add(Beat);
        if (PC->GetCampaignState()->BeginMission(Mission) != ESovCampaignResult::Applied) { return; }
        auto* Source = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(); if (!Source) { return; } Source->InitializeTestCombat(0);
        Source->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Selene);
        Selene = World->SpawnActorDeferred<ASovAurelionThermalTestCompanion>(ASovAurelionThermalTestCompanion::StaticClass(),
            FTransform(FVector(0, 500, 0)), PC, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        FString Error;
        if (!Selene || !Selene->PrepareProxy(FSovGameplayTags::Get().Character_Player_Selene, TEXT("Selene"),
            Source->GetNarrativeAbilitySystemComponent(), {}, Error)) { return; }
        Source->Destroy();
        Selene->FinishSpawning(FTransform(FVector(0, 500, 0))); Selene->InitializeTestCombat();
        // Populate the pre-existing convergence owner, the fixture's external campaign setup.
        // The tested component has no special access to or replacement for this ownership check.
        auto* ActiveProperty = FindFProperty<FObjectPropertyBase>(USovConvergenceCompanionState::StaticClass(), TEXT("Active"));
        if (!ActiveProperty) { return; }
        ActiveProperty->SetObjectPropertyValue_InContainer(PC->GetConvergenceCompanionState(), Selene);
        Selene->GetCompanionComponent()->SetLeader(Player, Error);
        if (Selene->GetCompanionComponent()->GetCurrentLeader() != Player) { return; }
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Elite = World->SpawnActor<ASovCampaignMassRoundTripNPC>(ASovCampaignMassRoundTripNPC::StaticClass(), FVector(180,0,0), FRotator::ZeroRotator, Spawn);
        if (!Elite) { return; } Elite->InitializeTestCombat();
        auto* ASC = Elite->GetNarrativeAbilitySystemComponent();
        ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxPoiseAttribute(), 100.f);
        ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 100.f);
        ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 0.f);
        Poise = NewObject<USovPoiseComponent>(Elite); Elite->AddInstanceComponent(Poise); Poise->RegisterComponent();
        if (!Poise->InitializeWithAbilitySystem(ASC)) { return; }
        Weak = NewObject<USovWeakPointRoutingTestComponent>(Elite); Elite->AddInstanceComponent(Weak); Weak->RegisterComponent();
        if (!Weak->InitializeWithAbilitySystem(ASC)) { return; }
        Director = World->SpawnActor<ASovAurelionThermalTestDirector>(); Director->EncounterId = TEXT("M12.Aurelion.ThermalFixture");
        if (!Director->RegisterParticipant(TEXT("Elite"), Elite, true)) { return; }
        Fracture = NewObject<USovAurelionThermalFractureComponent>(Elite); Elite->AddInstanceComponent(Fracture); Fracture->RegisterComponent();
        auto* Anchor = World->SpawnActor<AActor>();
        auto* Root = NewObject<USceneComponent>(Anchor); Anchor->AddInstanceComponent(Root); Anchor->SetRootComponent(Root); Root->RegisterComponent();
        Anchor->SetActorLocation(Selene->GetActorLocation()); Anchor->Tags.Add(Fracture->FrostAnchorId); Fracture->FrostAnchor = Anchor;
        auto* Floor = World->SpawnActor<AActor>(); auto* Box = NewObject<UBoxComponent>(Floor);
        Floor->AddInstanceComponent(Box); Floor->SetRootComponent(Box); Box->SetBoxExtent(FVector(2500,2500,10));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent(); Floor->SetActorLocation(FVector(0,0,-105));
        if (!Fracture->InitializeBindings()) { return; }
        Director->StartTestAttempt(Player);
        bReady = Director->GetEncounterPlayer() == Player && Director->HasEncounterPlayer(Player);
    }
    ~FThermalWorld()
    { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
    void Frost(bool bFreeze = true)
    {
        FSovSelenePayloadContext C; C.SourceAvatar = Selene; C.SourceASC = Selene->GetNarrativeAbilitySystemComponent();
        C.AbilityTag = FSovGameplayTags::Get().Ability_Echo_Selene_StillpointGrenade;
        SovSelenePayload::Control(C, Elite, 5.f, 0.f, bFreeze);
    }
    void Hit(bool bThermal = true, float Damage = 5.f)
    {
        auto* ASC = Player->GetNarrativeAbilitySystemComponent();
        auto Spec = ASC->MakeOutgoingSpec(USovGameplayEffect_CinderGrenadeExplosionDamage::StaticClass(), 1.f, ASC->MakeEffectContext());
        Spec.Data->AddDynamicAssetTag(bThermal ? FSovGameplayTags::Get().Damage_Channel_Thermal : FSovGameplayTags::Get().Damage_Channel_Kinetic);
        Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, Damage);
        Spec.Data->SetSetByCallerMagnitude(FSovGameplayTags::Get().SetByCaller_Damage_PoiseDamage, 0.f);
        ASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Elite->GetNarrativeAbilitySystemComponent());
    }
    void NextFrame()
    { TGuardValue<uint64> ScopedFrame(GFrameCounter, ++Frame); World->GetTimerManager().Tick(.016f); }
    void AdvanceTime(float Seconds)
    { TGuardValue<uint64> ScopedFrame(GFrameCounter, ++Frame); World->Tick(LEVELTICK_TimeOnly, Seconds); }
    bool Complete() const { return Fracture->HasCompletedFracture(Director, Director->GetAttemptId()); }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionThermalNativePayoffTest,
    "ProjectVelkorran.Campaign.Aurelion.ThermalFracture.NativeFrostHeatPoisePayoff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionThermalNativePayoffTest::RunTest(const FString& Parameters)
{
    FThermalWorld F; if (!TestTrue(TEXT("Real campaign, proxy, target and ASC setup"), F.bReady)) { return false; }
    F.Hit(); F.NextFrame(); TestFalse(TEXT("Heat alone supplies no fracture"), F.Complete());
    F.Frost(); TestTrue(TEXT("Real native Selene control opens recoverable window"), F.Fracture->GetFractureWindowRemainingSeconds() > 0.f);
    TestTrue(TEXT("Frost actually reveals the elite weak points"), F.Weak->IsWeakPointRevealActive());
    F.Hit(false); F.NextFrame(); TestFalse(TEXT("Kinetic impact is not a heat trigger"), F.Complete());
    F.Hit(); const float HealthAfterHeat = F.Elite->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
    TestFalse(TEXT("Trigger does not run payoff recursively in its source transaction"), F.Complete());
    F.NextFrame();
    if (!TestTrue(TEXT("Actual control, heat hit and native Poise break create proof"), F.Complete())) { AddError(F.Fracture->LastError); return false; }
    TestTrue(TEXT("Thermal fracture has a real stagger payoff"), F.Poise->IsPoiseBroken());
    TestEqual(TEXT("Poise fracture itself deals no health damage"),
        F.Elite->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), HealthAfterHeat);
    TestTrue(TEXT("Elite survives for the player's conventional finish"), F.Elite->IsAlive());
    TestEqual(TEXT("Local payoff does not auto-complete combat"), F.Director->GetEncounterState(), ESovEncounterState::Active);
    TestEqual(TEXT("Local payoff awards no campaign fact"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    const auto Receipt = F.Fracture->GetFractureReceipt(); TestTrue(TEXT("Receipt contains distinct native transactions"), Receipt.IsComplete());
    F.Hit(); F.NextFrame(); TestEqual(TEXT("Repeated heat cannot mint another payoff"), F.Fracture->GetFractureReceipt().PayoffTransactionId, Receipt.PayoffTransactionId);
    F.Fracture->PrepareForSave_Implementation(); F.Fracture->Load_Implementation();
    TestFalse(TEXT("Actor/component load never manufactures live fracture proof"), F.Complete());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionThermalCompletedActionTest,
    "ProjectVelkorran.Campaign.Aurelion.ThermalFracture.CompletedProofSurvivesCombatActions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionThermalCompletedActionTest::RunTest(const FString& Parameters)
{
    FThermalWorld F; if (!TestTrue(TEXT("Ready thermal fixture"), F.bReady)) { return false; }
    F.Frost(); F.Hit(); F.NextFrame();
    if (!TestTrue(TEXT("Native fracture completed before combat resumes"), F.Complete())) { return false; }
    const auto Payoff = F.Fracture->GetFractureReceipt().PayoffTransactionId;
    const auto& N = FNarrativeGameplayTags::Get();
    for (auto* ASC : {F.Player->GetNarrativeAbilitySystemComponent(), F.Selene->GetNarrativeAbilitySystemComponent()})
    {
        for (const auto Tag : {N.State_Busy, N.State_Interacting, N.State_Movement_Lock, FSovGameplayTags::Get().State_Poise_Broken})
        {
            ASC->AddLooseGameplayTag(Tag);
            TestTrue(TEXT("Subsequent action or stagger does not erase an earned payoff"), F.Complete());
            TestEqual(TEXT("Existing native transaction is retained"), F.Fracture->GetFractureReceipt().PayoffTransactionId, Payoff);
            ASC->RemoveLooseGameplayTag(Tag);
        }
    }
    F.Fracture->Load_Implementation();
    TestFalse(TEXT("Reload still retires live proof"), F.Complete());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionThermalReleaseHoldTest,
    "ProjectVelkorran.Campaign.Aurelion.ThermalFracture.ReleaseOnlyCompletedFrostHold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionThermalReleaseHoldTest::RunTest(const FString& Parameters)
{
    for (bool bNewerHold : {false, true})
    {
        FThermalWorld F; if (!TestTrue(TEXT("Ready thermal fixture"), F.bReady)) { return false; }
        auto* AI = F.World->SpawnActor<ASovCoActionTestNPCController>();
        AI->Possess(F.Selene); F.Selene->InitializeTestCombat();
        auto* Activities = CastChecked<USovCoActionTestActivities>(AI->GetActivityComponent());
        Activities->InitializeForCoAction();
        auto* Companion = F.Selene->GetCompanionComponent(); FString Error;
        if (!TestTrue(TEXT("Real command owner accepts leader"), Companion->SetLeader(F.Player, Error))) { AddError(Error); return false; }
        if (!TestTrue(TEXT("Real frost hold accepted"), Companion->RequestCommand(F.Player, ESovCompanionCommand::HoldPosition, F.Fracture->FrostAnchor, Error))) { AddError(Error); return false; }
        F.Frost(); F.Hit();
        TestTrue(TEXT("Hold remains until native payoff"), Companion->HasAcceptedHoldPosition(F.Fracture->FrostAnchor));
        if (bNewerHold)
        { TestTrue(TEXT("Newer hold accepted before deferred payoff"), Companion->RequestCommand(F.Player, ESovCompanionCommand::HoldPosition, F.Selene, Error)); }
        F.NextFrame();
        if (!TestTrue(TEXT("Actual payoff completes"), F.Complete())) { AddError(F.Fracture->LastError); return false; }
        bool bFound = false;
        const auto* Goal = Cast<USovCompanionCommandGoal>(Activities->GetGoalByKey(USovCompanionCommandGoal::StaticClass(), Companion, bFound));
        if (!TestTrue(TEXT("Accepted command remains owned"), bFound && Goal && Companion->IsCommandCurrent(Goal))) { return false; }
        TestEqual(TEXT("Completed frost hold regroups; newer hold is preserved"), Goal->Command,
            bNewerHold ? ESovCompanionCommand::HoldPosition : ESovCompanionCommand::Regroup);
        if (bNewerHold) { TestTrue(TEXT("Newer target is unchanged"), Companion->HasAcceptedHoldPosition(F.Selene)); }
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionThermalRetirementTest,
    "ProjectVelkorran.Campaign.Aurelion.ThermalFracture.ExpiryForgeryAndStaleOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionThermalRetirementTest::RunTest(const FString& Parameters)
{
    for (int32 Case = 0; Case < 5; ++Case)
    {
        FThermalWorld F; if (!TestTrue(TEXT("Ready fracture fixture"), F.bReady)) { return false; }
        F.Frost(); if (!TestTrue(TEXT("Native frost opens the test window"), F.Fracture->GetFractureWindowRemainingSeconds() > 0.f)) { return false; }
        if (Case == 0)
        {
            FSovDamageResult Forged; Forged.TransactionId = FGuid::NewGuid(); Forged.SourceActor = F.Player; Forged.TargetActor = F.Elite;
            Forged.AppliedHealthDamage = 5.f; Forged.DamageChannels.AddTag(FSovGameplayTags::Get().Damage_Channel_Thermal);
            F.Elite->GetNarrativeAbilitySystemComponent()->OnDamageResolvedAsTarget.Broadcast(Forged);
        }
        else if (Case == 1) { for (int32 I = 0; I < 65; ++I) { F.AdvanceTime(.05f); } F.Hit(); }
        else
        {
            F.Hit();
            if (Case == 2) { F.Player->GetNarrativeAbilitySystemComponent()->SetCharacterReadyEpoch(F.Player->GetNarrativeAbilitySystemComponent()->GetCharacterReadyEpoch() + 1); }
            if (Case == 3) { F.Director->StartTestAttempt(F.Player); }
            if (Case == 4) { F.Fracture->Load_Implementation(); }
        }
        F.NextFrame(); TestFalse(TEXT("Forged, expired, readiness, attempt and loaded callbacks cannot fracture"), F.Complete());
        TestFalse(TEXT("Rejected callbacks cause no Poise payoff"), F.Poise->IsPoiseBroken());
        if (Case == 1)
        {
            F.Frost(false); TestTrue(TEXT("Fresh frost reopens an expired opportunity"), F.Fracture->GetFractureWindowRemainingSeconds() > 0.f);
            F.Hit(); F.NextFrame(); TestTrue(TEXT("A recovered attempt remains physically achievable"), F.Complete());
        }
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionThermalContextualInteractionTest,
    "ProjectVelkorran.Campaign.Aurelion.ThermalFracture.PhysicalContextSetupAndConfirmCostNoScarceResources",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionThermalContextualInteractionTest::RunTest(const FString& Parameters)
{
    FThermalWorld F; if (!TestTrue(TEXT("Ready contextual fixture"), F.bReady)) { return false; }
    auto* PlayerASC = F.Player->GetNarrativeAbilitySystemComponent(); auto* SeleneASC = F.Selene->GetNarrativeAbilitySystemComponent();
    PlayerASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 0.f);
    PlayerASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 0.f);
    SeleneASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 0.f);
    SeleneASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 0.f);
    FString Error;
    F.Fracture->FrostAnchor->SetActorLocation(F.Selene->GetActorLocation() + FVector(800,0,0));
    TestFalse(TEXT("A remote companion cannot fabricate arrival at the clean mark"), F.Fracture->RequestFrostSetup(F.Player, Error));
    F.Fracture->FrostAnchor->SetActorLocation(F.Selene->GetActorLocation());
    auto* Duplicate = F.World->SpawnActor<AActor>(); Duplicate->Tags.Add(F.Fracture->FrostAnchorId);
    TestFalse(TEXT("Duplicated authored mark identity is rejected"), F.Fracture->RequestFrostSetup(F.Player, Error));
    Duplicate->Destroy();
    // A nearby hostile can block a visibility trace before the actual walkable floor.
    // Its Pawn collision must not make Selene's clean mark intermittently unusable.
    auto* CrossingPawn = F.World->SpawnActor<AActor>();
    auto* CrossingBody = NewObject<USphereComponent>(CrossingPawn);
    CrossingPawn->AddInstanceComponent(CrossingBody); CrossingPawn->SetRootComponent(CrossingBody);
    CrossingBody->SetSphereRadius(40.f); CrossingBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CrossingBody->SetCollisionObjectType(ECC_Pawn); CrossingBody->SetCollisionResponseToAllChannels(ECR_Block);
    CrossingBody->RegisterComponent();
    CrossingPawn->SetActorLocation(F.Selene->GetActorLocation() + FVector(35.f, 0.f, -40.f));
    FHitResult VisibilityBlock;
    FCollisionQueryParams Probe(SCENE_QUERY_STAT(AurelionFrostGroundFixture), false, F.Selene);
    TestTrue(TEXT("Pawn collision actually intercepts the old visibility ground ray"),
        F.World->LineTraceSingleByChannel(VisibilityBlock, F.Selene->GetActorLocation()+FVector(0,0,20),
            F.Selene->GetActorLocation()-FVector(0,0,250), ECC_Visibility, Probe)
        && VisibilityBlock.GetActor()==CrossingPawn && VisibilityBlock.ImpactNormal.Z<.7f);
    if (!TestTrue(TEXT("Physical Selene mark opens actual control through crossing Pawn collision at zero Echo"),
        F.Fracture->RequestFrostSetup(F.Player, Error))) { AddError(Error); return false; }
    TestTrue(TEXT("Contextual setup applies real frozen or chilled state"),
        F.Elite->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Status_Frozen)
        || F.Elite->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Status_Chilled));
    TestFalse(TEXT("Repeated setup cannot extend the same open window"), F.Fracture->RequestFrostSetup(F.Player, Error));
    const float HealthBefore = F.Elite->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
    F.Player->SetActorLocation(FVector(-2000,0,0));
    TestFalse(TEXT("Remote heat confirmation cannot create a hit"), F.Fracture->RequestConfirmFracture(F.Player, Error));
    F.Player->SetActorLocation(FVector::ZeroVector);
    if (!TestTrue(TEXT("Nearby Tarrik creates a real heat transaction with no resources"), F.Fracture->RequestConfirmFracture(F.Player, Error))) { AddError(Error); return false; }
    F.NextFrame();
    if (!TestTrue(TEXT("Context interaction produces verified native fracture"), F.Complete())) { AddError(F.Fracture->LastError); return false; }
    TestEqual(TEXT("Setup, impact and fracture leave Health for conventional combat"),
        F.Elite->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), HealthBefore);
    for (auto* ASC : {PlayerASC, SeleneASC})
    {
        TestEqual(TEXT("Neither participant spends or gains Echo"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetEchoAttribute()), 0.f);
        TestEqual(TEXT("Neither participant spends or gains stamina"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute()), 0.f);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionThermalInterruptionTest,
    "ProjectVelkorran.Campaign.Aurelion.ThermalFracture.HeroInterruptPermanentlyRetiresPendingWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionThermalInterruptionTest::RunTest(const FString& Parameters)
{
    for (int32 Hero = 0; Hero < 2; ++Hero)
    {
        FThermalWorld F; if (!TestTrue(TEXT("Ready interruption fixture"), F.bReady)) { return false; }
        F.Frost(); if (!TestTrue(TEXT("Native frost opens before interruption"), F.Fracture->GetFractureWindowRemainingSeconds() > 0.f)) { return false; }
        F.Hit();
        auto* ASC = Hero == 0 ? F.Player->GetNarrativeAbilitySystemComponent() : F.Selene->GetNarrativeAbilitySystemComponent();
        const auto Tag = Hero == 0 ? FNarrativeGameplayTags::Get().State_Movement_Lock : FNarrativeGameplayTags::Get().State_SequencerControlled;
        // Real ASC tag registration must cancel the scheduled payoff immediately, even if
        // the physical interruption ends before the next frame's callback would execute.
        ASC->AddLooseGameplayTag(Tag);
        TestEqual(TEXT("Interruption closes the setup immediately"), F.Fracture->GetFractureWindowRemainingSeconds(), 0.f);
        ASC->RemoveLooseGameplayTag(Tag); F.NextFrame();
        TestFalse(TEXT("Recovery cannot revive the retired callback"), F.Complete());
        TestFalse(TEXT("Retired impact does not damage Poise"), F.Poise->IsPoiseBroken());
        FString Error;
        TestFalse(TEXT("Still-live old frost GE is insufficient after interruption"), F.Fracture->RequestConfirmFracture(F.Player, Error));
        F.Frost(false); F.Hit(); F.NextFrame();
        TestTrue(TEXT("A newly applied frost setup works after recovery"), F.Complete());
    }
    return true;
}
#endif
