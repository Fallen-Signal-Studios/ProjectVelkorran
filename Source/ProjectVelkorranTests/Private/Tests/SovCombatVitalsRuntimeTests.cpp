// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovCombatVitalsWidget.h"
#include "Tests/SovReadinessRuntimeTestFixtures.h"
#include "Character/PlayerDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
namespace
{
    struct FVitalsWorld
    {
        FEditorScriptExecutionGuard ScriptGuard;
        UWorld* World = nullptr;
        ASovHandoffRuntimeTestController* PC = nullptr;
        ASovPlayerState* PS = nullptr;
        FVitalsWorld()
        {
            const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false)
                .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
            if (!World) { return; }
            if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
            PC = World->SpawnActor<ASovHandoffRuntimeTestController>();
            PS = World->SpawnActor<ASovPlayerState>();
            if (PC && PS) { PC->SetTestPlayerState(PS); }
        }
        ~FVitalsWorld()
        { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
        ASovReadinessRuntimeTestPawn* StagePawn()
        {
            if (!PC || !PS) { return nullptr; }
            auto* Pawn = World->SpawnActor<ASovReadinessRuntimeTestPawn>();
            auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
            if (!Pawn || !Pawn->PrepareCampaignInitialization(Definition)) { return nullptr; }
            PC->Possess(Pawn);
            if (!Pawn->StageTestReadiness(PS, true)) { return nullptr; }
            Pawn->BindProductionReadiness();
            return Pawn;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatVitalsHandoffTest,
    "ProjectVelkorran.Campaign.Frontend.CombatVitalsFollowOnlyReadyCurrentPawn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCombatVitalsHandoffTest::RunTest(const FString& Parameters)
{
    FVitalsWorld F;
    auto* First = F.StagePawn();
    if (!TestNotNull(TEXT("Staged first pawn"), First)) { return false; }
    FSovCombatVitalsSnapshot View;
    TestFalse(TEXT("Incomplete initial data cannot appear in combat readout"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    TestTrue(TEXT("First pawn crosses production readiness"), First->CompleteCampaignDataInitialization(false));
    auto* ASC = Cast<UNarrativeAbilitySystemComponent>(F.PS->GetAbilitySystemComponent());
    if (!TestNotNull(TEXT("PlayerState ASC"), ASC)) { return false; }
    const FGameplayAttribute Attributes[] = {UNarrativeAttributeSetBase::GetHealthAttribute(),
        UNarrativeAttributeSetBase::GetShieldAttribute(), UNarrativeAttributeSetBase::GetStaminaAttribute(),
        UNarrativeAttributeSetBase::GetPoiseAttribute(), UNarrativeAttributeSetBase::GetEchoAttribute()};
    const float Values[] = {41.f, 23.f, 60.f, 17.f, 78.f};
    for (int32 Index = 0; Index < 5; ++Index) { ASC->SetNumericAttributeBase(Attributes[Index], Values[Index]); }
    TestTrue(TEXT("Ready current pawn projects all five resources"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    TestTrue(TEXT("Snapshot names the current pawn"), View.Pawn.Get() == First);
    for (int32 Index = 0; Index < 5; ++Index)
    {
        TestEqual(FString::Printf(TEXT("Resource %d reads its canonical value"), Index), View.Values[Index].Current, Values[Index]);
        TestEqual(FString::Printf(TEXT("Projection cannot write resource %d"), Index), ASC->GetNumericAttribute(Attributes[Index]), Values[Index]);
    }
    auto* Second = F.StagePawn();
    if (!TestNotNull(TEXT("Replacement pawn"), Second)) { return false; }
    TestFalse(TEXT("Handoff to an unready pawn hides the old display"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    TestFalse(TEXT("Hidden view does not retain the old pawn"), View.Pawn.IsValid());
    TestEqual(TEXT("Hidden view clears old resource values"), View.Values[4].Current, 0.f);
    TestTrue(TEXT("Replacement completes its own production readiness"), Second->CompleteCampaignDataInitialization(false));
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 12.f);
    TestTrue(TEXT("Readout follows the replacement after readiness"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    TestTrue(TEXT("Readout never pins the outgoing pawn"), View.Pawn.Get() == Second);
    TestEqual(TEXT("Replacement resource is projected without old-value carryover"), View.Values[4].Current, 12.f);
    ASC->InitAbilityActorInfo(F.PS, First);
    TestFalse(TEXT("An ASC/avatar mismatch hides even a previously ready pawn"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    TestFalse(TEXT("Retired binding clears the projected owner"), View.Pawn.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatVitalsHUDHideTest,
    "ProjectVelkorran.Campaign.Frontend.CombatVitalsHonorsCountedHUDHideRequests",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCombatVitalsHUDHideTest::RunTest(const FString& Parameters)
{
    FVitalsWorld F;
    auto* Pawn = F.StagePawn();
    if (!TestNotNull(TEXT("Staged current pawn"), Pawn)
        || !TestTrue(TEXT("Pawn reaches production readiness"), Pawn->CompleteCampaignDataInitialization(false))) { return false; }
    auto* ASC = Cast<UNarrativeAbilitySystemComponent>(F.PS->GetAbilitySystemComponent());
    if (!TestNotNull(TEXT("Current player ASC"), ASC)) { return false; }
    const FGameplayAttribute Attributes[] = {UNarrativeAttributeSetBase::GetHealthAttribute(),
        UNarrativeAttributeSetBase::GetShieldAttribute(), UNarrativeAttributeSetBase::GetStaminaAttribute(),
        UNarrativeAttributeSetBase::GetPoiseAttribute(), UNarrativeAttributeSetBase::GetEchoAttribute()};
    const float Values[] = {41.f, 23.f, 60.f, 17.f, 78.f};
    for (int32 Index = 0; Index < 5; ++Index) { ASC->SetNumericAttributeBase(Attributes[Index], Values[Index]); }
    FSovCombatVitalsSnapshot View;
    TestTrue(TEXT("Ready pawn projects before a hide request"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    const auto CheckResources = [&]()
    {
        for (int32 Index = 0; Index < 5; ++Index)
        { TestEqual(FString::Printf(TEXT("HUD visibility never changes resource %d"), Index), ASC->GetNumericAttribute(Attributes[Index]), Values[Index]); }
    };
    const auto CheckHidden = [&]()
    {
        TestFalse(TEXT("Owned HUD-hide request suppresses the resource projection"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
        TestFalse(TEXT("Hidden projection clears its pawn"), View.Pawn.IsValid());
        for (int32 Index = 0; Index < 5; ++Index)
        {
            TestEqual(TEXT("Hidden projection clears every current value"), View.Values[Index].Current, 0.f);
            TestEqual(TEXT("Hidden projection clears every maximum value"), View.Values[Index].Maximum, 0.f);
        }
        CheckResources();
    };
    const FGameplayTag Hide = FNarrativeGameplayTags::Get().State_Player_WantsHideHUD;
    const FGameplayTag HideAll = FNarrativeGameplayTags::Get().State_Player_WantsHideHUD_All;
    ASC->AddLooseGameplayTag(Hide, 1, EGameplayTagReplicationState::CountToOwner);
    CheckHidden();
    ASC->AddLooseGameplayTag(Hide, 1, EGameplayTagReplicationState::CountToOwner);
    TestEqual(TEXT("Two independent owners retain two hide counts"), ASC->GetTagCount(Hide), 2);
    ASC->RemoveLooseGameplayTag(Hide, 1, EGameplayTagReplicationState::CountToOwner);
    TestEqual(TEXT("Releasing one owner preserves the other request"), ASC->GetTagCount(Hide), 1);
    CheckHidden();
    ASC->RemoveLooseGameplayTag(Hide, 1, EGameplayTagReplicationState::CountToOwner);
    TestTrue(TEXT("Projection resumes when the last ordinary hide owner releases"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    ASC->AddLooseGameplayTag(HideAll, 1, EGameplayTagReplicationState::CountToOwner);
    CheckHidden();
    ASC->RemoveLooseGameplayTag(HideAll, 1, EGameplayTagReplicationState::CountToOwner);
    TestTrue(TEXT("Projection resumes after the all-HUD request ends"), USovCombatVitalsWidget::ReadCurrentVitals(F.PC, View));
    TestTrue(TEXT("Resumed view belongs to the same current pawn"), View.Pawn.Get() == Pawn);
    for (int32 Index = 0; Index < 5; ++Index)
    { TestEqual(TEXT("Resumed view displays the preserved resource"), View.Values[Index].Current, Values[Index]); }
    CheckResources();
    return true;
}
#endif
