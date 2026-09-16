// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Weapons/SovAdsComponent.h"

#include "ArsenalStatics.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Items/RangedWeaponItem.h"
#include "Items/WeaponItem.h"
#include "NarrativeGameplayTags.h"
#include "UI/SovAdsSightWidget.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"

USovAdsComponent::USovAdsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// After movement and camera work for the frame, so the held field of view is the last word.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(false);

	// Authored per-weapon sights. An unresolvable entry simply falls back to the default sight,
	// which matters here: WI_Staccato is not version controlled, so it exists on one machine only.
	FSovAdsWeaponSight Cinderline;
	Cinderline.Weapon = TSoftClassPtr<UWeaponItem>(FSoftObjectPath(TEXT("/Game/Items/Weapons/WI_Cinderline.WI_Cinderline_C")));
	Cinderline.Sight = ESovAdsSight::IronSight;
	WeaponSights.Add(Cinderline);

	FSovAdsWeaponSight Staccato;
	Staccato.Weapon = TSoftClassPtr<UWeaponItem>(FSoftObjectPath(TEXT("/Game/Items/Weapons/WI_Staccato.WI_Staccato_C")));
	Staccato.Sight = ESovAdsSight::Scope;
	WeaponSights.Add(Staccato);
}

void USovAdsComponent::BeginPlay()
{
	Super::BeginPlay();
	AimBlend = 0.f;
	HeldFieldOfView = 0.f;
}

void USovAdsComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	bEndingPlay = true;
	if (auto* Owner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		ReleaseFieldOfView(Cast<ASovPlayerController>(Owner->GetController()));
		RefreshCrosshairSuppression(Owner, false);
	}
	RetireSightWidget();
	Super::EndPlay(Reason);
}

const SovAdsPolicy::FProfile& USovAdsComponent::ProfileFor(ESovAdsSight Sight) const
{
	switch (Sight)
	{
	case ESovAdsSight::Scope: return SovAdsPolicy::Staccato;
	case ESovAdsSight::IronSight: return SovAdsPolicy::Cinderline;
	default: return SovAdsPolicy::Default;
	}
}

ESovAdsSight USovAdsComponent::SightForWeapon(const UWeaponItem* Weapon) const
{
	if (!IsValid(Weapon)) { return ESovAdsSight::Default; }
	for (const FSovAdsWeaponSight& Entry : WeaponSights)
	{
		// Soft: an absent asset resolves to null and is skipped rather than failing the aim.
		if (UClass* const Configured = Entry.Weapon.Get())
		{
			if (Weapon->IsA(Configured)) { return Entry.Sight; }
		}
	}
	return ESovAdsSight::Default;
}

bool USovAdsComponent::IsAimingRangedWeapon(ESovAdsSight& OutSight) const
{
	OutSight = ESovAdsSight::Default;
	const auto* Owner = Cast<ANarrativeCharacter>(GetOwner());
	const auto* ASC = IsValid(Owner) ? Owner->GetNarrativeAbilitySystemComponent() : nullptr;
	if (!IsValid(ASC) || !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Weapon_IsAiming))
	{
		return false;
	}
	// Only a wielded ranged weapon has sights to look through.
	const auto* Weapon = Cast<URangedWeaponItem>(const_cast<ANarrativeCharacter*>(Owner)->GetWeapon(true));
	if (!IsValid(Weapon) || !Weapon->IsWielded()) { return false; }
	OutSight = SightForWeapon(Weapon);
	return true;
}

void USovAdsComponent::ApplyFieldOfView(ASovPlayerController* Controller, const SovAdsPolicy::FProfile& Profile)
{
	APlayerCameraManager* const Camera = IsValid(Controller) ? Controller->PlayerCameraManager : nullptr;
	if (!IsValid(Camera)) { return; }
	// Non-const: the settings accessor for the player's field of view is not a const method.
	UNarrativeGameUserSettings* const Settings = UArsenalStatics::GetNarrativeGameUserSettings();
	// The player's own field of view is the starting point; aiming narrows it, never replaces it.
	const float BaseFieldOfView = Settings ? Settings->GetFieldOfView() : 90.f;
	// Prefer the weapon's authored aim field of view when it has one, so authored scopes still win.
	float Target = SovAdsPolicy::FieldOfView(BaseFieldOfView, AimBlend, Profile);
	if (const auto* Owner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		if (const auto* Weapon = Cast<URangedWeaponItem>(const_cast<ANarrativeCharacter*>(Owner)->GetWeapon(true)))
		{
			const float Authored = Weapon->GetAimFOV();
			if (FMath::IsFinite(Authored) && Authored > 0.f && Authored < BaseFieldOfView)
			{
				Target = FMath::Lerp(BaseFieldOfView, Authored, FMath::Clamp(AimBlend, 0.f, 1.f));
			}
		}
	}
	if (!FMath::IsFinite(Target) || Target <= 0.f) { return; }
	HeldFieldOfView = Target;
	Camera->SetFOV(Target);
}

