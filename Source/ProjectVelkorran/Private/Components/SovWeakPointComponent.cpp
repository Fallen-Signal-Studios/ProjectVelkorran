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
#include "Effects/SovGameplayEffect_WeakPointConsequence.h"
#include "GameplayEffect.h"
#include "NarrativeGameplayTags.h"
#include "GameFramework/GameStateBase.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"
#include "UnrealFramework/NarrativeCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovWeakPoint, Log, All);

USovWeakPointComponent::USovWeakPointComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

const FSovWeakPointZone* USovWeakPointComponent::FindZoneById(const FName ZoneId) const
{
	return WeakPointZones.FindByPredicate([ZoneId](const FSovWeakPointZone& Zone)
	{
		return ZoneId != NAME_None && Zone.ZoneId == ZoneId;
	});
}

void USovWeakPointComponent::PrepareForSave_Implementation()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SavedWeakPointState = CaptureWeakPointState();
		bHasSavedWeakPointState = true;
	}
}

void USovWeakPointComponent::Load_Implementation()
{
	if (bHasSavedWeakPointState)
	{
		RestoreWeakPointState(SavedWeakPointState);
	}
}

FSovWeakPointStateSnapshot USovWeakPointComponent::CaptureWeakPointState() const
{
	if (bPendingConsequenceRestore)
	{
		return PendingConsequenceRestore;
	}
	FSovWeakPointStateSnapshot State;
	State.BrokenZoneIds = BrokenWeakPointIds;
	if (AbilitySystemComponent && GetWorld())
	{
		for (const FActiveConsequence& Consequence : ActiveConsequences)
		{
			const FActiveGameplayEffect* Effect = AbilitySystemComponent->GetActiveGameplayEffect(Consequence.Handle);
			if (!Effect) { continue; }
			const float Remaining = Effect->GetTimeRemaining(GetWorld()->GetTimeSeconds());
			if (Remaining > KINDA_SMALL_NUMBER || Effect->Spec.GetDuration() == UGameplayEffect::INFINITE_DURATION)
			{
				FSovWeakPointConsequenceSnapshot& Entry = State.Consequences.AddDefaulted_GetRef();
				Entry.ZoneId = Consequence.ZoneId;
				Entry.RemainingSeconds = Effect->Spec.GetDuration() == UGameplayEffect::INFINITE_DURATION ? -1.f : Remaining;
			}
		}
	}
	return State;
}

bool USovWeakPointComponent::CanRestoreWeakPointState(const FSovWeakPointStateSnapshot& State) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bRestoringState || bUninitializingState || !HasValidWeakPointConfiguration())
	{
		return false;
	}
	TSet<FName> BrokenIds;
	for (const FName Id : State.BrokenZoneIds)
	{
		if (!FindZoneById(Id) || BrokenIds.Contains(Id)) { return false; }
		BrokenIds.Add(Id);
	}
	TSet<FName> ConsequenceIds;
	for (const FSovWeakPointConsequenceSnapshot& Entry : State.Consequences)
	{
		const FSovWeakPointZone* Zone = FindZoneById(Entry.ZoneId);
		if (!Zone || !BrokenIds.Contains(Entry.ZoneId) || ConsequenceIds.Contains(Entry.ZoneId)
			|| !FMath::IsFinite(Entry.RemainingSeconds)
			|| (Entry.RemainingSeconds != -1.f && Entry.RemainingSeconds <= KINDA_SMALL_NUMBER)
			|| ((Zone->Consequence.Duration == 0.f) != (Entry.RemainingSeconds == -1.f))
			|| (Entry.RemainingSeconds > Zone->Consequence.Duration && Entry.RemainingSeconds != -1.f))
		{
			return false;
		}
		ConsequenceIds.Add(Entry.ZoneId);
	}
	return true;
}

