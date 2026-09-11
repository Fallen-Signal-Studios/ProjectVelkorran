// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Companions/SovConvergenceCompanionState.h"
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "Resonance/SovResonanceComponent.h"
#include "Resonance/SovResonanceAbility.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Characters/SovTarrikCharacter.h"
#include "Characters/SovSeleneCharacter.h"
#include "Character/PlayerDefinition.h"
#include "AI/NPCDefinition.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#if WITH_AUTOMATION_TESTS
struct FSovResonanceTestAccess
{
    static void SetPair(USovResonanceComponent* Component, UNarrativeAbilitySystemComponent* Player, UNarrativeAbilitySystemComponent* Partner)
    { Component->PlayerASC = Player; Component->PartnerASC = Partner; }
    static void Offer(USovResonanceComponent* Component)
    {
        Component->Interaction.InteractionId = FGuid::NewGuid(); Component->Interaction.State = ESovResonanceState::Offered;
        Component->bOwnsAvailableTag = true;
        Component->PlayerASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_Resonance_Available);
    }
    static USovResonanceTicket* AddParticipant(USovResonanceComponent* Component, UNarrativeAbilitySystemComponent* ASC)
    {
        if (!Component->Interaction.InteractionId.IsValid()) { Component->Interaction.InteractionId = FGuid::NewGuid(); }
        Component->Interaction.State = ESovResonanceState::Committed;
        auto* Ticket = NewObject<USovResonanceTicket>(Component); Ticket->Coordinator = Component;
        Ticket->ExpectedASC = ASC; Ticket->InteractionId = Component->Interaction.InteractionId; Component->Tickets.Add(Ticket);
        const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(USovResonanceAbility::StaticClass(), 1, INDEX_NONE, Ticket));
        if (ASC == Component->PlayerASC) { Component->PlayerHandle = Handle; } else { Component->PartnerHandle = Handle; }
        ASC->TryActivateAbility(Handle, false); return Ticket;
    }
    static FGameplayAbilitySpecHandle PlayerHandle(USovResonanceComponent* Component) { return Component->PlayerHandle; }
    static FGameplayAbilitySpecHandle PartnerHandle(USovResonanceComponent* Component) { return Component->PartnerHandle; }
};
struct FSovConvergenceTestAccess
{
    static const TArray<FSovCompanionKitGrant>& Kit(const ASovProtagonistCompanionCharacter* Proxy) { return Proxy->CopiedGrants; }
    static void SeedOwnership(USovConvergenceCompanionState* State, ASovProtagonistCompanionCharacter* Active, ASovProtagonistCompanionCharacter* Staged)
    { State->Active = Active; State->Staged = Staged; }
};
namespace
{
struct FResonanceWorld
{
    UWorld* World = nullptr;
    FResonanceWorld()
    {
        const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
            ERHIFeatureLevel::Num, &WorldInitialization);
        if (!World) { return; }
        if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
    }
    ~FResonanceWorld() { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
    ASovAxiomRuntimeTestCharacter* Character()
    {
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Result = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(), FTransform::Identity, Params);
        if (Result) { Result->InitializeTestCombat(0); } return Result;
    }
    ASovProtagonistCompanionCharacter* DeferredProxy(AActor* Owner)
    { return World->SpawnActorDeferred<ASovProtagonistCompanionCharacter>(ASovProtagonistCompanionCharacter::StaticClass(), FTransform::Identity, Owner, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn); }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovResonanceOwnershipRuntimeTest, "ProjectVelkorran.Campaign.Resonance.PairedOwnershipAndCancellation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovResonanceOwnershipRuntimeTest::RunTest(const FString& Parameters)
{
    FResonanceWorld F; auto* Tarrik = F.Character(); auto* Selene = F.Character();
    if (!Tarrik || !Selene) { return false; }
    auto* A = Tarrik->GetNarrativeAbilitySystemComponent(); auto* B = Selene->GetNarrativeAbilitySystemComponent();
    auto* Coordinator = NewObject<USovResonanceComponent>(Tarrik); Tarrik->AddInstanceComponent(Coordinator); Coordinator->RegisterComponent();
    FSovResonanceTestAccess::SetPair(Coordinator, A, B);
    const auto& T = FSovGameplayTags::Get(); const auto& N = FNarrativeGameplayTags::Get();
    A->AddLooseGameplayTag(T.State_Resonance_Available);
    FSovResonanceTestAccess::Offer(Coordinator); Coordinator->CancelInteraction();
    TestEqual(TEXT("Cancel releases only its own available count"), A->GetTagCount(T.State_Resonance_Available), 1);
    A->AddLooseGameplayTag(N.State_Busy); B->AddLooseGameplayTag(N.State_Busy);
    auto* TicketA = FSovResonanceTestAccess::AddParticipant(Coordinator, A);
    auto* TicketB = FSovResonanceTestAccess::AddParticipant(Coordinator, B);
    const auto HandleA = FSovResonanceTestAccess::PlayerHandle(Coordinator); const auto HandleB = FSovResonanceTestAccess::PartnerHandle(Coordinator);
    TestTrue(TEXT("Separate ASCs accept their owned participation"), A != B && Coordinator->IsTicketCurrent(TicketA) && Coordinator->IsTicketCurrent(TicketB));
    TestTrue(TEXT("Both native participation abilities are active"), A->FindAbilitySpecFromHandle(HandleA)->IsActive() && B->FindAbilitySpecFromHandle(HandleB)->IsActive());
    auto* Forged = NewObject<USovResonanceTicket>(Coordinator); Forged->Coordinator = Coordinator; Forged->ExpectedASC = A; Forged->InteractionId = TicketA->InteractionId;
    TestFalse(TEXT("Matching a GUID cannot manufacture membership"), Coordinator->IsTicketCurrent(Forged));
    Coordinator->CancelInteraction();
    TestFalse(TEXT("Ticket cannot survive cancellation"), Coordinator->IsTicketCurrent(TicketA));
    TestNull(TEXT("Player grant is retired"), A->FindAbilitySpecFromHandle(HandleA)); TestNull(TEXT("Partner grant is retired"), B->FindAbilitySpecFromHandle(HandleB));
    TestFalse(TEXT("Player commit tag is cleared"), A->HasMatchingGameplayTag(T.State_Resonance_Committed));
    TestFalse(TEXT("Partner commit tag is cleared"), B->HasMatchingGameplayTag(T.State_Resonance_Committed));
    TestEqual(TEXT("Foreign player Busy survives"), A->GetTagCount(N.State_Busy), 1); TestEqual(TEXT("Foreign partner Busy survives"), B->GetTagCount(N.State_Busy), 1);
    Coordinator->CancelInteraction();
    TestEqual(TEXT("Cancellation is idempotent"), A->GetTagCount(N.State_Busy), 1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovConvergenceProxyRuntimeTest, "ProjectVelkorran.Campaign.Companion.CuratedKitAndRollback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovConvergenceProxyRuntimeTest::RunTest(const FString& Parameters)
{
    FResonanceWorld F; auto* Source = F.Character(); if (!Source) { return false; }
    auto* ASC = Source->GetNarrativeAbilitySystemComponent(); const auto Identity = FSovGameplayTags::Get().Character_Player_Tarrik;
    ASC->AddLooseGameplayTag(Identity);
    const auto SourceGrant = ASC->GiveAbility(FGameplayAbilitySpec(USovWeakPointFireTestAbility::StaticClass(), 3));
    const float EchoBefore = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetEchoAttribute());
    auto* Active = F.DeferredProxy(Source); auto* Staged = F.DeferredProxy(Source); if (!Active || !Staged) { return false; }
    FString Reason;
    const TArray<TSubclassOf<UGameplayAbility>> Curated { USovWeakPointFireTestAbility::StaticClass(), USovWeakPointMeleeTestAbility::StaticClass() };
    TestTrue(TEXT("Proxy reads the actual outgoing unlocked kit"), Staged->PrepareProxy(Identity, TEXT("Tarrik"), ASC, Curated, Reason));
    const auto& Grants = FSovConvergenceTestAccess::Kit(Staged);
    TestEqual(TEXT("Locked curated ability stays locked"), Grants.Num(), 1);
    if (Grants.Num() == 1) { TestEqual(TEXT("Actual upgrade level is copied"), Grants[0].Level, 3); }
    TestNotNull(TEXT("Source grant remains owned by player"), ASC->FindAbilitySpecFromHandle(SourceGrant));
    TestEqual(TEXT("No player Echo was spent by AI preparation"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetEchoAttribute()), EchoBefore);
    auto* State = NewObject<USovConvergenceCompanionState>(Source); Source->AddInstanceComponent(State); State->RegisterComponent();
    FSovConvergenceTestAccess::SeedOwnership(State, Active, Staged);
    State->RollbackStaged();
    TestTrue(TEXT("Only the staged proxy is destroyed by rollback"), Staged->IsActorBeingDestroyed());
    TestFalse(TEXT("Original companion survives failed handoff"), Active->IsActorBeingDestroyed());
    TestFalse(TEXT("No stale stage survives rollback"), State->HasStagedProxy());
    TestFalse(TEXT("Missing serialized companion data fails closed"), State->StageSavedRecord({}, nullptr, Identity, Reason));
    TestEqual(TEXT("Native companion owner restores in companion phase"), State->GetSaveRestorePhase(), ENarrativeRestorePhase::Companions);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovConvergenceDefinitionRuntimeTest, "ProjectVelkorran.Campaign.Companion.ProfileAndControlGraphValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovConvergenceDefinitionRuntimeTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<USovCampaignDefinition> Mission(NewObject<USovCampaignDefinition>());
    TStrongObjectPtr<UPlayerDefinition> TarrikPlayer(NewObject<UPlayerDefinition>()), SelenePlayer(NewObject<UPlayerDefinition>());
    TStrongObjectPtr<UNPCDefinition> TarrikNPC(NewObject<UNPCDefinition>()), SeleneNPC(NewObject<UNPCDefinition>());
    const auto& Tags = FSovGameplayTags::Get(); FString Reason;
    Mission->MissionId = TEXT("M12_ConvergenceTest"); Mission->Protagonist = Tags.Character_Player_Tarrik;
    Mission->PawnClass = ASovTarrikCharacter::StaticClass(); Mission->PlayerDefinition = TarrikPlayer.Get();
    FSovCampaignProtagonistProfile Alternate; Alternate.Protagonist = Tags.Character_Player_Selene;
    Alternate.PawnClass = ASovSeleneCharacter::StaticClass(); Alternate.PlayerDefinition = SelenePlayer.Get(); Mission->AlternateProtagonists.Add(Alternate);
    Mission->bAllowJointResonance = true; Mission->AllowedResonanceTypes.Add(ESovResonanceType::SupportSever);
    Mission->AllowedCompanionIds = { TEXT("Tarrik"), TEXT("Selene") };
    FSovCampaignCompanionProfile Tarrik; Tarrik.Protagonist = Tags.Character_Player_Tarrik; Tarrik.CompanionId = TEXT("Tarrik");
    Tarrik.CompanionClass = ASovProtagonistCompanionCharacter::StaticClass(); Tarrik.CompanionDefinition = TarrikNPC.Get(); Tarrik.EntryAnchorTag = TEXT("TarrikEntry");
    FSovCampaignCompanionProfile Selene = Tarrik; Selene.Protagonist = Tags.Character_Player_Selene; Selene.CompanionId = TEXT("Selene");
    Selene.CompanionDefinition = SeleneNPC.Get(); Selene.EntryAnchorTag = TEXT("SeleneEntry"); Mission->ProtagonistCompanions = { Tarrik, Selene };
    FSovCampaignBeatDefinition Intro; Intro.BeatId = TEXT("Intro"); Intro.RequiredProtagonist = Tags.Character_Player_Tarrik;
    FSovCampaignBeatDefinition Handoff; Handoff.BeatId = TEXT("Handoff"); Handoff.RequiredProtagonist = Tags.Character_Player_Tarrik;
    Handoff.HandoffToProtagonist = Tags.Character_Player_Selene; Handoff.RequiredHandoffAnchorId = TEXT("ControlMark"); Handoff.PrerequisiteBeats.Add(Intro.BeatId);
    FSovCampaignBeatDefinition Finish; Finish.BeatId = TEXT("Finish"); Finish.RequiredProtagonist = Tags.Character_Player_Selene; Finish.PrerequisiteBeats.Add(Handoff.BeatId);
    Mission->Beats = { Intro, Handoff, Finish };
    TestTrue(TEXT("Ordered Tarrik intro, Selene handoff and Selene beat validate"), Mission->ValidateDefinition(Reason));
    FSovCampaignBeatDefinition Stranded; Stranded.BeatId = TEXT("Stranded"); Stranded.RequiredProtagonist = Tags.Character_Player_Tarrik;
    Mission->Beats.Add(Stranded);
    TestFalse(TEXT("Handoff cannot strand unordered mandatory outgoing work"), Mission->ValidateDefinition(Reason)); Mission->Beats.Pop();
    Mission->Beats[2].PrerequisiteBeats = { Intro.BeatId };
    TestFalse(TEXT("Incoming lead beat needs a handoff ancestor"), Mission->ValidateDefinition(Reason)); Mission->Beats[2] = Finish;
    Mission->ProtagonistCompanions[1].EntryAnchorTag = Tarrik.EntryAnchorTag;
    TestFalse(TEXT("Companion recovery anchor tags are unique"), Mission->ValidateDefinition(Reason)); Mission->ProtagonistCompanions[1] = Selene;
    Mission->ProtagonistCompanions[0].CuratedCompanionAbilities = { USovWeakPointFireTestAbility::StaticClass(), USovWeakPointFireTestAbility::StaticClass() };
    TestFalse(TEXT("Curated kit rejects duplicate grants"), Mission->ValidateDefinition(Reason)); Mission->ProtagonistCompanions[0] = Tarrik;
    Mission->ProtagonistCompanions.Pop(); TestFalse(TEXT("Joint play requires both canonical companion profiles"), Mission->ValidateDefinition(Reason));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovResonanceExposureExtensionTest,
    "ProjectVelkorran.Campaign.Resonance.SecondRevealExtendsExposureWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovResonanceExposureExtensionTest::RunTest(const FString& Parameters)
{
    FResonanceWorld Fixture;
    if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
    auto* Selene = Fixture.Character();
    auto* Target = Fixture.Character();
    if (!TestNotNull(TEXT("Selene"), Selene) || !TestNotNull(TEXT("Target"), Target)) { return false; }

    auto* WeakPoints = NewObject<USovWeakPointRoutingTestComponent>(Target);
    Target->AddInstanceComponent(WeakPoints);
    WeakPoints->RegisterComponent();
    WeakPoints->InitializeWithAbilitySystem(Target->GetNarrativeAbilitySystemComponent());
    WeakPoints->OnWeakPointRevealStateChanged.AddDynamic(
        Selene, &ASovAxiomRuntimeTestCharacter::RecordWeakPointReveal);

    // E4 severs two independent Weaver anchors. The first opens the reveal; the second
    // arrives while it is still lit and extends it. RevealWeakPoints is documented to
    // "begin or extend" the window and takes the max end time, so the extension is real
    // state - the defect was that the edge-gated broadcast never published it, leaving
    // USovResonanceTargetComponent's ExposureUntil pinned to the first window.
    if (!TestTrue(TEXT("First anchor sever opens the reveal"), WeakPoints->RevealWeakPoints(5.f, Selene))) { return false; }
    if (!TestEqual(TEXT("Opening the reveal broadcasts once"), Selene->RecordedRevealActive.Num(), 1)) { return false; }
    TestTrue(TEXT("First broadcast reports an active reveal"), Selene->RecordedRevealActive[0]);
    const float FirstRemaining = Selene->RecordedRevealRemaining[0];
    TestTrue(TEXT("First broadcast advertises the opening window"), FirstRemaining > 4.f && FirstRemaining <= 5.f + KINDA_SMALL_NUMBER);

    if (!TestTrue(TEXT("Second anchor sever extends the reveal"), WeakPoints->RevealWeakPoints(9.f, Selene))) { return false; }
    if (!TestEqual(TEXT("Extending an active reveal broadcasts again"), Selene->RecordedRevealActive.Num(), 2)) { return false; }
    TestTrue(TEXT("Second broadcast still reports an active reveal"), Selene->RecordedRevealActive[1]);
    TestTrue(TEXT("Second broadcast advertises the extended window, not the first"),
        Selene->RecordedRevealRemaining[1] > FirstRemaining + 1.f);
    TestTrue(TEXT("Second broadcast retains the severing instigator"),
        Selene->RecordedRevealInstigators[1].Get() == Selene);

    // A shorter reveal cannot retract a longer live window, and must not re-broadcast.
    TestTrue(TEXT("A shorter reveal is still accepted"), WeakPoints->RevealWeakPoints(1.f, Selene));
    TestEqual(TEXT("A shorter reveal does not shorten or re-broadcast the live window"),
        Selene->RecordedRevealActive.Num(), 2);

    WeakPoints->ClearWeakPointReveal();
    if (!TestEqual(TEXT("Clearing the reveal broadcasts the inactive edge"), Selene->RecordedRevealActive.Num(), 3)) { return false; }
    TestFalse(TEXT("Final broadcast reports an inactive reveal"), Selene->RecordedRevealActive[2]);
    TestEqual(TEXT("Final broadcast advertises no remaining window"), Selene->RecordedRevealRemaining[2], 0.f);
    return true;
}
#endif
