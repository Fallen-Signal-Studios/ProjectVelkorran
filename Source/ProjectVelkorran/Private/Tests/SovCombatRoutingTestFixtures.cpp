// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Components/SovStatusComponent.h"
#include "GAS/NarrativeDamageExecCalc.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

void USovDamagePublicationRepairObserver::OnTargetResult(const FSovDamageResult& Result)
{
	if (bRestoreSourceOnTargetResult && SourceASC)
	{
		bRestoreSourceOnTargetResult = false;
		SourceASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
		SourceASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 50.f);
	}
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
	LastSourceResult = Result;
}

void USovDamagePublicationRepairObserver::OnStatusRequest(const FSovStatusApplicationRequest& Request)
{
	StatusRequests.Add(Request);
	if (StatusRequests.Num() != 1 || !TargetASC) { return; }
	bStatusPresentBeforeFirstRequest = StatusComponent && StatusComponent->HasActiveStatus(Request.StatusTag);
	if (ReplacementAvatar)
	{
		AActor* OriginalAvatar = TargetASC->GetAvatarActor();
		TargetASC->InitAbilityActorInfo(TargetASC->GetOwnerActor(), ReplacementAvatar);
		if (bRestoreAvatarAfterReplacement)
		{
			TargetASC->InitAbilityActorInfo(TargetASC->GetOwnerActor(), OriginalAvatar);
		}
	}
	else if (bRestoreOnFirstStatus)
	{
		TargetASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
		TargetASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 50.f);
	}
}

bool USovIdentityDamagePolicyTestComponent::LimitSovDamage(AActor* Target,
	const FGameplayEffectContextHandle& Context, float& InOutShieldDamage,
	float& InOutHealthDamage, float& InOutPoiseDamage) const
{
	++PolicyCalls;
	ApprovedShieldDamage = InOutShieldDamage;
	ApprovedHealthDamage = InOutHealthDamage;
	return true;
}

ASovCombatAdmissionTestCharacter::ASovCombatAdmissionTestCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

ETeamAttitude::Type ASovCombatAdmissionTestCharacter::GetTeamAttitudeTowards(const AActor& Other) const
{
	// Take ownership before invocation so a nested query cannot recurse this hook.
	TFunction<void()> Callback = MoveTemp(OnNextTeamQuery);
	if (Callback) { Callback(); }
	return Super::GetTeamAttitudeTowards(Other);
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
