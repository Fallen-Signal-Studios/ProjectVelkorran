// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovWeakPointComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Components/DecalComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NarrativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sovereign/SovGameplayTags.h"
#include "TimerManager.h"
#include "UnrealFramework/NarrativeCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovWeakPoint, Log, All);

namespace
{
	/**
	 * Reveal presentation is derived from the same matchable zones used by the
	 * damage resolver. A socket-only entry is presentation authoring, not a
	 * gameplay weak point, and must never create a phantom target.
	 */
	bool HasWeakPointGameplayMatcher(const FSovWeakPointZone& Zone)
	{
		return Zone.HitBones.ContainsByPredicate(
			[](const FName BoneName)
			{
				return BoneName != NAME_None;
			})
			|| Zone.PhysicalMaterials.ContainsByPredicate(
				[](const TObjectPtr<UPhysicalMaterial>& PhysicalMaterial)
				{
					return IsValid(PhysicalMaterial.Get());
				});
	}

	bool IsRevealableWeakPointZone(const FSovWeakPointZone& Zone)
	{
		return Zone.ZoneId != NAME_None
			&& HasWeakPointGameplayMatcher(Zone);
	}
}

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
		NarrativeOwner->CharacterVisualInitialized.AddUniqueDynamic(
			this,
			&ThisClass::HandleCharacterVisualInitialized);
		BindCharacterVisual(NarrativeOwner->GetCharacterVisual());
	}
	TryInitializeFromOwner();
	ApplyWeakPointRevealState();
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
		NarrativeOwner->CharacterVisualInitialized.RemoveDynamic(
			this,
			&ThisClass::HandleCharacterVisualInitialized);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			WeakPointRevealExpiryTimerHandle);
		World->GetTimerManager().ClearTimer(
			WeakPointRevealVisualRefreshTimerHandle);
	}
	bWeakPointRevealVisualRefreshPending = false;
	BindCharacterVisual(nullptr);
	ClearWeakPointRevealPresentation();
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void USovWeakPointComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USovWeakPointComponent, BrokenWeakPointIds);
	DOREPLIFETIME(USovWeakPointComponent, WeakPointRevealState);
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
	ApplyWeakPointRevealState();
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

bool USovWeakPointComponent::RevealWeakPoints(
	const float Duration,
	AActor* RevealInstigator)
{
	AActor* Owner = GetOwner();
	const UNarrativeAbilitySystemComponent* NarrativeAbilitySystem =
		AbilitySystemComponent.Get();
	if (!IsValid(Owner)
		|| !Owner->HasAuthority()
		|| (IsValid(NarrativeAbilitySystem)
			&& (NarrativeAbilitySystem->IsDead()
				|| NarrativeAbilitySystem->HasMatchingGameplayTag(
					FNarrativeGameplayTags::Get().State_IsDead)
				|| NarrativeAbilitySystem->HasMatchingGameplayTag(
					FSovGameplayTags::Get().State_Fatal)))
		|| !FMath::IsFinite(Duration)
		|| Duration <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	TSet<FName> SeenZoneIds;
	const bool bHasUnbrokenZone = WeakPointZones.ContainsByPredicate(
		[this, &SeenZoneIds](const FSovWeakPointZone& Zone)
		{
			if (Zone.ZoneId == NAME_None
				|| SeenZoneIds.Contains(Zone.ZoneId))
			{
				return false;
			}
			SeenZoneIds.Add(Zone.ZoneId);
			return IsRevealableWeakPointZone(Zone)
				&& !IsWeakPointBroken(Zone.ZoneId);
		});
	if (!bHasUnbrokenZone)
	{
		return false;
	}

	const float RequestedEndTime =
		GetSynchronizedServerWorldTimeSeconds() + Duration;
	WeakPointRevealState.EndServerWorldTime = FMath::Max(
		WeakPointRevealState.EndServerWorldTime,
		RequestedEndTime);
	WeakPointRevealState.RevealInstigator = RevealInstigator;
	WeakPointRevealState.Serial =
		WeakPointRevealState.Serial >= MAX_int32
			? 1
			: WeakPointRevealState.Serial + 1;

	ApplyWeakPointRevealState();
	Owner->FlushNetDormancy();
	Owner->ForceNetUpdate();
	return true;
}

void USovWeakPointComponent::ClearWeakPointReveal()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	const bool bHadRevealState =
		WeakPointRevealState.EndServerWorldTime > 0.0f
		|| IsValid(WeakPointRevealState.RevealInstigator.Get());
	if (!bHadRevealState)
	{
		ApplyWeakPointRevealState();
		return;
	}

	WeakPointRevealState.EndServerWorldTime = 0.0f;
	WeakPointRevealState.RevealInstigator = nullptr;
	WeakPointRevealState.Serial =
		WeakPointRevealState.Serial >= MAX_int32
			? 1
			: WeakPointRevealState.Serial + 1;

	ApplyWeakPointRevealState();
	Owner->FlushNetDormancy();
	Owner->ForceNetUpdate();
}