bool USovWeakPointComponent::RestoreWeakPointState(const FSovWeakPointStateSnapshot& State)
{
	if (!CanRestoreWeakPointState(State)) { return false; }
	// State may alias a member cleared by lifecycle callbacks.
	const FSovWeakPointStateSnapshot Snapshot = State;
	TGuardValue<bool> Restoring(bRestoringState, true);
	++StateEpoch;
	ClearPendingBreaks();
	ClearWeakPointReveal();
	ClearConsequences();
	const TArray<FName> OldIds = BrokenWeakPointIds;
	BrokenWeakPointIds = Snapshot.BrokenZoneIds;
	bAppliedStartingState = true;
	bPendingConsequenceRestore = !IsInitialized();
	PendingConsequenceRestore = Snapshot;
	if (IsInitialized())
	{
		for (const FSovWeakPointConsequenceSnapshot& Entry : Snapshot.Consequences)
		{
			if (const FSovWeakPointZone* Zone = FindZoneById(Entry.ZoneId))
			{
				ApplyConsequence(*Zone, Entry.RemainingSeconds, false);
			}
		}
	}
	for (const FName Id : OldIds)
	{
		if (!BrokenWeakPointIds.Contains(Id)) { OnWeakPointStateChanged.Broadcast(Id, false); }
	}
	for (const FName Id : BrokenWeakPointIds)
	{
		if (!OldIds.Contains(Id)) { OnWeakPointStateChanged.Broadcast(Id, true); }
	}
	ApplyWeakPointRevealState();
	GetOwner()->FlushNetDormancy();
	GetOwner()->ForceNetUpdate();
	return true;
}

bool USovWeakPointComponent::BreakWeakPointWithoutReward(const FName WeakPointId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bRestoringState || bUninitializingState
		|| !FindZoneById(WeakPointId) || IsWeakPointBroken(WeakPointId)) { return false; }
	SetWeakPointBroken(WeakPointId, true);
	return IsWeakPointBroken(WeakPointId);
}

void USovWeakPointComponent::ApplyConsequence(const FSovWeakPointZone& Zone, const float RemainingSeconds, const bool bCancelActiveAbilities)
{
	UNarrativeAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC || !GetOwner() || !GetOwner()->HasAuthority()
		|| !FMath::IsFinite(RemainingSeconds)
		|| (RemainingSeconds != -1.f && RemainingSeconds <= KINDA_SMALL_NUMBER)
		|| (Zone.Consequence.BlockedAbilityTags.IsEmpty() && Zone.Consequence.GrantedStateTags.IsEmpty()
			&& Zone.Consequence.CancelAbilityTags.IsEmpty())
		|| ActiveConsequences.ContainsByPredicate([&Zone](const FActiveConsequence& Entry) { return Entry.ZoneId == Zone.ZoneId; }))
	{
		return;
	}
	const FGameplayTagContainer CancelTags = Zone.Consequence.CancelAbilityTags;
	const uint32 Epoch = StateEpoch;
	const uint32 Serial = ++ConsequenceSerial;
	FActiveConsequence& Entry = ActiveConsequences.AddDefaulted_GetRef();
	Entry.ZoneId = Zone.ZoneId;
	Entry.Serial = Serial;
	Entry.BlockedAbilityTags = Zone.Consequence.BlockedAbilityTags;
	ASC->BlockAbilitiesWithTags(Entry.BlockedAbilityTags);
	const UGameplayEffect* Effect = RemainingSeconds < 0.f
		? static_cast<const UGameplayEffect*>(GetDefault<USovGameplayEffect_WeakPointPermanentConsequence>())
		: static_cast<const UGameplayEffect*>(GetDefault<USovGameplayEffect_WeakPointTimedConsequence>());
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	FGameplayEffectSpec Spec(Effect, Context, 1.f);
	Spec.DynamicGrantedTags.AppendTags(Zone.Consequence.GrantedStateTags);
	if (RemainingSeconds > 0.f)
	{
		Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Duration, RemainingSeconds);
	}
	const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(Spec);
	FActiveConsequence* Stored = ActiveConsequences.FindByPredicate([Serial](const FActiveConsequence& Item) { return Item.Serial == Serial; });
	if (!Stored || StateEpoch != Epoch || AbilitySystemComponent != ASC)
	{
		if (Handle.IsValid()) { ASC->RemoveActiveGameplayEffect(Handle); }
		return;
	}
	Stored->Handle = Handle;
	if (!Handle.IsValid() || !ASC->GetActiveGameplayEffect(Handle))
	{
		const FGameplayTagContainer Blocks = Stored->BlockedAbilityTags;
		ActiveConsequences.RemoveAll([Serial](const FActiveConsequence& Item) { return Item.Serial == Serial; });
		ASC->UnBlockAbilitiesWithTags(Blocks);
		return;
	}
	if (bCancelActiveAbilities && !CancelTags.IsEmpty())
	{
		ASC->CancelAbilities(&CancelTags);
	}
}

