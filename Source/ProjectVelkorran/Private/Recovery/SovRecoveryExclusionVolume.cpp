// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Recovery/SovRecoveryExclusionVolume.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/SecureHash.h"

ASovRecoveryExclusionVolume::ASovRecoveryExclusionVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	ExclusionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ExclusionBounds"));
	SetRootComponent(ExclusionBounds); ExclusionBounds->SetBoxExtent(FVector(200.f));
	ExclusionBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ExclusionBounds->SetGenerateOverlapEvents(false);
}
void ASovRecoveryExclusionVolume::SetExclusionActive(bool bEnabled) { if (HasAuthority()) { bActive = bEnabled; } }
FGuid ASovRecoveryExclusionVolume::GetActorGUID_Implementation() const
{
	if (!SaveGuid.IsValid())
	{
		FGuid Stable;
		FGuid::ParseExact(FMD5::HashAnsiString(*GetPathName()), EGuidFormats::Digits, Stable);
		const_cast<ASovRecoveryExclusionVolume*>(this)->SaveGuid = Stable;
	}
	return SaveGuid;
}
bool ASovRecoveryExclusionVolume::ExcludesCapsule(const UWorld* World, const FVector& Center, float Radius, float HalfHeight)
{
	if (!World || Center.ContainsNaN() || !FMath::IsFinite(Radius) || !FMath::IsFinite(HalfHeight)
		|| Radius <= 0.f || HalfHeight < Radius) { return true; }
	const FVector Extent(Radius, Radius, HalfHeight);
	const FBox CapsuleBounds(Center - Extent, Center + Extent);
	for (TActorIterator<ASovRecoveryExclusionVolume> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (!It->bActive || !IsValid(It->ExclusionBounds)) { continue; }
		const FBox Bounds = It->ExclusionBounds->CalcBounds(It->ExclusionBounds->GetComponentTransform()).GetBox();
		if (!Bounds.IsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN() || Bounds.Intersect(CapsuleBounds)) { return true; }
	}
	return false;
}
