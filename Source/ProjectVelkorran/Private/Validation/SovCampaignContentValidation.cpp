// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Validation/SovCampaignContentValidation.h"

#include "Validation/SovCampaignContentPolicy.h"
#include "Validation/SovCampaignCookRootPolicy.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Tales/Dialogue.h"
#include "Tales/Quest.h"
#include "AssetRegistry/AssetBundleData.h"
#include "AssetRegistry/AssetData.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Misc/Parse.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Tales/NarrativeEvent.h"
#include "UObject/Package.h"

namespace
{
    FString AuthoredName(FString Name)
    {
        Name.RemoveFromEnd(TEXT("_C"));
        return Name;
    }

    FString KnownSystemReason(const FString& Name, const UClass* EffectiveClass)
    {
        if (!EffectiveClass) { return {}; }
        if (Name.Equals(TEXT("GE_GiveXP"), ESearchCase::IgnoreCase)
            && EffectiveClass->IsChildOf(UGameplayEffect::StaticClass()))
        { return TEXT("legacy XP award GameplayEffect GE_GiveXP"); }
        if (Name.Equals(TEXT("NE_GiveXP"), ESearchCase::IgnoreCase)
            && EffectiveClass->IsChildOf(UNarrativeEvent::StaticClass()))
        { return TEXT("legacy XP award Narrative event NE_GiveXP"); }
        if (Name.Equals(TEXT("BPE_AddCurrency"), ESearchCase::IgnoreCase)
            && EffectiveClass->IsChildOf(UNarrativeEvent::StaticClass()))
        { return TEXT("legacy currency award Narrative event BPE_AddCurrency"); }
        if (Name.Equals(TEXT("W_NarrativeMenu_MPMainMenu"), ESearchCase::IgnoreCase)
            && EffectiveClass->IsChildOf(UUserWidget::StaticClass()))
        { return TEXT("multiplayer main menu W_NarrativeMenu_MPMainMenu"); }

        // Loot economy. Matched by authored name and class rather than by the word
        // "loot": FLootTableRoll is the framework's ordinary grant struct and is how
        // IC_Tarrik and IC_Selene equip the protagonists, so a keyword rule here would
        // reject the campaign's own fixed-equipment path.
        static const TCHAR* LootWidgets[] = {
            TEXT("W_NarrativeMenu_Looting"),
            TEXT("WBP_Loot_TheirInventory"),
            TEXT("WBP_Loot_YourInventory"),
        };
        for (const TCHAR* LootWidget : LootWidgets)
        {
            if (Name.Equals(LootWidget, ESearchCase::IgnoreCase)
                && EffectiveClass->IsChildOf(UUserWidget::StaticClass()))
            { return FString::Printf(TEXT("loot economy interface %s"), LootWidget); }
        }
        static const TCHAR* LootActors[] = {
            TEXT("BP_LootableChest"),
            TEXT("Interactable_Loot"),
        };
        for (const TCHAR* LootActor : LootActors)
        {
            if (Name.Equals(LootActor, ESearchCase::IgnoreCase)
                && EffectiveClass->IsChildOf(AActor::StaticClass()))
            { return FString::Printf(TEXT("loot economy actor %s"), LootActor); }
        }
        if (Name.Equals(TEXT("DT_LootChest"), ESearchCase::IgnoreCase)
            && EffectiveClass->IsChildOf(UDataTable::StaticClass()))
        { return TEXT("loot economy table DT_LootChest"); }
        return {};
    }

    /** Narrative ships its demo items under this root. Demo VFX and audio are legitimately
     * referenced elsewhere, so only the item tree is treated as a loadout defect. */
    const TCHAR* DemoItemRoot = TEXT("/NarrativePro/Pro/Demo/Items/");

    bool IsDemoItemPath(const FSoftObjectPath& Path)
    {
        return !Path.IsNull() && Path.ToString().Contains(DemoItemRoot, ESearchCase::IgnoreCase);
    }
}

FString SovCampaignContentValidation::ProhibitedSystemReasonForName(
    const FString& AuthoredName, const UClass* EffectiveClass)
{
    return KnownSystemReason(AuthoredName, EffectiveClass);
}

