// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovEchoResourceTestFixtures.h"
#include "Sovereign/SovGameplayTags.h"
#include "Items/WeaponItem.h"
#include "Components/SovEchoComponent.h"
#include "GAS/NarrativeDamageExecCalc.h"
USovEchoHeavyTestAbility::USovEchoHeavyTestAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	bRequiresAmmo = false;
	SetAssetTags(FGameplayTagContainer(FSovGameplayTags::Get().Damage_Heavy));
}
void USovEchoHeavyTestAbility::Finish()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
USovEchoReadyTestSignature::USovEchoReadyTestSignature()
{
	bRequiresAllowedWeapon = true;
	AllowedWeaponClasses.Add(UWeaponItem::StaticClass());
	EchoCost = 90.f;
	MinimumEchoRequired = 95.f;
}
void ASovEchoVisualTestActor::SetTestCharacter(ANarrativeCharacter* Character)
{
	CharacterOwner = Character;
}

USovEchoUnbrokenTestWeakPoint::USovEchoUnbrokenTestWeakPoint()
{
	MinimumAppliedDamage = 50.f;
	FSovWeakPointZone& Zone = WeakPointZones.AddDefaulted_GetRef();
	Zone.ZoneId = TEXT("ArmourJoint");
	Zone.HitBones.Add(TEXT("joint"));
}
void USovEchoCallbackTestObserver::EndSource(const FSovDamageResult&)
{
	if (SourceAbility.IsValid()) SourceAbility->Finish();
	SourceAbility.Reset();
}
void USovEchoCallbackTestObserver::HandleEchoChanged(float, float, float)
{
	if (bSpendOnNextChange && Echo.IsValid())
	{
		bSpendOnNextChange = false;
		Echo->TrySpendEcho(2.f, FGameplayTag());
	}
}

USovEchoPeriodicTestEffect::USovEchoPeriodicTestEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.f));
	Period = FScalableFloat(0.2f);
	bExecutePeriodicEffectOnApplication = true;
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UNarrativeDamageExecCalc::StaticClass();
	Executions.Add(Execution);
}
