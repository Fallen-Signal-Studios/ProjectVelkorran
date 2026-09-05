// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAccountPreferenceRepairTestFixtures.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "Misc/SecureHash.h"
#include "Containers/Ticker.h"

#if WITH_DEV_AUTOMATION_TESTS
struct FSovAccountSettingsTestAccess
{
    static void Observe(USovGameUserSettings* S,const FString& Owner,bool Authorized=true) { S->ObserveVerifiedSettingsOwner(Owner,0,Authorized); }
    static void Suspend(USovGameUserSettings* S,bool Suspended) { S->SetPlatformSettingsSuspended(Suspended); }
    static void SeedInput(USovGameUserSettings* S,const TArray<uint8>& Input) { S->AccountInputProfile=Input; }
    static const TArray<uint8>& Input(USovGameUserSettings* S) { return S->AccountInputProfile; }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAccountPreferencesIsolationRepair,"ProjectVelkorran.UI.AccountPreferences.IsolationAndVerifiedRollback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovAccountPreferencesIsolationRepair::RunTest(const FString&)
{
    auto* S=NewObject<USovAccountPreferenceRepairSettings>(); FString Error;
    FSovAccountSettingsTestAccess::Observe(S,TEXT("VerifiedA"));
    auto A=S->GetSettingsSnapshot(); A.UIScale=1.75f; A.bMenuNarration=true;
    TestTrue(TEXT("A commits through native framed preference banks"),S->ApplySettingsSnapshot(A,Error));
    TestTrue(TEXT("A explicitly finishes setup"),S->CompleteAccessibilitySetup());
    TestEqual(TEXT("Global device config receives neutral accessibility values"),S->DeviceSavedUIScale,1.f);
    FSovAccountSettingsTestAccess::Observe(S,TEXT("VerifiedB"));
    TestEqual(TEXT("B does not inherit A's scale"),S->GetSettingsSnapshot().UIScale,1.f);
    TestFalse(TEXT("B does not inherit A's first-boot completion"),S->HasCompletedAccessibilitySetup());
    auto B=S->GetSettingsSnapshot(); B.UIScale=1.25f;
    TestTrue(TEXT("B commits independent bank"),S->ApplySettingsSnapshot(B,Error));
    FSovAccountSettingsTestAccess::Observe(S,TEXT("VerifiedA"));
    TestEqual(TEXT("Returning A restores A's highest verified bank"),S->GetSettingsSnapshot().UIScale,1.75f);
    TestTrue(TEXT("Returning A restores setup completion"),S->HasCompletedAccessibilitySetup());
    S->bRejectWrites=true; A.UIScale=2.f;
    TestFalse(TEXT("Rejected native write reports failure"),S->ApplySettingsSnapshot(A,Error));
    TestEqual(TEXT("Failed write restores committed A values"),S->GetSettingsSnapshot().UIScale,1.75f);
    FSovAccountSettingsTestAccess::Observe(S,TEXT("VerifiedA"),false);
    TestEqual(TEXT("Revocation applies neutral values"),S->GetSettingsSnapshot().UIScale,1.f);
    TestFalse(TEXT("Revoked owner cannot mark setup complete"),S->CompleteAccessibilitySetup());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAccountPreferencesReentrantRepair,"ProjectVelkorran.UI.AccountPreferences.OwnerChangeDuringIOAndResume",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovAccountPreferencesReentrantRepair::RunTest(const FString&)
{
    auto* S=NewObject<USovAccountPreferenceRepairSettings>(); FString Error;
    FSovAccountSettingsTestAccess::Observe(S,TEXT("B")); auto B=S->GetSettingsSnapshot(); B.UIScale=1.5f; S->ApplySettingsSnapshot(B,Error);
    FSovAccountSettingsTestAccess::Observe(S,TEXT("A")); auto A=S->GetSettingsSnapshot(); A.UIScale=2.f;
    S->DuringWrite=[S]() { FSovAccountSettingsTestAccess::Observe(S,TEXT("B")); };
    TestFalse(TEXT("A's write cannot report B's transaction as successful"),S->ApplySettingsSnapshot(A,Error));
    FTSTicker::GetCoreTicker().Tick(.01f);
    TestTrue(TEXT("Stable B loads after A's I/O unwinds, without a second account event"),S->AreAccountPreferencesReady());
    TestEqual(TEXT("Deferred B load restores B rather than old A"),S->GetSettingsSnapshot().UIScale,1.5f);
    for(int32 Cycle=0;Cycle<3;++Cycle)
    {
        const int32 Writes=S->Writes; FSovAccountSettingsTestAccess::Suspend(S,true);
        B.UIScale=1.75f; TestFalse(TEXT("Suspended preference writes are rejected"),S->ApplySettingsSnapshot(B,Error));
        TestEqual(TEXT("Suspension initiates no disk write"),S->Writes,Writes);
        FSovAccountSettingsTestAccess::Suspend(S,false);
        TestEqual(TEXT("Repeated resume restores committed owner preferences"),S->GetSettingsSnapshot().UIScale,1.5f);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInputAccountProfileRepair,"ProjectVelkorran.UI.AccountPreferences.NativeInputProfilesAndRegistrationReentry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FSovInputAccountProfileRepair::RunTest(const FString&)
{
    auto* Local=NewObject<ULocalPlayer>(); auto* Input=NewObject<USovAccountInputRepairSettings>(); Input->Stage(Local);
    auto* ContextA=NewObject<UInputMappingContext>(); auto* ContextB=NewObject<UInputMappingContext>();
    Input->RegisterInputMappingContext(ContextA); Input->RegisterInputMappingContext(ContextB);
    Input->StageSensitivity(2.f); TArray<uint8> ProfileA;
    TestTrue(TEXT("Capture actual Enhanced Input profile"),Input->CaptureAccountProfile(ProfileA));
    const TArray<uint8> Defaults; TestTrue(TEXT("B applies fresh native default profile"),Input->ApplyAccountProfile(Defaults));
    TestEqual(TEXT("B does not inherit sensitivity"),Input->GetAimSensitivity(),1.f);
    TestTrue(TEXT("A restores native profile bytes"),Input->ApplyAccountProfile(ProfileA));
    TestEqual(TEXT("A's sensitivity round trips"),Input->GetAimSensitivity(),2.f);
    // Trailing garbage is rejected by the bounded native-version frame before
    // live registrations are mutated.
    auto Trailing=ProfileA; Trailing.Add(0xff);
    TestFalse(TEXT("Trailing payload rejected"),Input->ApplyAccountProfile(Trailing));
    TestTrue(TEXT("Failed profile keeps both registered contexts for neutral recovery"),Input->IsMappingContextRegistered(ContextA) && Input->IsMappingContextRegistered(ContextB));
    TestTrue(TEXT("Neutral fallback remains usable"),Input->ApplyAccountProfile(Defaults));
    bool bReplacementApplied=false;
    Input->DuringRegister=[&]() { bReplacementApplied=Input->ApplyAccountProfile(Defaults); };
    TestFalse(TEXT("Nested owner replacement invalidates outer profile apply"),Input->ApplyAccountProfile(ProfileA));
    TestTrue(TEXT("Replacement completes"),bReplacementApplied);
    TestEqual(TEXT("Replacement sensitivity survives outer continuation"),Input->GetAimSensitivity(),1.f);
    TestTrue(TEXT("Nested replacement inherits full outer registration baseline"),Input->IsMappingContextRegistered(ContextA) && Input->IsMappingContextRegistered(ContextB));
    auto* S=NewObject<USovAccountPreferenceRepairSettings>(); FString Error;
    FSovAccountSettingsTestAccess::Observe(S,TEXT("InputA")); FSovAccountSettingsTestAccess::SeedInput(S,ProfileA); S->SaveSettings();
    FSovAccountSettingsTestAccess::Observe(S,TEXT("InputB")); TestTrue(TEXT("B has no inherited saved input payload"),FSovAccountSettingsTestAccess::Input(S).IsEmpty());
    FSovAccountSettingsTestAccess::Observe(S,TEXT("InputA"));
    TestTrue(TEXT("A's actual native input bytes are restored by the account preference producer"),Input->ApplyAccountProfile(FSovAccountSettingsTestAccess::Input(S)));
    TestEqual(TEXT("Account-bank to input-consumer round trip restores A"),Input->GetAimSensitivity(),2.f);
    return true;
}
#endif
