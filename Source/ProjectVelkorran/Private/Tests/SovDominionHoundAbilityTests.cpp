// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_DominionHound.h"
#include "Effects/SovGameplayEffect_DominionHound.h"
#include "GAS/NarrativeCombatAbility.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

#include <type_traits>

static_assert(std::is_base_of_v<
	UNarrativeCombatAbility,
	USovGameplayAbility_DominionHoundAttackBase>);
static_assert(std::is_base_of_v<
	USovGameplayAbility_DominionHoundAttackBase,
	USovGameplayAbility_DominionHoundBite>);
static_assert(std::is_base_of_v<
	USovGameplayAbility_DominionHoundAttackBase,
	USovGameplayAbility_DominionHoundHornCharge>);
static_assert(std::is_base_of_v<
	USovGameplayAbility_DominionHoundAttackBase,
	USovGameplayAbility_DominionHoundPounce>);
static_assert(std::is_base_of_v<
	UGameplayEffect,
	USovGameplayEffect_DominionHoundDamage>);

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSovDominionHoundAbilityContractTest,
	"ProjectVelkorran.Campaign.DominionHound.AbilityContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovDominionHoundAbilityContractTest::RunTest(const FString& Parameters)
{
	const USovGameplayAbility_DominionHoundBite* Bite =
		GetDefault<USovGameplayAbility_DominionHoundBite>();
	const USovGameplayAbility_DominionHoundHornCharge* HornCharge =
		GetDefault<USovGameplayAbility_DominionHoundHornCharge>();
	const USovGameplayAbility_DominionHoundPounce* Pounce =
		GetDefault<USovGameplayAbility_DominionHoundPounce>();
	const USovGameplayEffect_DominionHoundDamage* DamageEffect =
		GetDefault<USovGameplayEffect_DominionHoundDamage>();

	TestNotNull(TEXT("Bite ability CDO exists"), Bite);
	TestNotNull(TEXT("Horn Charge ability CDO exists"), HornCharge);
	TestNotNull(TEXT("Pounce ability CDO exists"), Pounce);
	TestNotNull(TEXT("Dominion hound damage effect CDO exists"), DamageEffect);
	if (!Bite || !HornCharge || !Pounce || !DamageEffect)
	{
		return false;
	}
	TestTrue(
		TEXT("Damage effect is one instant Narrative damage execution"),
		DamageEffect->UsesNarrativeDamageExecution());

	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	const FNarrativeGameplayTags& NarrativeTags =
		FNarrativeGameplayTags::Get();

	TestTrue(
		TEXT("Bite identity tag is registered"),
		SovTags.Ability_NPC_DominionHound_Bite.IsValid());
	TestTrue(
		TEXT("Horn Charge identity tag is registered"),
		SovTags.Ability_NPC_DominionHound_HornCharge.IsValid());
	TestTrue(
		TEXT("Pounce identity tag is registered"),
		SovTags.Ability_NPC_DominionHound_Pounce.IsValid());
	TestTrue(
		TEXT("Weapon-equipping state tag is available to Hound CDOs"),
		NarrativeTags.State_Weapon_Equipping.IsValid());
	TestTrue(
		TEXT("Bite carries its stable identity"),
		Bite->GetHoundAbilityTag()
			== SovTags.Ability_NPC_DominionHound_Bite);
	TestTrue(
		TEXT("Horn Charge carries its stable identity"),
		HornCharge->GetHoundAbilityTag()
			== SovTags.Ability_NPC_DominionHound_HornCharge);
	TestTrue(
		TEXT("Pounce carries its stable identity"),
		Pounce->GetHoundAbilityTag()
			== SovTags.Ability_NPC_DominionHound_Pounce);
	TestTrue(
		TEXT("Bite publishes its identity through GAS asset tags"),
		Bite->GetAssetTags().HasTagExact(
			SovTags.Ability_NPC_DominionHound_Bite));
	TestTrue(
		TEXT("Horn Charge publishes its identity through GAS asset tags"),
		HornCharge->GetAssetTags().HasTagExact(
			SovTags.Ability_NPC_DominionHound_HornCharge));
	TestTrue(
		TEXT("Pounce publishes its identity through GAS asset tags"),
		Pounce->GetAssetTags().HasTagExact(
			SovTags.Ability_NPC_DominionHound_Pounce));

	const FGameplayTag BiteInput = Bite->GetConfiguredInputTag();
	const FGameplayTag HornChargeInput = HornCharge->GetConfiguredInputTag();
	const FGameplayTag PounceInput = Pounce->GetConfiguredInputTag();
	TestTrue(
		TEXT("Bite uses the ordinary attack input"),
		BiteInput == NarrativeTags.Narrative_Input_Attack);
	TestTrue(
		TEXT("Horn Charge uses Ability 1"),
		HornChargeInput == NarrativeTags.Narrative_Input_Ability1);
	TestTrue(
		TEXT("Pounce uses Ability 2"),
		PounceInput == NarrativeTags.Narrative_Input_Ability2);
	TestTrue(
		TEXT("Every hound attack has a distinct input tag"),
		BiteInput != HornChargeInput
			&& BiteInput != PounceInput
			&& HornChargeInput != PounceInput);

	const auto TestSaneAttackDefaults = [this](
		const TCHAR* AttackName,
		const USovGameplayAbility_DominionHoundAttackBase* Ability)
	{
		TestTrue(
			*FString::Printf(TEXT("%s has positive finite damage"), AttackName),
			FMath::IsFinite(Ability->GetDamageAmount())
				&& Ability->GetDamageAmount() > 0.0f);
		TestTrue(
			*FString::Printf(TEXT("%s has positive finite Poise pressure"), AttackName),
			FMath::IsFinite(Ability->GetPoiseDamageAmount())
				&& Ability->GetPoiseDamageAmount() > 0.0f);
		TestTrue(
			*FString::Printf(TEXT("%s has a nonnegative minimum range"), AttackName),
			FMath::IsFinite(Ability->GetMinimumAttackRange())
				&& Ability->GetMinimumAttackRange() >= 0.0f);
		TestTrue(
			*FString::Printf(TEXT("%s has an ordered finite range"), AttackName),
			FMath::IsFinite(Ability->GetMaximumAttackRange())
				&& Ability->GetMaximumAttackRange()
					> Ability->GetMinimumAttackRange());
		TestTrue(
			*FString::Printf(TEXT("%s carries Kinetic damage"), AttackName),
			Ability->GetDamageChannels().HasTagExact(
				FSovGameplayTags::Get().Damage_Channel_Kinetic));
		TestTrue(
			*FString::Printf(TEXT("%s is instanced per actor"), AttackName),
			Ability->GetInstancingPolicy()
				== EGameplayAbilityInstancingPolicy::InstancedPerActor);
		TestTrue(
			*FString::Printf(TEXT("%s executes only on the server"), AttackName),
			Ability->GetNetExecutionPolicy()
				== EGameplayAbilityNetExecutionPolicy::ServerOnly);
		TestTrue(
			*FString::Printf(TEXT("%s accepts activation only from the server"), AttackName),
			Ability->GetNetSecurityPolicy()
				== EGameplayAbilityNetSecurityPolicy::ServerOnly);
		TestTrue(
			*FString::Printf(TEXT("%s blocks weapon-equipping state"), AttackName),
			Ability->BlocksWeaponEquippingAtActivation());
		TestFalse(
			*FString::Printf(TEXT("%s does not require ammunition"), AttackName),
			Ability->RequiresAmmoForAttack());
	};

	TestSaneAttackDefaults(TEXT("Bite"), Bite);
	TestSaneAttackDefaults(TEXT("Horn Charge"), HornCharge);
	TestSaneAttackDefaults(TEXT("Pounce"), Pounce);

	TestTrue(
		TEXT("Bite is a Standard guard-class attack"),
		Bite->GetAttackClassifications().HasTagExact(
			SovTags.Damage_GuardClass_Standard));
	TestFalse(
		TEXT("Bite is not classified as Heavy"),
		Bite->GetAttackClassifications().HasTagExact(
			SovTags.Damage_GuardClass_Heavy));
	TestTrue(
		TEXT("Horn Charge is a Heavy guard-class attack"),
		HornCharge->GetAttackClassifications().HasTagExact(
			SovTags.Damage_GuardClass_Heavy));
	TestTrue(
		TEXT("Pounce is a Heavy guard-class attack"),
		Pounce->GetAttackClassifications().HasTagExact(
			SovTags.Damage_GuardClass_Heavy));
	TestFalse(
		TEXT("Horn Charge remains answerable rather than Unblockable"),
		HornCharge->GetAttackClassifications().HasTagExact(
			SovTags.Damage_GuardClass_Unblockable));
	TestFalse(
		TEXT("Pounce remains answerable rather than Unblockable"),
		Pounce->GetAttackClassifications().HasTagExact(
			SovTags.Damage_GuardClass_Unblockable));

	TestEqual(
		TEXT("Bite exposes three optional animation variants"),
		Bite->GetBiteVariantCount(),
		3);
	TestTrue(
		TEXT("Horn Charge uses native authority movement by default"),
		HornCharge->IsUsingNativeMovement());
	TestTrue(
		TEXT("Horn Charge keeps a nonzero native command wind-up"),
		FMath::IsFinite(HornCharge->GetConfiguredImpactDelay())
			&& HornCharge->GetConfiguredImpactDelay() >= 0.05f);
	TestTrue(
		TEXT("Pounce uses native authority movement by default"),
		Pounce->IsUsingNativeMovement());
	TestFalse(
		TEXT("Sever does not interrupt an ordinary Bite"),
		Bite->IsInterruptedByCommandLinkSever());
	TestFalse(
		TEXT("Ordinary Bite does not require an active command link"),
		Bite->RequiresActiveCommandLink());
	TestFalse(
		TEXT("Ordinary Bite does not require a Handler order"),
		Bite->RequiresHandlerOrderAuthorization());
	TestTrue(
		TEXT("Bite reserves or borrows a Narrative attack token"),
		Bite->RequiresNarrativeAttackToken());
	TestTrue(
		TEXT("Sever interrupts the specialist Horn Charge"),
		HornCharge->IsInterruptedByCommandLinkSever());
	TestTrue(
		TEXT("Specialist Horn Charge requires an active command link"),
		HornCharge->RequiresActiveCommandLink());
	TestTrue(
		TEXT("Specialist Horn Charge requires a Handler order"),
		HornCharge->RequiresHandlerOrderAuthorization());
	TestTrue(
		TEXT("Handler-issued Horn Charge reserves a Narrative attack token"),
		HornCharge->RequiresNarrativeAttackToken());
	TestTrue(
		TEXT("Horn Charge GAS requirements contain the active-link tag"),
		HornCharge->HasActiveCommandLinkActivationRequirement());
	TestTrue(
		TEXT("Horn Charge GAS requirements contain the Handler-order tag"),
		HornCharge->HasHandlerOrderAuthorizationActivationRequirement());
	TestTrue(
		TEXT("Horn Charge GAS blockers contain the Severed tag"),
		HornCharge->BlocksCommandLinkSeverAtActivation());

	UNarrativeAbilitySystemComponent* StateTestAbilitySystem =
		NewObject<UNarrativeAbilitySystemComponent>(GetTransientPackage());
	TestNotNull(
		TEXT("Hound state-blocker contract ASC exists"),
		StateTestAbilitySystem);
	if (StateTestAbilitySystem)
	{
		TestFalse(
			TEXT("An untagged Hound has no activation-blocking state"),
			HornCharge->HasAnyActivationBlockingState(
				StateTestAbilitySystem));
		const TArray<FGameplayTag> SharedBlockers = {
			NarrativeTags.State_IsDead,
			NarrativeTags.State_Busy,
			NarrativeTags.State_Interacting,
			NarrativeTags.State_SequencerControlled,
			NarrativeTags.State_Movement_Ragdoll,
			NarrativeTags.State_Weapon_Equipping,
			NarrativeTags.State_Weapon_BlockFiring,
			NarrativeTags.State_Weapon_IsFiring,
			SovTags.State_Fatal,
			SovTags.State_Poise_Broken,
			SovTags.State_Status_Frozen,
			SovTags.State_Status_DeviceDisabled,
			SovTags.State_CommandLink_Severed};
		for (const FGameplayTag& Blocker : SharedBlockers)
		{
			StateTestAbilitySystem->AddLooseGameplayTag(Blocker);
			TestTrue(
				*FString::Printf(
					TEXT("Horn Charge selection honors blocker %s"),
					*Blocker.ToString()),
				HornCharge->HasAnyActivationBlockingState(
					StateTestAbilitySystem));
			StateTestAbilitySystem->RemoveLooseGameplayTag(Blocker);
		}
	}
	TestFalse(
		TEXT("Native Horn Charge requires a grounded Character avatar"),
		HornCharge->HasRequiredMovementStateForActivation(nullptr));
	TestFalse(
		TEXT("Sever does not interrupt an ordinary Pounce"),
		Pounce->IsInterruptedByCommandLinkSever());
	TestFalse(
		TEXT("Ordinary Pounce does not require an active command link"),
		Pounce->RequiresActiveCommandLink());
	TestFalse(
		TEXT("Ordinary Pounce does not require a Handler order"),
		Pounce->RequiresHandlerOrderAuthorization());
	TestTrue(
		TEXT("Pounce reserves or borrows a Narrative attack token"),
		Pounce->RequiresNarrativeAttackToken());

	return true;
}

#endif
