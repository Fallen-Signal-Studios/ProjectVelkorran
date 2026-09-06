// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Components/SovShieldComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS
namespace SovResourceSnapshotTests
{
struct FWorld
{
    UWorld* World=nullptr;
    FWorld()
    {
        const UWorld::InitializationValues Values=UWorld::InitializationValues().AllowAudioPlayback(false)
            .RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
            .CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
        if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
    }
    ~FWorld()
    {
        if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
    }
    ASovAxiomRuntimeTestCharacter* Character()
    {
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Result=World?World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(),
            FVector::ZeroVector,FRotator::ZeroRotator,Spawn):nullptr;
        if (Result) { Result->InitializeTestCombat(0); }
        return Result;
    }
};
FActiveGameplayEffectHandle Modifier(UAbilitySystemComponent* ASC,const FGameplayAttribute& Attribute,float Magnitude,
    EGameplayModOp::Type Operation=EGameplayModOp::Additive)
{
    TStrongObjectPtr<UGameplayEffect> Effect(NewObject<UGameplayEffect>());
    Effect->DurationPolicy=EGameplayEffectDurationType::Infinite;
    FGameplayModifierInfo Modifier;
    Modifier.Attribute=Attribute; Modifier.ModifierOp=Operation; Modifier.ModifierMagnitude=FScalableFloat(Magnitude);
    Effect->Modifiers.Add(Modifier);
    FGameplayEffectSpec Spec(Effect.Get(),ASC->MakeEffectContext(),1.f);
    return ASC->ApplyGameplayEffectSpecToSelf(Spec);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovResourceSnapshotAdditiveBaseTest,
    "ProjectVelkorran.Campaign.Encounter.Resources.AdditiveBaseRoundTrip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovResourceSnapshotAdditiveBaseTest::RunTest(const FString& Parameters)
{
    SovResourceSnapshotTests::FWorld Fixture; auto* Actor=Fixture.Character();
    if (!Actor) { return false; }
    auto* ASC=Actor->GetNarrativeAbilitySystemComponent();
    const auto Stamina=UNarrativeAttributeSetBase::GetStaminaAttribute(), Echo=UNarrativeAttributeSetBase::GetEchoAttribute();
    ASC->SetNumericAttributeBase(Stamina,50.f); ASC->SetNumericAttributeBase(Echo,35.f);
    const auto Penalty=SovResourceSnapshotTests::Modifier(ASC,Stamina,-10.f);
    const auto Bonus=SovResourceSnapshotTests::Modifier(ASC,Echo,15.f);
    if (!TestTrue(TEXT("Real continuous modifiers applied"),Penalty.IsValid()&&Bonus.IsValid())) { return false; }
    FSovCombatResourceSnapshot Snapshot;
    if (!TestTrue(TEXT("Capture supported additive resources"),USovEncounterSnapshotLibrary::CaptureResources(ASC,Snapshot))) { return false; }
    TestEqual(TEXT("Explicit resource schema"),Snapshot.SchemaVersion,2);
    TestEqual(TEXT("Saved underlying Stamina"),Snapshot.BaseStamina,50.f);
    TestEqual(TEXT("Saved resolved Stamina"),Snapshot.Stamina,40.f);
    TArray<uint8> Bytes;
    { FMemoryWriter Writer(Bytes); FObjectAndNameAsStringProxyArchive Archive(Writer,true); Archive.ArIsSaveGame=true; Archive.ArNoDelta=true;
      FSovCombatResourceSnapshot::StaticStruct()->SerializeItem(Archive,&Snapshot,nullptr); }
    FSovCombatResourceSnapshot Loaded;
    { FMemoryReader Reader(Bytes); FObjectAndNameAsStringProxyArchive Archive(Reader,true); Archive.ArIsSaveGame=true;
      FSovCombatResourceSnapshot::StaticStruct()->SerializeItem(Archive,&Loaded,nullptr); }
    TestEqual(TEXT("Reflected save archive retains base"),Loaded.BaseStamina,50.f);
    for (int32 Index=0;Index<3;++Index)
    {
        ASC->SetNumericAttributeBase(Stamina,20.f); ASC->SetNumericAttributeBase(Echo,10.f);
        TestTrue(TEXT("Repeated restore accepted"),USovEncounterSnapshotLibrary::RestoreResources(ASC,Loaded));
        TestEqual(TEXT("Penalty is applied exactly once"),ASC->GetNumericAttribute(Stamina),40.f);
        TestEqual(TEXT("Echo timestamp reset does not apply bonus twice"),ASC->GetNumericAttribute(Echo),50.f);
        TestEqual(TEXT("Stamina base remains stable"),ASC->GetNumericAttributeBase(Stamina),50.f);
    }
    ASC->RemoveActiveGameplayEffect(Penalty); ASC->RemoveActiveGameplayEffect(Bonus);
    TestEqual(TEXT("Penalty expiry restores underlying resource"),ASC->GetNumericAttribute(Stamina),50.f);
    TestEqual(TEXT("Bonus expiry removes only its contribution"),ASC->GetNumericAttribute(Echo),35.f);
    TestTrue(TEXT("Restore after expired transient modifiers uses saved base"),USovEncounterSnapshotLibrary::RestoreResources(ASC,Loaded));
    TestEqual(TEXT("Expired effects are not invented from UI current"),ASC->GetNumericAttribute(Stamina),50.f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxStaminaAttribute(),30.f);
    TestTrue(TEXT("Retuned lower maximum restores safely"),USovEncounterSnapshotLibrary::RestoreResources(ASC,Loaded));
    TestEqual(TEXT("Underlying resource respects lower current maximum"),ASC->GetNumericAttribute(Stamina),30.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovResourceSnapshotAdmissionTest,
    "ProjectVelkorran.Campaign.Encounter.Resources.LegacyAndUnsupportedPreflight",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovResourceSnapshotAdmissionTest::RunTest(const FString& Parameters)
{
    SovResourceSnapshotTests::FWorld Fixture; auto* Actor=Fixture.Character();
    if (!Actor) { return false; }
    auto* ASC=Actor->GetNarrativeAbilitySystemComponent();
    const auto Stamina=UNarrativeAttributeSetBase::GetStaminaAttribute();
    FSovCombatResourceSnapshot Snapshot;
    if (!USovEncounterSnapshotLibrary::CaptureResources(ASC,Snapshot)) { return false; }
    FSovCombatResourceSnapshot Legacy=Snapshot; Legacy.SchemaVersion=1;
    const auto Penalty=SovResourceSnapshotTests::Modifier(ASC,Stamina,-10.f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(),23.f);
    TestFalse(TEXT("Ambiguous legacy current cannot be applied into a modifier"),USovEncounterSnapshotLibrary::RestoreResources(ASC,Legacy));
    TestEqual(TEXT("Legacy preflight occurs before first Shield write"),ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()),23.f);
    ASC->RemoveActiveGameplayEffect(Penalty);
    TestTrue(TEXT("Unmodified legacy resources retain compatibility"),USovEncounterSnapshotLibrary::RestoreResources(ASC,Legacy));
    const auto Override=SovResourceSnapshotTests::Modifier(ASC,Stamina,25.f,EGameplayModOp::Override);
    TestFalse(TEXT("Override aggregator cannot be reconstructed"),USovEncounterSnapshotLibrary::RestoreResources(ASC,Snapshot));
    FSovCombatResourceSnapshot Out=Snapshot;
    TestFalse(TEXT("Unsupported capture fails closed"),USovEncounterSnapshotLibrary::CaptureResources(ASC,Out));
    TestEqual(TEXT("Failed capture preserves caller record"),Out.Stamina,Snapshot.Stamina);
    ASC->RemoveActiveGameplayEffect(Override);
    Snapshot.SchemaVersion=77;
    TestFalse(TEXT("Unknown resource schema rejected"),USovEncounterSnapshotLibrary::RestoreResources(ASC,Snapshot));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovResourceSnapshotOwnershipTest,
    "ProjectVelkorran.Campaign.Encounter.Resources.ReentrantAndReboundOwnership",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovResourceSnapshotOwnershipTest::RunTest(const FString& Parameters)
{
    SovResourceSnapshotTests::FWorld Fixture; auto* Actor=Fixture.Character();
    if (!Actor) { return false; }
    auto* ASC=Actor->GetNarrativeAbilitySystemComponent();
    const auto Shield=UNarrativeAttributeSetBase::GetShieldAttribute(), Stamina=UNarrativeAttributeSetBase::GetStaminaAttribute();
    FSovCombatResourceSnapshot Snapshot;
    if (!USovEncounterSnapshotLibrary::CaptureResources(ASC,Snapshot)) { return false; }
    ASC->SetNumericAttributeBase(Shield,20.f); ASC->SetNumericAttributeBase(Stamina,30.f);
    bool bNestedRejected=false,bCaptureRejected=false,bCallback=false;
    auto Callback=ASC->GetGameplayAttributeValueChangeDelegate(Shield).AddLambda([&](const FOnAttributeChangeData&)
    {
        if (bCallback) { return; } bCallback=true;
        bNestedRejected=!USovEncounterSnapshotLibrary::RestoreResources(ASC,Snapshot);
        FSovCombatResourceSnapshot Captured;
        bCaptureRejected=!USovEncounterSnapshotLibrary::CaptureResources(ASC,Captured);
        ASC->ClearActorInfo(); ASC->InitAbilityActorInfo(Actor,Actor);
    });
    TestFalse(TEXT("Same-pointer rebind retires entire restore"),USovEncounterSnapshotLibrary::RestoreResources(ASC,Snapshot));
    ASC->GetGameplayAttributeValueChangeDelegate(Shield).Remove(Callback);
    TestTrue(TEXT("Nested restore is rejected"),bNestedRejected);
    TestTrue(TEXT("Partial resource recapture is rejected"),bCaptureRejected);
    TestEqual(TEXT("Retired operation stops before Stamina write"),ASC->GetNumericAttribute(Stamina),30.f);
    TestTrue(TEXT("Retired operation releases restore guard"),USovEncounterSnapshotLibrary::CaptureResources(ASC,Snapshot));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovResourceSnapshotZeroHealthTest,
    "ProjectVelkorran.Campaign.Encounter.Resources.ZeroHealthIsNotRevived",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovResourceSnapshotZeroHealthTest::RunTest(const FString& Parameters)
{
    SovResourceSnapshotTests::FWorld Fixture; auto* Actor=Fixture.Character();
    if (!Actor) { return false; }
    auto* ASC=Actor->GetNarrativeAbilitySystemComponent(); const auto Health=UNarrativeAttributeSetBase::GetHealthAttribute();
    ASC->SetNumericAttributeBase(Health,0.f);
    FSovCombatResourceSnapshot Snapshot;
    TestTrue(TEXT("Dead resource state is a valid capture"),USovEncounterSnapshotLibrary::CaptureResources(ASC,Snapshot));
    TestTrue(TEXT("Dead state restores without implicit revive"),USovEncounterSnapshotLibrary::RestoreResources(ASC,Snapshot));
    TestEqual(TEXT("Health remains zero"),ASC->GetNumericAttribute(Health),0.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovResourceSnapshotPassiveReplacementTest,
    "ProjectVelkorran.Campaign.Encounter.Resources.PassiveReplacementRetiresRestore",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovResourceSnapshotPassiveReplacementTest::RunTest(const FString& Parameters)
{
    SovResourceSnapshotTests::FWorld Fixture; auto* Actor=Fixture.Character();
    if (!Actor) { return false; }
    auto* ASC=Actor->GetNarrativeAbilitySystemComponent();
    auto* ShieldOwner=NewObject<USovShieldComponent>(Actor);
    Actor->AddInstanceComponent(ShieldOwner); ShieldOwner->RegisterComponent();
    if (!ShieldOwner->InitializeWithAbilitySystem(ASC)) { return false; }
    FSovCombatResourceSnapshot Snapshot;
    if (!USovEncounterSnapshotLibrary::CaptureResources(ASC,Snapshot)) { return false; }
    const auto Shield=UNarrativeAttributeSetBase::GetShieldAttribute(), Stamina=UNarrativeAttributeSetBase::GetStaminaAttribute();
    ASC->SetNumericAttributeBase(Shield,20.f); ASC->SetNumericAttributeBase(Stamina,30.f);
    bool bReplaced=false;
    const auto Callback=ASC->GetGameplayAttributeValueChangeDelegate(Shield).AddLambda([&](const FOnAttributeChangeData&)
    {
        if (bReplaced) { return; } bReplaced=true;
        ShieldOwner->DestroyComponent();
        auto* Replacement=NewObject<USovShieldComponent>(Actor);
        Actor->AddInstanceComponent(Replacement); Replacement->RegisterComponent(); Replacement->InitializeWithAbilitySystem(ASC);
    });
    TestFalse(TEXT("Passive replacement on unchanged ASC retires the restore"),USovEncounterSnapshotLibrary::RestoreResources(ASC,Snapshot));
    ASC->GetGameplayAttributeValueChangeDelegate(Shield).Remove(Callback);
    TestTrue(TEXT("Real passive owner was replaced"),bReplaced);
    TestEqual(TEXT("Replacement stops later resource writes"),ASC->GetNumericAttribute(Stamina),30.f);
    return true;
}
#endif
