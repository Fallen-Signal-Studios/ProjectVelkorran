// Copyright Fallen Signal Studios. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/AssetManagerSettings.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Tests/SovCampaignCookRootValidationFixtures.h"
#include "Validation/SovCampaignContentValidation.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    const FString* RouteOf(const TArray<FSovCookRoot>& Roots, const FString& Package)
    {
        const FName Name(*Package);
        const FSovCookRoot* Found = Roots.FindByPredicate([Name](const FSovCookRoot& Root) { return Root.Package == Name; });
        return Found ? &Found->Route : nullptr;
    }

    /** Restores the fixture default object even when an assertion fails part-way through. */
    struct FCookRootFixtureDefaults
    {
        USovCookRootConfigFixture* Defaults = GetMutableDefault<USovCookRootConfigFixture>();
        ~FCookRootFixtureDefaults()
        {
            Defaults->EntryMap.Reset();
            Defaults->SoftAsset.Reset();
            Defaults->SoftClass.Reset();
            Defaults->LoadedClass = nullptr;
            Defaults->NativeClass = nullptr;
            Defaults->MapList.Reset();
            Defaults->UntrackedPath.Reset();
            Defaults->NotConfigPath.Reset();
#if WITH_EDITORONLY_DATA
            Defaults->EditorOnlyPath.Reset();
#endif
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignConfiguredCookRoots,
    "ProjectVelkorran.Campaign.Validation.ConfiguredCookRoots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCampaignConfiguredCookRoots::RunTest(const FString& Parameters)
{
    using namespace SovCampaignContentValidation;
    FCookRootFixtureDefaults Fixture;
    USovCookRootConfigFixture* Defaults = Fixture.Defaults;
    TStrongObjectPtr<UPackage> LoadedPackage(CreatePackage(
        *FString::Printf(TEXT("/Game/SovCookRootTests/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))));
    Defaults->EntryMap = FSoftObjectPath(TEXT("/NarrativePro/Pro/Demo/Maps/OpenWorld/L_DemoMap_OpenWorld.L_DemoMap_OpenWorld"));
    Defaults->SoftAsset = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/SovCookRootTests/SoftAsset.SoftAsset")));
    Defaults->SoftClass = TSoftClassPtr<UObject>(FSoftObjectPath(TEXT("/Game/SovCookRootTests/BP_Soft.BP_Soft_C")));
    // The transient generated-class shape the Kismet compiler itself creates; only its identity and package are read.
    UBlueprintGeneratedClass* LoadedClass = NewObject<UBlueprintGeneratedClass>(LoadedPackage.Get(), TEXT("BP_LoadedClass_C"), RF_Public | RF_Transient);
    LoadedClass->SetSuperStruct(UObject::StaticClass());
    Defaults->LoadedClass = LoadedClass;
    Defaults->NativeClass = UObject::StaticClass();
    Defaults->MapList = { FSoftObjectPath(TEXT("/Game/SovCookRootTests/MapA.MapA")), FSoftObjectPath(TEXT("/Game/SovCookRootTests/MapB.MapB")) };
    Defaults->UntrackedPath = FSoftObjectPath(TEXT("/Game/SovCookRootTests/Untracked.Untracked"));
    Defaults->NotConfigPath = FSoftObjectPath(TEXT("/Game/SovCookRootTests/NotConfig.NotConfig"));
#if WITH_EDITORONLY_DATA
    Defaults->EditorOnlyPath = FSoftObjectPath(TEXT("/Game/SovCookRootTests/EditorOnly.EditorOnly"));
#endif

    FSovConfiguredCookInputs Inputs;
    Inputs.GameDefaults = {
        { TEXT("GameDefaultMap"), TEXT("/Game/SovCookRootTests/Front.Front") },
        { TEXT("GlobalDefaultGameMode"), TEXT("/Script/Engine.BlueprintGeneratedClass'/NarrativePro/Pro/Core/BP/Framework/BP_NarrativeGameMode.BP_NarrativeGameMode_C'") },
        { TEXT("GameInstanceClass"), TEXT("/Script/Engine.GameInstance") },
        { TEXT("ServerDefaultMap"), TEXT("/Engine/Maps/Entry.Entry") },
        { TEXT("GlobalDefaultServerGameMode"), TEXT("None") } };
    Inputs.Maps = { TEXT("/Game/SovCookRootTests/Mission"), TEXT("/Game/SovCookRootTests/Front.Front") };
    Inputs.Directories = { TEXT("/Engine/BasicShapes/") };
    Inputs.TouchInterface = TEXT("/Game/SovCookRootTests/Touch.Touch");
    Inputs.ConfigClasses = { USovCookRootConfigFixture::StaticClass() };
    TArray<FSovCookRoot> Roots;
    GatherConfiguredCookRoots(Inputs, Roots);
    const auto Has = [&Roots](const FString& Package, const TCHAR* RoutePart)
    {
        const FString* Route = RouteOf(Roots, Package);
        return Route && Route->Contains(RoutePart);
    };

    TestTrue(TEXT("Game default map is a root"), Has(TEXT("/Game/SovCookRootTests/Front"), TEXT("GameDefaultMap")));
    TestTrue(TEXT("Class-qualified game mode export text resolves to its Blueprint package"),
        Has(TEXT("/NarrativePro/Pro/Core/BP/Framework/BP_NarrativeGameMode"), TEXT("GlobalDefaultGameMode")));
    TestNull(TEXT("Server default map is excluded, as in a default cook"), RouteOf(Roots, TEXT("/Engine/Maps/Entry")));
    TestTrue(TEXT("Packaging map is a root"), Has(TEXT("/Game/SovCookRootTests/Mission"), TEXT("MapsToCook")));
    TestEqual(TEXT("A package reached by two routes is one root with its first route"),
        Roots.FilterByPredicate([](const FSovCookRoot& Root) { return Root.Package == FName(TEXT("/Game/SovCookRootTests/Front")); }).Num(), 1);
    TestTrue(TEXT("Touch interface is a root"), Has(TEXT("/Game/SovCookRootTests/Touch"), TEXT("DefaultTouchInterface")));
    const TArray<FSovCookRoot> Directory = Roots.FilterByPredicate(
        [](const FSovCookRoot& Root) { return Root.Route.Contains(TEXT("DirectoriesToAlwaysCook")); });
    TestTrue(TEXT("Always-cook directory expands to its packages"), Directory.Num() > 0);
    TestNull(TEXT("Directory expansion stays inside the directory"), Directory.FindByPredicate(
        [](const FSovCookRoot& Root) { return !Root.Package.ToString().StartsWith(TEXT("/Engine/BasicShapes/")); }));
    TestTrue(TEXT("Config soft path is a startup root"),
        Has(TEXT("/NarrativePro/Pro/Demo/Maps/OpenWorld/L_DemoMap_OpenWorld"), TEXT("SovCookRootConfigFixture.EntryMap")));
    TestTrue(TEXT("Config soft object pointer is a root"), Has(TEXT("/Game/SovCookRootTests/SoftAsset"), TEXT("SoftAsset")));
    TestTrue(TEXT("Config soft class pointer is a root"), Has(TEXT("/Game/SovCookRootTests/BP_Soft"), TEXT("SoftClass")));
    TestTrue(TEXT("Loaded config class's package is a root"), Has(LoadedPackage->GetName(), TEXT("LoadedClass")));
    TestTrue(TEXT("Every config array element is a root"),
        Has(TEXT("/Game/SovCookRootTests/MapA"), TEXT("MapList")) && Has(TEXT("/Game/SovCookRootTests/MapB"), TEXT("MapList")));
    TestNull(TEXT("Untracked soft path is not collected"), RouteOf(Roots, TEXT("/Game/SovCookRootTests/Untracked")));
    TestNull(TEXT("Non-config property is not collected"), RouteOf(Roots, TEXT("/Game/SovCookRootTests/NotConfig")));
    TestNull(TEXT("Editor-only config soft path is not collected"), RouteOf(Roots, TEXT("/Game/SovCookRootTests/EditorOnly")));
    TestNull(TEXT("No native script package becomes a root"), Roots.FindByPredicate(
        [](const FSovCookRoot& Root) { return Root.Package.ToString().StartsWith(TEXT("/Script/")); }));

    TArray<FSovCookRoot> Empty;
    GatherConfiguredCookRoots(FSovConfiguredCookInputs(), Empty);
    TestEqual(TEXT("No configured input produces no root"), Empty.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignProductionCookInputs,
    "ProjectVelkorran.Campaign.Validation.ProductionCookInputs", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCampaignProductionCookInputs::RunTest(const FString& Parameters)
{
    using namespace SovCampaignContentValidation;
    const FSovConfiguredCookInputs Inputs = ReadConfiguredCookInputs();
    TestTrue(TEXT("GameMapsSettings default map is read"), Inputs.GameDefaults.ContainsByPredicate(
        [](const TPair<FString, FString>& Entry) { return Entry.Key == TEXT("GameDefaultMap"); }));
    TestTrue(TEXT("Native engine config classes are read"), Inputs.ConfigClasses.Contains(UAssetManagerSettings::StaticClass()));
    TestFalse(TEXT("Non-config classes are not read"), Inputs.ConfigClasses.Contains(UObject::StaticClass()));
    TArray<FString> RawDirectories;
    GConfig->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("DirectoriesToAlwaysCook"), RawDirectories, GGameIni);
    TestEqual(TEXT("Every configured always-cook directory survives parsing"), Inputs.Directories.Num(), RawDirectories.Num());
    TestNull(TEXT("Every parsed always-cook directory is a rooted path"), Inputs.Directories.FindByPredicate(
        [](const FString& Directory) { return !Directory.StartsWith(TEXT("/")); }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignCookListRoots,
    "ProjectVelkorran.Campaign.Validation.CookListRoots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCampaignCookListRoots::RunTest(const FString& Parameters)
{
    using SovCampaignContentValidation::ReadCookListRoots;
    const FString Log = TEXT(
        "[2026.09.13-03.58.38:798][  0]LogCook: Display: SkipOnlyEditorOnly is enabled.\r\n"
        "[2026.09.13-04.20.00:000][  0]LogCookList: Display: /Game/Aurelion/Maps/L_Aurelion_M12, Instigator: CommandLinePackage\r\n"
        "[2026.09.13-04.20.00:000][  0]LogCookList: Display: Rejected: /NarrativePro/Pro/Editor/UI/Widgets/Tales/WBP_DefaultDialogueNode, Instigator: StartupPackage\n"
        "LogCookList: Display: /NarrativePro/Pro/Core/Tales/Events/NE_GiveXP, Instigator: HardDependency: /NarrativePro/Pro/Demo/Character/Definitions/Luca/Dialogue/DBP_Luca\n"
        "LogCookList: Display: /Game/Aurelion/Maps/L_Aurelion_M12, Instigator: HardDependency: /Game/Aurelion/Data/DA_M12_FireAndFrost\n");
    TArray<FName> Packages;
    TestTrue(TEXT("A cook list with cooked packages is accepted"), ReadCookListRoots(Log, Packages));
    TestTrue(TEXT("Cooked packages are roots once each, in log order"), Packages == TArray<FName>{
        FName(TEXT("/Game/Aurelion/Maps/L_Aurelion_M12")), FName(TEXT("/NarrativePro/Pro/Core/Tales/Events/NE_GiveXP")) });
    TestFalse(TEXT("A log with no cooked package fails closed"), ReadCookListRoots(TEXT("LogCook: Display: nothing cooked\n"), Packages));
    TestTrue(TEXT("A failed read leaves no stale roots"), Packages.IsEmpty());
    return true;
}
#endif
