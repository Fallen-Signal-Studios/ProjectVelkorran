// Copyright Fallen Signal Studios. All Rights Reserved.
#include "World/SovAurelionNavigationSystem.h"
#include "AI/NavigationSystemBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_AUTOMATION_TESTS
struct FSovAurelionNavigationTestAccess
{
    static UNavigationSystemV1::ERegistrationResult Register(USovAurelionNavigationSystem* Navigation, ANavigationData* Data)
    { return Navigation->RegisterNavData(Data); }
};
namespace
{
    struct FNavigationWorld
    {
        UWorld* World = nullptr;
        explicit FNavigationWorld(EWorldType::Type Type)
        {
            const UWorld::InitializationValues Init = UWorld::InitializationValues()
                .AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(false)
                .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(Type, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            if (World && GEngine) { GEngine->CreateNewWorldContext(Type).SetCurrentWorld(World); }
        }
        ~FNavigationWorld()
        { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
    };
    bool EquivalentAgents(const TArray<FNavDataConfig>& Left, const TArray<FNavDataConfig>& Right)
    {
        if (Left.Num() != Right.Num()) { return false; }
        for (int32 Index = 0; Index < Left.Num(); ++Index)
        {
            if (!Left[Index].IsEquivalent(Right[Index]) || Left[Index].Name != Right[Index].Name
                || Left[Index].GetNavDataClass<ANavigationData>() != Right[Index].GetNavDataClass<ANavigationData>())
            { return false; }
        }
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionNavigationProfileTest,
    "ProjectVelkorran.Aurelion.Navigation.SavedWorldProfileSurvivesReinstantiationAndRegistration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionNavigationProfileTest::RunTest(const FString& Parameters)
{
    const auto* StockSystem = GetDefault<UNavigationSystemV1>();
    const TArray<FNavDataConfig> StockAgents = StockSystem->GetSupportedAgents();
    const FNavAgentSelector StockMask = StockSystem->GetSupportedAgentsMask();
    const auto* StockData = GetDefault<ARecastNavMesh>();
    const float StockRadius = StockData->AgentRadius, StockHeight = StockData->AgentHeight;
    const ERuntimeGenerationType StockGeneration = StockData->GetRuntimeGenerationMode();
    FNavigationWorld First(EWorldType::Game), Reopened(EWorldType::PIE);
    if (!TestNotNull(TEXT("First isolated world"), First.World) || !TestNotNull(TEXT("Recreated PIE world"), Reopened.World)) { return false; }
    auto* SavedConfigProperty = FindFProperty<FObjectPropertyBase>(AWorldSettings::StaticClass(), TEXT("NavigationSystemConfig"));
    if (!TestNotNull(TEXT("Engine's saved config property"), SavedConfigProperty)) { return false; }
    TestTrue(TEXT("Config is an owned instanced property"), SavedConfigProperty->HasAnyPropertyFlags(CPF_InstancedReference));
    TestFalse(TEXT("Config survives package persistence, unlike its temporary override"), SavedConfigProperty->HasAnyPropertyFlags(CPF_Transient));
    auto* OriginalConfig = NewObject<USovAurelionNavigationConfig>(First.World->GetWorldSettings());
    auto* DuplicatedConfig = DuplicateObject<USovAurelionNavigationConfig>(OriginalConfig, Reopened.World->GetWorldSettings());
    if (!TestNotNull(TEXT("Instanced world config duplicates for PIE"), DuplicatedConfig)) { return false; }
    TestTrue(TEXT("PIE config gets the new WorldSettings owner"), DuplicatedConfig->GetOuter() == Reopened.World->GetWorldSettings());
    TestTrue(TEXT("Native system class survives instanced duplication"),
        DuplicatedConfig->NavigationSystemClass.ResolveClass() == USovAurelionNavigationSystem::StaticClass());
    const FNavDataConfig Expected = USovAurelionNavigationSystem::MakeAgentConfig();
    const TPair<UWorld*, USovAurelionNavigationConfig*> Cases[] = {
        { First.World, OriginalConfig }, { Reopened.World, DuplicatedConfig }
    };
    for (const auto& Case : Cases)
    {
        SavedConfigProperty->SetObjectPropertyValue_InContainer(Case.Key->GetWorldSettings(), Case.Value);
        // Exercise the real config selection/creation seam before the engine registers navigation data.
        FNavigationSystem::AddNavigationSystemToWorld(*Case.Key, FNavigationSystemRunMode::GameMode, nullptr, false);
        auto* Navigation = Cast<USovAurelionNavigationSystem>(Case.Key->GetNavigationSystem());
        if (!TestNotNull(TEXT("Saved world config selects its own navigation system"), Navigation)) { return false; }
        TestEqual(TEXT("Exactly one supported profile"), Navigation->GetSupportedAgents().Num(), 1);
        if (Navigation->GetSupportedAgents().Num() != 1) { return false; }
        TestTrue(TEXT("Actual capsule dimensions survive inherited project config"), Navigation->GetSupportedAgents()[0].IsEquivalent(Expected));
        // This operation used to silently discard instance-only dimensions by resetting from the stock CDO.
        Navigation->OverrideSupportedAgents({ Expected });
        Navigation->SetSupportedAgentsMask(Case.Value->SupportedAgentsMask);
        TestTrue(TEXT("Agent filtering still uses the owned class profile"), Navigation->GetSupportedAgents()[0].IsEquivalent(Expected));
        TestTrue(TEXT("Filtered profile remains enabled"), Navigation->GetSupportedAgentsMask().Contains(0));
        auto* Data = Case.Key->SpawnActor<ASovAurelionRecastNavMesh>();
        if (!TestNotNull(TEXT("Owned native navigation data recreated"), Data)) { return false; }
        Data->SetConfig(Expected);
        TestEqual(TEXT("Fresh data has the required dynamic geometry mode"), Data->GetRuntimeGenerationMode(), ERuntimeGenerationType::Dynamic);
        TestEqual(TEXT("Radius is restored from the supported profile"), Data->AgentRadius, Expected.AgentRadius);
        TestEqual(TEXT("Height is restored from the supported profile"), Data->AgentHeight, Expected.AgentHeight);
        TestEqual(TEXT("Engine accepts the recreated data's exact class and dimensions"),
            FSovAurelionNavigationTestAccess::Register(Navigation, Data), UNavigationSystemV1::RegistrationSuccessful);
        TestTrue(TEXT("Data remains registered"), Data->IsRegistered());
        auto* LegacyData = Case.Key->SpawnActor<ARecastNavMesh>();
        if (!TestNotNull(TEXT("Legacy class fixture"), LegacyData)) { return false; }
        LegacyData->SetConfig(Expected);
        Navigation->UnregisterNavData(Data);
        TestEqual(TEXT("Identical instance dimensions alone do not admit the old class"),
            FSovAurelionNavigationTestAccess::Register(Navigation, LegacyData), UNavigationSystemV1::RegistrationFailed_AgentNotValid);
        TestEqual(TEXT("Owned data can re-register after an invalid legacy candidate"),
            FSovAurelionNavigationTestAccess::Register(Navigation, Data), UNavigationSystemV1::RegistrationSuccessful);
    }
    TestTrue(TEXT("Stock supported agents remain byte-semantically equivalent"), EquivalentAgents(StockAgents, StockSystem->GetSupportedAgents()));
    TestTrue(TEXT("Stock agent selection is unchanged"), StockMask.IsSame(StockSystem->GetSupportedAgentsMask()));
    TestEqual(TEXT("Stock Recast radius unchanged"), StockData->AgentRadius, StockRadius);
    TestEqual(TEXT("Stock Recast height unchanged"), StockData->AgentHeight, StockHeight);
    TestEqual(TEXT("Stock Recast generation unchanged"), StockData->GetRuntimeGenerationMode(), StockGeneration);
    return true;
}
#endif
