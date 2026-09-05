// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovLegacyCorruptionComponent.h"
#include "Components/SovStatusComponent.h"
#include "GAS/SovCombatTypes.h"
#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"
#include "Status/SovStatusDefinition.h"

#include <type_traits>

static_assert(std::is_same_v<
	std::underlying_type_t<ESovStatusApplicationResult>,
	uint8>);
static_assert(std::is_same_v<
	std::underlying_type_t<ESovLegacyCorruptionBand>,
	uint8>);

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSovStatusDefinitionContractTest,
	"ProjectVelkorran.Campaign.Status.DefinitionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovStatusDefinitionContractTest::RunTest(const FString& Parameters)
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	USovStatusDefinition* Definition = NewObject<USovStatusDefinition>();
	TestNotNull(TEXT("A status definition can be created"), Definition);
	if (!Definition)
	{
		return false;
	}

	TestFalse(
		TEXT("An unauthored status definition fails closed"),
		Definition->IsStructurallyValid());

	Definition->RequestTag = Tags.Status_Apply_Burn;
	Definition->StateTag = Tags.State_Status_Burning;
	Definition->DisplayName = FText::FromString(TEXT("Automation Burn"));
	Definition->DefaultDuration = 5.0f;
	Definition->CleanseTags.AddTag(Tags.Status_Cleanse_Burn);
	Definition->UIPriority = 50;
	Definition->PresentationTag = Tags.Status_Apply_Burn;
	Definition->AccessibilityPresentationTag = Tags.State_Status_Burning;
	TestTrue(
		TEXT("A fully authored timed status is structurally valid"),
		Definition->IsStructurallyValid());

	Definition->bHardCrowdControl = true;
	TestFalse(
		TEXT("Hard control requires an authored recovery-immunity window"),
		Definition->IsStructurallyValid());
	Definition->RecoveryImmunityTag = Tags.Status_Immunity_Freeze;
	Definition->RecoveryImmunityDuration = 1.5f;
	TestTrue(
		TEXT("Hard control becomes valid with recovery immunity"),
		Definition->IsStructurallyValid());

	const FSovStatusCheckpointState EmptyCheckpoint;
	TestEqual(
		TEXT("Status checkpoint schema starts at version one"),
		EmptyCheckpoint.SchemaVersion,
		1);
	TestTrue(
		TEXT("A new status checkpoint contains no runtime records"),
		EmptyCheckpoint.Statuses.IsEmpty());

	const FSovStatusApplicationRequest EmptyRequest;
	TestFalse(
		TEXT("An unauthored status request has no transaction identity"),
		EmptyRequest.RequestId.IsValid());
	TestTrue(
		TEXT("Burn request and state identities remain distinct"),
		Tags.Status_Apply_Burn != Tags.State_Status_Burning);
	TestTrue(
		TEXT("Global and family-specific cleanses remain distinct"),
		Tags.Status_Cleanse_All != Tags.Status_Cleanse_Burn);
	TestTrue(
		TEXT("Status application has one aggregate compatibility event"),
		Tags.Event_Status_ApplicationRequested.IsValid());
	const FSovStatusPresentationEntry PresentationEntry;
	TestEqual(
		TEXT("Replicated status presentation starts with one semantic stack"),
		PresentationEntry.StackCount,
		1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSovCorruptionPrototypeContractTest,
	"ProjectVelkorran.Campaign.Corruption.PrototypeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovCorruptionPrototypeContractTest::RunTest(const FString& Parameters)
{
	const USovLegacyCorruptionComponent* Corruption =
		GetDefault<USovLegacyCorruptionComponent>();
	TestNotNull(TEXT("Corruption component CDO exists"), Corruption);
	if (!Corruption)
	{
		return false;
	}

	TestTrue(
		TEXT("Zero exposure is the internal None state"),
		Corruption->DetermineBandForExposure(0.0f)
			== ESovLegacyCorruptionBand::None);
	TestTrue(
		TEXT("Positive sub-threshold exposure is Trace"),
		Corruption->DetermineBandForExposure(1.0f)
			== ESovLegacyCorruptionBand::Trace);
	TestTrue(
		TEXT("Prototype Intrusion begins at 25 percent"),
		Corruption->DetermineBandForExposure(25.0f)
			== ESovLegacyCorruptionBand::Intrusion);
	TestTrue(
		TEXT("Prototype Contest begins at 55 percent"),
		Corruption->DetermineBandForExposure(55.0f)
			== ESovLegacyCorruptionBand::Contest);
	TestTrue(
		TEXT("Overwrite Risk is impossible without mission and source authorization"),
		Corruption->DetermineBandForExposure(100.0f)
			== ESovLegacyCorruptionBand::Contest);
	TestFalse(
		TEXT("Overwrite Risk mission permission defaults off"),
		Corruption->DoesMissionAllowOverwriteRisk());

	FSovCorruptionSourceSpec SourceSpec;
	TestFalse(
		TEXT("An unauthored corruption source fails closed"),
		SourceSpec.HasValidNumbers());
	SourceSpec.SourceId = TEXT("Automation.UniqueSource");
	SourceSpec.RemedyTag =
		FSovGameplayTags::Get().Status_Cleanse_Corruption;
	SourceSpec.RemedyInstruction = FText::FromString(
		TEXT("Leave the field or reach the purge point."));
	SourceSpec.PresentationProfile = TEXT("Automation.Corruption");
	SourceSpec.AccessibilitySubstitute = FText::FromString(
		TEXT("Directional warning, meter, and named corruption band."));
	TestTrue(
		TEXT("A complete prototype corruption source contract is valid"),
		SourceSpec.HasValidNumbers());

	SourceSpec.FalloffPolicy = ESovCorruptionFalloffPolicy::Linear;
	SourceSpec.InnerRadius = 500.0f;
	SourceSpec.OuterRadius = 500.0f;
	TestFalse(
		TEXT("Linear falloff requires an outer radius beyond its inner radius"),
		SourceSpec.HasValidNumbers());
	SourceSpec.FalloffPolicy = ESovCorruptionFalloffPolicy::None;
	SourceSpec.ExposurePerSecond = 0.0f;
	SourceSpec.InstantExposure = 0.0f;
	TestFalse(
		TEXT("A source with no continuous or instant exposure fails closed"),
		SourceSpec.HasValidNumbers());
	SourceSpec.ExposurePerSecond = 5.0f;
	SourceSpec.BandCap = static_cast<ESovLegacyCorruptionBand>(255);
	TestFalse(
		TEXT("An invalid corruption band cap fails closed"),
		SourceSpec.HasValidNumbers());

	const FSovCorruptionCheckpointData EmptyCheckpoint;
	TestEqual(
		TEXT("Corruption checkpoint schema starts at the current version"),
		EmptyCheckpoint.Version,
		FSovCorruptionCheckpointData::CurrentVersion);
	TestTrue(
		TEXT("Corruption checkpoint stores semantic source IDs, not handles"),
		EmptyCheckpoint.ActiveSourceIds.IsEmpty());
	TestTrue(
		TEXT("Corruption checkpoint starts with no canon-persistent source IDs"),
		EmptyCheckpoint.CanonPersistentSourceIds.IsEmpty());
	const FSovCorruptionPresentationSnapshot Presentation;
	TestTrue(
		TEXT("Reduced presentation is explicitly gameplay-equivalent"),
		Presentation.bGameplayEquivalentInReducedEffects);

	return true;
}

#endif
