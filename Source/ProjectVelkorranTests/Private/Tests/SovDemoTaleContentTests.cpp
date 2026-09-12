// Copyright Fallen Signal Studios. All Rights Reserved.
// Covers the demo tale-content rule: a Narrative demo quest or dialogue reaching campaign content.
//
// TDD Appendix F forbids vendor systems, and Narrative's demo root ships a SecretMerchant quest and
// dialogue. The rule deliberately requires BOTH a demo path and a quest/dialogue class, because the
// same root holds VFX, audio and meshes the campaign legitimately reuses — the tracked content even
// includes a rock mesh named Rock_shopk that a keyword rule would reject.
//
// These tests build their subjects in packages they name themselves rather than loading the real
// demo assets. That is not a convenience: StaticLoadObject on
// /NarrativePro/Pro/Demo/Quests/SecretMerchant/QBP_Demo_Narrative_SecretMerchant hangs this editor
// inside FlushAsyncLoading — observed stalling a run for twenty minutes before it was killed. Demo
// tale content is not loadable headlessly here, which is itself a reason the campaign should not
// ship it. A synthetic package exercises the same class-and-path rule deterministically and pulls in
// no demo dependency graph.
//
// The path arithmetic is covered exhaustively by Tests/Portable/SovCampaignContentPolicyTests.cpp
// without an editor. These cover what that cannot: the class gate.
#include "Engine/DataTable.h"
#include "Misc/AutomationTest.h"
#include "Tales/Dialogue.h"
#include "Tales/Quest.h"
#include "UObject/Package.h"
#include "Validation/SovCampaignContentValidation.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace SovDemoTaleContentTests
{
/** Build an object of the given class inside a package at PackagePath, without touching disk. */
template<typename ObjectType>
ObjectType* MakeAt(const TCHAR* PackagePath, const TCHAR* ObjectName)
{
    UPackage* Package = CreatePackage(PackagePath);
    if (!Package) { return nullptr; }
    Package->AddToRoot();
    return NewObject<ObjectType>(Package, ObjectType::StaticClass(), ObjectName, RF_Transient);
}

void Release(UObject* Object)
{
    if (Object && Object->GetOutermost()) { Object->GetOutermost()->RemoveFromRoot(); }
}

const TCHAR* DemoQuestPackage = TEXT("/NarrativePro/Pro/Demo/Quests/SecretMerchant/QBP_Synthetic_SecretMerchant");
const TCHAR* DemoDialoguePackage = TEXT("/NarrativePro/Pro/Demo/Character/Definitions/Luca/Dialogue/DBP_Synthetic_SecretMerchant");
const TCHAR* DemoVfxPackage = TEXT("/NarrativePro/Pro/Demo/VFX/Epic/Niagara/StaticMesh/S_Synthetic_Rock_shopk");
const TCHAR* CampaignQuestPackage = TEXT("/Game/Aurelion/Quests/QBP_Synthetic_Campaign");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDemoTaleContentRejected, "ProjectVelkorran.Campaign.Validation.DemoTaleContentRejected", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovDemoTaleContentRejected::RunTest(const FString& Parameters)
{
    using namespace SovDemoTaleContentTests;

    UQuest* DemoQuest = MakeAt<UQuest>(DemoQuestPackage, TEXT("QBP_Synthetic_SecretMerchant"));
    if (!TestNotNull(TEXT("Synthetic demo quest"), DemoQuest)) { return false; }
    TestFalse(TEXT("A quest under the demo root is reported"),
        SovCampaignContentValidation::DemoTaleContentReason(DemoQuest).IsEmpty());
    TestFalse(TEXT("The aggregate prohibited-asset reason reports it too"),
        SovCampaignContentValidation::ProhibitedAssetReason(DemoQuest).IsEmpty());
    Release(DemoQuest);

    UDialogue* DemoDialogue = MakeAt<UDialogue>(DemoDialoguePackage, TEXT("DBP_Synthetic_SecretMerchant"));
    if (!TestNotNull(TEXT("Synthetic demo dialogue"), DemoDialogue)) { return false; }
    TestFalse(TEXT("A dialogue under the demo root is reported"),
        SovCampaignContentValidation::DemoTaleContentReason(DemoDialogue).IsEmpty());
    Release(DemoDialogue);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDemoTaleContentDoesNotOverreach, "ProjectVelkorran.Campaign.Validation.DemoTaleContentDoesNotOverreach", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovDemoTaleContentDoesNotOverreach::RunTest(const FString& Parameters)
{
    using namespace SovDemoTaleContentTests;

    // Non-tale content under the same demo root must be accepted. If the rule ever degraded into a
    // path-only check this fails, which is exactly why it is asserted. A DataTable stands in for the
    // meshes, VFX and audio that really live there: UObject itself is abstract and cannot be
    // instantiated, so it is not a usable stand-in.
    UObject* DemoVfx = MakeAt<UDataTable>(DemoVfxPackage, TEXT("S_Synthetic_Rock_shopk"));
    if (!TestNotNull(TEXT("Synthetic demo non-tale asset"), DemoVfx)) { return false; }
    TestTrue(TEXT("Non-tale content under the demo root is accepted"),
        SovCampaignContentValidation::DemoTaleContentReason(DemoVfx).IsEmpty());
    Release(DemoVfx);

    // Tale content outside the demo root is ordinary campaign content.
    UQuest* CampaignQuest = MakeAt<UQuest>(CampaignQuestPackage, TEXT("QBP_Synthetic_Campaign"));
    if (!TestNotNull(TEXT("Synthetic campaign quest"), CampaignQuest)) { return false; }
    TestTrue(TEXT("A quest outside the demo root is accepted"),
        SovCampaignContentValidation::DemoTaleContentReason(CampaignQuest).IsEmpty());
    Release(CampaignQuest);

    // Transient-package objects carry no demo path and must not be reported.
    UQuest* TransientQuest = NewObject<UQuest>(GetTransientPackage(), UQuest::StaticClass());
    TestTrue(TEXT("A transient quest is accepted"),
        SovCampaignContentValidation::DemoTaleContentReason(TransientQuest).IsEmpty());

    TestTrue(TEXT("A null asset is handled"),
        SovCampaignContentValidation::DemoTaleContentReason(nullptr).IsEmpty());
    return true;
}
#endif
