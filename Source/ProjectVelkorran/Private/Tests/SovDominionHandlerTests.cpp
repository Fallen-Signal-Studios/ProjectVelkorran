// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_DominionHandler.h"
#include "Abilities/SovGameplayAbility_DominionHound.h"
#include "Characters/SovDominionHandler.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Components/SovCommandLinkComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeCombatAbility.h"
#include "GameplayAbilitySpec.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

#include <type_traits>

static_assert(std::is_base_of_v<ASovNPCCharacterBase, ASovDominionHandler>);
static_assert(std::is_base_of_v<
	UNarrativeCombatAbility,
	USovGameplayAbility_DominionHandlerCommandHound>);

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSovDominionHandlerProfileContractTest,
	"ProjectVelkorran.Campaign.DominionHandler.ProfileContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovDominionHandlerProfileContractTest::RunTest(
	const FString& Parameters)
{
	const ASovDominionHandler* Handler = GetDefault<ASovDominionHandler>();
	const USovGameplayAbility_DominionHandlerCommandHound* CommandAbility =
		GetDefault<USovGameplayAbility_DominionHandlerCommandHound>();

	TestNotNull(TEXT("Dominion Handler CDO exists"), Handler);
	TestNotNull(TEXT("Handler command ability CDO exists"), CommandAbility);
	if (!Handler || !CommandAbility)
	{
		return false;
	}

	USovCommandLinkComponent* CommandLink =
		Handler->GetCommandLinkComponent();
	TestNotNull(
		TEXT("Dominion Handler owns a native command-link component"),
		CommandLink);
	TestNotNull(
		TEXT("Dominion Handler retains inherited dismemberment support"),
		Handler->GetDismembermentComponent());
	TestNotNull(
		TEXT("Dominion Handler retains inherited combat-sustain drops"),
		Handler->GetCombatSustainDropComponent());
	TArray<USovCommandLinkComponent*> CommandLinkComponents;
	Handler->GetComponents(CommandLinkComponents);
	TestEqual(
		TEXT("Dominion Handler owns exactly one command-link component"),
		CommandLinkComponents.Num(),
		1);
	if (CommandLinkComponents.Num() == 1)
	{
		TestTrue(
			TEXT("The public command-link getter returns the owned component"),
			CommandLink == CommandLinkComponents[0]);
	}
	if (CommandLink)
	{
		TestTrue(
			TEXT("Handler command link CDO begins inactive"),
			CommandLink->GetCommandLinkState()
				== ESovCommandLinkState::Inactive);
		TestFalse(
			TEXT("Handler command link CDO has no authored encounter identity"),
			CommandLink->HasValidCommandLinkConfiguration());
		TestTrue(
			TEXT("Handler command link includes its owner as a participant"),
			CommandLink->IncludesOwnerAsParticipant());
	}

	TestTrue(
		TEXT("Maximum Handler-to-Hound command distance is positive and finite"),
		FMath::IsFinite(Handler->GetMaximumCommandDistance())
			&& Handler->GetMaximumCommandDistance() > 0.0f);
	TestTrue(
		TEXT("Handler command distance retains the prototype default"),
		FMath::IsNearlyEqual(Handler->GetMaximumCommandDistance(), 2500.0f));
	TestTrue(
		TEXT("Handler command requires line of sight by default"),
		Handler->RequiresCommandLineOfSight());
	TestTrue(
		TEXT("Handler CDO begins without a last commanded Hound"),
		Handler->GetLastCommandedHound() == nullptr);

	const UFunction* AnticipationMulticast = Handler->FindFunction(
		TEXT("MulticastPresentHoundHornChargeAnticipation"));
	const UFunction* SuccessMulticast = Handler->FindFunction(
		TEXT("MulticastPresentHoundHornChargeOrder"));
	TestNotNull(
		TEXT("Handler exposes its replicated anticipation cue"),
		AnticipationMulticast);
	TestNotNull(
		TEXT("Handler exposes its replicated successful-order cue"),
		SuccessMulticast);
	if (AnticipationMulticast)
	{
		TestTrue(
			TEXT("Anticipation cue is a multicast RPC"),
			AnticipationMulticast->HasAnyFunctionFlags(FUNC_NetMulticast));
		TestTrue(
			TEXT("Anticipation cue is reliable"),
			AnticipationMulticast->HasAnyFunctionFlags(FUNC_NetReliable));
	}
	if (SuccessMulticast)
	{
		TestTrue(
			TEXT("Successful-order cue is a multicast RPC"),
			SuccessMulticast->HasAnyFunctionFlags(FUNC_NetMulticast));
		TestFalse(
			TEXT("Successful-order cue remains non-blocking/unreliable"),
			SuccessMulticast->HasAnyFunctionFlags(FUNC_NetReliable));
	}

	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	const FNarrativeGameplayTags& NarrativeTags =
		FNarrativeGameplayTags::Get();
	TestTrue(
		TEXT("Handler command identity tag is registered"),
		SovTags.Ability_NPC_DominionHandler_CommandHound.IsValid());
	TestTrue(
		TEXT("Transient Hound order authorization tag is registered"),
		SovTags.State_CommandLink_HoundChargeAuthorized.IsValid());
	TestTrue(
		TEXT("Hound order authorization is distinct from durable link state"),
		SovTags.State_CommandLink_HoundChargeAuthorized
			!= SovTags.State_CommandLink_Active
			&& SovTags.State_CommandLink_HoundChargeAuthorized
				!= SovTags.State_CommandLink_Severed);
	TestTrue(
		TEXT("Handler command carries its stable identity"),
		CommandAbility->GetHandlerCommandAbilityTag()
			== SovTags.Ability_NPC_DominionHandler_CommandHound);
	TestTrue(
		TEXT("Handler command publishes its identity through GAS asset tags"),
		CommandAbility->GetAssetTags().HasTagExact(
			SovTags.Ability_NPC_DominionHandler_CommandHound));
	TestTrue(
		TEXT("Handler command identity is distinct from Hound Horn Charge"),
		SovTags.Ability_NPC_DominionHandler_CommandHound
			!= SovTags.Ability_NPC_DominionHound_HornCharge);
	TestTrue(
		TEXT("Handler command uses Ability 1"),
		CommandAbility->GetConfiguredInputTag()
			== NarrativeTags.Narrative_Input_Ability1);
	TestTrue(
		TEXT("Handler command explicitly requires an active command link"),
		CommandAbility->RequiresActiveCommandLink());
	TestTrue(
		TEXT("Handler command GAS requirements contain the active-link tag"),
		CommandAbility->HasActiveCommandLinkActivationRequirement());
	TestTrue(
		TEXT("Handler command GAS blockers contain the Severed tag"),
		CommandAbility->BlocksCommandLinkSeverAtActivation());
	TestTrue(
		TEXT("Handler command GAS blockers contain weapon-equipping state"),
		CommandAbility->BlocksWeaponEquippingAtActivation());
	TestFalse(
		TEXT("Handler command does not require weapon ammunition"),
		CommandAbility->RequiresAmmoForCommand());
	TestFalse(
		TEXT("Handler command has no anticipation-time GAS cost effect"),
		CommandAbility->HasGameplayEffectCost());
	TestFalse(
		TEXT("Handler command has no anticipation-time GAS cooldown effect"),
		CommandAbility->HasGameplayEffectCooldown());

	TestTrue(
		TEXT("Handler command is instanced per actor"),
		CommandAbility->GetInstancingPolicy()
			== EGameplayAbilityInstancingPolicy::InstancedPerActor);
	TestTrue(
		TEXT("Handler command executes only on the server"),
		CommandAbility->GetNetExecutionPolicy()
			== EGameplayAbilityNetExecutionPolicy::ServerOnly);
	TestTrue(
		TEXT("Handler command accepts activation only from the server"),
		CommandAbility->GetNetSecurityPolicy()
			== EGameplayAbilityNetSecurityPolicy::ServerOnly);

	const float IssueDelay = CommandAbility->GetCommandIssueDelay();
	const float Recovery = CommandAbility->GetPostIssueRecovery();
	const float Cooldown = CommandAbility->GetCommandCooldownDuration();
	const float Watchdog = CommandAbility->GetMaximumActiveDuration();
	const float BotRange = CommandAbility->GetBotAttackRange();
	const float BotFrequency = CommandAbility->GetBotAttackFrequency();

	TestTrue(
		TEXT("Handler order issue delay is nonnegative and finite"),
		FMath::IsFinite(IssueDelay) && IssueDelay >= 0.0f);
	TestTrue(
		TEXT("Handler order issue delay retains the prototype default"),
		FMath::IsNearlyEqual(IssueDelay, 0.35f));
	TestTrue(
		TEXT("Handler post-issue recovery is nonnegative and finite"),
		FMath::IsFinite(Recovery) && Recovery >= 0.0f);
	TestTrue(
		TEXT("Handler recovery retains the prototype default"),
		FMath::IsNearlyEqual(Recovery, 0.45f));
	TestTrue(
		TEXT("Handler command cooldown is positive and finite"),
		FMath::IsFinite(Cooldown) && Cooldown > 0.0f);
	TestTrue(
		TEXT("Handler cooldown retains the prototype default"),
		FMath::IsNearlyEqual(Cooldown, 5.5f));
	TestTrue(
		TEXT("Handler command watchdog covers issue plus recovery"),
		FMath::IsFinite(Watchdog)
			&& Watchdog > IssueDelay + Recovery);
	TestTrue(
		TEXT("Handler watchdog retains the prototype default"),
		FMath::IsNearlyEqual(Watchdog, 1.5f));
	TestTrue(
		TEXT("Handler bot engagement range is positive and finite"),
		FMath::IsFinite(BotRange) && BotRange > 0.0f);
	TestTrue(
		TEXT("Handler bot engagement range retains the prototype default"),
		FMath::IsNearlyEqual(BotRange, 2200.0f));
	TestTrue(
		TEXT("Handler bot command cadence is positive and finite"),
		FMath::IsFinite(BotFrequency) && BotFrequency > 0.0f);
	TestTrue(
		TEXT("Bot cadence matches the native success-only command cooldown"),
		FMath::IsNearlyEqual(BotFrequency, Cooldown));
	TestTrue(
		TEXT("Bot engagement range remains distinct from command communication range"),
		!FMath::IsNearlyEqual(
			BotRange,
			Handler->GetMaximumCommandDistance()));

	TestFalse(
		TEXT("Handler command CDO has no captured link instance"),
		CommandAbility->GetCapturedLinkInstanceId().IsValid());
	TestTrue(
		TEXT("Handler command CDO has no pending Hound"),
		CommandAbility->GetPendingCommandHound() == nullptr);
	TestTrue(
		TEXT("Handler command CDO has no ordered Hound"),
		CommandAbility->GetOrderedHound() == nullptr);
	TestTrue(
		TEXT("Handler command CDO has no resolved command target"),
		CommandAbility->GetCommandTarget() == nullptr);

	const USovGameplayAbility_DominionHoundBite* Bite =
		GetDefault<USovGameplayAbility_DominionHoundBite>();
	const USovGameplayAbility_DominionHoundHornCharge* HornCharge =
		GetDefault<USovGameplayAbility_DominionHoundHornCharge>();
	const USovGameplayAbility_DominionHoundPounce* Pounce =
		GetDefault<USovGameplayAbility_DominionHoundPounce>();
	TestNotNull(TEXT("Bite CDO exists for command-link contract"), Bite);
	TestNotNull(
		TEXT("Horn Charge CDO exists for command-link contract"),
		HornCharge);
	TestNotNull(TEXT("Pounce CDO exists for command-link contract"), Pounce);
	if (Bite && HornCharge && Pounce)
	{
		TestFalse(
			TEXT("Ordinary Bite does not require an active command link"),
			Bite->RequiresActiveCommandLink());
		TestTrue(
			TEXT("Specialist Horn Charge requires an active command link"),
			HornCharge->RequiresActiveCommandLink());
		TestTrue(
			TEXT("Specialist Horn Charge requires a Handler-issued order"),
			HornCharge->RequiresHandlerOrderAuthorization());
		TestFalse(
			TEXT("Ordinary Pounce does not require an active command link"),
			Pounce->RequiresActiveCommandLink());
		TestTrue(
			TEXT("Sever interrupts the link-gated Horn Charge"),
			HornCharge->IsInterruptedByCommandLinkSever());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSovDominionHandlerHornChargeSpecMultiplicityTest,
	"ProjectVelkorran.Campaign.DominionHandler.HornChargeSpecMultiplicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovDominionHandlerHornChargeSpecMultiplicityTest::RunTest(
	const FString& Parameters)
{
	UNarrativeAbilitySystemComponent* AbilitySystem =
		NewObject<UNarrativeAbilitySystemComponent>(GetTransientPackage());
	TestNotNull(TEXT("Test ability system exists"), AbilitySystem);
	if (!AbilitySystem)
	{
		return false;
	}

	TArray<FGameplayAbilitySpec>& Specs =
		AbilitySystem->GetActivatableAbilities();
	Specs.Emplace(USovGameplayAbility_DominionHoundBite::StaticClass());
	TestFalse(
		TEXT("An unrelated Hound attack is not a Horn Charge candidate"),
		ASovDominionHandler::FindSingleInactiveExactHornChargeAbility(
			AbilitySystem).IsValid());

	const FGameplayAbilitySpec& UniqueHornSpec = Specs.Emplace_GetRef(
		USovGameplayAbility_DominionHoundHornCharge::StaticClass());
	const FGameplayAbilitySpecHandle UniqueHornHandle = UniqueHornSpec.Handle;
	TestTrue(
		TEXT("Exactly one inactive exact Horn Charge is accepted"),
		ASovDominionHandler::FindSingleInactiveExactHornChargeAbility(
			AbilitySystem) == UniqueHornHandle);

	FGameplayAbilitySpec& DuplicateHornSpec = Specs.Emplace_GetRef(
		USovGameplayAbility_DominionHoundHornCharge::StaticClass());
	TestFalse(
		TEXT("Duplicate inactive exact Horn Charge grants fail closed"),
		ASovDominionHandler::FindSingleInactiveExactHornChargeAbility(
			AbilitySystem).IsValid());

	DuplicateHornSpec.ActiveCount = 1;
	TestTrue(
		TEXT("Only inactive exact Horn Charge specs participate in selection"),
		ASovDominionHandler::FindSingleInactiveExactHornChargeAbility(
			AbilitySystem) == UniqueHornHandle);

	DuplicateHornSpec.ActiveCount = 0;
	DuplicateHornSpec.PendingRemove = true;
	TestTrue(
		TEXT("Pending-removal Horn Charge specs do not participate in selection"),
		ASovDominionHandler::FindSingleInactiveExactHornChargeAbility(
			AbilitySystem) == UniqueHornHandle);
	DuplicateHornSpec.PendingRemove = false;
	DuplicateHornSpec.RemoveAfterActivation = true;
	TestTrue(
		TEXT("Remove-after-activation Horn Charge specs do not participate in selection"),
		ASovDominionHandler::FindSingleInactiveExactHornChargeAbility(
			AbilitySystem) == UniqueHornHandle);

	return true;
}

#endif