FString SovCampaignContentValidation::DemoItemLoadoutReason(const UObject* Asset)
{
    const UCharacterDefinition* Definition = Cast<UCharacterDefinition>(Asset);
    if (!Definition) { return {}; }
    for (const FLootTableRoll& Roll : Definition->DefaultItemLoadout)
    {
        for (const FItemWithQuantity& Grant : Roll.ItemsToGrant)
        {
            // Soft path only: a demo placeholder must never be loaded to be reported.
            if (IsDemoItemPath(Grant.Item.ToSoftObjectPath()))
            {
                return FString::Printf(TEXT("Narrative demo/template item grant %s in the default loadout"),
                    *Grant.Item.ToString());
            }
        }
        for (const UItemCollection* Collection : Roll.ItemCollectionsToGrant)
        {
            if (!IsValid(Collection)) { continue; }
            for (const FItemWithQuantity& Grant : Collection->Items)
            {
                if (IsDemoItemPath(Grant.Item.ToSoftObjectPath()))
                {
                    return FString::Printf(TEXT("Narrative demo/template item grant %s through collection %s"),
                        *Grant.Item.ToString(), *Collection->GetName());
                }
            }
        }
    }
    return {};
}

bool SovCampaignContentValidation::ParseAssetListArgument(const FString& Params, const TCHAR* Match, TArray<FString>& OutPaths)
{
    OutPaths.Reset();
    FString Value;
    // FParse defaults to stopping at commas. The real runner supplies raw comma lists,
    // so disable only separator stopping here; quoted parsing and whitespace bounds stay native.
    if (!FParse::Value(*Params, Match, Value, false)) { return false; }
    Value.ParseIntoArray(OutPaths, TEXT(","), true);
    for (FString& Path : OutPaths) { Path.TrimStartAndEndInline(); }
    OutPaths.RemoveAll([](const FString& Path) { return Path.IsEmpty(); });
    return !OutPaths.IsEmpty();
}

FString SovCampaignContentValidation::ProhibitedAssetReason(const UObject* Asset)
{
    if (!IsValid(Asset)) { return {}; }
    const UClass* EffectiveClass = Cast<UClass>(Asset);
    if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
    { EffectiveClass = Blueprint->GeneratedClass ? Blueprint->GeneratedClass : Blueprint->ParentClass; }
    if (!EffectiveClass) { EffectiveClass = Asset->GetClass(); }

    FString Reason = KnownSystemReason(AuthoredName(Asset->GetName()), EffectiveClass);
    if (!Reason.IsEmpty()) { return Reason; }

    Reason = DemoItemLoadoutReason(Asset);
    if (!Reason.IsEmpty()) { return Reason; }

    Reason = DemoTaleContentReason(Asset);
    if (!Reason.IsEmpty()) { return Reason; }
    // A renamed child retains its actual authored parent, even when moved outside the legacy directory.
    for (const UClass* Ancestor = EffectiveClass; Ancestor; Ancestor = Ancestor->GetSuperClass())
    {
        Reason = KnownSystemReason(AuthoredName(Ancestor->GetName()), Ancestor);
        if (!Reason.IsEmpty())
        { return FString::Printf(TEXT("inherits %s through %s"), *Reason, *Ancestor->GetPathName()); }
    }

    const UGameplayEffect* Effect = Cast<UGameplayEffect>(Asset);
    if (!Effect && EffectiveClass->IsChildOf(UGameplayEffect::StaticClass()))
    { Effect = Cast<UGameplayEffect>(EffectiveClass->GetDefaultObject(false)); }
    if (Effect)
    {
        for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
        {
            if (Modifier.Attribute != UNarrativeAttributeSetBase::GetXPAttribute()) { continue; }
            // An unused inherited XP field initialized to literal zero does not create an XP economy.
            // Equality compares the complete scalable magnitude, so a curve evaluating to zero at level1
            // or a dynamic SetByCaller cannot masquerade as that inert initialization.
            const bool bInertInitialization = (Modifier.ModifierOp == EGameplayModOp::Additive
                || Modifier.ModifierOp == EGameplayModOp::Override)
                && Modifier.ModifierMagnitude == FGameplayEffectModifierMagnitude(FScalableFloat(0.f));
            if (!bInertInitialization)
            { return TEXT("GameplayEffect modifies legacy Narrative XP progression attribute"); }
        }
    }
    return {};
}