bool USovWeakPointComponent::IsWeakPointRevealActive() const
{
	const UNarrativeAbilitySystemComponent* NarrativeAbilitySystem =
		AbilitySystemComponent.Get();
	if (IsValid(NarrativeAbilitySystem)
		&& (NarrativeAbilitySystem->IsDead()
			|| NarrativeAbilitySystem->HasMatchingGameplayTag(
				FNarrativeGameplayTags::Get().State_IsDead)
			|| NarrativeAbilitySystem->HasMatchingGameplayTag(
				FSovGameplayTags::Get().State_Fatal)))
	{
		return false;
	}

	return WeakPointRevealState.EndServerWorldTime
		> GetSynchronizedServerWorldTimeSeconds() + KINDA_SMALL_NUMBER;
}

float USovWeakPointComponent::GetWeakPointRevealRemainingSeconds() const
{
	if (!IsWeakPointRevealActive())
	{
		return 0.0f;
	}

	return FMath::Max(
		WeakPointRevealState.EndServerWorldTime
			- GetSynchronizedServerWorldTimeSeconds(),
		0.0f);
}

TArray<FName> USovWeakPointComponent::GetRevealedWeakPointIds() const
{
	TArray<FName> RevealedIds;
	if (!IsWeakPointRevealActive())
	{
		return RevealedIds;
	}

	TSet<FName> SeenZoneIds;
	for (const FSovWeakPointZone& Zone : WeakPointZones)
	{
		if (Zone.ZoneId == NAME_None
			|| SeenZoneIds.Contains(Zone.ZoneId))
		{
			continue;
		}
		SeenZoneIds.Add(Zone.ZoneId);
		if (IsRevealableWeakPointZone(Zone)
			&& !IsWeakPointBroken(Zone.ZoneId))
		{
			RevealedIds.Add(Zone.ZoneId);
		}
	}
	return RevealedIds;
}

