// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCombatInterruptionTestFixtures.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Items/WeaponItem.h"
#include "Sovereign/SovGameplayTags.h"

USovCombatDroneRocketTestAbility::USovCombatDroneRocketTestAbility()
{
	bAutoReleasePayload = false; CooldownDuration = 0.f;
}
void USovCombatDroneRocketTestAbility::ProcessEvent(UFunction* Function, void* Parameters)
{
	if (Function && Function->GetFName() == TEXT("ReceiveDroneWeaponPayloadReleased"))
	{
		auto* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
		if (ASC && bDisableDuringRelease)
		{
			bDisableDuringRelease = false;
			ASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_Status_DeviceDisabled);
			ASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().State_Status_DeviceDisabled);
		}
		if (ASC && bRestartDuringRelease)
		{
			bRestartDuringRelease = false;
			const auto Handle = CurrentSpecHandle;
			ASC->CancelAbilityHandle(Handle);
			bRestartAccepted = ASC->TryActivateAbility(Handle);
		}
	}
	Super::ProcessEvent(Function, Parameters);
}
USovCombatDroneGunTestAbility::USovCombatDroneGunTestAbility()
{
	bAutoReleasePayload = false; CooldownDuration = 0.f;
	FallbackMuzzleOffset = FVector::ZeroVector; SpreadDegrees = 0.f; MaximumRange = 2000.f;
}
USovCombatDroneSuicideTestAbility::USovCombatDroneSuicideTestAbility()
{
	bAutoReleasePayload = false; CooldownDuration = 0.f;
	bOnlyAcquirePlayerControlledTargets = false;
	DetonationTriggerRadius = 500.f; ExplosionRadius = 500.f;
	ExplosionDamage = 10.f; ExplosionPoiseDamage = 5.f; DetonationWarningDuration = 0.05f;
}
USovCombatJudgementTestAbility::USovCombatJudgementTestAbility()
{
	AllowedWeaponClasses.Add(UWeaponItem::StaticClass());
	bAutoReleasePayload = false; FallbackMuzzleOffset = FVector(95.f, 0.f, 0.f);
	MaximumRange = 2000.f; DirectDamage = 20.f; DirectPoiseDamage = 10.f;
	ExplosionDamage = 10.f; ExplosionPoiseDamage = 5.f; ExplosionRadius = 250.f;
	bApplyExplosionPhysicsImpulse = false;
}
void USovCombatInterruptionDamageProbe::ReceiveDamage(const FSovDamageResult& Result)
{
	if (!bArmed) { return; }
	bArmed = false;
	if (bHealTarget && TargetASC)
	{
		TargetASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 100.f);
		TargetASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 100.f);
	}
	if (bRebindSource && SourceASC && ReplacementAvatar)
	{
		SourceASC->InitAbilityActorInfo(OriginalAvatar, ReplacementAvatar);
		if (bRestoreOriginalAvatar) { SourceASC->InitAbilityActorInfo(OriginalAvatar, OriginalAvatar); }
	}
	if (bCancelSource && SourceASC) { SourceASC->CancelAllAbilities(); }
	if (bRebindTarget && TargetASC && ReplacementAvatar)
	{
		TargetASC->InitAbilityActorInfo(OriginalAvatar, ReplacementAvatar);
		if (bRestoreOriginalAvatar) { TargetASC->InitAbilityActorInfo(OriginalAvatar, OriginalAvatar); }
	}
	if (bReviveTarget && TargetASC)
	{
		TargetASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
		TargetASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	}
}
