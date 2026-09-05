// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovPlayerCombatRepairTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SovEchoComponent.h"
#include "Combat/SovFinisherTargetComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Items/InventoryComponent.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "Settings/NarrativeCombatDeveloperSettings.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Class.h"

#if WITH_AUTOMATION_TESTS
struct FSovPlayerCombatRepairTestAccess
{
    static void Strike(USovGameplayAbility_Finisher* Ability) { Ability->Strike(); }
};
namespace
{
struct FPlayerCombatRepairWorld
{
    UWorld* World=nullptr;
    FPlayerCombatRepairWorld()
    {
        World=UWorld::CreateWorld(EWorldType::Game,false);
        if (!World) { return; }
        if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
        World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
    }
    ~FPlayerCombatRepairWorld()
    { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
    ASovAxiomRuntimeTestCharacter* Character(FVector Location,int32 Team)
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Actor=World?World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(),Location,FRotator::ZeroRotator,Spawn):nullptr;
        if (Actor)
        {
            Actor->InitializeTestCombat(Team);
            Actor->GetCapsuleComponent()->SetCollisionResponseToChannel(UArsenalStatics::GetNarrativeProSettings()->WeaponTraceChannel,ECR_Block);
        }
        return Actor;
    }
    void Advance(float Seconds)
    { TGuardValue<uint64> Frame(GFrameCounter,GFrameCounter+1); World->Tick(LEVELTICK_TimeOnly,Seconds); }
    void Wall(FVector Location)
    {
        AActor* Actor=World->SpawnActor<AActor>();
        auto* Box=NewObject<UBoxComponent>(Actor); Actor->AddInstanceComponent(Box); Actor->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(10,400,300)); Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->RegisterComponent(); Actor->SetActorLocation(Location);
    }
};
void SetRepairTarrik(ASovAxiomRuntimeTestCharacter* Source)
{
    auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    ASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Selene);
    ASC->AddLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Tarrik);
}
USovRepairJudgementAbility* ActivateRepairJudgement(ASovAxiomRuntimeTestCharacter* Source,FGameplayAbilitySpecHandle& Handle)
{
    SetRepairTarrik(Source); auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovRepairJudgementAbility::StaticClass(),1,INDEX_NONE,Source->SetTestWeapon()));
    if (!ASC->TryActivateAbility(Handle,false)) { return nullptr; }
    const auto* Spec=ASC->FindAbilitySpecFromHandle(Handle);
    return Spec?Cast<USovRepairJudgementAbility>(Spec->GetPrimaryInstance()):nullptr;
}
void ApplyRepairHit(ASovAxiomRuntimeTestCharacter* Source,ASovAxiomRuntimeTestCharacter* Target)
{
    auto* ASC=Source->GetNarrativeAbilitySystemComponent(); auto Context=ASC->MakeEffectContext();
    Context.AddInstigator(Source,Source);
    FGameplayEffectSpec Spec(GetDefault<USovCombatRoutingTestEffect>(),Context,1.f);
    Spec.AddDynamicAssetTag(FSovGameplayTags::Get().Damage_Channel_Kinetic);
    Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage,1.f);
    ASC->ApplyGameplayEffectSpecToTarget(Spec,Target->GetNarrativeAbilitySystemComponent());
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovExactAmmoRepairTest,"ProjectVelkorran.Campaign.Repairs.Player.ExactAmmoAndRecoil",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovExactAmmoRepairTest::RunTest(const FString& Parameters)
{
    FPlayerCombatRepairWorld F; auto* Source=F.Character(FVector::ZeroVector,0); if (!Source) { return false; }
    auto* Inventory=NewObject<UNarrativeInventoryComponent>(Source); Source->AddInstanceComponent(Inventory); Inventory->RegisterComponent();
    Inventory->TryAddItemFromClass(USovRepairAmmo::StaticClass(),20);
    Inventory->TryAddItemFromClass(USovRepairWeapon::StaticClass(),1);
    auto* Ammo=Cast<USovRepairAmmo>(Inventory->FindItemOfClass(USovRepairAmmo::StaticClass()));
    auto* Weapon=Cast<USovRepairWeapon>(Inventory->FindItemOfClass(USovRepairWeapon::StaticClass()));
    if (!TestNotNull(TEXT("Real inventory ammo"),Ammo)||!TestNotNull(TEXT("Real inventory weapon"),Weapon)) { return false; }
    Weapon->SetLoaded(1);
    for (int32 Invalid : {0,-1,MIN_int32}) { TestFalse(TEXT("Nonpositive requests never authorize a shot"),Weapon->ConsumeAmmo(Invalid)); }
    TestEqual(TEXT("Invalid requests preserve the loaded round"),Weapon->RawLoaded(),1);
    TestEqual(TEXT("Invalid requests preserve inventory"),Ammo->GetQuantity(),20);
    Ammo->bAllowRemoval=false;
    TestFalse(TEXT("Permission rejection cannot authorize unpaid shot"),Weapon->ConsumeAmmo(1));
    TestEqual(TEXT("Rejected unchanged transaction returns reserved round"),Weapon->RawLoaded(),1);
    TestEqual(TEXT("Rejected removal preserves stack"),Ammo->GetQuantity(),20);
    Ammo->bAllowRemoval=true;
    TStrongObjectPtr<USovPlayerCombatRepairProbe> Probe(NewObject<USovPlayerCombatRepairProbe>()); Probe->Weapon=Weapon;
    Ammo->OnItemModified.AddDynamic(Probe.Get(),&USovPlayerCombatRepairProbe::DuringAmmoMutation);
    TestTrue(TEXT("One real debit succeeds"),Weapon->ConsumeAmmo(1));
    TestFalse(TEXT("Synchronous nested shot rejected"),Probe->bNestedConsumeAccepted);
    TestFalse(TEXT("Synchronous reload cannot reuse reserved round"),Probe->bNestedReloadAccepted);
    TestEqual(TEXT("Raw clip cannot become negative"),Weapon->RawLoaded(),0);
    TestEqual(TEXT("Exactly one inventory unit removed"),Ammo->GetQuantity(),19);
    TestTrue(TEXT("Next ordinary reload succeeds after transaction"),Weapon->Reload());
    TestEqual(TEXT("Reload uses existing stack without duplicating inventory"),Ammo->GetQuantity(),19);
    TestEqual(TEXT("Hip recoil uses authored hip preset"),Weapon->GetRecoilImpulse().GetTranslation(),FVector(7,0,0));
    auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    const auto AimTag=Source->IsLocallyControlled()?FNarrativeGameplayTags::Get().State_Weapon_IsAiming_Local:FNarrativeGameplayTags::Get().State_Weapon_IsAiming;
    ASC->AddLooseGameplayTag(AimTag);
    TestEqual(TEXT("Aim recoil uses authored aim preset"),Weapon->GetRecoilImpulse().GetTranslation(),FVector(2,0,0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEchoCancellationRepairTest,"ProjectVelkorran.Campaign.Repairs.Player.EchoDebitCancellation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovEchoCancellationRepairTest::RunTest(const FString& Parameters)
{
    FPlayerCombatRepairWorld F; auto* Source=F.Character(FVector::ZeroVector,0); if (!Source) { return false; }
    auto* ASC=Source->GetNarrativeAbilitySystemComponent();
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovRepairEchoAbility::StaticClass(),1));
    TStrongObjectPtr<USovPlayerCombatRepairProbe> Probe(NewObject<USovPlayerCombatRepairProbe>()); Probe->ASC=ASC; Probe->Handle=Handle;
    Source->TestEcho->OnEchoChanged.AddDynamic(Probe.Get(),&USovPlayerCombatRepairProbe::DuringEchoDebit);
    ASC->TryActivateAbility(Handle,false);
    const auto* Spec=ASC->FindAbilitySpecFromHandle(Handle);
    auto* Ability=Spec?Cast<USovRepairEchoAbility>(Spec->GetPrimaryInstance()):nullptr;
    if (!TestNotNull(TEXT("GAS created instance before synchronous cost event"),Ability)) { return false; }
    TestEqual(TEXT("Cancelled committed debit remains exactly twenty"),Source->TestEcho->GetEcho(),80.f);
    TestFalse(TEXT("Cancellation during cost leaves no active ability"),Ability->IsActive());
    TestEqual(TEXT("No started hook follows cancelled payment"),Ability->StartedCount,0);
    TestFalse(TEXT("Old activation cannot bind Narrative target data after ending"),
        ASC->AbilityTargetDataSetDelegate(Handle,Ability->GetCurrentActivationInfoRef().GetActivationPredictionKey()).IsBoundToObject(Ability));
    TestTrue(TEXT("A subsequent activation starts cleanly"),ASC->TryActivateAbility(Handle,false));
    TestEqual(TEXT("Only subsequent activation reaches started hook"),Ability->StartedCount,1);
    TestEqual(TEXT("Subsequent activation pays once"),Source->TestEcho->GetEcho(),60.f);
    Ability->FinishEchoAbility(true);
    TestFalse(TEXT("Subsequent teardown removes target data binding"),
        ASC->AbilityTargetDataSetDelegate(Handle,Ability->GetCurrentActivationInfoRef().GetActivationPredictionKey()).IsBoundToObject(Ability));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovJudgementCoverRepairTest,"ProjectVelkorran.Campaign.Repairs.Player.JudgementThinCover",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovJudgementCoverRepairTest::RunTest(const FString& Parameters)
{
    FPlayerCombatRepairWorld F; auto* Source=F.Character(FVector::ZeroVector,0); auto* Hidden=F.Character(FVector(160,0,0),1);
    if (!Source||!Hidden) { return false; } F.Wall(FVector(50,0,0));
    FGameplayAbilitySpecHandle Handle; auto* Ability=ActivateRepairJudgement(Source,Handle);
    if (!TestNotNull(TEXT("Native Judgement activation"),Ability)) { return false; }
    TestTrue(TEXT("Near-side impact releases paid shot"),Ability->ReleaseCinderJudgementFromAim());
    TestEqual(TEXT("Muzzle beyond wall cannot hit reverse face and detonate through cover"),Hidden->ResolvedHitCount,0);
    Ability->FinishEchoAbility(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovJudgementContinuationRepairTest,"ProjectVelkorran.Campaign.Repairs.Player.JudgementRetiredRelease",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovJudgementContinuationRepairTest::RunTest(const FString& Parameters)
{
    FPlayerCombatRepairWorld F; auto* Source=F.Character(FVector::ZeroVector,0);
    auto* Direct=F.Character(FVector(200,0,0),1); auto* Radial=F.Character(FVector(200,150,0),1);
    if (!Source||!Direct||!Radial) { return false; }
    FGameplayAbilitySpecHandle Handle; auto* Ability=ActivateRepairJudgement(Source,Handle); if (!Ability) { return false; }
    TStrongObjectPtr<USovPlayerCombatRepairProbe> Probe(NewObject<USovPlayerCombatRepairProbe>());
    Probe->ASC=Source->GetNarrativeAbilitySystemComponent(); Probe->Handle=Handle; Probe->Judgement=Ability; Probe->bReactivateJudgement=true;
    Direct->GetNarrativeAbilitySystemComponent()->OnDamageResolvedAsTarget.AddDynamic(Probe.Get(),&USovPlayerCombatRepairProbe::DuringJudgementDamage);
    TestTrue(TEXT("First direct packet committed before callback"),Ability->ReleaseCinderJudgementFromAim());
    TestEqual(TEXT("Only original direct packet resolved"),Direct->ResolvedHitCount,1);
    TestTrue(TEXT("Observer starts a new paid activation on same instance"),Probe->bReactivationAccepted);
    TestEqual(TEXT("Retired release cannot continue radial packets"),Radial->ResolvedHitCount,0);
    F.Advance(.01f); F.Advance(.3f);
    TestTrue(TEXT("Old recovery cannot end replacement activation"),Ability->IsActive());
    Ability->FinishEchoAbility(true); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDamageReplayRepairTest,"ProjectVelkorran.Campaign.Repairs.Player.DamageReceiptReplayOwnership",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovDamageReplayRepairTest::RunTest(const FString& Parameters)
{
    FPlayerCombatRepairWorld F; auto* Source=F.Character(FVector::ZeroVector,0); auto* Target=F.Character(FVector(200,0,0),1);
    if (!Source||!Target) { return false; }
    TStrongObjectPtr<UObject> Consumer(NewObject<UObject>());
    ApplyRepairHit(Source,Target); const FSovDamageResult Original=Target->LastDamageResult; FSovDamageResult Copy=Original;
    TestTrue(TEXT("Only authoritative real damage mints receipt"),Original.HasNativeReceipt());
    TestTrue(TEXT("First consumer application succeeds"),Original.ConsumeNativeReceipt(Consumer.Get()));
    TestFalse(TEXT("Struct copy shares consumed state"),Copy.ConsumeNativeReceipt(Consumer.Get()));
    FSovDamageResult ScriptCopy;
    FSovDamageResult::StaticStruct()->CopyScriptStruct(&ScriptCopy,&Original);
    TestTrue(TEXT("Reflection copy retains native proof"),ScriptCopy.HasNativeReceipt());
    TestFalse(TEXT("Reflection copy also shares consumed state"),ScriptCopy.ConsumeNativeReceipt(Consumer.Get()));
    Copy.TransactionId=FGuid::NewGuid();
    TestFalse(TEXT("Changing copied transaction cannot renew receipt"),Copy.ConsumeNativeReceipt(Consumer.Get(),1));
    FSovDamageResult Forged; Forged.TransactionId=FGuid::NewGuid();
    TestFalse(TEXT("Blueprint-style manufactured result has no native proof"),Forged.ConsumeNativeReceipt(Consumer.Get()));
    for (int32 Index=0;Index<1024;++Index)
    {
        Target->GetNarrativeAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(),100.f);
        ApplyRepairHit(Source,Target);
        if (!Target->LastDamageResult.ConsumeNativeReceipt(Consumer.Get())) { AddError(TEXT("New authoritative receipt unexpectedly reused")); return false; }
    }
    TestFalse(TEXT("High transaction volume never evicts retained old replay protection"),Original.ConsumeNativeReceipt(Consumer.Get()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovLegacyEffectRepairTest,"ProjectVelkorran.Campaign.Repairs.Player.LegacySeleneEffectValidation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovLegacyEffectRepairTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<USovRepairStaccatoAbility> Ability(NewObject<USovRepairStaccatoAbility>()); FString Error;
    TestTrue(TEXT("Canonical native default validates"),Ability->ValidateNativeEffectOverrides(Error));
    Ability->SetRetiredDamageOverride(USovCombatRoutingTestEffect::StaticClass());
    TestFalse(TEXT("Incompatible legacy effect override is explicitly rejected"),Ability->ValidateNativeEffectOverrides(Error));
    TestTrue(TEXT("Validation names the exact retired property"),Error.Contains(TEXT("EmpoweredShotDamageEffectClass")));
    Ability->SetRetiredDamageOverride(nullptr);
    TestTrue(TEXT("Cleared legacy override uses native contract"),Ability->ValidateNativeEffectOverrides(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFinisherSelectionRepairTest,"ProjectVelkorran.Campaign.Repairs.Player.FinisherSelectionReactivation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovFinisherSelectionRepairTest::RunTest(const FString& Parameters)
{
    // First callback: target search. Second callback: reservation eligibility.
    for (int32 TriggerCall : {1,2})
    {
        FPlayerCombatRepairWorld F; if (!F.World) { return false; }
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Player=F.World->SpawnActor<ASovRepairFinisherCharacter>(ASovRepairFinisherCharacter::StaticClass(),FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
        auto* Enemy=F.Character(FVector(150,0,0),1); if (!Player||!Enemy) { return false; }
        Player->InitializeTestCombat(0);
        auto* Target=NewObject<USovFinisherTargetComponent>(Enemy); Enemy->AddInstanceComponent(Target); Target->RegisterComponent();
        Enemy->GetNarrativeAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(),10.f);
        auto* ASC=Player->GetNarrativeAbilitySystemComponent();
        Player->FinisherHandle=ASC->GiveAbility(FGameplayAbilitySpec(USovRepairFinisherAbility::StaticClass(),1));
        Player->ReenterOnAttitudeCall=TriggerCall;
        ASC->TryActivateAbility(Player->FinisherHandle,false);
        const auto* Spec=ASC->FindAbilitySpecFromHandle(Player->FinisherHandle);
        auto* Ability=Spec?Cast<USovRepairFinisherAbility>(Spec->GetPrimaryInstance()):nullptr;
        if (!TestNotNull(TEXT("Finisher instance"),Ability)) { return false; }
        TestTrue(TEXT("Team callback reactivated same ability instance"),Player->bRestarted);
        TestTrue(TEXT("Old target selection cannot reset replacement action"),Ability->IsActive());
        TestEqual(TEXT("Exactly one target protection owner survives"),Enemy->GetNarrativeAbilitySystemComponent()->GetTagCount(FSovGameplayTags::Get().State_InterruptProtected),1);
        F.Advance(.01f); F.Advance(.03f);
        TestTrue(TEXT("Replacement reservation remains valid on watchdog"),Ability->IsActive());
        ASC->CancelAbilityHandle(Player->FinisherHandle);
        TestFalse(TEXT("Cleanup removes replacement target protection"),Enemy->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(FSovGameplayTags::Get().State_InterruptProtected));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFinisherScopeLockRepairTest,"ProjectVelkorran.Campaign.Repairs.Player.FinisherScopeLockedEnd",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovFinisherScopeLockRepairTest::RunTest(const FString& Parameters)
{
    FPlayerCombatRepairWorld F; auto* Player=F.Character(FVector::ZeroVector,0); auto* Enemy=F.Character(FVector(150,0,0),1);
    if (!Player||!Enemy) { return false; }
    auto* Target=NewObject<USovFinisherTargetComponent>(Enemy); Enemy->AddInstanceComponent(Target); Target->RegisterComponent();
    Enemy->GetNarrativeAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(),10.f);
    auto* ASC=Player->GetNarrativeAbilitySystemComponent();
    const auto Handle=ASC->GiveAbility(FGameplayAbilitySpec(USovRepairFinisherAbility::StaticClass(),1));
    if (!TestTrue(TEXT("Finisher starts before deferred cancellation"),ASC->TryActivateAbility(Handle,false))) { return false; }
    auto* Ability=Cast<USovRepairFinisherAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
    if (!Ability) { return false; }
    Ability->LockEndForTest(); Ability->RequestEndForTest();
    TestTrue(TEXT("GAS active state waits for scope unlock"),Ability->IsActive());
    FGuid Attack; TestFalse(TEXT("Native dispatch identity retires immediately"),Ability->GetSovAttackIdentity(Player,Attack));
    TestFalse(TEXT("Target lease stays reserved until queued teardown"),Target->IsAvailableFor(Player));
    Ability->UnlockEndForTest();
    TestFalse(TEXT("Queued virtual end executes after unlock"),Ability->IsActive());
    TestTrue(TEXT("Queued teardown releases target lease"),Target->IsAvailableFor(Player));
    TestFalse(TEXT("Queued teardown clears target protection"),Enemy->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(FSovGameplayTags::Get().State_InterruptProtected));
    F.Advance(.01f); F.Advance(.5f);
    TestEqual(TEXT("Retired timer cannot commit a late strike"),Enemy->ResolvedHitCount,0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCommandLinkReceiptRepairTest,"ProjectVelkorran.Campaign.Repairs.Player.CommandLinkReceiptRestore",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovCommandLinkReceiptRepairTest::RunTest(const FString& Parameters)
{
    FPlayerCombatRepairWorld F; auto* Source=F.Character(FVector::ZeroVector,0); auto* Enemy=F.Character(FVector(200,0,0),1);
    if (!Source||!Enemy) { return false; }
    auto* Link=NewObject<USovAxiomRuntimeTestCommandLink>(Enemy); Enemy->AddInstanceComponent(Link); Link->RegisterComponent();
    if (!TestTrue(TEXT("Real link starts"),Link->ActivateCommandLink(Enemy))) { return false; }
    FSovCommandLinkSeverResult Result;
    TestTrue(TEXT("Real sever mints receipt"),Link->TrySeverCommandLink(Source,Result)==ESovCommandLinkSeverResolution::NewlySevered);
    FSovCommandLinkSeverResult Copy; FSovCommandLinkSeverResult::StaticStruct()->CopyScriptStruct(&Copy,&Result);
    TestTrue(TEXT("First real sever consumption succeeds"),Result.ConsumeNativeReward(Source));
    TestFalse(TEXT("Reflected sever copy cannot duplicate reward"),Copy.ConsumeNativeReward(Source));
    TestTrue(TEXT("New authored activation creates a new link instance"),Link->ActivateCommandLink(Enemy));
    TestTrue(TEXT("New instance can sever once"),Link->TrySeverCommandLink(Source,Result)==ESovCommandLinkSeverResolution::NewlySevered);
    const auto Snapshot=Link->CaptureCommandLinkState();
    TestTrue(TEXT("Same-instance checkpoint restore succeeds"),Link->RestoreCommandLinkState(Snapshot,Enemy,{}));
    TestFalse(TEXT("Even never-consumed pre-restore receipt is retired"),Result.ConsumeNativeReward(Source));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFinisherStrikeRepairTest,"ProjectVelkorran.Campaign.Repairs.Player.FinisherStrikeReactivation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovFinisherStrikeRepairTest::RunTest(const FString& Parameters)
{
    TGuardValue<bool> FriendlyFire(GetMutableDefault<UNarrativeCombatDeveloperSettings>()->bAllowFriendlyFire,false);
    FPlayerCombatRepairWorld F; if (!F.World) { return false; }
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player=F.World->SpawnActor<ASovRepairFinisherCharacter>(ASovRepairFinisherCharacter::StaticClass(),FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
    auto* Enemy=F.Character(FVector(150,0,0),1); if (!Player||!Enemy) { return false; }
    Player->InitializeTestCombat(0);
    auto* Target=NewObject<USovFinisherTargetComponent>(Enemy); Enemy->AddInstanceComponent(Target); Target->RegisterComponent();
    Enemy->GetNarrativeAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(),10.f);
    auto* ASC=Player->GetNarrativeAbilitySystemComponent();
    Player->FinisherHandle=ASC->GiveAbility(FGameplayAbilitySpec(USovRepairFinisherAbility::StaticClass(),1));
    if (!ASC->TryActivateAbility(Player->FinisherHandle,false)) { return false; }
    auto* Ability=Cast<USovRepairFinisherAbility>(ASC->FindAbilitySpecFromHandle(Player->FinisherHandle)->GetPrimaryInstance());
    if (!Ability) { return false; }
    // Strike first checks reserved-target attitude, then the damage admission policy.
    Player->ReenterOnAttitudeCall=2;
    FSovPlayerCombatRepairTestAccess::Strike(Ability);
    TestTrue(TEXT("Damage admission callback replaces finisher"),Player->bRestarted);
    TestTrue(TEXT("Retired strike preserves replacement ability"),Ability->IsActive());
    TestEqual(TEXT("Retired strike cannot apply its packet using new lease"),Enemy->ResolvedHitCount,0);
    TestEqual(TEXT("Replacement owns exactly one protection effect"),Enemy->GetNarrativeAbilitySystemComponent()->GetTagCount(FSovGameplayTags::Get().State_InterruptProtected),1);
    ASC->CancelAbilityHandle(Player->FinisherHandle);
    return true;
}
#endif