void USovWeakPointComponent::HandleConsequenceRemoved(const FActiveGameplayEffect& Effect)
{
	const int32 Index = ActiveConsequences.IndexOfByPredicate([&Effect](const FActiveConsequence& Entry) { return Entry.Handle == Effect.Handle; });
	if (Index != INDEX_NONE)
	{
		const FGameplayTagContainer Blocks = ActiveConsequences[Index].BlockedAbilityTags;
		ActiveConsequences.RemoveAt(Index);
		if (AbilitySystemComponent) { AbilitySystemComponent->UnBlockAbilitiesWithTags(Blocks); }
	}
}

void USovWeakPointComponent::ClearConsequences()
{
	const TArray<FActiveConsequence> Removed = MoveTemp(ActiveConsequences);
	ActiveConsequences.Reset();
	UNarrativeAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC) { return; }
	for (const FActiveConsequence& Entry : Removed)
	{
		ASC->UnBlockAbilitiesWithTags(Entry.BlockedAbilityTags);
		if (Entry.Handle.IsValid()) { ASC->RemoveActiveGameplayEffect(Entry.Handle); }
	}
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
	if (bRestoringState || bUninitializingState) { return false; }
	if (!IsValid(InAbilitySystemComponent) || !IsValid(GetOwner())
		|| InAbilitySystemComponent->GetAvatarActor() != GetOwner())
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
	AbilitySystemComponent->OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &ThisClass::HandleConsequenceRemoved);
	if (GetOwner()->HasAuthority())
	{
		if (bPendingConsequenceRestore)
		{
			const FSovWeakPointStateSnapshot Pending = PendingConsequenceRestore;
			bPendingConsequenceRestore = false;
			RestoreWeakPointState(Pending);
		}
		else { ApplyAuthoredStartingState(); }
	}
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
	if (!IsValid(Owner)
		|| !Owner->HasAuthority()
		|| (IsValid(AbilitySystemComponent.Get())
			&& AbilitySystemComponent->IsDead())
		|| !FMath::IsFinite(Duration)
		|| Duration <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const bool bHasUnbrokenZone = WeakPointZones.ContainsByPredicate(
		[this](const FSovWeakPointZone& Zone)
		{
			return Zone.ZoneId != NAME_None
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
	if (IsValid(AbilitySystemComponent.Get())
		&& AbilitySystemComponent->IsDead())
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

	for (const FSovWeakPointZone& Zone : WeakPointZones)
	{
		if (Zone.ZoneId != NAME_None
			&& !IsWeakPointBroken(Zone.ZoneId))
		{
			RevealedIds.AddUnique(Zone.ZoneId);
		}
	}
	return RevealedIds;
}

TArray<FVector> USovWeakPointComponent::GetRevealedWeakPointAnchors() const
{
	TArray<FVector> Result;
	if (!IsWeakPointRevealActive()) { return Result; }
	for (const FSovWeakPointZone& Zone : WeakPointZones)
	{
		if (Zone.ZoneId.IsNone() || IsWeakPointBroken(Zone.ZoneId)) { continue; }
		const FName AttachPoint = ResolveRevealAttachPoint(Zone);
		if (const UMeshComponent* Mesh = ResolveRevealAttachmentMesh(Zone, AttachPoint))
		{
			const FVector Anchor = Mesh->GetSocketTransform(AttachPoint).TransformPosition(Zone.RevealRelativeTransform.GetLocation());
			if (!Anchor.ContainsNaN()) { Result.Add(Anchor); }
		}
	}
	return Result;
}

bool USovWeakPointComponent::HasValidWeakPointConfiguration() const
{
	TSet<FName> SeenZoneIds;
	for (const FSovWeakPointZone& Zone : WeakPointZones)
	{
		if (Zone.ZoneId == NAME_None
			|| SeenZoneIds.Contains(Zone.ZoneId)
			|| (Zone.HitBones.IsEmpty() && Zone.PhysicalMaterials.IsEmpty())
			|| !FMath::IsFinite(Zone.Consequence.Duration) || Zone.Consequence.Duration < 0.f)
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
		|| bRestoringState || bUninitializingState
		|| !DamageResult.TransactionId.IsValid()
		|| DamageResult.TargetActor.Get() != GetOwner()
		|| DamageResult.SourceActor.Get() == GetOwner()
		|| !DamageResult.HasNativeReceipt()
		|| DamageResult.bGuarded || DamageResult.bDeflected
		|| !FMath::IsFinite(DamageResult.AppliedShieldDamage + DamageResult.AppliedHealthDamage)
		|| DamageResult.AppliedShieldDamage + DamageResult.AppliedHealthDamage <= 0.0f)
	{
		return ESovWeakPointHitResolution::NotWeakPoint;
	}

	const FSovWeakPointZone* MatchingZone = FindMatchingZone(DamageResult);
	if (!MatchingZone)
	{
		return ESovWeakPointHitResolution::NotWeakPoint;
	}
	// Even a hit received while this zone is already broken must be retired.
	// Otherwise a retained copy could break it later after reset/recovery.
	if (!DamageResult.ConsumeNativeReceipt(this)) { return ESovWeakPointHitResolution::NotWeakPoint; }

	OutWeakPointId = MatchingZone->ZoneId;
	if (IsWeakPointBroken(MatchingZone->ZoneId))
	{
		return ESovWeakPointHitResolution::AlreadyBroken;
	}

	const uint32 Epoch = StateEpoch;
	const FName ZoneId = MatchingZone->ZoneId;
	// Periodic contexts can retain the original bone, but are not another precision impact.
	if (!DamageResult.bPeriodicDamage)
	{
		// Hits and breaks are distinct receipts. Below-threshold damage still hit a live weak point.
		if (PendingHits.Num() >= 32) PendingHits.RemoveAt(0);
		FPendingWeakPointBreak& PendingHit = PendingHits.AddDefaulted_GetRef();
		PendingHit.TransactionId = DamageResult.TransactionId;
		PendingHit.SourceActor = DamageResult.SourceActor.Get();
		PendingHit.EffectContext = DamageResult.EffectContext;
		PendingHit.HitZone = DamageResult.HitZone;
		PendingHit.WeakPointId = ZoneId;
	}
	if (DamageResult.AppliedShieldDamage + DamageResult.AppliedHealthDamage + KINDA_SMALL_NUMBER
		< FMath::Max(MinimumAppliedDamage, 0.0f))
		return DamageResult.bPeriodicDamage ? ESovWeakPointHitResolution::NotWeakPoint : ESovWeakPointHitResolution::AcceptedUnbrokenHit;
	FPendingWeakPointBreak& PendingBreak = PendingBreaks.AddDefaulted_GetRef();
	PendingBreak.TransactionId = DamageResult.TransactionId;
	PendingBreak.SourceActor = DamageResult.SourceActor.Get();
	PendingBreak.EffectContext = DamageResult.EffectContext;
	PendingBreak.HitZone = DamageResult.HitZone;
	PendingBreak.WeakPointId = ZoneId;
	SetWeakPointBroken(ZoneId, true);
	if (Epoch != StateEpoch || !IsWeakPointBroken(ZoneId)) { return ESovWeakPointHitResolution::NotWeakPoint; }
	OnWeakPointBroken.Broadcast(ZoneId, DamageResult);
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

bool USovWeakPointComponent::ConsumeWeakPointHit(const FSovDamageResult& DamageResult, FName& OutWeakPointId)
{
	OutWeakPointId = NAME_None;
	if (!GetOwner() || !GetOwner()->HasAuthority() || DamageResult.TargetActor.Get() != GetOwner()) return false;
	for (int32 Index = 0; Index < PendingHits.Num(); ++Index)
	{
		const FPendingWeakPointBreak& Hit = PendingHits[Index];
		if (!Hit.WeakPointId.IsNone() && Hit.TransactionId.IsValid()
			&& Hit.TransactionId == DamageResult.TransactionId
			&& Hit.SourceActor.Get() == DamageResult.SourceActor.Get()
			&& Hit.EffectContext.Get() == DamageResult.EffectContext.Get()
			&& Hit.HitZone == DamageResult.HitZone)
		{
			OutWeakPointId = Hit.WeakPointId;
			PendingHits.RemoveAt(Index);
			return true;
		}
	}
	return false;
}

void USovWeakPointComponent::ResetWeakPoints()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bRestoringState) { return; }
	FSovWeakPointStateSnapshot StartingState;
	for (const FSovWeakPointZone& Zone : WeakPointZones)
	{
		if (Zone.bStartsBroken && Zone.ZoneId != NAME_None)
		{
			StartingState.BrokenZoneIds.AddUnique(Zone.ZoneId);
			if (!Zone.Consequence.BlockedAbilityTags.IsEmpty() || !Zone.Consequence.GrantedStateTags.IsEmpty())
			{
				FSovWeakPointConsequenceSnapshot& Entry = StartingState.Consequences.AddDefaulted_GetRef();
				Entry.ZoneId = Zone.ZoneId;
				Entry.RemainingSeconds = Zone.Consequence.Duration > 0.f ? Zone.Consequence.Duration : -1.f;
			}
		}
	}
	RestoreWeakPointState(StartingState);
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

	const bool bHasPresentationMaterial = WeakPointZones.ContainsByPredicate(
		[this](const FSovWeakPointZone& Zone)
		{
			return Zone.ZoneId != NAME_None
				&& !IsWeakPointBroken(Zone.ZoneId)
				&& (IsValid(Zone.RevealDecalMaterialOverride.Get())
					|| IsValid(WeakPointRevealDecalMaterial.Get()));
		});
	if (!bHasPresentationMaterial)
	{
		ClearDecalReceiverBindings();
		return;
	}

	RefreshDecalReceiverBindings();
	for (const FSovWeakPointZone& Zone : WeakPointZones)
	{
		if (Zone.ZoneId == NAME_None || IsWeakPointBroken(Zone.ZoneId))
		{
			continue;
		}

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
	if (bUninitializingState) { return; }
	TGuardValue<bool> Uninitializing(bUninitializingState, true);
	++StateEpoch;
	if (IsInitialized() && GetOwner() && GetOwner()->HasAuthority())
	{
		PendingConsequenceRestore = CaptureWeakPointState();
		bPendingConsequenceRestore = true;
	}
	ClearConsequences();
	if (IsValid(AbilitySystemComponent.Get()))
	{
		AbilitySystemComponent->OnAnyGameplayEffectRemovedDelegate().RemoveAll(this);
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
	PendingHits.Reset();
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

	const uint32 Epoch = StateEpoch;
	if (bShouldBeBroken)
	{
		BrokenWeakPointIds.AddUnique(WeakPointId);
		if (const FSovWeakPointZone* Zone = FindZoneById(WeakPointId))
		{
			const float Duration = Zone->Consequence.Duration > 0.f ? Zone->Consequence.Duration : -1.f;
			if (IsInitialized()) { ApplyConsequence(*Zone, Duration, true); }
			else
			{
				PendingConsequenceRestore.BrokenZoneIds = BrokenWeakPointIds;
				if (!Zone->Consequence.BlockedAbilityTags.IsEmpty() || !Zone->Consequence.GrantedStateTags.IsEmpty())
				{
					FSovWeakPointConsequenceSnapshot& Entry = PendingConsequenceRestore.Consequences.AddDefaulted_GetRef();
					Entry.ZoneId = WeakPointId;
					Entry.RemainingSeconds = Duration;
				}
				bPendingConsequenceRestore = true;
				bAppliedStartingState = true;
			}
		}
	}
	else
	{
		BrokenWeakPointIds.Remove(WeakPointId);
	}
	if (Epoch != StateEpoch || IsWeakPointBroken(WeakPointId) != bShouldBeBroken) { return; }
	OnWeakPointStateChanged.Broadcast(WeakPointId, bShouldBeBroken);
	if (Epoch != StateEpoch) { return; }
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
