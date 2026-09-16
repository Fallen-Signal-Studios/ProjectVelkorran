// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Weapons/SovAdsPolicy.h"
#include "SovAdsComponent.generated.h"

class ANarrativeCharacter;
class ASovPlayerController;
class UWeaponItem;
class USovAdsSightWidget;

/** Authoring mirror of the engine-free sight kinds, so per-weapon sights can be configured. */
UENUM(BlueprintType)
enum class ESovAdsSight : uint8 { Default, IronSight, Scope };

/**
 * One weapon's sight. Matched with IsA, the same way the Echo abilities match their allowed
 * weapons, rather than by asset name.
 */
USTRUCT(BlueprintType)
struct FSovAdsWeaponSight
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|ADS")
	TSoftClassPtr<UWeaponItem> Weapon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|ADS")
	ESovAdsSight Sight = ESovAdsSight::Default;
};

/**
 * Aim down sights: the zoom and the sight picture.
 *
 * Aiming itself already exists. Narrative's own aim ability applies the aiming state when the
 * player holds the aim input, and the plugin already fixes the first-person weapon framing from it.
 * What was missing is the world zoom and a sight the player actually looks through, which is what
 * this component adds. It deliberately does not introduce a second ability on that input: Narrative
 * activates every granted ability matching a pressed input tag, so a rival ability would double-fire
 * against the existing one.
 *
 * The zoom is applied by locking the camera manager's field of view from the project side rather
 * than by editing the plugin's camera manager, which cannot see project types. That also composes
 * with the plugin's existing weapon-framing maths, because it derives its own blend from the
 * current field of view.
 */
UCLASS(ClassGroup = (Sovereign), meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovAdsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovAdsComponent();

	/** 0 at the hip, 1 fully sighted. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|ADS")
	float GetAimBlend() const { return AimBlend; }

	/** Opacity the sight picture should be drawn at, 0 while hip firing. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|ADS")
	float GetSightAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|ADS")
	ESovAdsSight GetActiveSight() const { return ActiveSight; }

	/** The field of view this component is currently holding, or 0 when it is not holding one. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|ADS")
	float GetHeldFieldOfView() const { return HeldFieldOfView; }

	/** Per-weapon sights. A weapon with no entry, or an unresolvable one, uses the default sight. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|ADS")
	TArray<FSovAdsWeaponSight> WeaponSights;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
	friend class FSovAdsRuntimeTestAccess;

	/** Authority-independent read of whether the owner is currently aiming a ranged weapon. */
	bool IsAimingRangedWeapon(ESovAdsSight& OutSight) const;
	const SovAdsPolicy::FProfile& ProfileFor(ESovAdsSight Sight) const;
	ESovAdsSight SightForWeapon(const UWeaponItem* Weapon) const;
	void ApplyFieldOfView(ASovPlayerController* Controller, const SovAdsPolicy::FProfile& Profile);
	void ReleaseFieldOfView(ASovPlayerController* Controller);
	void RefreshSightWidget(ASovPlayerController* Controller);
	void RetireSightWidget();
	/** The hip crosshair is suppressed only while a real sight picture is up. */
	void RefreshCrosshairSuppression(ANarrativeCharacter* Owner, bool bReplaced);

	UPROPERTY(Transient) TObjectPtr<USovAdsSightWidget> SightWidget;
	float AimBlend = 0.f;
	float HeldFieldOfView = 0.f;
	ESovAdsSight ActiveSight = ESovAdsSight::Default;
	bool bSuppressingCrosshair = false;
	bool bEndingPlay = false;
};
