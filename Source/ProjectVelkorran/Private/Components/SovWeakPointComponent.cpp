// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovWeakPointComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkinnedMeshComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UnrealFramework/NarrativeCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovWeakPoint, Log, All);

USovWeakPointComponent::USovWeakPointComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USovWeakPointComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ApplyAuthoredStartingState();
	}

	if (ANarrativeCharacter* NarrativeOwner =
		Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.AddUniqueDynamic(
			this,
			&ThisClass::HandleOwnerASCInitialized);
	}
	TryInitializeFromOwner();
}

void USovWeakPointComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (ANarrativeCharacter* NarrativeOwner =
		Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.RemoveDynamic(
			this,
			&ThisClass::HandleOwnerASCInitialized);
	}

	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void USovWeakPointComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USovWeakPointComponent, BrokenWeakPointIds);
}

bool USovWeakPointComponent::InitializeWithAbilitySystem(
	UNarrativeAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!IsValid(InAbilitySystemComponent) || !IsValid(GetOwner()))
	{
		UninitializeFromAbilitySystem();
		return false;
	}

	if (GetOwner()->FindComponentByClass<USovWeakPointComponent>() != this)
	{
		UE_LOG(
			LogSovWeakPoint,
			Error,
			TEXT("%s has more than one Weak Point component. Ignoring duplicate %s."),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(this));
		return false;
	}

	if (AbilitySystemComponent == InAbilitySystemComponent)
	{
		return true;
	}

	UninitializeFromAbilitySystem();
	AbilitySystemComponent = InAbilitySystemComponent;
	AbilitySystemComponent->OnDamageResolvedAsTarget.AddUniqueDynamic(
		this,
		&ThisClass::HandleDamageResolvedAsTarget);
	AbilitySystemComponent->OnDeathStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleDeathStateChanged);
	return true;
}

bool USovWeakPointComponent::IsInitialized() const
{
	return IsValid(AbilitySystemComponent.Get());
}

bool USovWeakPointComponent::IsWeakPointBroken(
	const FName WeakPointId) const
{
	return WeakPointId != NAME_None
		&& BrokenWeakPointIds.Contains(WeakPointId);
}

bool USovWeakPointComponent::HasValidWeakPointConfiguration() const
{
	TSet<FName> SeenZoneIds;
	for (const FSovWeakPointZone& Zone : WeakPointZones)
	{
		if (Zone.ZoneId == NAME_None
			|| SeenZoneIds.Contains(Zone.ZoneId)
			|| (Zone.HitBones.IsEmpty() && Zone.PhysicalMaterials.IsEmpty()))
		{
			return false;
		}
		SeenZoneIds.Add(Zone.ZoneId);
	}
	return true;
}

ESovWeakPointHitResolution USovWeakPointComponent::ResolveWeakPointHit(
	const FSovDamageResult& DamageResult,
	FName& OutWeakPointId)
{
	OutWeakPointId = NAME_None;
	if (!GetOwner()
		|| !GetOwner()->HasAuthority()
		|| !DamageResult.TransactionId.IsValid()
		|| DamageResult.TargetActor.Get() != GetOwner()
		|| DamageResult.SourceActor.Get() == GetOwner()
		|| DamageResult.AppliedShieldDamage
			+ DamageResult.AppliedHealthDamage
			+ KINDA_SMALL_NUMBER
			< FMath::Max(MinimumAppliedDamage, 0.0f))
	{
		return ESovWeakPointHitResolution::NotWeakPoint;
	}

	const FSovWeakPointZone* MatchingZone = FindMatchingZone(DamageResult);
	if (!MatchingZone)
	{
		return ESovWeakPointHitResolution::NotWeakPoint;
	}

	OutWeakPointId = MatchingZone->ZoneId;
	if (IsWeakPointBroken(MatchingZone->ZoneId))
	{
		return ESovWeakPointHitResolution::AlreadyBroken;
	}

	SetWeakPointBroken(MatchingZone->ZoneId, true);
	FPendingWeakPointBreak& PendingBreak = PendingBreaks.AddDefaulted_GetRef();
	PendingBreak.TransactionId = DamageResult.TransactionId;
	PendingBreak.SourceActor = DamageResult.SourceActor.Get();
	PendingBreak.EffectContext = DamageResult.EffectContext;
	PendingBreak.HitZone = DamageResult.HitZone;
	PendingBreak.WeakPointId = MatchingZone->ZoneId;
	OnWeakPointBroken.Broadcast(MatchingZone->ZoneId, DamageResult);
	return ESovWeakPointHitResolution::NewlyBroken;
}

