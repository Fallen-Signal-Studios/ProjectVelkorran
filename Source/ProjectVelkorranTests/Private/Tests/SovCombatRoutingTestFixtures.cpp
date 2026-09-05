// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "GAS/NarrativeDamageExecCalc.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

void USovDamagePublicationRepairObserver::OnTargetResult(const FSovDamageResult& Result)
{
	if (bRestoreOnTargetResult && Result.bFatal && TargetASC)
	{
		bRestoreOnTargetResult = false;
		TargetASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 50.f);
	}
}
void USovDamagePublicationRepairObserver::OnSourceResult(const FSovDamageResult& Result)
{
	++SourceResults;
	bLastSourceFatal = Result.bFatal;
}

USovCombatRoutingTestEffect::USovCombatRoutingTestEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UNarrativeDamageExecCalc::StaticClass();
	Executions.Add(Execution);
}

USovCombatDirectPoiseTestEffect::USovCombatDirectPoiseTestEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UNarrativeAttributeSetBase::GetPoiseDamageAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(40.f));
}

USovWeakPointRoutingTestComponent::USovWeakPointRoutingTestComponent()
{
	FSovWeakPointZone Weapon;
	Weapon.ZoneId = TEXT("Weapon");
	Weapon.HitBones.Add(TEXT("weapon"));
	Weapon.Consequence.Duration = 5.f;
	Weapon.Consequence.BlockedAbilityTags.AddTag(FNarrativeGameplayTags::Get().Ability_WeaponFire);
	Weapon.Consequence.CancelAbilityTags = Weapon.Consequence.BlockedAbilityTags;
	Weapon.Consequence.GrantedStateTags.AddTag(FSovGameplayTags::Get().State_Status_DeviceDisabled);
	WeakPointZones.Add(Weapon);
	FSovWeakPointZone Sensor = Weapon;
	Sensor.ZoneId = TEXT("Sensor");
	Sensor.HitBones = {TEXT("sensor")};
	Sensor.Consequence.Duration = 0.f;
	WeakPointZones.Add(Sensor);
}
void USovWeakPointRoutingTestComponent::Observe()
{
	OnWeakPointStateChanged.AddUniqueDynamic(this, &ThisClass::ObserveState);
	OnWeakPointBroken.AddUniqueDynamic(this, &ThisClass::ObserveBreak);
}
void USovWeakPointRoutingTestComponent::ObserveState(FName Id, bool bBroken)
{
	if (bResetOnBreakNotification && bBroken) { ResetWeakPoints(); }
}
void USovWeakPointRoutingTestComponent::ObserveBreak(FName Id, const FSovDamageResult& Result)
{
	++DetailedBreakCount;
}
USovWeakPointFireTestAbility::USovWeakPointFireTestAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	SetAssetTags(FGameplayTagContainer(FNarrativeGameplayTags::Get().Ability_WeaponFire));
}
void USovWeakPointFireTestAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	SetCanBeCanceled(true);
}
USovWeakPointMeleeTestAbility::USovWeakPointMeleeTestAbility()
{
	SetAssetTags(FGameplayTagContainer(FNarrativeGameplayTags::Get().Ability_MeleeAttack));
}
