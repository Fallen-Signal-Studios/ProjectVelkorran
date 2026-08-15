// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Items/NarrativeItem.h"
#include "AmmoFragment.generated.h"

/**
 * Can be added to any item to specify that the item can be used as ammo. Contains information like a special projectile
 * we might need to spawn when using this ammo, or a custom damage effect to apply when using the ammo. Up to caller to use how they wish. 
 * 
 * You can add even more data by subclassing this, for example an ability the ammo should override the weapon with. 
 */
UCLASS()
class NARRATIVEARSENAL_API UAmmoFragment : public UNarrativeItemFragment
{
	GENERATED_BODY()
	
public: 

	UAmmoFragment(const FObjectInitializer& ObjectInitializer);

	/**Custom damage amount to use with this ammo*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ammo")
	float AmmoDamageOverride;

	/**Custom damage GE to use with this ammo*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ammo")
	TSubclassOf<class UGameplayEffect> DamageEffect;

	/**Custom damage GE to use with this ammo*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ammo")
	TSubclassOf<class ANarrativeProjectile> ProjectileClass;

	/**Custom trace data to use with this ammo*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ammo")
	bool bOverrideTraceData;

	/**Custom trace data to use with this ammo*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ammo")
	FCombatTraceData TraceData;

};