FString SovCampaignContentValidation::DemoTaleContentReason(const UObject* Asset)
{
    if (!IsValid(Asset)) { return {}; }
    const UClass* EffectiveClass = Cast<UClass>(Asset);
    if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
    { EffectiveClass = Blueprint->GeneratedClass ? Blueprint->GeneratedClass : Blueprint->ParentClass; }
    if (!EffectiveClass) { EffectiveClass = Asset->GetClass(); }

    // Tale content only. The demo root also holds VFX, audio and meshes that the campaign may
    // legitimately reference, so location alone never decides this.
    const bool bTaleContent = EffectiveClass->IsChildOf(UQuest::StaticClass())
        || EffectiveClass->IsChildOf(UDialogue::StaticClass());
    if (!bTaleContent) { return {}; }

    const FString Path = Asset->GetPathName();
    if (!SovCampaignContentPolicy::IsDemoContentPath(*Path, static_cast<std::size_t>(Path.Len())))
    { return {}; }

    return FString::Printf(TEXT("Narrative demo tale content %s"), *AuthoredName(Asset->GetName()));
}

bool SovCampaignContentValidation::GatherAlwaysCookPackages(UAssetManager& Manager,
    TArray<FName>& OutPackages, FString& Error)
{
    OutPackages.Reset();
    Error.Reset();
#if WITH_EDITOR
    // Match the production subset of UE5.7 UAssetManager::ModifyCook. Per-package rules include
    // label/manager priorities and overrides that reading the config/type default would miss.
    Manager.UpdateManagementDatabase();
    TArray<FPrimaryAssetTypeInfo> Types;
    Manager.GetPrimaryAssetTypeInfoList(Types);
    const FName TransientPackageName = GetTransientPackage()->GetFName();
    TSet<FName> Candidates;
    for (const FPrimaryAssetTypeInfo& Type : Types)
    {
        if (Type.bIsEditorOnly) { continue; }
        TArray<FPrimaryAssetId> Ids;
        Manager.GetPrimaryAssetIdList(Type.PrimaryAssetType, Ids);
        for (const FPrimaryAssetId& Id : Ids)
        {
            FAssetData Data;
            if (Manager.GetPrimaryAssetData(Id, Data) && Data.PackageName != TransientPackageName)
            { Candidates.Add(Data.PackageName); }
            TArray<FAssetBundleEntry> Entries;
            if (Manager.GetAssetBundleEntries(Id, Entries))
            {
                for (const FAssetBundleEntry& Entry : Entries)
                {
                    for (const FTopLevelAssetPath& Reference : Entry.AssetPaths)
                    { Candidates.Add(Reference.GetPackageName()); }
                }
            }
        }
    }
    for (FName Package : Candidates)
    {
        if (Package.IsNone() || Package == TransientPackageName) { continue; }
        if (Manager.GetPackageCookRule(Package) == EPrimaryAssetCookRule::AlwaysCook
            && Manager.VerifyCanCookPackage(nullptr, Package, false))
        { OutPackages.Add(Package); }
    }
    OutPackages.Sort(FNameLexicalLess());
    return true;
#else
    Error = TEXT("Effective AlwaysCook validation requires the Editor asset-management database.");
    return false;
#endif
}

namespace
{
    std::basic_string<TCHAR> ToStd(const FString& Text) { return std::basic_string<TCHAR>(*Text, Text.Len()); }

    /** Long package name of a config object path, including the Class'/Path.Object' export-text form. */
    FName PackageOfConfigPath(const FString& Value)
    {
        FString Path = Value.TrimStartAndEnd();
        if (Path.IsEmpty() || Path.Equals(TEXT("None"), ESearchCase::IgnoreCase)) { return NAME_None; }
        Path = FPackageName::ExportTextPathToObjectPath(Path);
        return FSoftObjectPath(Path).GetLongPackageFName();
    }

    struct FCookRootCollector
    {
        TArray<FSovCookRoot>& Roots;
        TSet<FName> Seen;

