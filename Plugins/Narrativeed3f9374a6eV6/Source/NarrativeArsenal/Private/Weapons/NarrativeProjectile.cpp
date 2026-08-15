// Copyright Narrative Tools 2025.


#include "Weapons/NarrativeProjectile.h"

static TAutoConsoleVariable<bool> CVarProjectilesDebug(
	TEXT("n.gas.Projectiles.Debug"),
	false,
	TEXT("Debug Projectiles 0=Off 1=On"),
	ECVF_Default);


// Sets default values
ANarrativeProjectile::ANarrativeProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	


}

class ANarrativeCharacter* ANarrativeProjectile::GetNarrativeCharacter() const
{
	return Cast<ANarrativeCharacter>(GetOwner());
}

void ANarrativeProjectile::SetProjectileTargetData(const FGameplayAbilityTargetDataHandle& TargetHandle)
{


	OnProjectileTargetData.Broadcast(TargetHandle);
}

// Called when the game starts or when spawned
void ANarrativeProjectile::BeginPlay()
{
	Super::BeginPlay();
	
}


