#include "Settings/SovCampaignModifiers.h"
#include "Tests/SovSettingsTestFixtures.h"
#include "Misc/AutomationTest.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"
#include "EngineUtils.h"
#include "Components/SovCombatSustainDropComponent.h"
#include "Combat/Pickups/SovAmmoCombatSustainPickup.h"
#include "Combat/Pickups/SovEchoCombatSustainPickup.h"
#include "Items/AmmoItem.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignModifierSettingsTest, "ProjectVelkorran.Campaign.Settings.IndependentModifiers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCampaignModifierSettingsTest::RunTest(const FString& Parameters)
{
    using namespace SovCampaignModifiers;
    auto* Settings = NewObject<USovSettingsTestSettings>();
    FString Error;
    FSovUserSettingsSnapshot Value = Settings->GetSettingsSnapshot();
    Value.bModifierBlackout = Value.bModifierFamine = Value.bModifierFrenzy = Value.bModifierAscendant = Value.bModifierGlassCannon = true;
    TestTrue(TEXT("Combined challenges accepted"), Settings->ApplySettingsSnapshot(Value, Error));
    TestEqual(TEXT("All five independent flags"), Settings->GetCampaignModifiers(), All);
    for (const auto Preset : {ESovDifficultyPreset::Story, ESovDifficultyPreset::Standard, ESovDifficultyPreset::Veteran, ESovDifficultyPreset::Custom})
    {
        TestTrue(TEXT("Difficulty change accepted"), Settings->ApplyDifficultyPreset(Preset, Error));
        TestEqual(TEXT("Difficulty changes preserve optional challenges"), Settings->GetCampaignModifiers(), All);
    }
    TArray<uint8> Bytes;
    TestTrue(TEXT("Challenges export with campaign gameplay settings"), Settings->CapturePortableSettings(Bytes));
    auto* Restored = NewObject<USovSettingsTestSettings>();
    TestTrue(TEXT("Combined challenge mask round trips"), Restored->RestorePortableSettings(Bytes, Error));
    TestEqual(TEXT("All five restored"), Restored->GetCampaignModifiers(), All);
    Bytes.Last() = 128;
    TestFalse(TEXT("Unknown challenge bits rejected atomically"), Restored->RestorePortableSettings(Bytes, Error));
    TestEqual(TEXT("Rejected import preserves active flags"), Restored->GetCampaignModifiers(), All);
    Bytes.SetNum(11); Bytes[0] = 1;
    TestTrue(TEXT("Version 1 campaign settings still load"), Restored->RestorePortableSettings(Bytes, Error));
    TestEqual(TEXT("Legacy saves do not gain challenges"), Restored->GetCampaignModifiers(), uint8(0));
    TestEqual(TEXT("Frenzy layers over Story tokens"), AttackTokens(Frenzy,1),3);
    TestEqual(TEXT("Frenzy layers over highest difficulty tokens"), AttackTokens(Frenzy,6),8);
    TestEqual(TEXT("Frenzy leaves cooldown animations separate"), AttackCooldown(Frenzy),.65f);
    TestEqual(TEXT("No challenge is neutral"), BodyDamage(0,true,false,false),1.f);
    TestEqual(TEXT("Glass Cannon outgoing"), BodyDamage(GlassCannon,false,true,false),2.f);
    TestEqual(TEXT("Glass Cannon incoming"), BodyDamage(GlassCannon,true,false,false),2.f);
    TestTrue(TEXT("Ascendant increases effective toughness"), FMath::IsNearlyEqual(BodyDamage(Ascendant,false,true,false),2.f/3.f));
    TestTrue(TEXT("Combined challenges stack once"), FMath::IsNearlyEqual(BodyDamage(All,false,true,false),4.f/3.f));
    TestEqual(TEXT("Canonical fatal remains fatal"), BodyDamage(All,false,true,true),1.f);
    TestEqual(TEXT("Unrelated or friendly damage stays unchanged"), BodyDamage(All,false,false,false),1.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignModifierDamageTest, "ProjectVelkorran.Campaign.Settings.ModifiersRouteActualCombat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCampaignModifierDamageTest::RunTest(const FString& Parameters)
{
    if (!GEngine) { return false; }
    FEditorScriptExecutionGuard NativeCallbacks;
    TStrongObjectPtr<UGameUserSettings> Previous(GEngine->GameUserSettings);
    TStrongObjectPtr<USovSettingsTestSettings> Settings(NewObject<USovSettingsTestSettings>());
    TGuardValue<TObjectPtr<UGameUserSettings>> ScopedSettings(GEngine->GameUserSettings, Settings.Get());
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
        .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!World) { return false; }
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    auto* Enemy = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(), FVector(150,0,0), FRotator::ZeroRotator, Spawn);
    auto* PC = World->SpawnActor<APlayerController>();
    if (Player && Enemy && PC)
    {
        PC->PlayerState = World->SpawnActor<APlayerState>();
        PC->Possess(Player);
        TestTrue(TEXT("Test player is actually possessed"), PC->GetPawn() == Player && Player->IsPlayerControlled());
        Player->InitializeTestCombat(0); Enemy->InitializeTestCombat(1);
        auto* PASC = Player->GetNarrativeAbilitySystemComponent();
        auto* EASC = Enemy->GetNarrativeAbilitySystemComponent();
        PASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(),0.f);
        EASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(),0.f);
        TestTrue(TEXT("Enemy is classified against the actual possessed campaign pawn"), UNarrativeGameUserSettings::IsCampaignEnemy(Enemy));
        const int32 BaseTokens = PASC->GetNumAttackTokens();
        auto Value = Settings->GetSettingsSnapshot();
        Value.bModifierFrenzy = Value.bModifierAscendant = Value.bModifierGlassCannon = true;
        FString Error; Settings->ApplySettingsSnapshot(Value, Error);
        TestEqual(TEXT("Frenzy expands actual ASC admission budget"), PASC->GetNumAttackTokens(), FMath::Min(BaseTokens+2,8));
        const auto Hit = [](ASovAxiomRuntimeTestCharacter* From, ASovAxiomRuntimeTestCharacter* To)
        {
            auto* ASC = From->GetNarrativeAbilitySystemComponent();
            auto Context = ASC->MakeEffectContext(); Context.AddInstigator(From,From);
            FGameplayEffectSpec Spec(GetDefault<USovCombatRoutingTestEffect>(),Context,1.f);
            Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage,15.f);
            Spec.SetSetByCallerMagnitude(FSovGameplayTags::Get().SetByCaller_Damage_PoiseDamage,6.f);
            ASC->ApplyGameplayEffectSpecToTarget(Spec,To->GetNarrativeAbilitySystemComponent());
        };
        Hit(Player,Enemy);
        TestTrue(TEXT("Actual outgoing 15 damage becomes 20 with both modifiers"), FMath::IsNearlyEqual(Enemy->LastDamageResult.AppliedHealthDamage,20.f));
        TestEqual(TEXT("Toughness does not mutate max health"), EASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxHealthAttribute()),100.f);
        TestTrue(TEXT("Poise remains separately authored"), FMath::IsNearlyEqual(Enemy->LastDamageResult.AppliedPoiseDamage,6.f));
        Hit(Enemy,Player);
        TestTrue(TEXT("Actual incoming 15 damage becomes 30"), FMath::IsNearlyEqual(Player->LastDamageResult.AppliedHealthDamage,30.f));
        const float WoundedHealth = EASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
        Value.bModifierAscendant = Value.bModifierGlassCannon = Value.bModifierFrenzy = false;
        Settings->ApplySettingsSnapshot(Value,Error);
        TestEqual(TEXT("Disabling challenges cannot heal an enemy"), EASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()),WoundedHealth);
        TestEqual(TEXT("Disabling Frenzy restores actual token budget"), PASC->GetNumAttackTokens(),BaseTokens);
        Hit(Player,Enemy);
        TestTrue(TEXT("Disabling restores ordinary routed damage"), FMath::IsNearlyEqual(Enemy->LastDamageResult.AppliedHealthDamage,15.f));
        // Isolated fatal receipts exercise the actual drop component; no fabricated result broadcasts.
        for (int32 Scenario = 0; Scenario < 2; ++Scenario)
        {
            Value.bModifierFamine = Scenario == 1; Settings->ApplySettingsSnapshot(Value,Error);
            auto* Victim = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(), FVector(600+Scenario*200,0,0), FRotator::ZeroRotator, Spawn);
            Victim->InitializeTestCombat(1);
            auto* ASC = Victim->GetNarrativeAbilitySystemComponent();
            ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(),0.f);
            ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(),5.f);
            auto* Drops = NewObject<USovCombatSustainDropComponent>(Victim);
            Victim->AddInstanceComponent(Drops); Drops->RegisterComponent();
            FindFProperty<FClassProperty>(Drops->GetClass(),TEXT("AmmoPickupClass"))->SetObjectPropertyValue_InContainer(Drops,ASovAmmoCombatSustainPickup::StaticClass());
            FindFProperty<FClassProperty>(Drops->GetClass(),TEXT("AmmoItemClass"))->SetObjectPropertyValue_InContainer(Drops,UAmmoItem::StaticClass());
            FindFProperty<FClassProperty>(Drops->GetClass(),TEXT("EchoPickupClass"))->SetObjectPropertyValue_InContainer(Drops,ASovEchoCombatSustainPickup::StaticClass());
            Drops->InitializeWithAbilitySystem(ASC);
            Hit(Player,Victim);
            TestTrue(TEXT("Real fatal transaction owns its drop decision"), Drops->HasSpawnedDropsForCurrentDeath());
            int32 AmmoCount=0, EchoCount=0;
            for (TActorIterator<ASovAmmoCombatSustainPickup> It(World); It; ++It) { ++AmmoCount; }
            for (TActorIterator<ASovEchoCombatSustainPickup> It(World); It; ++It) { ++EchoCount; }
            TestEqual(TEXT("Famine suppresses the second enemy ammo drop"),AmmoCount,1);
            TestEqual(TEXT("Famine preserves Echo drops"),EchoCount,Scenario+1);
        }
    }
    else { AddError(TEXT("Combat fixture failed to spawn")); }
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}
#endif