        void Add(FName Package, const FString& Route)
        {
            if (Package.IsNone()) { return; }
            const FString Name = Package.ToString();
            // Native script packages hold class defaults, not cooked content; the transient package never cooks.
            if (Name.StartsWith(TEXT("/Script/")) || Package == GetTransientPackage()->GetFName()) { return; }
            if (!Seen.Contains(Package)) { Seen.Add(Package); Roots.Add({ Package, Route }); }
        }

        void AddConfigValue(const FProperty* Property, const void* Value, const FString& Route, bool bCollectSoft)
        {
            if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
            {
                FScriptArrayHelper Helper(Array, Value);
                for (int32 Index = 0; Index < Helper.Num(); ++Index)
                { AddConfigValue(Array->Inner, Helper.GetRawPtr(Index), Route, bCollectSoft); }
                return;
            }
            // Soft object and soft class properties first: both also derive from FObjectPropertyBase.
            if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
            {
                if (bCollectSoft) { Add(Soft->GetPropertyValue(Value).ToSoftObjectPath().GetLongPackageFName(), Route); }
                return;
            }
            if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
            {
                // Config holds class references rather than object instances. The class is loaded when the default
                // object reads config, so the cooker sees its package as a startup package whether or not the
                // property is editor-only.
                if (const UObject* Referenced = Object->GetObjectPropertyValue(Value))
                { Add(Referenced->GetOutermost()->GetFName(), Route); }
                return;
            }
            if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
            {
                if (bCollectSoft && (Struct->Struct == TBaseStructure<FSoftObjectPath>::Get()
                    || Struct->Struct == TBaseStructure<FSoftClassPath>::Get()))
                { Add(static_cast<const FSoftObjectPath*>(Value)->GetLongPackageFName(), Route); }
            }
        }
    };
}

FSovConfiguredCookInputs SovCampaignContentValidation::ReadConfiguredCookInputs()
{
    FSovConfiguredCookInputs Inputs;
#if WITH_EDITOR
    if (!GConfig) { return Inputs; }
    const TCHAR* MapsSection = TEXT("/Script/EngineSettings.GameMapsSettings");
    for (const TCHAR* Key : { TEXT("GameDefaultMap"), TEXT("GlobalDefaultGameMode"), TEXT("GlobalDefaultServerGameMode"), TEXT("GameInstanceClass") })
    {
        FString Value;
        if (GConfig->GetString(MapsSection, Key, Value, GEngineIni)) { Inputs.GameDefaults.Emplace(Key, Value); }
    }
    const TCHAR* PackagingSection = TEXT("/Script/UnrealEd.ProjectPackagingSettings");
    TArray<FString> Raw;
    GConfig->GetArray(PackagingSection, TEXT("MapsToCook"), Raw, GGameIni);
    for (const FString& Entry : Raw)
    {
        std::basic_string<TCHAR> Path;
        if (SovCampaignCookRootPolicy::PathFromConfigStruct(ToStd(Entry), "FilePath", Path)) { Inputs.Maps.Add(FString(Path.c_str())); }
    }
    // UEditorEngine::LoadMapListFromIni: Map= entries, and Section= entries naming further map-list sections.
    TArray<FString> MapSections = { TEXT("AlwaysCookMaps") };
    TSet<FString> VisitedSections;
    while (!MapSections.IsEmpty())
    {
        const FString Section = MapSections.Pop();
        if (VisitedSections.Contains(Section)) { continue; }
        VisitedSections.Add(Section);
        if (const FConfigSection* Entries = GConfig->GetSection(*Section, false, GEditorIni))
        {
            for (FConfigSectionMap::TConstIterator Entry(*Entries); Entry; ++Entry)
            {
                if (Entry.Key() == NAME_Map) { Inputs.Maps.AddUnique(Entry.Value().GetValue()); }
                else if (Entry.Key() == FName(TEXT("Section"))) { MapSections.Add(Entry.Value().GetValue()); }
            }
        }
    }
    Raw.Reset();
    GConfig->GetArray(PackagingSection, TEXT("DirectoriesToAlwaysCook"), Raw, GGameIni);
    for (const FString& Entry : Raw)
    {
        std::basic_string<TCHAR> Path;
        if (SovCampaignCookRootPolicy::PathFromConfigStruct(ToStd(Entry), "Path", Path)) { Inputs.Directories.Add(FString(Path.c_str())); }
    }
    GConfig->GetString(TEXT("/Script/Engine.InputSettings"), TEXT("DefaultTouchInterface"), Inputs.TouchInterface, GInputIni);
    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (It->HasAllClassFlags(CLASS_Config | CLASS_Native) && !It->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
        { Inputs.ConfigClasses.Add(*It); }
    }
#endif
    return Inputs;
}

