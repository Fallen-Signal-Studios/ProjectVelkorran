// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Validation/SovCampaignWorldValidation.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignHandoffAnchor.h"
#include "Diagnostics/SovDiagnosticsSubsystem.h"
#include "Tests/SovSettingsTestFixtures.h"
#include "Tests/SovBuildValidationTestFixtures.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Items/VendorInventoryComponent.h"
#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"
#include "World/SovWorldTransitActor.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEchoAuthoredValidation, "ProjectVelkorran.Campaign.Validation.RawEchoDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEchoAuthoredValidation::RunTest(const FString& Parameters)
{
	auto* Ability = NewObject<USovEchoValidationTestAbility>(); FString Error;
	TestTrue(TEXT("Complete native authored contract is accepted"), Ability->ValidateAuthoredConfiguration(Error));
	Ability->SetRawCost(-1.f);
	TestEqual(TEXT("Runtime getter would conceal negative raw cost"), Ability->GetEchoCost(), 0.f);
	TestFalse(TEXT("Authoring validator rejects before clamping"), Ability->ValidateAuthoredConfiguration(Error));
	Ability->SetRawCost(0.f); Ability->SetRawThreshold(std::numeric_limits<float>::quiet_NaN());
	TestFalse(TEXT("Nonfinite raw threshold rejected"), Ability->ValidateAuthoredConfiguration(Error));
	Ability->SetRawThreshold(0.f); Ability->RequireAuthoredWeapon(true);
	TestFalse(TEXT("Unconfigured weapon gate rejected before play"), Ability->ValidateAuthoredConfiguration(Error));
	Ability->RequireAuthoredWeapon(false); Ability->bPayloadConfigured = false;
	TestFalse(TEXT("Concrete payload validator is actually called"), Ability->ValidateAuthoredConfiguration(Error));
	Ability->bPayloadConfigured = true; Ability->SetTestIdentity(FGameplayTag());
	TestFalse(TEXT("Missing protagonist identity rejected"), Ability->ValidateAuthoredConfiguration(Error));
	return true;
}
namespace SovBuildValidationTests
{
	struct FWorld
	{
		UWorld* World = nullptr;
		FWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
		}
		~FWorld() { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
		template<typename T> T* Spawn()
		{
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World ? World->SpawnActor<T>(T::StaticClass(), FTransform::Identity, Params) : nullptr;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlacedWorldValidation, "ProjectVelkorran.Campaign.Validation.PlacedWorldContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPlacedWorldValidation::RunTest(const FString& Parameters)
{
	SovBuildValidationTests::FWorld Fixture;
	if (!Fixture.World) { AddError(TEXT("World fixture failed")); return false; }
	auto* Mission = NewObject<USovCampaignDefinition>(Fixture.World);
	Mission->MissionId = TEXT("ValidationMission"); Mission->EntryPlayerStartTag = TEXT("Entry");
	auto* Start = Fixture.Spawn<APlayerStart>(); auto* Other = Fixture.Spawn<APlayerStart>();
	if (!Start || !Other) { AddError(TEXT("PlayerStart fixture failed")); return false; }
	Start->PlayerStartTag = TEXT("Entry"); Other->PlayerStartTag = TEXT("Other");
	TestEqual(TEXT("Minimal authored map passes pure configuration checks"), SovCampaignWorldValidation::Validate(Fixture.World, Mission, true), 0);
	Other->PlayerStartTag = TEXT("Entry");
	AddExpectedError(TEXT("[WORLD.ENTRY_START]"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("Duplicate entry tag is rejected"), SovCampaignWorldValidation::Validate(Fixture.World, Mission, true), 1);
	Other->PlayerStartTag = TEXT("Other");
	FSovCampaignBeatDefinition Beat; Beat.BeatId = TEXT("Handoff"); Beat.RequiredHandoffAnchorId = TEXT("Anchor");
	Beat.HandoffToProtagonist = FSovGameplayTags::Get().Character_Player_Selene; Mission->Beats.Add(Beat);
	AddExpectedError(TEXT("[WORLD.MISSING_HANDOFF]"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("Required physical handoff must exist"), SovCampaignWorldValidation::Validate(Fixture.World, Mission, true), 1);
	auto* Anchor = Fixture.Spawn<ASovCampaignHandoffAnchor>();
	if (!Anchor) { AddError(TEXT("Anchor fixture failed")); return false; }
	Anchor->AnchorId = TEXT("Anchor"); Anchor->MissionId = Mission->MissionId; Anchor->HandoffBeat = Beat.BeatId;
	TestEqual(TEXT("Correct physical handoff resolves the failure"), SovCampaignWorldValidation::Validate(Fixture.World, Mission, true), 0);
	auto* Vendor = NewObject<UVendorInventoryComponent>(Anchor); Anchor->AddInstanceComponent(Vendor);
	AddExpectedError(TEXT("[WORLD.PROHIBITED_VENDOR]"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("Renamed vendor content cannot evade class validation"), SovCampaignWorldValidation::Validate(Fixture.World, Mission, true), 1);
	Anchor->RemoveInstanceComponent(Vendor);
	auto* FirstTransit = Fixture.Spawn<ASovWorldTransitActor>(); auto* SecondTransit = Fixture.Spawn<ASovWorldTransitActor>();
	if (!FirstTransit || !SecondTransit) { AddError(TEXT("Transit fixture failed")); return false; }
	FirstTransit->TransitId = TEXT("DuplicateTransit"); SecondTransit->TransitId = FirstTransit->TransitId;
	SecondTransit->SetActorGUID_Implementation(FirstTransit->GetActorGUID_Implementation());
	AddExpectedError(TEXT("[WORLD.STABLE_GUID]"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("[WORLD.TRANSIT_ID]"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("Duplicate save identity and transit contract are both rejected"), SovCampaignWorldValidation::Validate(Fixture.World, Mission, true), 2);
	return true;
}

struct FSovDiagnosticsTestAccess
{
	static void SetSettings(USovDiagnosticsSubsystem& Diagnostics, USovGameUserSettings* Settings) { Diagnostics.BoundSettings = Settings; }
	static void Bind(USovDiagnosticsSubsystem& Diagnostics, UNarrativeAbilitySystemComponent* ASC)
	{
		Diagnostics.BoundASC = ASC;
		ASC->OnDamageResolvedAsTarget.AddDynamic(&Diagnostics, &USovDiagnosticsSubsystem::HandleDamage);
	}
	static bool IsBound(const USovDiagnosticsSubsystem& Diagnostics) { return Diagnostics.BoundASC.IsValid(); }
	static void Append(USovDiagnosticsSubsystem& Diagnostics) { Diagnostics.Append(FSovDiagnosticRecord()); }
	static void SettingsChanged(USovDiagnosticsSubsystem& Diagnostics) { Diagnostics.HandleSettings(FSovUserSettingsSnapshot()); }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDiagnosticsOptOut, "ProjectVelkorran.Campaign.Validation.DiagnosticsOptOutReleasesBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDiagnosticsOptOut::RunTest(const FString& Parameters)
{
	SovBuildValidationTests::FWorld Fixture;
	if (!Fixture.World) { AddError(TEXT("World fixture failed")); return false; }
	auto* Settings = NewObject<USovSettingsTestSettings>();
	auto* Diagnostics = NewObject<USovDiagnosticsSubsystem>(Fixture.World);
	auto* Owner = Fixture.Spawn<AActor>();
	if (!Owner) { AddError(TEXT("ASC owner fixture failed")); return false; }
	auto* ASC = NewObject<UNarrativeAbilitySystemComponent>(Owner);
	FSovDiagnosticsTestAccess::SetSettings(*Diagnostics, Settings);
	Settings->SetLocalDiagnosticsEnabled(true);
	FSovDiagnosticsTestAccess::Bind(*Diagnostics, ASC);
	FSovDiagnosticsTestAccess::Append(*Diagnostics);
	TestEqual(TEXT("Enabled recorder accepts one native event"), Diagnostics->GetRecentRecords().Num(), 1);
	Settings->SetLocalDiagnosticsEnabled(false);
	FSovDiagnosticsTestAccess::SettingsChanged(*Diagnostics);
	TestFalse(TEXT("Opt-out releases its ASC"), FSovDiagnosticsTestAccess::IsBound(*Diagnostics));
	TestFalse(TEXT("Opt-out removes its real damage delegate"), ASC->OnDamageResolvedAsTarget.IsBound());
	TestTrue(TEXT("Opt-out clears retained events"), Diagnostics->GetRecentRecords().IsEmpty());
	TestFalse(TEXT("Disabled recorder has no ticking work"), Diagnostics->IsTickable());
	FSovDiagnosticsTestAccess::Append(*Diagnostics);
	TestTrue(TEXT("Late event cannot restart disabled recording"), Diagnostics->GetRecentRecords().IsEmpty());
	return true;
}
#endif
