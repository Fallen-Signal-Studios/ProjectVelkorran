// Copyright Fallen Signal Studios. All Rights Reserved.
// Guards against Narrative Pro demo/template weapons reaching campaign actors through a
// character's default item loadout. Three Aurelion roles shipped with demo placeholders
// (Weaver and Wall Runner with a demo pistol/sword, Elite with a demo sword) and the
// packaged build granted them at runtime. Soft paths are asserted rather than loaded, so
// these tests never pull demo content into the process.
#include "Validation/SovCampaignContentValidation.h"
#include "Character/CharacterDefinition.h"
#include "Items/InventoryComponent.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
const TCHAR* DemoPistol =
    TEXT("/NarrativePro/Pro/Demo/Items/Examples/Items/Weapons/Firearms/Weapon_DemoPistol.Weapon_DemoPistol_C");
const TCHAR* DemoSword =
    TEXT("/NarrativePro/Pro/Demo/Items/Examples/Items/Weapons/Melee/Weapon_DemoSword.Weapon_DemoSword_C");
/** Narrative's shared non-demo infrastructure must stay usable by the campaign. */
const TCHAR* CoreFirearmAbility =
    TEXT("/NarrativePro/Pro/Core/Abilities/GameplayAbilities/Attacks/Firearms/GA_Firearm_Rifle.GA_Firearm_Rifle_C");

UCharacterDefinition* DefinitionGranting(const TCHAR* ItemPath)
{
    UCharacterDefinition* Definition = NewObject<UCharacterDefinition>();
    FLootTableRoll Roll;
    FItemWithQuantity Grant;
    Grant.Item = TSoftClassPtr<UNarrativeItem>(FSoftObjectPath(ItemPath));
    Grant.Quantity = 1;
    Roll.ItemsToGrant.Add(Grant);
    Definition->DefaultItemLoadout.Add(Roll);
    return Definition;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDemoLoadoutRejected, "ProjectVelkorran.Campaign.Validation.DemoItemLoadoutRejected", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovDemoLoadoutRejected::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("A demo firearm grant is reported"),
        SovCampaignContentValidation::DemoItemLoadoutReason(DefinitionGranting(DemoPistol)).IsEmpty());
    TestFalse(TEXT("A demo melee grant is reported"),
        SovCampaignContentValidation::DemoItemLoadoutReason(DefinitionGranting(DemoSword)).IsEmpty());
    // The reason must name the offending item so an author can find it without a debugger.
    const FString Reason = SovCampaignContentValidation::DemoItemLoadoutReason(DefinitionGranting(DemoPistol));
    TestTrue(TEXT("The report names the offending demo item"), Reason.Contains(TEXT("Weapon_DemoPistol")));
    TestTrue(TEXT("A demo grant is also prohibited through the shared campaign entry point"),
        !SovCampaignContentValidation::ProhibitedAssetReason(DefinitionGranting(DemoSword)).IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAuthoredLoadoutAccepted, "ProjectVelkorran.Campaign.Validation.AuthoredItemLoadoutAccepted", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovAuthoredLoadoutAccepted::RunTest(const FString& Parameters)
{
    // An empty loadout is the approved shape for roles whose offense comes from their
    // ability kit: Linkbound, Security Drone, Contaminated Drone, and now Weaver,
    // Wall Runner and Elite.
    UCharacterDefinition* AbilityOnly = NewObject<UCharacterDefinition>();
    TestTrue(TEXT("An ability-only role with no loadout is accepted"),
        SovCampaignContentValidation::DemoItemLoadoutReason(AbilityOnly).IsEmpty());
    TestTrue(TEXT("Narrative's shared non-demo content is not treated as a demo grant"),
        SovCampaignContentValidation::DemoItemLoadoutReason(DefinitionGranting(CoreFirearmAbility)).IsEmpty());
    // A project-authored weapon is the intended Enforcer fix and must pass once it exists.
    TestTrue(TEXT("A project-authored weapon grant is accepted"),
        SovCampaignContentValidation::DemoItemLoadoutReason(
            DefinitionGranting(TEXT("/Game/Items/Weapons/Weapon_AurelionRifle.Weapon_AurelionRifle_C"))).IsEmpty());
    TestTrue(TEXT("A non-character asset is not misreported"),
        SovCampaignContentValidation::DemoItemLoadoutReason(NewObject<UItemCollection>()).IsEmpty());
    TestTrue(TEXT("A null asset is handled"),
        SovCampaignContentValidation::DemoItemLoadoutReason(nullptr).IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDemoLoadoutThroughCollection, "ProjectVelkorran.Campaign.Validation.DemoItemLoadoutThroughCollection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovDemoLoadoutThroughCollection::RunTest(const FString& Parameters)
{
    // Tarrik and Selene are equipped through item collections, so a demo item hidden in a
    // collection must be reported as well as a direct grant.
    UItemCollection* Collection = NewObject<UItemCollection>();
    FItemWithQuantity Grant;
    Grant.Item = TSoftClassPtr<UNarrativeItem>(FSoftObjectPath(DemoPistol));
    Grant.Quantity = 1;
    Collection->Items.Add(Grant);
    UCharacterDefinition* Definition = NewObject<UCharacterDefinition>();
    FLootTableRoll Roll;
    Roll.ItemCollectionsToGrant.Add(Collection);
    Definition->DefaultItemLoadout.Add(Roll);
    const FString Reason = SovCampaignContentValidation::DemoItemLoadoutReason(Definition);
    TestFalse(TEXT("A demo item inside a granted collection is reported"), Reason.IsEmpty());
    TestTrue(TEXT("The report names the collection that carried it"), Reason.Contains(Collection->GetName()));
    return true;
}
#endif