bool USovWeakPointComponent::HasValidWeakPointConfiguration() const
{
	TSet<FName> SeenZoneIds;
	for (const FSovWeakPointZone& Zone : WeakPointZones)
	{
		if (Zone.ZoneId == NAME_None
			|| SeenZoneIds.Contains(Zone.ZoneId)
			|| !HasWeakPointGameplayMatcher(Zone))
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
	ClearWeakPointReveal();

	TArray<FName> DesiredBrokenIds;
	TSet<FName> SeenZoneIds;
	for (const FSovWeakPointZone& Zone : WeakPointZones)
	{
		if (Zone.ZoneId == NAME_None
			|| SeenZoneIds.Contains(Zone.ZoneId))
		{
			continue;
		}
		SeenZoneIds.Add(Zone.ZoneId);
		if (Zone.bStartsBroken && IsRevealableWeakPointZone(Zone))
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

void USovWeakPointComponent::RefreshWeakPointRevealPresentation()
{
	DestroyActiveRevealDecals();

	const UWorld* World = GetWorld();
	const float RemainingSeconds = GetWeakPointRevealRemainingSeconds();
	if (!IsValid(World)
		|| World->GetNetMode() == NM_DedicatedServer
		|| RemainingSeconds <= KINDA_SMALL_NUMBER)
	{
		ClearDecalReceiverBindings();
		return;
	}

	const TArray<FName> RevealedWeakPointIds = GetRevealedWeakPointIds();
	TSet<FName> InspectedZoneIds;
	const bool bHasPresentationMaterial = WeakPointZones.ContainsByPredicate(
		[this, &RevealedWeakPointIds, &InspectedZoneIds](
			const FSovWeakPointZone& Zone)
		{
			if (Zone.ZoneId == NAME_None
				|| InspectedZoneIds.Contains(Zone.ZoneId))
			{
				return false;
			}
			InspectedZoneIds.Add(Zone.ZoneId);
			return IsRevealableWeakPointZone(Zone)
				&& RevealedWeakPointIds.Contains(Zone.ZoneId)
				&& (IsValid(Zone.RevealDecalMaterialOverride.Get())
					|| IsValid(WeakPointRevealDecalMaterial.Get()));
		});
	if (!bHasPresentationMaterial)
	{
		ClearDecalReceiverBindings();
		return;
	}

	RefreshDecalReceiverBindings();
	TSet<FName> PresentedZoneIds;
	for (const FSovWeakPointZone& Zone : WeakPointZones)
	{
		if (!IsRevealableWeakPointZone(Zone)
			|| !RevealedWeakPointIds.Contains(Zone.ZoneId)
			|| PresentedZoneIds.Contains(Zone.ZoneId))
		{
			continue;
		}
		PresentedZoneIds.Add(Zone.ZoneId);

		UMaterialInterface* SourceMaterial =
			IsValid(Zone.RevealDecalMaterialOverride.Get())
				? Zone.RevealDecalMaterialOverride.Get()
				: WeakPointRevealDecalMaterial.Get();
		if (!IsValid(SourceMaterial))
		{
			continue;
		}

		const FName AttachPoint = ResolveRevealAttachPoint(Zone);
		UMeshComponent* AttachmentMesh =
			ResolveRevealAttachmentMesh(Zone, AttachPoint);
		if (!IsValid(AttachmentMesh))
		{
			continue;
		}

		UMaterialInstanceDynamic* RevealMaterial =
			UMaterialInstanceDynamic::Create(SourceMaterial, this);
		if (!IsValid(RevealMaterial))
		{
			continue;
		}
		if (RevealColorParameterName != NAME_None)
		{
			RevealMaterial->SetVectorParameterValue(
				RevealColorParameterName,
				WeakPointRevealColor);
		}

		const FVector DecalSize(
			FMath::Max(FMath::Abs(Zone.RevealDecalSize.X), 1.0f),
			FMath::Max(FMath::Abs(Zone.RevealDecalSize.Y), 1.0f),
			FMath::Max(FMath::Abs(Zone.RevealDecalSize.Z), 1.0f));
		UDecalComponent* Decal = UGameplayStatics::SpawnDecalAttached(
			RevealMaterial,
			DecalSize,
			AttachmentMesh,
			AttachPoint,
			Zone.RevealRelativeTransform.GetLocation(),
			Zone.RevealRelativeTransform.Rotator(),
			EAttachLocation::KeepRelativeOffset,
			RemainingSeconds);
		if (!IsValid(Decal))
		{
			continue;
		}

		Decal->SetRelativeScale3D(
			Zone.RevealRelativeTransform.GetScale3D());
		const float FadeOutSeconds = FMath::Clamp(
			RevealFadeOutDuration,
			0.0f,
			RemainingSeconds);
		if (FadeOutSeconds > KINDA_SMALL_NUMBER)
		{
			Decal->SetFadeOut(
				FMath::Max(RemainingSeconds - FadeOutSeconds, 0.0f),
				FadeOutSeconds,
				false);
		}
		ActiveRevealDecals.Add(Decal);
	}
}

void USovWeakPointComponent::BindCharacterVisual(
	ANarrativeCharacterVisual* NewCharacterVisual)
{
	if (BoundCharacterVisual == NewCharacterVisual)
	{
		return;
	}

	if (IsValid(BoundCharacterVisual.Get()))
	{
		BoundCharacterVisual->OnBaseAppearanceApplied.RemoveDynamic(
			this,
			&ThisClass::HandleBaseAppearanceApplied);
		BoundCharacterVisual->OnAppearancePartChanged.RemoveDynamic(
			this,
			&ThisClass::HandleAppearancePartChanged);
	}

	BoundCharacterVisual = NewCharacterVisual;
	if (IsValid(BoundCharacterVisual.Get()))
	{
		BoundCharacterVisual->OnBaseAppearanceApplied.AddUniqueDynamic(
			this,
			&ThisClass::HandleBaseAppearanceApplied);
		BoundCharacterVisual->OnAppearancePartChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleAppearancePartChanged);
	}
}

void USovWeakPointComponent::ApplyWeakPointRevealState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			WeakPointRevealExpiryTimerHandle);
	}

	const bool bTimedRevealActive = IsWeakPointRevealActive();
	const bool bShouldBeActive = bTimedRevealActive
		&& !GetRevealedWeakPointIds().IsEmpty();
	if (bTimedRevealActive)
	{
		ScheduleWeakPointRevealExpiry();
	}
	if (bShouldBeActive)
	{
		RefreshWeakPointRevealPresentation();
	}
	else
	{
		ClearWeakPointRevealPresentation();
	}

	if (bLocalWeakPointRevealActive != bShouldBeActive)
	{
		bLocalWeakPointRevealActive = bShouldBeActive;
		OnWeakPointRevealStateChanged.Broadcast(
			bShouldBeActive,
			bShouldBeActive
				? GetWeakPointRevealRemainingSeconds()
				: 0.0f,
			bShouldBeActive
				? WeakPointRevealState.RevealInstigator.Get()
				: nullptr);
	}
}

