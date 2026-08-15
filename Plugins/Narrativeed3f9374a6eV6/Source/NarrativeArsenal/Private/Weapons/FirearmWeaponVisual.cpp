// Copyright Narrative Tools 2025.

#include "Weapons/FirearmWeaponVisual.h"
#include "Components/SkeletalMeshComponent.h"

AFirearmWeaponVisual::AFirearmWeaponVisual(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ADSSightSocketName = FName("Socket_ADS");
}

FVector AFirearmWeaponVisual::GetADSLocation_Implementation() const
{
	//Ask our attachmenhts if they want to return an ADS location
	if (WeaponMesh)
	{
		return WeaponMesh->GetSocketTransform(ADSSightSocketName, ERelativeTransformSpace::RTS_World).GetLocation();
	}

	return FVector();
}

FVector AFirearmWeaponVisual::GetADSRelativeLocation() const 
{
	const FVector WorldLocation = GetADSLocation();

	if (WeaponMesh)
	{
		const FName RootBoneName = WeaponMesh->GetBoneName(0);

		FVector OutLoc;
		FRotator OutRot; 
		WeaponMesh->TransformToBoneSpace(RootBoneName, WorldLocation, FRotator(), OutLoc, OutRot);

		return OutLoc;
	}

	return FVector();

}
