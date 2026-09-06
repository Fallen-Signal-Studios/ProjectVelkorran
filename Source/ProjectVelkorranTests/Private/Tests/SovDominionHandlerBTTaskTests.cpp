// Copyright Fallen Signal Studios. All Rights Reserved.

#include "AI/BTTask_SovDominionHandlerCommandHound.h"

#include "Abilities/SovGameplayAbility_DominionHandler.h"
#include "Abilities/SovGameplayAbility_DominionHound.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayAbilitySpec.h"
#include "Misc/AutomationTest.h"

#include <type_traits>

static_assert(std::is_base_of_v<
	UBTTaskNode,
	UBTTask_SovDominionHandlerCommandHound>);

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSovDominionHandlerBTTaskContractTest,
	"ProjectVelkorran.Campaign.DominionHandler.BTTaskContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovDominionHandlerBTTaskContractTest::RunTest(
	const FString& Parameters)
{
	const UBTTask_SovDominionHandlerCommandHound* Task =
		GetDefault<UBTTask_SovDominionHandlerCommandHound>();
	const USovGameplayAbility_DominionHandlerCommandHound* HandlerCommand =
		GetDefault<USovGameplayAbility_DominionHandlerCommandHound>();
	const USovGameplayAbility_DominionHoundBite* HoundBite =
		GetDefault<USovGameplayAbility_DominionHoundBite>();
	const USovGameplayAbility_DominionHoundHornCharge* HoundHornCharge =
		GetDefault<USovGameplayAbility_DominionHoundHornCharge>();
	const USovGameplayAbility_DominionHoundPounce* HoundPounce =
		GetDefault<USovGameplayAbility_DominionHoundPounce>();

	TestNotNull(TEXT("Handler command BT task CDO exists"), Task);
	TestNotNull(TEXT("Handler command ability CDO exists"), HandlerCommand);
	TestNotNull(TEXT("Dominion Hound Bite CDO exists"), HoundBite);
	TestNotNull(TEXT("Dominion Hound Horn Charge CDO exists"), HoundHornCharge);
	TestNotNull(TEXT("Dominion Hound Pounce CDO exists"), HoundPounce);
	if (!Task || !HandlerCommand)
	{
		return false;
	}

	TestTrue(
		TEXT("Handler command BT task requests a per-AI node instance"),
		Task->HasInstance());
	TestTrue(
		TEXT("Handler command BT task ignores duplicate self-restarts"),
		Task->ShouldIgnoreRestartSelf());
	TestTrue(
		TEXT("Failure backoff is positive and finite"),
		FMath::IsFinite(Task->GetFailureBackoffSeconds())
			&& Task->GetFailureBackoffSeconds() > 0.0f);
	TestTrue(
		TEXT("Default failure backoff is shorter than the native success cooldown"),
		Task->GetFailureBackoffSeconds()
			< HandlerCommand->GetCommandCooldownDuration());

	TestTrue(
		TEXT("Task accepts the exact Handler command class and identity"),
		UBTTask_SovDominionHandlerCommandHound::
			IsExactCommandAbilityDefinition(HandlerCommand));
	TestFalse(
		TEXT("Task rejects a null ability definition"),
		UBTTask_SovDominionHandlerCommandHound::
			IsExactCommandAbilityDefinition(nullptr));
	TestFalse(
		TEXT("Task rejects Dominion Hound Bite"),
		UBTTask_SovDominionHandlerCommandHound::
			IsExactCommandAbilityDefinition(HoundBite));
	TestFalse(
		TEXT("Task rejects Dominion Hound Horn Charge"),
		UBTTask_SovDominionHandlerCommandHound::
			IsExactCommandAbilityDefinition(HoundHornCharge));
	TestFalse(
		TEXT("Task rejects Dominion Hound Pounce"),
		UBTTask_SovDominionHandlerCommandHound::
			IsExactCommandAbilityDefinition(HoundPounce));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSovDominionHandlerBTTaskExactSpecSelectionTest,
	"ProjectVelkorran.Campaign.DominionHandler.BTTaskExactSpecSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovDominionHandlerBTTaskExactSpecSelectionTest::RunTest(
	const FString& Parameters)
{
	using ESelection = ESovDominionHandlerCommandSpecSelection;
	FGameplayAbilitySpecHandle SelectedHandle;
	UGameplayAbility* SelectedAbility = nullptr;
	TArray<FGameplayAbilitySpec> Specs;

	auto Select = [&]()
	{
		return UBTTask_SovDominionHandlerCommandHound::
			SelectExactCommandAbilitySpec(
				MakeArrayView(Specs),
				SelectedHandle,
				SelectedAbility);
	};

	TestTrue(
		TEXT("An empty ability list has no command spec"),
		Select() == ESelection::Missing && !SelectedHandle.IsValid());

	FGameplayAbilitySpec HoundSpec(
		USovGameplayAbility_DominionHoundHornCharge::StaticClass());
	Specs.Add(HoundSpec);
	TestTrue(
		TEXT("Hound Horn Charge is not the Handler command spec"),
		Select() == ESelection::Missing && !SelectedHandle.IsValid());

	const USovGameplayAbility_DominionHandlerCommandHound* HandlerCommand =
		GetDefault<USovGameplayAbility_DominionHandlerCommandHound>();
	TestNotNull(TEXT("Handler command ability CDO exists"), HandlerCommand);
	if (!HandlerCommand)
	{
		return false;
	}

	// A dynamic source tag must not let an unrelated ability impersonate the
	// command's native class plus asset-identity contract.
	Specs[0].GetDynamicSpecSourceTags().AddTag(
		HandlerCommand->GetHandlerCommandAbilityTag());
	TestTrue(
		TEXT("A dynamic identity-tag spoof is rejected"),
		Select() == ESelection::Missing && !SelectedHandle.IsValid());

	FGameplayAbilitySpec CommandSpec(
		USovGameplayAbility_DominionHandlerCommandHound::StaticClass());
	const FGameplayAbilitySpecHandle ExpectedCommandHandle = CommandSpec.Handle;
	Specs.Add(CommandSpec);
	TestTrue(
		TEXT("One exact inactive command spec is selected"),
		Select() == ESelection::UniqueInactive
			&& SelectedHandle == ExpectedCommandHandle
			&& SelectedAbility == HandlerCommand);

	Specs.Last().ActiveCount = 1;
	TestTrue(
		TEXT("An already-active command spec is unavailable, not adopted"),
		Select() == ESelection::Unavailable
			&& !SelectedHandle.IsValid());
	Specs.Last().ActiveCount = 0;

	Specs.Add(FGameplayAbilitySpec(
		USovGameplayAbility_DominionHandlerCommandHound::StaticClass()));
	TestTrue(
		TEXT("Duplicate exact command specs fail closed as ambiguous"),
		Select() == ESelection::Ambiguous
			&& !SelectedHandle.IsValid());

	return true;
}

#endif
