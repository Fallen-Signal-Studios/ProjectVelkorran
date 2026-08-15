// Copyright Narrative Tools 2024. 


#include "Items/RangedWeaponItem.h"
#include "Items/AmmoItem.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "NarrativeGameplayTags.h"
#include "Character/NarrativeCharacterMovement.h"
#include "Items/MeleeWeaponItem.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "Items/WeaponAttachmentItem.h"

URangedWeaponItem::URangedWeaponItem()
{
	AimFOVPct = 0.75f;
	AimWeaponRenderFOV = 30.f; 
	AimWeaponFStop = 80.f; 
	TraceData.TraceDistance = 15000.f;

	BaseSpreadDegrees = 0.f;
	MoveSpeedAddDegrees = 1.f;
	CrouchSpreadMultiplier = 0.5f; 
	AimSpreadMultiplier = 0.f; 
	SpreadFireBump = 5.f;
	MaxSpreadDegrees = 20.f;
	SpreadDecreaseSpeed = 10.f;

	RecoilImpulseTranslationMin = FVector(-0.1f, -5.f, -0.1f);
	RecoilImpulseTranslationMax = FVector(.1f, -3.f, .1f);
	HipRecoilImpulseTranslationMin = FVector(-0.1f, -5.f, -0.1f);
	HipRecoilImpulseTranslationMax = FVector(.1f, -3.f, .1f);

	//Ranged weapons always want us to face our camera direction, and strafe instead of rotation to face velocity.
	OwnerCapsuleRotationSetting = ECapsuleRotationSetting::UseControllerYawDirect;
	bPawnFollowsControlRotation = true;
	bPawnOrientsRotationToMovement = false;
}

void URangedWeaponItem::TickItem(const float DeltaTime)
{
	TickSpread(DeltaTime);
}

void URangedWeaponItem::HandleWield_Implementation()
{
	Super::HandleWield_Implementation();

	EnableItemTick(true);
}

void URangedWeaponItem::HandleUnWield_Implementation()
{
	Super::HandleUnWield_Implementation();

	EnableItemTick(false);
}

bool URangedWeaponItem::CanDualWieldWith_Implementation(class UWeaponItem* Other)
{
	//For the base tool, we don't support wielding a ranged weapon with a melee weapon. If you're extending pro
	//and need that, you can override this function to remove this.
	if (Other && Other->GetClass() && Other->GetClass()->IsChildOf<UMeleeWeaponItem>())
	{
		return false; 
	}
	
	return Super::CanDualWieldWith_Implementation(Other);
}

FCombatTraceData URangedWeaponItem::GetTraceData() const
{
	return TraceData;
}

float URangedWeaponItem::GetAttackRange() const
{
	return TraceData.TraceDistance;
}

float URangedWeaponItem::GetWeaponSpread_Implementation() const
{
	ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter();

	if (!CharacterOwner)
	{
		return 0.f;
	}



	float Spread = CurrentSpread;

	//Want to apply extra base spread based on percentage of move speed
	if (UNarrativeCharacterMovement* NCMC = CharacterOwner->GetNarrativeCharacterMovement())
	{
		const float SpeedPct = CharacterOwner->GetVelocity().Size() / NCMC->MaxWalkSpeed;

		Spread += SpeedPct * MoveSpeedAddDegrees;

		if (NCMC->IsCrouching())
		{
			Spread *= CrouchSpreadMultiplier;
		}
	}

	const FGameplayTag AimingTag = CharacterOwner->IsLocallyControlled() ? FNarrativeGameplayTags::Get().State_Weapon_IsAiming_Local : FNarrativeGameplayTags::Get().State_Weapon_IsAiming;
	
	if (CharacterOwner->HasMatchingGameplayTag(AimingTag))
	{
		Spread *= AimSpreadMultiplier;
	}

	return Spread;
}

void URangedWeaponItem::TickSpread(const float DeltaTime)
{
	//TODO use a float curve to allow more control over this
	const float DecreaseAmount = DeltaTime * SpreadDecreaseSpeed;
	
	CurrentSpread = FMath::Clamp(CurrentSpread - DecreaseAmount, BaseSpreadDegrees, MaxSpreadDegrees);
}

void URangedWeaponItem::AddSpread()
{
	CurrentSpread = FMath::Clamp(CurrentSpread + SpreadFireBump, BaseSpreadDegrees, MaxSpreadDegrees);
}

void URangedWeaponItem::OnAttack_Implementation()
{
	Super::OnAttack_Implementation();

	AddSpread();
}

float URangedWeaponItem::GetAimFOV() const
{
	if (UNarrativeGameUserSettings* GUS = Cast<UNarrativeGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
	{
		const float BaseFOV = GUS->GetFieldOfView();

		for (auto& AttachmentKVP : WeaponAttachments)
		{
			if (const UWeaponAttachmentItem* Attachment = AttachmentKVP.Value)
			{
				const float OverrideFOV = Attachment->OverrideWeaponCameraFOV();

				if (OverrideFOV > 0.f)
				{
					return OverrideFOV;
				}
			}
		}

		return BaseFOV * AimFOVPct;
	}

	return 90.f; 
}

float URangedWeaponItem::GetAimWeaponRenderFOV() const
{
	//Our weapon has a default FOV when aiming, but attachments like scopes may need to override this. 
	const float BaseFOV = 30.f;

	for (auto& AttachmentKVP : WeaponAttachments)
	{
		if (const UWeaponAttachmentItem* Attachment = AttachmentKVP.Value)
		{
			const float OverrideFOV = Attachment->OverrideWeaponRenderFOV();

			if (OverrideFOV > 0.f)
			{
				return OverrideFOV;
			}
		}
	}

	return AimWeaponRenderFOV;

}

float URangedWeaponItem::GetAimFStop() const
{
	for (auto& AttachmentKVP : WeaponAttachments)
	{
		if (const UWeaponAttachmentItem* Attachment = AttachmentKVP.Value)
		{
			const float OverrideFStop = Attachment->OverrideWeaponAimFStop();

			if (OverrideFStop > 0.f)
			{
				return OverrideFStop;
			}
		}
	}

	return AimWeaponFStop;
}

FTransform URangedWeaponItem::GetRecoilImpulse() const
{

	bool bIsAiming = false;

	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		if (CharacterOwner->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Weapon_IsAiming))
		{
			bIsAiming = true;
		}
	}

	const FVector MinImpulse = bIsAiming ? HipRecoilImpulseTranslationMin : RecoilImpulseTranslationMin;
	const FVector MaxImpulse = bIsAiming ? HipRecoilImpulseTranslationMax : RecoilImpulseTranslationMax;

	const float X = FMath::RandRange(FMath::Min(MinImpulse.X, MaxImpulse.X), FMath::Max(MinImpulse.X, MaxImpulse.X));
	const float Y = FMath::RandRange(FMath::Min(MinImpulse.Y, MaxImpulse.Y), FMath::Max(MinImpulse.Y, MaxImpulse.Y));
	const float Z = FMath::RandRange(FMath::Min(MinImpulse.Z, MaxImpulse.Z), FMath::Max(MinImpulse.Z, MaxImpulse.Z));

	return FTransform(FVector(X, Y, Z));
}
