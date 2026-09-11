// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Presentation/SovCombatFeedbackComponent.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatFeedbackOutcomeTest,
    "ProjectVelkorran.Presentation.CombatFeedback.OutcomePriority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCombatFeedbackOutcomeTest::RunTest(const FString& Parameters)
{
    FSovDamageResult Hit;
    Hit.RequestedStatusTags.AddTag(FSovGameplayTags::Get().Status_Apply_Freeze);
    Hit.bStatusApplicationRequested = true; Hit.BaseDamage = 500.f;
    TestTrue(TEXT("Requested damage/status is not visible hit proof"), FSovCombatFeedbackPolicy::Select(Hit, true) == ESovCombatFeedback::None);
    Hit.AppliedShieldDamage = 1.f;
    TestTrue(TEXT("Shield damage gives Tarrik hit signal"), FSovCombatFeedbackPolicy::Select(Hit, false) == ESovCombatFeedback::TarrikImpact);
    TestTrue(TEXT("Same accepted hit gives distinct Selene signal"), FSovCombatFeedbackPolicy::Select(Hit, true) == ESovCombatFeedback::SeleneImpact);
    Hit.bPeriodicDamage = true;
    TestTrue(TEXT("Periodic damage cannot flood routine bursts"), FSovCombatFeedbackPolicy::Select(Hit, true) == ESovCombatFeedback::None);
    Hit.bShieldBroken = true;
    TestTrue(TEXT("A real periodic shield break retains its important signal"), FSovCombatFeedbackPolicy::Select(Hit, true) == ESovCombatFeedback::Break);
    Hit.bPeriodicDamage = false; Hit.bShieldBroken = false; Hit.DefenseKind = ESovDefenseKind::Guard;
    TestTrue(TEXT("Existing guard cue is not duplicated as an ice hit"), FSovCombatFeedbackPolicy::Select(Hit, true) == ESovCombatFeedback::None);
    Hit.AppliedShieldDamage = 0.f; Hit.DefenseKind = ESovDefenseKind::None; Hit.bPoiseBroken = true;
    TestTrue(TEXT("A break flag with no application does not create a break"), FSovCombatFeedbackPolicy::Select(Hit, false) == ESovCombatFeedback::None);
    Hit.AppliedPoiseDamage = 10.f;
    TestTrue(TEXT("Pure Poise break retains a break signal"), FSovCombatFeedbackPolicy::Select(Hit, false) == ESovCombatFeedback::Break);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatFeedbackBudgetTest,
    "ProjectVelkorran.Presentation.CombatFeedback.BoundedRapidFireAndCriticalReserve",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCombatFeedbackBudgetTest::RunTest(const FString& Parameters)
{
    double LastRoutine = -100., LastCritical = -100.; int32 Accepted = 0;
    for (int32 I = 0; I < 1000; ++I)
    { Accepted += FSovCombatFeedbackPolicy::Admit(I * .001, LastRoutine, ESovCombatFeedback::TarrikImpact, 0, 0) ? 1 : 0; }
    TestTrue(TEXT("1000 callbacks in one second yield at most 13 routine bursts"), Accepted <= 13);
    TestFalse(TEXT("Four live local routine bursts prevent another"), FSovCombatFeedbackPolicy::Admit(2., LastRoutine, ESovCombatFeedback::SeleneImpact, 4, 4));
    TestFalse(TEXT("Twelve global routine bursts reserve four slots"), FSovCombatFeedbackPolicy::Admit(2., LastRoutine, ESovCombatFeedback::SeleneImpact, 0, 12));
    TestTrue(TEXT("A break can use the reserved slots at the same time"), FSovCombatFeedbackPolicy::Admit(2., LastCritical, ESovCombatFeedback::Break, 4, 12));
    TestFalse(TEXT("Repeated break in the same frame is bounded"), FSovCombatFeedbackPolicy::Admit(2., LastCritical, ESovCombatFeedback::Break, 4, 12));
    TestFalse(TEXT("Six local effects are an absolute cap"), FSovCombatFeedbackPolicy::Admit(3., LastCritical, ESovCombatFeedback::Break, 6, 12));
    TestFalse(TEXT("Sixteen world effects are an absolute cap"), FSovCombatFeedbackPolicy::Admit(3., LastCritical, ESovCombatFeedback::Break, 0, 16));
    const auto* Player = GetDefault<ASovPlayerCharacterBase>()->FindComponentByClass<USovCombatFeedbackComponent>();
    const auto* Companion = GetDefault<ASovProtagonistCompanionCharacter>()->FindComponentByClass<USovCombatFeedbackComponent>();
    TestNotNull(TEXT("Native player owns presenter"), Player);
    TestNotNull(TEXT("Native companion owns presenter"), Companion);
    if (Player && Companion)
    {
        TestFalse(TEXT("Presenter has no gameplay polling tick"), Player->PrimaryComponentTick.bCanEverTick);
        TestTrue(TEXT("Companion cosmetic multicast is replicated"), Companion->GetIsReplicated());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatFeedbackNativeReceiptTest,
    "ProjectVelkorran.Presentation.CombatFeedback.NativeDamageAndRetiredLife",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCombatFeedbackNativeReceiptTest::RunTest(const FString& Parameters)
{
    FEditorScriptExecutionGuard NativeCallbacks;
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
        .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Native damage test world"), World)) { return false; }
    if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Source = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    auto* Target = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(), FVector(150, 0, 0), FRotator::ZeroRotator, Spawn);
    if (TestNotNull(TEXT("Source"), Source) && TestNotNull(TEXT("Target"), Target))
    {
        Source->InitializeTestCombat(0); Target->InitializeTestCombat(1);
        auto* SourceASC = Source->GetNarrativeAbilitySystemComponent();
        auto* TargetASC = Target->GetNarrativeAbilitySystemComponent();
        auto Context = SourceASC->MakeEffectContext(); Context.AddInstigator(Source, Source);
        FGameplayEffectSpec Spec(GetDefault<USovCombatRoutingTestEffect>(), Context, 1.f);
        Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, 5.f);
        SourceASC->ApplyGameplayEffectSpecToTarget(Spec, TargetASC);
        const FSovDamageResult Real = Target->LastDamageResult;
        TestTrue(TEXT("Actual native damage is eligible for cosmetic feedback"), USovCombatFeedbackComponent::HasAdmissibleNativeReceipt(Real));
        FSovDamageResult Forged; Forged.TransactionId = Real.TransactionId; Forged.SourceActor = Source;
        Forged.TargetActor = Target; Forged.AppliedHealthDamage = 5.f;
        TestFalse(TEXT("A copied set of visible fields cannot manufacture feedback"), USovCombatFeedbackComponent::HasAdmissibleNativeReceipt(Forged));
        TargetASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
        TargetASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
        TestFalse(TEXT("A real historical hit cannot flash on the restored life"), USovCombatFeedbackComponent::HasAdmissibleNativeReceipt(Real));
    }
    World->DestroyWorld(false);
    if (GEngine) { GEngine->DestroyWorldContext(World); }
    return true;
}
#endif