void USovAdsComponent::ReleaseFieldOfView(ASovPlayerController* Controller)
{
	APlayerCameraManager* const Camera = IsValid(Controller) ? Controller->PlayerCameraManager : nullptr;
	if (HeldFieldOfView > 0.f && IsValid(Camera)) { Camera->UnlockFOV(); }
	HeldFieldOfView = 0.f;
}

void USovAdsComponent::RefreshCrosshairSuppression(ANarrativeCharacter* Owner, bool bReplaced)
{
	auto* ASC = IsValid(Owner) ? Owner->GetNarrativeAbilitySystemComponent() : nullptr;
	if (!IsValid(ASC) || bReplaced == bSuppressingCrosshair) { return; }
	const FGameplayTag Hide = FNarrativeGameplayTags::Get().State_UI_HideCrosshair;
	// Only ever remove a suppression this component added.
	if (bReplaced) { ASC->AddLooseGameplayTag(Hide); } else { ASC->RemoveLooseGameplayTag(Hide); }
	bSuppressingCrosshair = bReplaced;
}

void USovAdsComponent::RefreshSightWidget(ASovPlayerController* Controller)
{
	if (bEndingPlay || !IsValid(Controller) || !Controller->IsLocalController() || !Controller->GetLocalPlayer())
	{
		RetireSightWidget();
		return;
	}
	if (AimBlend <= 0.f)
	{
		RetireSightWidget();
		return;
	}
	if (!IsValid(SightWidget))
	{
		SightWidget = CreateWidget<USovAdsSightWidget>(Controller, USovAdsSightWidget::StaticClass());
		// Beneath menus and modal layers: a sight picture is never interactive.
		if (IsValid(SightWidget)) { SightWidget->AddToPlayerScreen(-1); }
	}
	if (IsValid(SightWidget))
	{
		SightWidget->UpdateSight(ActiveSight, GetSightAlpha(), AimBlend, Controller);
	}
}

void USovAdsComponent::RetireSightWidget()
{
	if (IsValid(SightWidget))
	{
		SightWidget->RemoveFromParent();
	}
	SightWidget = nullptr;
}

float USovAdsComponent::GetSightAlpha() const
{
	return SovAdsPolicy::SightAlpha(AimBlend, ProfileFor(ActiveSight));
}

void USovAdsComponent::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, TickFunction);
	auto* Owner = Cast<ANarrativeCharacter>(GetOwner());
	auto* Controller = IsValid(Owner) ? Cast<ASovPlayerController>(Owner->GetController()) : nullptr;
	if (bEndingPlay || !IsValid(Owner))
	{
		return;
	}

	ESovAdsSight DesiredSight = ESovAdsSight::Default;
	const bool bAiming = IsAimingRangedWeapon(DesiredSight);
	// Keep the sight that is currently up until the blend closes, so lowering it stays coherent
	// even if the weapon is holstered or swapped mid-blend.
	if (bAiming) { ActiveSight = DesiredSight; }
	const SovAdsPolicy::FProfile& Profile = ProfileFor(ActiveSight);
	AimBlend = SovAdsPolicy::AdvanceBlend(AimBlend, bAiming, DeltaSeconds, Profile);

	if (AimBlend > 0.f) { ApplyFieldOfView(Controller, Profile); }
	else { ReleaseFieldOfView(Controller); }

	RefreshCrosshairSuppression(Owner, SovAdsPolicy::ReplacesCrosshair(AimBlend, Profile));
	RefreshSightWidget(Controller);
}
