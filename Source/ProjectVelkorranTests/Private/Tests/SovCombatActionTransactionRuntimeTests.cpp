// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCombatActionTransactionTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovSettingsTestFixtures.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Components/CapsuleComponent.h"
#include "Components/SovEchoComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_AUTOMATION_TESTS
namespace
{
struct FCombatActionTransactionWorld
{
    UWorld* World=nullptr;
    FCombatActionTransactionWorld()
    {
        // Preserve the UE 5.7 Mac fix: create and initialize this world exactly once.
        const UWorld::InitializationValues Values=UWorld::InitializationValues().AllowAudioPlayback(false)
            .RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
            .CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
        if (World&&GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
    }
    ~FCombatActionTransactionWorld()
    {
        if (World)
        {
            World->DestroyWorld(false);
            if (GEngine) { GEngine->DestroyWorldContext(World); }
        }
    }
    ASovAxiomRuntimeTestCharacter* Character(FVector Location=FVector::ZeroVector)
    {
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Actor=World?World->SpawnActor<ASovAxiomRuntimeTestCharacter>(
            ASovAxiomRuntimeTestCharacter::StaticClass(),Location,FRotator::ZeroRotator,Spawn):nullptr;
        if (Actor) { Actor->InitializeTestCombat(0); }
        return Actor;
    }
};

struct FCombatActionTransactionSettings
{
    TStrongObjectPtr<USovSettingsTestSettings> Settings{NewObject<USovSettingsTestSettings>()};
    TStrongObjectPtr<UObject> Previous;
    FObjectProperty* Property=nullptr;
    FCombatActionTransactionSettings()
    {
        if (!GEngine) { return; }
        Property=FindFProperty<FObjectProperty>(GEngine->GetClass(),TEXT("GameUserSettings"));
        if (!Property) { return; }
        Previous.Reset(Property->GetObjectPropertyValue_InContainer(GEngine));
        Property->SetObjectPropertyValue_InContainer(GEngine,Settings.Get());
    }
    ~FCombatActionTransactionSettings()
    { if (Property&&GEngine) { Property->SetObjectPropertyValue_InContainer(GEngine,Previous.Get()); } }
};

USovCombatActionTransactionEchoAbility* GetTransactionEcho(UNarrativeAbilitySystemComponent* ASC,FGameplayAbilitySpecHandle Handle)
{
    auto* Spec=ASC?ASC->FindAbilitySpecFromHandle(Handle):nullptr;
    return Spec?Cast<USovCombatActionTransactionEchoAbility>(Spec->GetPrimaryInstance()):nullptr;
}

void AddTransactionMeleeMesh(ASovAxiomRuntimeTestCharacter* Source)
{
    auto* Mesh=NewObject<USovMeleeRuntimeTestMesh>(Source);
    Source->AddInstanceComponent(Mesh);
    Mesh->SetupAttachment(Source->GetRootComponent());
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->RegisterComponent();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatActionEchoDebitCancellationTest,
    "ProjectVelkorran.Campaign.Transactions.Actions.EchoDebitCancellation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovCombatActionEchoDebitCancellationTest::RunTest(const FString& Parameters)
{
    FCombatActionTransactionWorld F; auto* Source=F.Character(); if (!Source) { return false; }
    auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovCombatActionTransactionEchoAbility::StaticClass(),1));
    TStrongObjectPtr<USovCombatActionTransactionProbe> Probe(NewObject<USovCombatActionTransactionProbe>());
    Probe->ASC=ASC; Probe->Handle=Handle;
    Source->TestEcho->OnEchoChanged.AddDynamic(Probe.Get(),&USovCombatActionTransactionProbe::DuringEchoDebit);
    ASC->TryActivateAbility(Handle,false);
    auto* Ability=GetTransactionEcho(ASC,Handle);
    if (!TestNotNull(TEXT("GAS creates the production-derived action"),Ability)) { return false; }
    TestEqual(TEXT("Cancellation preserves exactly one committed debit"),Source->TestEcho->GetEcho(),80.f);
    TestFalse(TEXT("Canceled cost cannot leave an active action"),Ability->IsActive());
    TestEqual(TEXT("No started hook follows canceled cost"),Ability->StartedCount,0);
    TestEqual(TEXT("No authority payload follows canceled cost"),Ability->CommittedCount,0);
    TestFalse(TEXT("Canceled payment cannot install stale target-data callback"),
        ASC->AbilityTargetDataSetDelegate(Handle,Ability->GetCurrentActivationInfoRef().GetActivationPredictionKey()).IsBoundToObject(Ability));
    TestTrue(TEXT("Next activation starts cleanly"),ASC->TryActivateAbility(Handle,false));
    TestEqual(TEXT("Next activation pays once"),Source->TestEcho->GetEcho(),60.f);
    TestEqual(TEXT("Only the replacement reaches authority payload"),Ability->CommittedCount,1);
    Ability->FinishEchoAbility(true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatActionEchoStartedReentryTest,
    "ProjectVelkorran.Campaign.Transactions.Actions.EchoStartedReentry",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovCombatActionEchoStartedReentryTest::RunTest(const FString& Parameters)
{
    FCombatActionTransactionWorld F; auto* Source=F.Character(); if (!Source) { return false; }
    auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovCombatActionTransactionEchoAbility::StaticClass(),1));
    auto* Ability=GetTransactionEcho(ASC,Handle);
    if (!TestNotNull(TEXT("Instanced-per-actor action is created when granted"),Ability)) { return false; }
    Ability->bRestartDuringStarted=true;
    ASC->TryActivateAbility(Handle,false);
    TestTrue(TEXT("Started callback can replace the same action instance"),Ability->bRestartAccepted);
    TestTrue(TEXT("Retired outer activation cannot end replacement"),Ability->IsActive());
    TestEqual(TEXT("Each started action pays exactly once"),Source->TestEcho->GetEcho(),60.f);
    TestEqual(TEXT("Both starts are observed"),Ability->StartedCount,2);
    TestEqual(TEXT("Only replacement reaches authority payload"),Ability->CommittedCount,1);
    TestEqual(TEXT("Only retired action has ended"),Ability->EndedCount,1);
    Ability->FinishEchoAbility(true);
    TestEqual(TEXT("Replacement owns its own cleanup"),Ability->EndedCount,2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatActionEchoScopeLockedEndTest,
    "ProjectVelkorran.Campaign.Transactions.Actions.EchoScopeLockedEnd",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovCombatActionEchoScopeLockedEndTest::RunTest(const FString& Parameters)
{
    FCombatActionTransactionWorld F; auto* Source=F.Character(); if (!Source) { return false; }
    auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovCombatActionTransactionEchoAbility::StaticClass(),1));
    if (!ASC->TryActivateAbility(Handle,false)) { return false; }
    auto* Ability=GetTransactionEcho(ASC,Handle); if (!Ability) { return false; }
    Ability->LockEndForTest(); Ability->RequestEndForTest();
    TestTrue(TEXT("GAS active flag waits for scope unlock"),Ability->IsActive());
    TestFalse(TEXT("Native continuation retires immediately"),Ability->OwnsExecutionForTest());
    TestEqual(TEXT("Presentation cleanup also waits for unlock"),Ability->EndedCount,0);
    Ability->UnlockEndForTest();
    TestFalse(TEXT("Queued virtual teardown runs"),Ability->IsActive());
    TestEqual(TEXT("Queued teardown publishes one ended hook"),Ability->EndedCount,1);
    TestFalse(TEXT("Queued teardown clears target-data binding"),
        ASC->AbilityTargetDataSetDelegate(Handle,Ability->GetCurrentActivationInfoRef().GetActivationPredictionKey()).IsBoundToObject(Ability));
    TestFalse(TEXT("Queued teardown clears owned Busy"),ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatActionEchoAvatarChangeTest,
    "ProjectVelkorran.Campaign.Transactions.Actions.EchoDebitAvatarReplacement",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovCombatActionEchoAvatarChangeTest::RunTest(const FString& Parameters)
{
    for (auto Mutation : {ESovCombatActionDebitMutation::ReplaceAvatar,ESovCombatActionDebitMutation::RestoreOriginalAvatar})
    {
        FCombatActionTransactionWorld F; auto* Source=F.Character(); auto* Replacement=F.Character(FVector(500,0,0));
        if (!Source||!Replacement) { return false; }
        auto* ASC=Source->GetNarrativeAbilitySystemComponent();
        const uint64 Before=ASC->GetCombatActorInfoEpoch();
        const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovCombatActionTransactionEchoAbility::StaticClass(),1));
        TStrongObjectPtr<USovCombatActionTransactionProbe> Probe(NewObject<USovCombatActionTransactionProbe>());
        Probe->ASC=ASC; Probe->Handle=Handle; Probe->ReplacementAvatar=Replacement; Probe->Mutation=Mutation;
        Source->TestEcho->OnEchoChanged.AddDynamic(Probe.Get(),&USovCombatActionTransactionProbe::DuringEchoDebit);
        ASC->TryActivateAbility(Handle,false);
        auto* Ability=GetTransactionEcho(ASC,Handle); if (!Ability) { return false; }
        TestTrue(TEXT("Real actor-info rebind advances the shared ownership token"),ASC->GetCombatActorInfoEpoch()!=Before);
        TestFalse(TEXT("Changed or A-to-B-to-A avatar retires old paid execution"),Ability->IsActive());
        TestEqual(TEXT("Replacement avatar never receives old started hook"),Ability->StartedCount,0);
        TestEqual(TEXT("Replacement avatar never receives old authority payload"),Ability->CommittedCount,0);
        ASC->InitAbilityActorInfo(Source,Source);
        TestEqual(TEXT("Original debit remains committed"),Source->TestEcho->GetEcho(),80.f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatActionEchoFreezeTest,
    "ProjectVelkorran.Campaign.Transactions.Actions.EchoFreezeAdmissionAndInterruption",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovCombatActionEchoFreezeTest::RunTest(const FString& Parameters)
{
    FCombatActionTransactionWorld F; auto* Source=F.Character(); if (!Source) { return false; }
    auto* ASC=Source->GetNarrativeAbilitySystemComponent(); const auto Frozen=FSovGameplayTags::Get().State_Status_Frozen;
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovCombatActionTransactionEchoAbility::StaticClass(),1));
    ASC->AddLooseGameplayTag(Frozen);
    TestFalse(TEXT("Already frozen action is rejected before payment"),ASC->TryActivateAbility(Handle,false));
    TestEqual(TEXT("Frozen admission spends no Echo"),Source->TestEcho->GetEcho(),100.f);
    ASC->RemoveLooseGameplayTag(Frozen);
    TStrongObjectPtr<USovCombatActionTransactionProbe> Probe(NewObject<USovCombatActionTransactionProbe>());
    Probe->ASC=ASC; Probe->Handle=Handle; Probe->Mutation=ESovCombatActionDebitMutation::Freeze;
    Source->TestEcho->OnEchoChanged.AddDynamic(Probe.Get(),&USovCombatActionTransactionProbe::DuringEchoDebit);
    ASC->TryActivateAbility(Handle,false);
    auto* Ability=GetTransactionEcho(ASC,Handle); if (!Ability) { return false; }
    TestFalse(TEXT("Freeze arriving during payment cancels before payload"),Ability->IsActive());
    TestEqual(TEXT("Freeze race has no authority continuation"),Ability->CommittedCount,0);
    TestEqual(TEXT("Committed debit is not refunded or duplicated"),Source->TestEcho->GetEcho(),80.f);
    ASC->RemoveLooseGameplayTag(Frozen);
    TestTrue(TEXT("An unfrozen action can activate"),ASC->TryActivateAbility(Handle,false));
    ASC->AddLooseGameplayTag(Frozen);
    TestFalse(TEXT("Active action cancels immediately on Freeze"),Ability->IsActive());
    ASC->RemoveLooseGameplayTag(Frozen);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatActionMeleeInterruptionTest,
    "ProjectVelkorran.Campaign.Transactions.Actions.MeleeImmediateInterruptions",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovCombatActionMeleeInterruptionTest::RunTest(const FString& Parameters)
{
    FCombatActionTransactionWorld F; auto* Source=F.Character(); if (!Source) { return false; }
    AddTransactionMeleeMesh(Source); auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovMeleeRuntimeTestAbility::StaticClass(),1));
    for (FGameplayTag Tag : {FSovGameplayTags::Get().State_Poise_Broken,FSovGameplayTags::Get().State_Guard_Broken,
        FSovGameplayTags::Get().State_Status_Frozen,FNarrativeGameplayTags::Get().State_SequencerControlled,
        FNarrativeGameplayTags::Get().State_Weapon_Equipping,FNarrativeGameplayTags::Get().State_Busy})
    {
        if (!TestTrue(TEXT("Melee starts with no interruption"),ASC->TryActivateAbility(Handle,false))) { return false; }
        ASC->AddLooseGameplayTag(Tag);
        TestFalse(TEXT("Interrupt immediately retires active melee"),ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
        ASC->RemoveLooseGameplayTag(Tag);
        TestFalse(TEXT("Interruption cannot leak an owned Busy count"),ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatActionEchoZeroHealthTest,
    "ProjectVelkorran.Campaign.Transactions.Actions.EchoZeroHealthBeforeDeathTag",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovCombatActionEchoZeroHealthTest::RunTest(const FString& Parameters)
{
    for (auto Mutation : {ESovCombatActionDebitMutation::ZeroHealth,ESovCombatActionDebitMutation::RestoreLife})
    {
        FCombatActionTransactionWorld F; auto* Source=F.Character(); if (!Source) { return false; }
        auto* ASC=Source->GetNarrativeAbilitySystemComponent();
        const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovCombatActionTransactionEchoAbility::StaticClass(),1));
        ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(),0.f);
        TestFalse(TEXT("No artificial death tag is needed for admission"),ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead));
        TestFalse(TEXT("Zero-Health action is rejected before payment"),ASC->TryActivateAbility(Handle,false));
        TestEqual(TEXT("Dead admission preserves Echo"),Source->TestEcho->GetEcho(),100.f);
        ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(),100.f);
        const uint64 Before=ASC->GetSet<UNarrativeAttributeSetBase>()->GetCombatLifeEpoch();
        TStrongObjectPtr<USovCombatActionTransactionProbe> Probe(NewObject<USovCombatActionTransactionProbe>());
        Probe->ASC=ASC; Probe->Handle=Handle; Probe->Mutation=Mutation;
        Source->TestEcho->OnEchoChanged.AddDynamic(Probe.Get(),&USovCombatActionTransactionProbe::DuringEchoDebit);
        ASC->TryActivateAbility(Handle,false);
        auto* Ability=GetTransactionEcho(ASC,Handle); if (!Ability) { return false; }
        if (Mutation==ESovCombatActionDebitMutation::RestoreLife)
        { TestTrue(TEXT("Health restoration advances existing combat-life token"),ASC->GetSet<UNarrativeAttributeSetBase>()->GetCombatLifeEpoch()!=Before); }
        TestFalse(TEXT("Health loss or restoration during cost retires old execution"),Ability->IsActive());
        TestEqual(TEXT("No old started event reaches dead or restored life"),Ability->StartedCount,0);
        TestEqual(TEXT("No old authority payload reaches dead or restored life"),Ability->CommittedCount,0);
        TestEqual(TEXT("One committed debit is retained"),Source->TestEcho->GetEcho(),80.f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatActionMeleeMeshReentryTest,
    "ProjectVelkorran.Campaign.Transactions.Actions.MeleeMeshResolutionReentry",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovCombatActionMeleeMeshReentryTest::RunTest(const FString& Parameters)
{
    FCombatActionTransactionWorld F; auto* Source=F.Character(); if (!Source) { return false; }
    AddTransactionMeleeMesh(Source); auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovCombatActionTransactionMeleeAbility::StaticClass(),1));
    ASC->TryActivateAbility(Handle,false);
    auto* Ability=Cast<USovCombatActionTransactionMeleeAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    if (!Ability) { return false; }
    TestTrue(TEXT("Authored mesh callback replaces the same instance"),Ability->bRestartAccepted);
    TestTrue(TEXT("Retired mesh continuation preserves replacement action"),Ability->IsActive());
    TestEqual(TEXT("Only replacement initializes a melee node"),Ability->NodeStartedCount,1);
    TestEqual(TEXT("Replacement owns exactly one Busy contribution"),ASC->GetTagCount(FNarrativeGameplayTags::Get().State_Busy),1);
    FGuid Attack;
    TestTrue(TEXT("Replacement retains its native attack identity"),Ability->GetSovAttackIdentity(Source,Attack));
    ASC->CancelAbilityHandle(Handle);
    TestFalse(TEXT("Replacement cleanup releases Busy"),ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCombatActionMeleeAimReentryTest,
    "ProjectVelkorran.Campaign.Transactions.Actions.MeleeAimPolicyReentry",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovCombatActionMeleeAimReentryTest::RunTest(const FString& Parameters)
{
    FCombatActionTransactionSettings Preferences;
    if (!TestNotNull(TEXT("Real settings owner can be temporarily replaced"),Preferences.Property)) { return false; }
    auto Snapshot=Preferences.Settings->GetSettingsSnapshot(); Snapshot.MeleeAimAssistStrength=1.f;
    FString Error;
    if (!TestTrue(TEXT("Native settings accept melee assistance"),Preferences.Settings->ApplySettingsSnapshot(Snapshot,Error))) { return false; }
    for (bool bCancel : {false,true})
    {
        FCombatActionTransactionWorld F;
        if (!F.World) { return false; }
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Source=F.World->SpawnActor<ASovCombatActionTransactionTeamCharacter>(
            ASovCombatActionTransactionTeamCharacter::StaticClass(),FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
        auto* Target=F.Character(FVector(120,15,0));
        if (!Source||!Target) { return false; }
        Source->InitializeTestCombat(0); Target->InitializeTestCombat(1);
        Target->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Target->GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
        Target->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        AddTransactionMeleeMesh(Source); auto* ASC=Source->GetNarrativeAbilitySystemComponent();
        Source->Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovMeleeRuntimeTestAbility::StaticClass(),1));
        Source->bCancelDuringAttitude=bCancel;
        ASC->TryActivateAbility(Source->Handle,false);
        TestTrue(TEXT("Production aim helper consults virtual team policy"),Source->bQueriedAttitude);
        if (bCancel)
        {
            TestTrue(TEXT("Retired helper cannot rotate the canceled source"),FMath::IsNearlyZero(Source->GetActorRotation().Yaw));
            TestFalse(TEXT("Policy callback ends the attack immediately"),ASC->FindAbilitySpecFromHandle(Source->Handle)->IsActive());
            TestFalse(TEXT("Policy cancellation releases Busy"),ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy));
        }
        else
        {
            TestTrue(TEXT("Positive control proves native aim assistance rotates toward target"),Source->GetActorRotation().Yaw>0.f);
            ASC->CancelAbilityHandle(Source->Handle);
        }
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEchoCastPlaybackTest,
    "ProjectVelkorran.Campaign.Transactions.Actions.EchoCastPlayback",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovEchoCastPlaybackTest::RunTest(const FString& Parameters)
{
    // Isolated GAS test world, not a mission progression or live gameplay receipt.
    FCombatActionTransactionWorld F; auto* Source=F.Character(); if (!Source) { return false; }
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/NarrativePro/Pro/Core/Character/Biped/Art/Mannequin/Meshes/SKM_Quinn.SKM_Quinn"));
    auto* A=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Characters/Animation/ProtagonistCasts/AM_Selene_AxiomNullPulse_A.AM_Selene_AxiomNullPulse_A"));
    auto* B=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Characters/Animation/ProtagonistCasts/AM_Selene_AxiomNullPulse_B.AM_Selene_AxiomNullPulse_B"));
    if (!TestNotNull(TEXT("Production mannequin"),Mesh) || !TestNotNull(TEXT("Cast A"),A) || !TestNotNull(TEXT("Cast B"),B)) { return false; }
    Source->GetMesh()->SetSkeletalMesh(Mesh);
    Source->GetMesh()->SetAnimInstanceClass(UAnimInstance::StaticClass());
    Source->GetMesh()->InitAnim(true);
    auto* Anim=Source->GetMesh()->GetAnimInstance();
    if (!TestNotNull(TEXT("Real montage playback instance"),Anim)) { return false; }
    auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    ASC->InitAbilityActorInfo(Source,Source);
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovCombatActionTransactionEchoAbility::StaticClass(),1));
    auto* Ability=GetTransactionEcho(ASC,Handle);
    if (!Ability) { return false; }
    Ability->SetCastPairForTest(A,B);
    TestTrue(TEXT("Paid cast A activates"),ASC->TryActivateAbility(Handle,false));
    TestTrue(TEXT("First cast plays A"),Anim->Montage_IsPlaying(A));
    Ability->FinishEchoAbility(false);
    TestTrue(TEXT("Instant successful payload retains cosmetic recovery"),Anim->Montage_IsPlaying(A));
    TestTrue(TEXT("Paid cast B activates"),ASC->TryActivateAbility(Handle,false));
    TestTrue(TEXT("Second cast plays B"),Anim->Montage_IsPlaying(B));
    Ability->FinishEchoAbility(true);
    TestFalse(TEXT("Cancellation stops its own cast"),Anim->Montage_IsPlaying(B));
    ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy);
    TestFalse(TEXT("Rejected input does not activate"),ASC->TryActivateAbility(Handle,false));
    ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy);
    TestTrue(TEXT("Third accepted cast activates"),ASC->TryActivateAbility(Handle,false));
    TestTrue(TEXT("Rejected input did not consume A"),Anim->Montage_IsPlaying(A));
    Ability->FinishEchoAbility(false);
    const auto OtherHandle=ASC->GiveAbility(FGameplayAbilitySpec(USovCombatActionTransactionEchoAbility::StaticClass(),1));
    auto* Other=GetTransactionEcho(ASC,OtherHandle);
    if (!Other) { return false; }
    Other->SetCastPairForTest(A,B);
    TestTrue(TEXT("Another ability activates"),ASC->TryActivateAbility(OtherHandle,false));
    TestTrue(TEXT("Another ability starts with its own A"),Anim->Montage_IsPlaying(A));
    Other->FinishEchoAbility(false);
    TestTrue(TEXT("Original ability activates again"),ASC->TryActivateAbility(Handle,false));
    TestTrue(TEXT("Original ability retains its own B"),Anim->Montage_IsPlaying(B));
    Ability->FinishEchoAbility(true);
    TestEqual(TEXT("Five real activations pay five costs"),Source->TestEcho->GetEcho(),0.f);
    return true;
}
#endif