void SovCampaignContentValidation::GatherConfiguredCookRoots(const FSovConfiguredCookInputs& Inputs, TArray<FSovCookRoot>& OutRoots)
{
    OutRoots.Reset();
    FCookRootCollector Collector{ OutRoots };
    for (const TPair<FString, FString>& Default : Inputs.GameDefaults)
    {
        if (Default.Key == TEXT("ServerDefaultMap")) { continue; }
        Collector.Add(PackageOfConfigPath(Default.Value), TEXT("GameMapsSettings ") + Default.Key);
    }
    for (const FString& Map : Inputs.Maps) { Collector.Add(PackageOfConfigPath(Map), TEXT("packaging MapsToCook/AlwaysCookMaps")); }
    Collector.Add(PackageOfConfigPath(Inputs.TouchInterface), TEXT("InputSettings DefaultTouchInterface"));
    if (!Inputs.Directories.IsEmpty())
    {
        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        for (const FString& Directory : Inputs.Directories)
        {
            FString Path = Directory.TrimStartAndEnd();
            Path.RemoveFromEnd(TEXT("/"));
            if (!Path.StartsWith(TEXT("/"))) { continue; }
            TArray<FAssetData> Assets;
            Registry.GetAssetsByPath(FName(*Path), Assets, true, true);
            Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });
            for (const FAssetData& Asset : Assets) { Collector.Add(Asset.PackageName, TEXT("packaging DirectoriesToAlwaysCook ") + Path); }
        }
    }
    for (const UClass* Class : Inputs.ConfigClasses)
    {
        const UObject* Defaults = Class ? Class->GetDefaultObject(false) : nullptr;
        if (!Defaults) { continue; }
        for (TFieldIterator<FProperty> It(Class); It; ++It)
        {
            const FProperty* Property = *It;
            if (!Property->HasAnyPropertyFlags(CPF_Config)) { continue; }
#if WITH_EDITORONLY_DATA
            const bool bUntracked = Property->HasMetaData(FSoftObjectPath::NAME_Untracked);
#else
            const bool bUntracked = false;
#endif
            const bool bCollectSoft = !bUntracked && !Property->IsEditorOnlyProperty();
            const FString Route = FString::Printf(TEXT("config reference %s.%s"), *Class->GetName(), *Property->GetName());
            for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
            { Collector.AddConfigValue(Property, Property->ContainerPtrToValuePtr<void>(Defaults, Index), Route, bCollectSoft); }
        }
    }
}

bool SovCampaignContentValidation::ReadCookListRoots(const FString& CookListText, TArray<FName>& OutPackages)
{
    OutPackages.Reset();
    TArray<FString> Lines;
    CookListText.ParseIntoArrayLines(Lines, true);
    for (const FString& Line : Lines)
    {
        std::basic_string<TCHAR> Package;
        if (SovCampaignCookRootPolicy::CookedPackageFromCookListLine(ToStd(Line), Package))
        { OutPackages.AddUnique(FName(Package.c_str())); }
    }
    return !OutPackages.IsEmpty();
}

FString SovCampaignContentValidation::DescribeDependencyChain(FName Package, const TMap<FName, FName>& Parents)
{
    TArray<FString> Chain;
    TSet<FName> Seen;
    while (!Package.IsNone() && !Seen.Contains(Package) && Chain.Num() < 1024)
    {
        Seen.Add(Package);
        Chain.Insert(Package.ToString(), 0);
        const FName* Parent = Parents.Find(Package);
        Package = Parent ? *Parent : NAME_None;
    }
    if (!Package.IsNone()) { Chain.Insert(TEXT("[cycle or truncated chain]"), 0); }
    return FString::Join(Chain, TEXT(" -> "));
}