void USovWeakPointComponent::ScheduleWeakPointRevealExpiry()
{
	UWorld* World = GetWorld();
	const float RemainingSeconds = GetWeakPointRevealRemainingSeconds();
	if (!IsValid(World) || RemainingSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		WeakPointRevealExpiryTimerHandle,
		this,
		&ThisClass::HandleWeakPointRevealExpired,
		RemainingSeconds,
		false,
		RemainingSeconds);
}

void USovWeakPointComponent::ScheduleWeakPointRevealVisualRefresh()
{
	if (!IsWeakPointRevealActive()
		|| bWeakPointRevealVisualRefreshPending)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		bWeakPointRevealVisualRefreshPending = true;
		WeakPointRevealVisualRefreshTimerHandle =
			World->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(
					this,
					&ThisClass::HandleDeferredWeakPointRevealVisualRefresh));
	}
	else
	{
		RefreshWeakPointRevealPresentation();
	}
}

void USovWeakPointComponent::HandleWeakPointRevealExpired()
{
	if (IsWeakPointRevealActive())
	{
		ScheduleWeakPointRevealExpiry();
		return;
	}

	if (AActor* Owner = GetOwner(); IsValid(Owner) && Owner->HasAuthority())
	{
		ClearWeakPointReveal();
	}
	else
	{
		ApplyWeakPointRevealState();
	}
}

void USovWeakPointComponent::HandleDeferredWeakPointRevealVisualRefresh()
{
	bWeakPointRevealVisualRefreshPending = false;
	RefreshWeakPointRevealPresentation();
}

void USovWeakPointComponent::ClearWeakPointRevealPresentation()
{
	DestroyActiveRevealDecals();
	ClearDecalReceiverBindings();
}

void USovWeakPointComponent::DestroyActiveRevealDecals()
{
	for (UDecalComponent* Decal : ActiveRevealDecals)
	{
		if (IsValid(Decal))
		{
			Decal->DestroyComponent();
		}
	}
	ActiveRevealDecals.Reset();
}

void USovWeakPointComponent::RefreshDecalReceiverBindings()
{
	ClearDecalReceiverBindings();

	TArray<UMeshComponent*> PresentationMeshes;
	GatherPresentationMeshes(PresentationMeshes);
	for (UMeshComponent* MeshComponent : PresentationMeshes)
	{
		if (!IsValid(MeshComponent))
		{
			continue;
		}

		FDecalReceiverBinding& Binding =
			DecalReceiverBindings.AddDefaulted_GetRef();
		Binding.MeshComponent = MeshComponent;
		Binding.bPreviouslyReceivedDecals = MeshComponent->bReceivesDecals;
		MeshComponent->SetReceivesDecals(true);
	}
}

void USovWeakPointComponent::ClearDecalReceiverBindings()
{
	for (const FDecalReceiverBinding& Binding : DecalReceiverBindings)
	{
		if (UMeshComponent* MeshComponent = Binding.MeshComponent.Get();
			IsValid(MeshComponent))
		{
			MeshComponent->SetReceivesDecals(
				Binding.bPreviouslyReceivedDecals);
		}
	}
	DecalReceiverBindings.Reset();
}

void USovWeakPointComponent::GatherPresentationMeshes(
	TArray<UMeshComponent*>& OutMeshes) const
{
	OutMeshes.Reset();
	const auto GatherFromActor = [&OutMeshes](AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return;
		}

		TInlineComponentArray<UMeshComponent*> MeshComponents;
		Actor->GetComponents(MeshComponents);
		for (UMeshComponent* MeshComponent : MeshComponents)
		{
			if (IsValid(MeshComponent)
				&& (MeshComponent->IsA<USkeletalMeshComponent>()
					|| MeshComponent->IsA<UStaticMeshComponent>()))
			{
				OutMeshes.AddUnique(MeshComponent);
			}
		}
	};

	GatherFromActor(GetOwner());
	GatherFromActor(BoundCharacterVisual.Get());
}