bool USovWeakPointComponent::ConsumeWeakPointBreak(
	const FSovDamageResult& DamageResult,
	FName& OutWeakPointId)
{
	OutWeakPointId = NAME_None;
	if (!GetOwner()
		|| !GetOwner()->HasAuthority()
		|| DamageResult.TargetActor.Get() != GetOwner())
	{
		return false;
	}

	for (int32 Index = 0; Index < PendingBreaks.Num(); ++Index)
	{
		const FPendingWeakPointBreak& PendingBreak = PendingBreaks[Index];
		if (PendingBreak.WeakPointId != NAME_None
			&& PendingBreak.TransactionId.IsValid()
			&& PendingBreak.TransactionId == DamageResult.TransactionId
			&& PendingBreak.SourceActor.Get() == DamageResult.SourceActor.Get()
			&& PendingBreak.EffectContext.Get()
				== DamageResult.EffectContext.Get()
			&& PendingBreak.HitZone == DamageResult.HitZone)
		{
			OutWeakPointId = PendingBreak.WeakPointId;
			PendingBreaks.RemoveAt(Index);
			return true;
		}
	}
	return false;
}

void USovWeakPointComponent::ResetWeakPoints()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	TArray<FName> DesiredBrokenIds;
	for (const FSovWeakPointZone& Zone : WeakPointZones)
	{
		if (Zone.bStartsBroken && Zone.ZoneId != NAME_None)
		{
			DesiredBrokenIds.AddUnique(Zone.ZoneId);
		}
	}

	const TArray<FName> OldBrokenIds = BrokenWeakPointIds;
	BrokenWeakPointIds = MoveTemp(DesiredBrokenIds);
	ClearPendingBreaks();
	bAppliedStartingState = true;

	for (const FName OldId : OldBrokenIds)
	{
		if (!BrokenWeakPointIds.Contains(OldId))
		{
			OnWeakPointStateChanged.Broadcast(OldId, false);
		}
	}
	for (const FName NewId : BrokenWeakPointIds)
	{
		if (!OldBrokenIds.Contains(NewId))
		{
			OnWeakPointStateChanged.Broadcast(NewId, true);
		}
	}

	GetOwner()->ForceNetUpdate();
}

void USovWeakPointComponent::TryInitializeFromOwner()
{
	if (!IsValid(GetOwner()))
	{
		return;
	}

	UNarrativeAbilitySystemComponent* OwnerAbilitySystem =
		Cast<UNarrativeAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()));
	if (IsValid(OwnerAbilitySystem)
		&& OwnerAbilitySystem != AbilitySystemComponent.Get())
	{
		InitializeWithAbilitySystem(OwnerAbilitySystem);
	}
}

void USovWeakPointComponent::UninitializeFromAbilitySystem()
{
	if (IsValid(AbilitySystemComponent.Get()))
	{
		AbilitySystemComponent->OnDamageResolvedAsTarget.RemoveDynamic(
			this,
			&ThisClass::HandleDamageResolvedAsTarget);
		AbilitySystemComponent->OnDeathStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleDeathStateChanged);
	}

	AbilitySystemComponent = nullptr;
	ClearPendingBreaks();
}

void USovWeakPointComponent::ApplyAuthoredStartingState()
{
	if (bAppliedStartingState)
	{
		return;
	}

	TSet<FName> SeenZoneIds;
	for (const FSovWeakPointZone& Zone : WeakPointZones)
	{
		if (Zone.ZoneId == NAME_None)
		{
			UE_LOG(
				LogSovWeakPoint,
				Warning,
				TEXT("%s has a Weak Point zone with no ZoneId; it will never break."),
				*GetNameSafe(GetOwner()));
			continue;
		}
		if (SeenZoneIds.Contains(Zone.ZoneId))
		{
			UE_LOG(
				LogSovWeakPoint,
				Warning,
				TEXT("%s has duplicate Weak Point ZoneId %s; only the first match is used."),
				*GetNameSafe(GetOwner()),
				*Zone.ZoneId.ToString());
		}
		SeenZoneIds.Add(Zone.ZoneId);
	}

	ResetWeakPoints();
}

void USovWeakPointComponent::ClearPendingBreaks()
{
	PendingBreaks.Reset();
}

