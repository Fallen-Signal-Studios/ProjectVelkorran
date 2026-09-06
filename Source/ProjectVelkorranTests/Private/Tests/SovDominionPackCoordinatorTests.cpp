// Copyright Fallen Signal Studios. All Rights Reserved.

#include "AI/SovDominionPackCoordinator.h"
#include "Abilities/SovGameplayAbility_DominionHound.h"
#include "Components/SovCommandLinkComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "Misc/AutomationTest.h"
#include "Spawners/NPCSpawner.h"
#include "UObject/UnrealType.h"

#include <type_traits>

static_assert(std::is_base_of_v<AActor, ASovDominionPackCoordinator>);

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSovDominionPackCoordinatorContractTest,
	"ProjectVelkorran.Campaign.DominionHandler.PackCoordinatorContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovDominionPackCoordinatorContractTest::RunTest(
	const FString& Parameters)
{
	const ASovDominionPackCoordinator* Coordinator =
		GetDefault<ASovDominionPackCoordinator>();
	TestNotNull(TEXT("Dominion pack coordinator CDO exists"), Coordinator);
	if (!Coordinator)
	{
		return false;
	}

	TestFalse(
		TEXT("Coordinator does not spend a per-frame actor tick"),
		Coordinator->PrimaryActorTick.bCanEverTick);
	TestFalse(
		TEXT("Coordinator leaves replication to the command-link component"),
		Coordinator->GetIsReplicated());
	TestFalse(
		TEXT("Coordinator CDO begins uninitialized"),
		Coordinator->IsPackInitialized());
	TestFalse(
		TEXT("Coordinator CDO has not failed initialization"),
		Coordinator->HasInitializationFailed());
	TestEqual(
		TEXT("Coordinator CDO has made no runtime attempts"),
		Coordinator->GetInitializationAttemptCount(),
		0);
	TestTrue(
		TEXT("A stable command-link identity must be authored per encounter"),
		Coordinator->GetCommandLinkId() == NAME_None);
	TestEqual(
		TEXT("No Hound spawners are silently inferred by class default"),
		Coordinator->GetConfiguredHoundSpawnerCount(),
		0);
	TestFalse(
		TEXT("Unassigned exact spawner references fail configuration"),
		Coordinator->HasValidSpawnerConfiguration());
	TestTrue(
		TEXT("Readiness retry interval is positive and finite"),
		FMath::IsFinite(Coordinator->GetInitializationRetryInterval())
			&& Coordinator->GetInitializationRetryInterval() >= 0.01f);
	TestTrue(
		TEXT("Readiness attempts are bounded"),
		Coordinator->GetMaximumInitializationAttempts() >= 1);

	const UClass* CoordinatorClass = Coordinator->GetClass();
	const FObjectPropertyBase* HandlerSpawnerProperty =
		FindFProperty<FObjectPropertyBase>(
			CoordinatorClass,
			TEXT("HandlerSpawner"));
	const FArrayProperty* HoundSpawnersProperty =
		FindFProperty<FArrayProperty>(
			CoordinatorClass,
			TEXT("HoundSpawners"));
	const FNameProperty* CommandLinkIdProperty =
		FindFProperty<FNameProperty>(
			CoordinatorClass,
			TEXT("CommandLinkId"));
	TestNotNull(
		TEXT("Coordinator exposes an exact Handler spawner reference"),
		HandlerSpawnerProperty);
	TestNotNull(
		TEXT("Coordinator exposes exact Hound spawner references"),
		HoundSpawnersProperty);
	TestNotNull(
		TEXT("Coordinator exposes a stable command-link identity"),
		CommandLinkIdProperty);

	const auto TestEditInstanceOnly = [this](
		const TCHAR* PropertyDescription,
		const FProperty* Property)
	{
		if (!Property)
		{
			return;
		}
		TestTrue(
			*FString::Printf(TEXT("%s is editable on placed instances"), PropertyDescription),
			Property->HasAnyPropertyFlags(CPF_Edit));
		TestTrue(
			*FString::Printf(TEXT("%s is disabled on class defaults"), PropertyDescription),
			Property->HasAnyPropertyFlags(CPF_DisableEditOnTemplate));
		TestFalse(
			*FString::Printf(TEXT("%s is not disabled on instances"), PropertyDescription),
			Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance));
	};
	TestEditInstanceOnly(TEXT("HandlerSpawner"), HandlerSpawnerProperty);
	TestEditInstanceOnly(TEXT("HoundSpawners"), HoundSpawnersProperty);
	TestEditInstanceOnly(TEXT("CommandLinkId"), CommandLinkIdProperty);

	if (HandlerSpawnerProperty)
	{
		TestTrue(
			TEXT("HandlerSpawner accepts exactly Narrative NPC spawners"),
			HandlerSpawnerProperty->PropertyClass == ANPCSpawner::StaticClass());
	}
	const FObjectPropertyBase* HoundSpawnerInner = HoundSpawnersProperty
		? CastField<FObjectPropertyBase>(HoundSpawnersProperty->Inner)
		: nullptr;
	TestNotNull(
		TEXT("HoundSpawners contains object references"),
		HoundSpawnerInner);
	if (HoundSpawnerInner)
	{
		TestTrue(
			TEXT("Each HoundSpawner entry is exactly a Narrative NPC spawner"),
			HoundSpawnerInner->PropertyClass == ANPCSpawner::StaticClass());
	}

	const UFunction* ConfigureLinkIdFunction =
		USovCommandLinkComponent::StaticClass()->FindFunctionByName(
			TEXT("ConfigureLinkId"));
	TestNotNull(
		TEXT("Command link exposes runtime LinkId configuration"),
		ConfigureLinkIdFunction);
	if (ConfigureLinkIdFunction)
	{
		TestTrue(
			TEXT("Runtime LinkId configuration is Blueprint-callable"),
			ConfigureLinkIdFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable));
		TestTrue(
			TEXT("Runtime LinkId configuration is authority-only"),
			ConfigureLinkIdFunction->HasAnyFunctionFlags(
				FUNC_BlueprintAuthorityOnly));
	}

	UNarrativeAbilitySystemComponent* AbilitySystem =
		NewObject<UNarrativeAbilitySystemComponent>(GetTransientPackage());
	TestNotNull(TEXT("Pack contract test ASC exists"), AbilitySystem);
	if (AbilitySystem)
	{
		TArray<FGameplayAbilitySpec>& Specs =
			AbilitySystem->GetActivatableAbilities();
		Specs.Emplace(USovGameplayAbility_DominionHoundBite::StaticClass());
		bool bSpecMissing = false;
		TestFalse(
			TEXT("A non-Horn Hound ability leaves pack wiring waiting"),
			ASovDominionPackCoordinator::HasExactlyOneReadyHornChargeSpec(
				AbilitySystem,
				bSpecMissing));
		TestTrue(TEXT("The absent Horn Charge is reported as missing"), bSpecMissing);

		const FGameplayAbilitySpec& UniqueHornSpec = Specs.Emplace_GetRef(
			USovGameplayAbility_DominionHoundHornCharge::StaticClass());
		const FGameplayAbilitySpecHandle UniqueHornHandle =
			UniqueHornSpec.Handle;
		TestTrue(
			TEXT("One inactive exact Horn Charge satisfies pack wiring"),
			ASovDominionPackCoordinator::HasExactlyOneReadyHornChargeSpec(
				AbilitySystem,
				bSpecMissing));
		TestFalse(TEXT("A ready Horn Charge is not missing"), bSpecMissing);

		Specs.Emplace(
			USovGameplayAbility_DominionHoundHornCharge::StaticClass());
		TestFalse(
			TEXT("Duplicate exact Horn Charge specs are malformed"),
			ASovDominionPackCoordinator::HasExactlyOneReadyHornChargeSpec(
				AbilitySystem,
				bSpecMissing));
		TestFalse(TEXT("Duplicate Horn Charge is not a startup-order wait"), bSpecMissing);
		Specs.Pop();

		FGameplayAbilitySpec* MutableUniqueHornSpec =
			AbilitySystem->FindAbilitySpecFromHandle(UniqueHornHandle);
		TestNotNull(TEXT("Unique Horn Charge spec remains addressable"), MutableUniqueHornSpec);
		if (MutableUniqueHornSpec)
		{
			MutableUniqueHornSpec->ActiveCount = 1;
			TestFalse(
				TEXT("An active Horn Charge is not pack-start ready"),
				ASovDominionPackCoordinator::HasExactlyOneReadyHornChargeSpec(
					AbilitySystem,
					bSpecMissing));
			TestFalse(TEXT("Active Horn Charge is malformed, not missing"), bSpecMissing);
			MutableUniqueHornSpec->ActiveCount = 0;
			MutableUniqueHornSpec->PendingRemove = true;
			TestFalse(
				TEXT("A pending-removal Horn Charge is malformed pack setup"),
				ASovDominionPackCoordinator::HasExactlyOneReadyHornChargeSpec(
					AbilitySystem,
					bSpecMissing));
			TestFalse(TEXT("Pending removal is malformed, not missing"), bSpecMissing);
			MutableUniqueHornSpec->PendingRemove = false;
			MutableUniqueHornSpec->RemoveAfterActivation = true;
			TestFalse(
				TEXT("A remove-after-activation Horn Charge is not a stable pack member"),
				ASovDominionPackCoordinator::HasExactlyOneReadyHornChargeSpec(
					AbilitySystem,
					bSpecMissing));
			TestFalse(TEXT("Remove-after-activation is malformed, not missing"), bSpecMissing);
		}
	}

	return true;
}

#endif
