// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCinderJudgementGeometryFixtures.h"
#include "Items/WeaponItem.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Weapons/WeaponVisual.h"
#include <limits>

USovCinderJudgementGeometryAbility::USovCinderJudgementGeometryAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	AllowedWeaponClasses.Add(UWeaponItem::StaticClass());
	bAutoReleasePayload = false;
	bApplyExplosionPhysicsImpulse = false;
	FallbackMuzzleOffset = FVector(100.0, 0.0, 0.0);
	MaximumRange = 1000.0f;
}

AWeaponVisual* USovCinderJudgementGeometryAbility::GetAbilityWeaponVisual() const
{
	return TestVisual ? TestVisual.Get() : Super::GetAbilityWeaponVisual();
}

void ASovCinderJudgementGeometryCharacter::GetActorEyesViewPoint(FVector& Location, FRotator& Rotation) const
{
	Super::GetActorEyesViewPoint(Location, Rotation);
	// Inject only at the accessor boundary; do not pass a malformed vector into
	// unrelated engine math/actor setters that would diagnose it before release.
	if (bInvalidEye) { Location.X = std::numeric_limits<double>::infinity(); }
}

void USovCinderJudgementDamageProbe::DuringDamage(const FSovDamageResult& Result)
{
	if (!bArmed || !SourceASC || ++HitCount < TriggerAfterHit) { return; }
	bArmed = false;
	if (Mutation == ESovJudgementDamageMutation::AvatarABA && Replacement)
	{
		AActor* OriginalOwner = SourceASC->GetOwnerActor();
		AActor* OriginalAvatar = SourceASC->GetAvatarActor();
		SourceASC->InitAbilityActorInfo(Replacement, Replacement);
		SourceASC->InitAbilityActorInfo(OriginalOwner, OriginalAvatar);
	}
	else if (Mutation == ESovJudgementDamageMutation::LifeABA)
	{
		SourceASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.0f);
		SourceASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.0f);
	}
	else
	{
		SourceASC->CancelAbilityHandle(Handle);
		if (Mutation == ESovJudgementDamageMutation::Restart)
		{ bRestartAccepted = SourceASC->TryActivateAbility(Handle, false); }
	}
}