const FSovWeakPointZone* USovWeakPointComponent::FindMatchingZone(
	const FSovDamageResult& DamageResult) const
{
	for (const FSovWeakPointZone& Zone : WeakPointZones)
	{
		if (Zone.ZoneId != NAME_None
			&& (MatchesBone(Zone, DamageResult)
				|| MatchesPhysicalMaterial(Zone, DamageResult)))
		{
			return &Zone;
		}
	}
	return nullptr;
}

bool USovWeakPointComponent::MatchesBone(
	const FSovWeakPointZone& Zone,
	const FSovDamageResult& DamageResult) const
{
	if (DamageResult.HitZone == NAME_None || Zone.HitBones.IsEmpty())
	{
		return false;
	}
	if (Zone.HitBones.Contains(DamageResult.HitZone))
	{
		return true;
	}
	if (!Zone.bMatchDescendantBones)
	{
		return false;
	}

	const FHitResult* HitResult = DamageResult.EffectContext.GetHitResult();
	const USkinnedMeshComponent* HitMesh = HitResult
		? Cast<USkinnedMeshComponent>(HitResult->GetComponent())
		: nullptr;
	if (!IsValid(HitMesh))
	{
		return false;
	}

	FName CurrentBone = DamageResult.HitZone;
	while (CurrentBone != NAME_None)
	{
		CurrentBone = HitMesh->GetParentBone(CurrentBone);
		if (Zone.HitBones.Contains(CurrentBone))
		{
			return true;
		}
	}
	return false;
}

bool USovWeakPointComponent::MatchesPhysicalMaterial(
	const FSovWeakPointZone& Zone,
	const FSovDamageResult& DamageResult) const
{
	if (Zone.PhysicalMaterials.IsEmpty())
	{
		return false;
	}

	const FHitResult* HitResult = DamageResult.EffectContext.GetHitResult();
	const UPhysicalMaterial* HitMaterial = HitResult
		? HitResult->PhysMaterial.Get()
		: nullptr;
	if (!IsValid(HitMaterial))
	{
		return false;
	}
	for (const TObjectPtr<UPhysicalMaterial>& AuthoredMaterial :
		Zone.PhysicalMaterials)
	{
		if (AuthoredMaterial.Get() == HitMaterial)
		{
			return true;
		}
	}
	return false;
}

void USovWeakPointComponent::SetWeakPointBroken(
	const FName WeakPointId,
	const bool bShouldBeBroken)
{
	if (WeakPointId == NAME_None
		|| !GetOwner()
		|| !GetOwner()->HasAuthority()
		|| IsWeakPointBroken(WeakPointId) == bShouldBeBroken)
	{
		return;
	}

	if (bShouldBeBroken)
	{
		BrokenWeakPointIds.AddUnique(WeakPointId);
	}
	else
	{
		BrokenWeakPointIds.Remove(WeakPointId);
	}
	OnWeakPointStateChanged.Broadcast(WeakPointId, bShouldBeBroken);
	GetOwner()->ForceNetUpdate();
}

void USovWeakPointComponent::HandleOwnerASCInitialized()
{
	TryInitializeFromOwner();
}

void USovWeakPointComponent::HandleDamageResolvedAsTarget(
	const FSovDamageResult& DamageResult)
{
	FName ResolvedWeakPointId = NAME_None;
	ResolveWeakPointHit(DamageResult, ResolvedWeakPointId);
}

void USovWeakPointComponent::HandleDeathStateChanged(
	AActor* ChangedActor,
	UNarrativeAbilitySystemComponent* ChangedActorASC,
	const bool bIsDead)
{
	if (!bIsDead
		&& (ChangedActor == GetOwner()
			|| ChangedActorASC == AbilitySystemComponent.Get()))
	{
		ResetWeakPoints();
	}
}

void USovWeakPointComponent::OnRep_BrokenWeakPointIds(
	const TArray<FName>& OldBrokenWeakPointIds)
{
	for (const FName OldId : OldBrokenWeakPointIds)
	{
		if (!BrokenWeakPointIds.Contains(OldId))
		{
			OnWeakPointStateChanged.Broadcast(OldId, false);
		}
	}
	for (const FName NewId : BrokenWeakPointIds)
	{
		if (!OldBrokenWeakPointIds.Contains(NewId))
		{
			OnWeakPointStateChanged.Broadcast(NewId, true);
		}
	}
}
