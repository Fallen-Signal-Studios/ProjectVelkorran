// Copyright Fallen Signal Studios. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Parse.h"
#include "Tests/SovCampaignContentValidationFixtures.h"
#include "Validation/SovCampaignContentValidation.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Engine/CurveTable.h"
#include "Engine/Texture2D.h"
#include "GameplayEffect.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Tales/NarrativeEvent.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    UPackage* ValidationPackage()
    { return CreatePackage(*FString::Printf(TEXT("/Game/SovValidationTests/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))); }

    UBlueprint* NamedBlueprint(UPackage* Package, const TCHAR* Name, UClass* Parent)
    {
        UBlueprint* Blueprint = NewObject<UBlueprint>(Package, Name, RF_Transient);
        Blueprint->ParentClass = Parent;
        return Blueprint;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignProhibitedContentIdentity,
    "ProjectVelkorran.Campaign.Validation.ProhibitedContentIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCampaignProhibitedContentIdentity::RunTest(const FString& Parameters)
{
    using SovCampaignContentValidation::ProhibitedAssetReason;
    TStrongObjectPtr<UPackage> Package(ValidationPackage());
    TestTrue(TEXT("Relocated XP effect is classified by compatible type and exact authored name"),
        ProhibitedAssetReason(NamedBlueprint(Package.Get(), TEXT("GE_GiveXP"), UGameplayEffect::StaticClass())).Contains(TEXT("XP")));
    TestTrue(TEXT("Relocated XP event is rejected"),
        ProhibitedAssetReason(NamedBlueprint(Package.Get(), TEXT("NE_GiveXP"), UNarrativeEvent::StaticClass())).Contains(TEXT("XP")));
    TestTrue(TEXT("Relocated currency event is rejected"),
        ProhibitedAssetReason(NamedBlueprint(Package.Get(), TEXT("BPE_AddCurrency"), UNarrativeEvent::StaticClass())).Contains(TEXT("currency")));
    TestTrue(TEXT("Relocated multiplayer menu is rejected"),
        ProhibitedAssetReason(NamedBlueprint(Package.Get(), TEXT("W_NarrativeMenu_MPMainMenu"), UUserWidget::StaticClass())).Contains(TEXT("multiplayer")));

    // The child has neither the original name nor directory; its real UClass ancestry carries the restriction.
    TestTrue(TEXT("Renamed derived event retains actual prohibited ancestry"),
        ProhibitedAssetReason(USovRenamedCampaignRewardTestEvent::StaticClass()).Contains(TEXT("inherits")));

    TStrongObjectPtr<UPackage> AllowedPackage(ValidationPackage());
    TestTrue(TEXT("A same-name texture is not an XP gameplay system"),
        ProhibitedAssetReason(NewObject<UTexture2D>(AllowedPackage.Get(), TEXT("GE_GiveXP"), RF_Transient)).IsEmpty());
    TestTrue(TEXT("Known event name with incompatible type is allowed"),
        ProhibitedAssetReason(NamedBlueprint(AllowedPackage.Get(), TEXT("BPE_AddCurrency"), UUserWidget::StaticClass())).IsEmpty());
    TestTrue(TEXT("A similar-name Narrative event is not classified by broad substring"),
        ProhibitedAssetReason(NamedBlueprint(AllowedPackage.Get(), TEXT("NE_GiveXP_Explanation"), UNarrativeEvent::StaticClass())).IsEmpty());
    TestTrue(TEXT("Approved technique infrastructure remains allowed"),
        ProhibitedAssetReason(NewObject<USovCampaignContentValidationDataAsset>(AllowedPackage.Get(), TEXT("DA_TechniqueReward"), RF_Transient)).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignXPModifierInspection,
    "ProjectVelkorran.Campaign.Validation.XPModifierInspection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCampaignXPModifierInspection::RunTest(const FString& Parameters)
{
    using SovCampaignContentValidation::ProhibitedAssetReason;
    TStrongObjectPtr<UGameplayEffect> Effect(NewObject<UGameplayEffect>());
    FGameplayModifierInfo& Modifier = Effect->Modifiers.AddDefaulted_GetRef();
    Modifier.Attribute = UNarrativeAttributeSetBase::GetXPAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(10.f));
    TestTrue(TEXT("Renaming a real XP effect does not hide its native attribute write"), ProhibitedAssetReason(Effect.Get()).Contains(TEXT("XP")));
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FSetByCallerFloat());
    TestTrue(TEXT("Dynamic XP grants cannot masquerade as zero initialization"), !ProhibitedAssetReason(Effect.Get()).IsEmpty());
    TStrongObjectPtr<UCurveTable> Table(NewObject<UCurveTable>());
    FRichCurve& Curve = Table->AddRichCurve(FName(TEXT("XP")));
    Curve.AddKey(1.f, 0.f); Curve.AddKey(2.f, 10.f);
    FScalableFloat ScaledXP(1.f);
    ScaledXP.Curve.CurveTable = Table.Get(); ScaledXP.Curve.RowName = TEXT("XP");
    TestEqual(TEXT("Fixture curve is zero at its initial level"), ScaledXP.GetValueAtLevel(1.f), 0.f);
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(ScaledXP);
    TestTrue(TEXT("A later-level XP grant is not accepted as constant zero"), !ProhibitedAssetReason(Effect.Get()).IsEmpty());
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.f));
    Modifier.ModifierOp = EGameplayModOp::Override;
    TestTrue(TEXT("Unused legacy storage may explicitly initialize to literal zero"), ProhibitedAssetReason(Effect.Get()).IsEmpty());
    Modifier.Attribute = UNarrativeAttributeSetBase::GetEchoAttribute();
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.f));
    TestTrue(TEXT("Approved small Echo sustain is not XP progression"), ProhibitedAssetReason(Effect.Get()).IsEmpty());
    Modifier.Attribute = UNarrativeAttributeSetBase::GetHealthAttribute();
    TestTrue(TEXT("Approved Health recovery remains allowed"), ProhibitedAssetReason(Effect.Get()).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignAlwaysCookRoots,
    "ProjectVelkorran.Campaign.Validation.AlwaysCookRoots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCampaignAlwaysCookRoots::RunTest(const FString& Parameters)
{
    using namespace SovCampaignContentValidation;
    TStrongObjectPtr<USovCampaignContentValidationAssetManager> Manager(NewObject<USovCampaignContentValidationAssetManager>());
    Manager->Types.Add(FPrimaryAssetTypeInfo(FName(TEXT("Runtime")), UObject::StaticClass(), false, false));
    Manager->Types.Add(FPrimaryAssetTypeInfo(FName(TEXT("Editor")), UObject::StaticClass(), false, true));
    TStrongObjectPtr<UPackage> RootPackage(ValidationPackage());
    TStrongObjectPtr<UPackage> ForbiddenPackage(ValidationPackage());
    UDataAsset* Primary = NewObject<USovCampaignContentValidationDataAsset>(RootPackage.Get(), TEXT("NPCDefinition"), RF_Transient);
    UBlueprint* Forbidden = NamedBlueprint(ForbiddenPackage.Get(), TEXT("NE_GiveXP"), UNarrativeEvent::StaticClass());
    const FPrimaryAssetId RuntimeId(TEXT("Runtime"), TEXT("Primary"));
    Manager->Assets.Add(RuntimeId, FAssetData(Primary));
    Manager->PackageRules.Add(RootPackage->GetFName(), EPrimaryAssetCookRule::AlwaysCook);
    Manager->PackageRules.Add(ForbiddenPackage->GetFName(), EPrimaryAssetCookRule::AlwaysCook);
    FAssetBundleEntry Bundle(FName(TEXT("Runtime")));
    Bundle.AssetPaths.Add(FTopLevelAssetPath(Forbidden));
    Bundle.AssetPaths.Add(FTopLevelAssetPath(Primary)); // duplicate root, deduplicated
    Manager->Bundles.Add(RuntimeId, {Bundle});
    TArray<FName> Roots; FString Error;
    TestTrue(TEXT("Effective AlwaysCook collection succeeds"), GatherAlwaysCookPackages(*Manager.Get(), Roots, Error));
    TestTrue(TEXT("Management rules refreshed before package decisions"), Manager->bDatabaseUpdated);
    TestEqual(TEXT("Primary and implicit bundle are both roots"), Roots.Num(), 2);
    TestTrue(TEXT("Unreferenced implicit XP package reaches classification"), Roots.Contains(ForbiddenPackage->GetFName()) && !ProhibitedAssetReason(Forbidden).IsEmpty());
    Manager->PackageRules[RootPackage->GetFName()] = EPrimaryAssetCookRule::NeverCook;
    Manager->PackageRules[ForbiddenPackage->GetFName()] = EPrimaryAssetCookRule::DevelopmentAlwaysProductionUnknownCook;
    GatherAlwaysCookPackages(*Manager.Get(), Roots, Error);
    TestEqual(TEXT("Effective override and development-only rules do not create production roots"), Roots.Num(), 0);
    Manager->PackageRules[RootPackage->GetFName()] = EPrimaryAssetCookRule::AlwaysCook;
    Manager->NonCookable.Add(RootPackage->GetFName());
    GatherAlwaysCookPackages(*Manager.Get(), Roots, Error);
    TestEqual(TEXT("Noncookable package is excluded even with AlwaysCook"), Roots.Num(), 0);
    Manager->NonCookable.Reset();
    Manager->Assets.Reset(); Manager->Bundles.Reset();
    Manager->Assets.Add(FPrimaryAssetId(TEXT("Editor"), TEXT("Only")), FAssetData(Primary));
    GatherAlwaysCookPackages(*Manager.Get(), Roots, Error);
    TestEqual(TEXT("Editor-only primary type cannot become a production root"), Roots.Num(), 0);

    TMap<FName, FName> Parents;
    Parents.Add(RootPackage->GetFName(), NAME_None);
    Parents.Add(ForbiddenPackage->GetFName(), RootPackage->GetFName());
    TestTrue(TEXT("Dependency diagnostic preserves the owning root"), DescribeDependencyChain(ForbiddenPackage->GetFName(), Parents).StartsWith(RootPackage->GetName()));
    Parents[RootPackage->GetFName()] = ForbiddenPackage->GetFName();
    TestTrue(TEXT("Cyclic diagnostic input terminates explicitly"), DescribeDependencyChain(ForbiddenPackage->GetFName(), Parents).Contains(TEXT("cycle")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignManifestArgumentLists,
    "ProjectVelkorran.Campaign.Validation.ManifestArgumentLists", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCampaignManifestArgumentLists::RunTest(const FString& Parameters)
{
    using SovCampaignContentValidation::ParseAssetListArgument;
    // Exact unquoted shape emitted by Validate-Campaign.py. The old FParse default silently
    // admitted only Tarrik, then reported its real Selene successor absent from the manifest.
    const FString Params(TEXT("-run=SovValidateCampaign -Missions=/Game/Campaign/DA_Tarrik.DA_Tarrik,/Game/Campaign/DA_Selene.DA_Selene "
        "-ShippingValidation -AdditionalAssets=/Game/Dialogue/DA_Voice.DA_Voice,/Game/Effects/DA_Solver.DA_Solver -AbsLog=Review.log"));
    TArray<FString> Missions, Additional;
    TestTrue(TEXT("Production parser admits the raw mission list"), ParseAssetListArgument(Params, TEXT("Missions="), Missions));
    TestTrue(TEXT("Both mission objects survive in manifest order"), Missions == TArray<FString>{
        TEXT("/Game/Campaign/DA_Tarrik.DA_Tarrik"), TEXT("/Game/Campaign/DA_Selene.DA_Selene") });
    TestTrue(TEXT("Production parser admits the raw dynamic list"), ParseAssetListArgument(Params, TEXT("AdditionalAssets="), Additional));
    TestTrue(TEXT("Both dynamic roots survive without swallowing the next option"), Additional == TArray<FString>{
        TEXT("/Game/Dialogue/DA_Voice.DA_Voice"), TEXT("/Game/Effects/DA_Solver.DA_Solver") });
    TestTrue(TEXT("Shipping flag retains its ordinary interpretation"), FParse::Param(*Params, TEXT("ShippingValidation")));
    FString DiagnosticLogPath;
    TestTrue(TEXT("Non-list option still uses native parsing"), FParse::Value(*Params, TEXT("AbsLog="), DiagnosticLogPath));
    TestEqual(TEXT("Following log option is unchanged"), DiagnosticLogPath, FString(TEXT("Review.log")));

    const FString Quoted(TEXT("-Missions=\"/Game/Campaign/DA_Tarrik.DA_Tarrik, /Game/Campaign/DA_Selene.DA_Selene\" -ShippingValidation"));
    TestTrue(TEXT("Quoted compatibility is retained"), ParseAssetListArgument(Quoted, TEXT("Missions="), Additional));
    TestTrue(TEXT("Quoted item whitespace is trimmed without losing the second asset"), Additional == Missions);
    TestTrue(TEXT("Single-asset calls remain valid"), ParseAssetListArgument(TEXT("-Missions=/Game/Campaign/DA_Only.DA_Only -nop4"), TEXT("Missions="), Additional));
    TestTrue(TEXT("Single-asset value does not consume following options"), Additional == TArray<FString>{TEXT("/Game/Campaign/DA_Only.DA_Only")});
    TestFalse(TEXT("Missing list fails without reusing earlier roots"), ParseAssetListArgument(TEXT("-ShippingValidation"), TEXT("Missions="), Additional));
    TestTrue(TEXT("Missing list clears the output"), Additional.IsEmpty());
    Additional.Add(TEXT("RetiredRoot"));
    TestFalse(TEXT("Empty quoted list fails closed"), ParseAssetListArgument(TEXT("-Missions=\" , \" -ShippingValidation"), TEXT("Missions="), Additional));
    TestTrue(TEXT("Empty list clears the output"), Additional.IsEmpty());
    return true;
}
#endif