UMeshComponent* USovWeakPointComponent::ResolveRevealAttachmentMesh(
	const FSovWeakPointZone& Zone,
	const FName AttachPoint) const
{
	TArray<UMeshComponent*> PresentationMeshes;
	GatherPresentationMeshes(PresentationMeshes);
	UMeshComponent* HiddenFallback = nullptr;
	for (UMeshComponent* MeshComponent : PresentationMeshes)
	{
		if (!IsValid(MeshComponent)
			|| (Zone.RevealMeshComponentTag != NAME_None
				&& !MeshComponent->ComponentTags.Contains(
					Zone.RevealMeshComponentTag)))
		{
			continue;
		}

		if (AttachPoint != NAME_None)
		{
			USkeletalMeshComponent* SkeletalMesh =
				Cast<USkeletalMeshComponent>(MeshComponent);
			if (!IsValid(SkeletalMesh)
				|| (SkeletalMesh->GetBoneIndex(AttachPoint) == INDEX_NONE
					&& !SkeletalMesh->DoesSocketExist(AttachPoint)))
			{
				continue;
			}
		}

		if (MeshComponent->IsVisible() && !MeshComponent->bHiddenInGame)
		{
			return MeshComponent;
		}
		if (!IsValid(HiddenFallback))
		{
			HiddenFallback = MeshComponent;
		}
	}
	return HiddenFallback;
}

FName USovWeakPointComponent::ResolveRevealAttachPoint(
	const FSovWeakPointZone& Zone) const
{
	if (Zone.RevealAttachPoint != NAME_None)
	{
		return Zone.RevealAttachPoint;
	}
	for (const FName HitBone : Zone.HitBones)
	{
		if (HitBone != NAME_None)
		{
			return HitBone;
		}
	}
	return NAME_None;
}

float USovWeakPointComponent::GetSynchronizedServerWorldTimeSeconds() const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return 0.0f;
	}
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	return World->GetTimeSeconds();
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
		if (!HasWeakPointGameplayMatcher(Zone))
		{
			UE_LOG(
				LogSovWeakPoint,
				Warning,
				TEXT("%s Weak Point zone %s has no valid hit bone or physical material; it cannot break or reveal."),
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
	TSet<FName> SeenZoneIds;
	for (const FSovWeakPointZone& Zone : WeakPointZones)
	{
		if (Zone.ZoneId == NAME_None
			|| SeenZoneIds.Contains(Zone.ZoneId))
		{
			continue;
		}
		SeenZoneIds.Add(Zone.ZoneId);
		if (IsRevealableWeakPointZone(Zone)
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
	ApplyWeakPointRevealState();
	GetOwner()->ForceNetUpdate();
}

void USovWeakPointComponent::HandleOwnerASCInitialized()
{
	TryInitializeFromOwner();
}

void USovWeakPointComponent::HandleCharacterVisualInitialized(
	ANarrativeCharacter* Character)
{
	if (Character != GetOwner())
	{
		return;
	}

	BindCharacterVisual(Character->GetCharacterVisual());
	ScheduleWeakPointRevealVisualRefresh();
}

void USovWeakPointComponent::HandleBaseAppearanceApplied()
{
	ScheduleWeakPointRevealVisualRefresh();
}

void USovWeakPointComponent::HandleAppearancePartChanged(
	const FGameplayTag AppearanceSlot)
{
	static_cast<void>(AppearanceSlot);
	ScheduleWeakPointRevealVisualRefresh();
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
	if (ChangedActor != GetOwner()
		&& ChangedActorASC != AbilitySystemComponent.Get())
	{
		return;
	}

	if (bIsDead)
	{
		if (AActor* Owner = GetOwner();
			IsValid(Owner) && Owner->HasAuthority())
		{
			ClearWeakPointReveal();
		}
		else
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(
					WeakPointRevealExpiryTimerHandle);
			}
			ClearWeakPointRevealPresentation();
			if (bLocalWeakPointRevealActive)
			{
				bLocalWeakPointRevealActive = false;
				OnWeakPointRevealStateChanged.Broadcast(
					false,
					0.0f,
					nullptr);
			}
		}
	}
	else
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
	ApplyWeakPointRevealState();
}

void USovWeakPointComponent::OnRep_WeakPointRevealState()
{
	ApplyWeakPointRevealState();
}
