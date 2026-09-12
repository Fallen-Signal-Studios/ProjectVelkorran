// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Validation/SovCampaignContentValidation.h"

#include "Validation/SovCampaignContentPolicy.h"
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
