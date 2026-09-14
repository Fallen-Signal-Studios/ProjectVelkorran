// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovShieldComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Components/MeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/SovCombatTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NarrativeGameplayTags.h"
#include "TimerManager.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovShield, Log, All);

USovShieldComponent::USovShieldComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void USovShieldComponent::BeginPlay()
{
	Super::BeginPlay();

	// Compatibility fallback for non-Narrative owners. The deterministic player
	// readiness path explicitly supplies the ASC and never polls.
	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.AddUniqueDynamic(this, &ThisClass::HandleOwnerASCInitialized);
		NarrativeOwner->CharacterVisualInitialized.AddUniqueDynamic(
			this,
			&ThisClass::HandleCharacterVisualInitialized);
		BindCharacterVisual(NarrativeOwner->GetCharacterVisual());
	}
	RefreshShieldVisuals();
	TryInitializeFromOwner();
}

void USovShieldComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.RemoveDynamic(this, &ThisClass::HandleOwnerASCInitialized);
		NarrativeOwner->CharacterVisualInitialized.RemoveDynamic(
			this,
			&ThisClass::HandleCharacterVisualInitialized);
	}
	BindCharacterVisual(nullptr);
	ClearShieldOverlayTargets();
	ShieldOverlayMaterialInstance = nullptr;
	AppliedShieldOverlayMaterial = nullptr;
	ShieldMaterialInstances.Reset();
	ClearLifecycleTimers();
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

bool USovShieldComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent)
{
	AActor* Owner = GetOwner();
	if (bEndingPlay || bChangingAbilitySystem || !IsValid(Owner) || Owner->IsActorBeingDestroyed()
		|| !IsValid(InAbilitySystemComponent) || InAbilitySystemComponent->GetAvatarActor() != Owner
		|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner) != InAbilitySystemComponent
		|| Owner->FindComponentByClass<USovShieldComponent>() != this)
	{
		return false;
	}

	if (AbilitySystemComponent == InAbilitySystemComponent
		&& IsInitialized()
		&& ShieldChangedDelegateHandle.IsValid()
		&& MaxShieldChangedDelegateHandle.IsValid())
	{
		return true;
	}

	const UNarrativeAttributeSetBase* Attributes = InAbilitySystemComponent->GetSet<UNarrativeAttributeSetBase>();
	if (!IsValid(Attributes) || Attributes->GetOwningAbilitySystemComponent() != InAbilitySystemComponent)
	{
		if (!bWarnedMissingAttributeSet)
		{
			UE_LOG(
				LogSovShield,
				Warning,
				TEXT("%s cannot initialize Shield: its Ability System has no UNarrativeAttributeSetBase."),
				*GetNameSafe(GetOwner()));
			bWarnedMissingAttributeSet = true;
		}
		return false;
	}

	const auto* RequestedNarrativeASC = Cast<UNarrativeAbilitySystemComponent>(InAbilitySystemComponent);
	const uint64 RequestedActorEpoch = RequestedNarrativeASC ? RequestedNarrativeASC->GetCombatActorInfoEpoch() : 0;
	const uint64 RequestedLifeEpoch = Attributes->GetCombatLifeEpoch();
	const int32 RequestedReadyEpoch = RequestedNarrativeASC ? RequestedNarrativeASC->GetCharacterReadyEpoch() : 0;
	TGuardValue<bool> ChangingGuard(bChangingAbilitySystem, true);
	UninitializeFromAbilitySystem();
	// Tag-removal observers may retire this avatar during teardown.
	if (!IsValid(Owner) || Owner->IsActorBeingDestroyed() || bEndingPlay || !IsValid(InAbilitySystemComponent)
		|| InAbilitySystemComponent->GetAvatarActor() != Owner
		|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner) != InAbilitySystemComponent
		|| InAbilitySystemComponent->GetSet<UNarrativeAttributeSetBase>() != Attributes
		|| !IsValid(Attributes) || Attributes->GetCombatLifeEpoch() != RequestedLifeEpoch
		|| (RequestedNarrativeASC && (RequestedNarrativeASC->GetCombatActorInfoEpoch() != RequestedActorEpoch
			|| RequestedNarrativeASC->GetCharacterReadyEpoch() != RequestedReadyEpoch))) { return false; }
	AbilitySystemComponent = InAbilitySystemComponent;
	BoundAttributes = Attributes;
	BoundLifeEpoch = Attributes->GetCombatLifeEpoch();
	const auto* EpochASC = Cast<UNarrativeAbilitySystemComponent>(InAbilitySystemComponent);
	BoundActorInfoEpoch = EpochASC ? EpochASC->GetCombatActorInfoEpoch() : 0;
	BoundReadyEpoch = EpochASC ? EpochASC->GetCharacterReadyEpoch() : 0;
	const uint64 Generation = ++BindingGeneration;
	bWarnedMissingAttributeSet = false;

	ShieldBrokenTag = FSovGameplayTags::Get().State_Shield_Broken;
	RechargeBlockedTag = FSovGameplayTags::Get().State_Shield_RechargeBlocked;
	HealthChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute())
		.AddUObject(this, &ThisClass::HandleHealthAttributeChanged);

	ShieldChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetShieldAttribute())
		.AddUObject(this, &ThisClass::HandleShieldAttributeChanged);

	MaxShieldChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetMaxShieldAttribute())
		.AddUObject(this, &ThisClass::HandleMaxShieldAttributeChanged);

	if (RechargeBlockedTag.IsValid())
	{
		RechargeBlockedTagChangedDelegateHandle = AbilitySystemComponent
			->RegisterGameplayTagEvent(
				RechargeBlockedTag,
				EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleRechargeBlockedTagChanged);
	}

	if (UNarrativeAbilitySystemComponent* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		NarrativeASC->OnDamageResolvedAsTarget.AddUniqueDynamic(this, &ThisClass::HandleDamageResolved);
		NarrativeASC->OnDeathStateChanged.AddUniqueDynamic(this, &ThisClass::HandleDeathStateChanged);
		NarrativeASC->OnCharacterReadyEpochChanged.AddUniqueDynamic(this, &ThisClass::HandleOwnerReadyEpochChanged);
	}

	bHasRecordedShieldDamage = false;
	bRechargeDelayElapsed = true;
	LastShieldDamageWorldTime = GetWorldTimeSeconds();
	LastRechargeUpdateWorldTime = LastShieldDamageWorldTime;
	RefreshShieldBrokenState(GetShield(), false);
	if (!IsCurrentOperation(Generation)) { return false; }
	RefreshShieldVisuals();
	if (!IsCurrentOperation(Generation)) { return false; }
	TryStartRecharge();
	return true;
}

void USovShieldComponent::ResetForCheckpoint()
{
	if (bChangingAbilitySystem || bEndingPlay) { return; }
	++ReviveRebindGeneration;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(ReviveRebindTimerHandle); }
	if (!IsInitialized() && !InitializeWithAbilitySystem(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))) { return; }
	if (!IsInitialized() || !GetOwner()->HasAuthority()) { return; }
	TGuardValue<bool> ChangingGuard(bChangingAbilitySystem, true);
	const uint64 Generation = ++BindingGeneration;
	ClearLifecycleTimers();
	RemoveShieldBrokenTag();
	if (!IsCurrentOperation(Generation)) { return; }
	bShieldBroken = false;
	bHasRecordedShieldDamage = false;
	bRechargeDelayElapsed = true;
	LastShieldDamageWorldTime = GetWorldTimeSeconds();
	LastRechargeUpdateWorldTime = LastShieldDamageWorldTime;
	RefreshShieldBrokenState(GetShield(), false);
	if (!IsCurrentOperation(Generation)) { return; }
	RefreshShieldVisuals();
	if (!IsCurrentOperation(Generation)) { return; }
	if (GetShield() + KINDA_SMALL_NUMBER < GetMaxShield())
	{
		bHasRecordedShieldDamage = true;
		bRechargeDelayElapsed = false;
		if (!bRestoringCheckpoint) { TryStartRecharge(); }
	}
	bCheckpointStateReconciled = IsCurrentOperation(Generation);
}

void USovShieldComponent::SetCheckpointRestoreInProgress(const bool bInProgress, const bool bResumePassiveWork)
{
	if (bInProgress) { ++CheckpointRestoreGeneration; }
	if (!bInProgress && bRestoringCheckpoint == bInProgress)
	{
		if (!bInProgress && !bResumePassiveWork) { ++BindingGeneration; bCheckpointStateReconciled = false; ClearLifecycleTimers(); }
		return;
	}
	bRestoringCheckpoint = bInProgress;
	++BindingGeneration;
	if (bInProgress)
	{
		bCheckpointStateReconciled = false; ClearLifecycleTimers(); ++ReviveRebindGeneration;
		if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(ReviveRebindTimerHandle); }
	}
	else if (bResumePassiveWork && bCheckpointStateReconciled && CanWriteShield())
	{
		// Time spent behind the restore barrier must not consume the fresh delay.
		if (GetShield() + KINDA_SMALL_NUMBER < GetMaxShield())
		{
			bHasRecordedShieldDamage = true;
			bRechargeDelayElapsed = false;
			LastShieldDamageWorldTime = GetWorldTimeSeconds();
		}
		TryStartRecharge();
	}
	else { bCheckpointStateReconciled = false; ClearLifecycleTimers(); }
}

bool USovShieldComponent::EndCheckpointRestore(const uint64 Generation, const bool bResumePassiveWork)
{
	if (Generation != CheckpointRestoreGeneration || (bResumePassiveWork && !bRestoringCheckpoint)) { return false; }
	SetCheckpointRestoreInProgress(false, bResumePassiveWork);
	return Generation == CheckpointRestoreGeneration;
}

bool USovShieldComponent::IsInitialized() const
{
	const AActor* Owner = GetOwner();
	const auto* ASC = AbilitySystemComponent.Get();
	const auto* Attributes = BoundAttributes.Get();
	const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(ASC);
	return !bEndingPlay && IsValid(Owner) && !Owner->IsActorBeingDestroyed()
		&& IsValid(ASC) && IsValid(Attributes) && ASC->GetAvatarActor() == Owner
		&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Owner)) == ASC
		&& Owner->FindComponentByClass<USovShieldComponent>() == this
		&& ASC->GetSet<UNarrativeAttributeSetBase>() == Attributes
		&& Attributes->GetOwningAbilitySystemComponent() == ASC
		&& Attributes->GetCombatLifeEpoch() == BoundLifeEpoch
		&& (!NarrativeASC || (NarrativeASC->GetCombatActorInfoEpoch() == BoundActorInfoEpoch
			&& NarrativeASC->GetCharacterReadyEpoch() == BoundReadyEpoch));
}

bool USovShieldComponent::IsCurrentOperation(const uint64 Generation) const
{
	return BindingGeneration == Generation && IsInitialized();
}

bool USovShieldComponent::ValidateBindingOrRetire()
{
	if (IsInitialized()) { return true; }
	const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent);
	if (IsValid(GetOwner()) && IsValid(AbilitySystemComponent) && BoundAttributes.IsValid()
		&& AbilitySystemComponent->GetAvatarActor() == GetOwner()
		&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()) == AbilitySystemComponent
		&& AbilitySystemComponent->GetSet<UNarrativeAttributeSetBase>() == BoundAttributes.Get()
		&& (!NarrativeASC || (NarrativeASC->GetCombatActorInfoEpoch() == BoundActorInfoEpoch
			&& NarrativeASC->GetCharacterReadyEpoch() == BoundReadyEpoch)))
	{
		// A new life cannot write, but must retain its explicit revive observer
		// until all Narrative death observers finish initializing attributes.
		ClearLifecycleTimers(); RemoveShieldBrokenTag(); return false;
	}
	if (AbilitySystemComponent && !bChangingAbilitySystem)
	{
		TGuardValue<bool> ChangingGuard(bChangingAbilitySystem, true);
		UninitializeFromAbilitySystem();
	}
	return false;
}

float USovShieldComponent::GetShield() const
{
	return IsInitialized()
		? AbilitySystemComponent->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute())
		: 0.0f;
}

float USovShieldComponent::GetMaxShield() const
{
	return IsInitialized()
		? FMath::Max(
			AbilitySystemComponent->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxShieldAttribute()),
			0.0f)
		: 0.0f;
}

bool USovShieldComponent::IsRechargeBlocked() const
{
	return IsInitialized()
		&& RechargeBlockedTag.IsValid()
		&& AbilitySystemComponent->HasMatchingGameplayTag(RechargeBlockedTag);
}

bool USovShieldComponent::IsRecharging() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetTimerManager().IsTimerActive(RechargeTimerHandle);
	}

	return false;
}

float USovShieldComponent::GetSecondsUntilRecharge() const
{
	if (!bHasRecordedShieldDamage || bRechargeDelayElapsed)
	{
		return 0.0f;
	}

	return FMath::Max(
		(LastShieldDamageWorldTime + FMath::Max(RechargeDelay, 0.0f)) - GetWorldTimeSeconds(),
		0.0f);
}

void USovShieldComponent::RefreshShieldVisuals()
{
	if (const UWorld* World = GetWorld();
		World && World->GetNetMode() == NM_DedicatedServer)
	{
		ClearShieldOverlayTargets();
		ShieldOverlayMaterialInstance = nullptr;
		ShieldMaterialInstances.Reset();
		return;
	}

	PruneShieldMaterialInstances();
	PruneShieldOverlayBindings();

	if (IsValid(ShieldOverlayMaterial))
	{
		if (PrepareShieldOverlayMaterial()
			&& ShouldDisplayShieldOverlay())
		{
			if (bAutoApplyShieldOverlayMaterial)
			{
				DiscoverShieldOverlayTargets();
			}
		}
		else
		{
			ClearShieldOverlayTargets();
		}
	}
	else
	{
		PrepareShieldOverlayMaterial();
		if (bAutoDiscoverShieldMaterials)
		{
			DiscoverShieldMaterialTargets();
		}
	}

	UpdateShieldVisualScalar();
}

bool USovShieldComponent::RegisterShieldOverlayTarget(UMeshComponent* MeshComponent)
{
	if (!IsValid(MeshComponent)
		|| (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
		|| !ShouldDisplayShieldOverlay()
		|| !PrepareShieldOverlayMaterial()
		|| !IsValid(ShieldOverlayMaterialInstance))
	{
		return false;
	}

	FSovShieldOverlayBinding* ExistingBinding = ShieldOverlayBindings.FindByPredicate(
		[MeshComponent](const FSovShieldOverlayBinding& Binding)
		{
			return Binding.MeshComponent.Get() == MeshComponent;
		});

	UMaterialInterface* CurrentOverlayMaterial = MeshComponent->GetOverlayMaterial();
	if (ExistingBinding)
	{
		if (CurrentOverlayMaterial == ShieldOverlayMaterialInstance.Get())
		{
			return true;
		}

		// Another system took the overlay after us. It becomes the material we
		// restore when the Shield releases this mesh.
		ExistingBinding->PreviousOverlayMaterial = CurrentOverlayMaterial;
	}
	else
	{
		if (IsValid(CurrentOverlayMaterial)
			&& CurrentOverlayMaterial != ShieldOverlayMaterialInstance.Get()
			&& !bOverrideExistingOverlayMaterials)
		{
			return false;
		}

		FSovShieldOverlayBinding& NewBinding = ShieldOverlayBindings.AddDefaulted_GetRef();
		NewBinding.MeshComponent = MeshComponent;
		NewBinding.PreviousOverlayMaterial = CurrentOverlayMaterial;
	}

	if (IsValid(CurrentOverlayMaterial)
		&& CurrentOverlayMaterial != ShieldOverlayMaterialInstance.Get()
		&& !bOverrideExistingOverlayMaterials)
	{
		ShieldOverlayBindings.RemoveAll(
			[MeshComponent](const FSovShieldOverlayBinding& Binding)
			{
				return Binding.MeshComponent.Get() == MeshComponent;
			});
		return false;
	}

	MeshComponent->SetOverlayMaterial(ShieldOverlayMaterialInstance.Get());
	return MeshComponent->GetOverlayMaterial() == ShieldOverlayMaterialInstance.Get();
}

bool USovShieldComponent::RegisterShieldMaterialTarget(
	UMeshComponent* MeshComponent,
	const int32 MaterialIndex)
{
	if (!IsValid(MeshComponent)
		|| (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
		|| MaterialIndex < 0
		|| MaterialIndex >= MeshComponent->GetNumMaterials())
	{
		return false;
	}

	UMaterialInterface* SourceMaterial = MeshComponent->GetMaterial(MaterialIndex);
	if (!MaterialExposesShieldScalar(SourceMaterial))
	{
		return false;
	}

	UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(SourceMaterial);
	if (!DynamicMaterial || DynamicMaterial->GetOuter() != MeshComponent)
	{
		DynamicMaterial = MeshComponent->CreateDynamicMaterialInstance(
			MaterialIndex,
			SourceMaterial);
	}

	if (!IsValid(DynamicMaterial))
	{
		return false;
	}

	ShieldMaterialInstances.AddUnique(DynamicMaterial);
	DynamicMaterial->SetScalarParameterValue(
		ShieldScalarParameterName,
		CurrentShieldVisualScalar);
	return true;
}

void USovShieldComponent::TryInitializeFromOwner()
{
	if (!IsValid(GetOwner()))
	{
		return;
	}

	if (UAbilitySystemComponent* OwnerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		if (OwnerASC != AbilitySystemComponent || !IsInitialized())
		{
			InitializeWithAbilitySystem(OwnerASC);
		}
	}
}

void USovShieldComponent::HandleOwnerASCInitialized()
{
	TryInitializeFromOwner();
}

void USovShieldComponent::HandleCharacterVisualInitialized(ANarrativeCharacter* Character)
{
	if (Character != GetOwner())
	{
		return;
	}

	BindCharacterVisual(Character->GetCharacterVisual());
	ScheduleShieldVisualRefresh();
}

void USovShieldComponent::HandleBaseAppearanceApplied()
{
	// Narrative invokes this delegate before its BlueprintNativeEvent hook. Defer
	// so a Blueprint material replacement cannot immediately invalidate our MIDs.
	ScheduleShieldVisualRefresh();
}

void USovShieldComponent::UninitializeFromAbilitySystem()
{
	++BindingGeneration;
	++ReviveRebindGeneration;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(ReviveRebindTimerHandle); }
	StopRecharge();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RechargeDelayTimerHandle);
	}

	if (!IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent = nullptr;
		ShieldChangedDelegateHandle.Reset();
		HealthChangedDelegateHandle.Reset();
		MaxShieldChangedDelegateHandle.Reset();
		RechargeBlockedTagChangedDelegateHandle.Reset();
		ShieldBrokenTag = FGameplayTag();
		RechargeBlockedTag = FGameplayTag();
		bShieldBroken = false;
		bAppliedShieldBrokenTag = false;
		bHasRecordedShieldDamage = false;
		bRechargeDelayElapsed = false;
		UpdateShieldVisualScalar();
		return;
	}
	UAbilitySystemComponent* PreviousASC = AbilitySystemComponent;
	const FGameplayTag PreviousBrokenTag = ShieldBrokenTag;
	const bool bRemovePreviousTag = bAppliedShieldBrokenTag;
	if (HealthChangedDelegateHandle.IsValid())
	{
		PreviousASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute())
			.Remove(HealthChangedDelegateHandle);
	}

	if (ShieldChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetShieldAttribute())
			.Remove(ShieldChangedDelegateHandle);
	}

	if (MaxShieldChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetMaxShieldAttribute())
			.Remove(MaxShieldChangedDelegateHandle);
	}

	if (RechargeBlockedTag.IsValid() && RechargeBlockedTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->RegisterGameplayTagEvent(
				RechargeBlockedTag,
				EGameplayTagEventType::NewOrRemoved)
			.Remove(RechargeBlockedTagChangedDelegateHandle);
	}

	if (UNarrativeAbilitySystemComponent* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		NarrativeASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::HandleDamageResolved);
		NarrativeASC->OnDeathStateChanged.RemoveDynamic(this, &ThisClass::HandleDeathStateChanged);
		NarrativeASC->OnCharacterReadyEpochChanged.RemoveDynamic(this, &ThisClass::HandleOwnerReadyEpochChanged);
	}

	AbilitySystemComponent = nullptr;
	BoundAttributes.Reset();
	ShieldChangedDelegateHandle.Reset();
	HealthChangedDelegateHandle.Reset();
	MaxShieldChangedDelegateHandle.Reset();
	RechargeBlockedTagChangedDelegateHandle.Reset();
	ShieldBrokenTag = FGameplayTag();
	RechargeBlockedTag = FGameplayTag();
	bShieldBroken = false;
	bAppliedShieldBrokenTag = false;
	bHasRecordedShieldDamage = false;
	bRechargeDelayElapsed = false;
	// Detach bookkeeping before publishing removal; a nested cleanup cannot steal
	// another contributor's tag or erase a newly initialized binding afterward.
	if (bRemovePreviousTag && PreviousBrokenTag.IsValid())
	{
		PreviousASC->RemoveLooseGameplayTag(PreviousBrokenTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
	UpdateShieldVisualScalar();
}

void USovShieldComponent::ClearLifecycleTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(RechargeDelayTimerHandle);
		TimerManager.ClearTimer(RechargeTimerHandle);
		TimerManager.ClearTimer(ShieldVisualRefreshTimerHandle);
	}
	bShieldVisualRefreshPending = false;
}

void USovShieldComponent::HandleShieldAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!ValidateBindingOrRetire() || bRestoringCheckpoint
		|| (GetOwner()->HasAuthority() && !CanWriteShield())) { return; }
	const uint64 Generation = BindingGeneration;
	const float OldShield = FMath::Max(ChangeData.OldValue, 0.0f);
	const float NewShield = FMath::Clamp(ChangeData.NewValue, 0.0f, GetMaxShield());

	if (CanWriteShield() && !bRestoringCheckpoint && NewShield + KINDA_SMALL_NUMBER < OldShield)
	{
		RecordShieldDamage();
	}

	UpdateShieldVisualScalar();
	if (!IsCurrentOperation(Generation)) { return; }
	RefreshShieldBrokenState(NewShield, !bRestoringCheckpoint);
	if (!IsCurrentOperation(Generation)) { return; }
	OnShieldChanged.Broadcast(OldShield, NewShield, GetMaxShield());

	if (!IsCurrentOperation(Generation) || !CanWriteShield())
	{
		return;
	}

	if (NewShield + KINDA_SMALL_NUMBER >= GetMaxShield())
	{
		StopRecharge();
	}
	else if (NewShield > OldShield)
	{
		TryStartRecharge();
	}
}

void USovShieldComponent::HandleMaxShieldAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!ValidateBindingOrRetire() || bRestoringCheckpoint
		|| (GetOwner()->HasAuthority() && !CanWriteShield())) { return; }
	const uint64 Generation = BindingGeneration;
	const float CurrentShield = GetShield();
	const float CurrentMaxShield = FMath::Max(ChangeData.NewValue, 0.0f);

	RefreshShieldBrokenState(CurrentShield, false);
	if (!IsCurrentOperation(Generation)) { return; }
	UpdateShieldVisualScalar();
	if (!IsCurrentOperation(Generation)) { return; }
	OnShieldChanged.Broadcast(CurrentShield, CurrentShield, CurrentMaxShield);
	if (!IsCurrentOperation(Generation)) { return; }

	if (!CanWriteShield() || CurrentShield + KINDA_SMALL_NUMBER >= CurrentMaxShield)
	{
		StopRecharge();
		return;
	}

	TryStartRecharge();
}

void USovShieldComponent::HandleHealthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (bRestoringCheckpoint || CanWriteShield()) { return; }
	++BindingGeneration;
	ClearLifecycleTimers();
	RemoveShieldBrokenTag();
}

void USovShieldComponent::HandleDeathStateChanged(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, const bool bDead)
{
	if (Actor != GetOwner() || ASC != AbilitySystemComponent || bEndingPlay || bRestoringCheckpoint) { return; }
	const uint64 Request = ++ReviveRebindGeneration;
	UWorld* World = GetWorld();
	if (!World) { return; }
	World->GetTimerManager().ClearTimer(ReviveRebindTimerHandle);
	if (bDead)
	{
		++BindingGeneration; ClearLifecycleTimers(); RemoveShieldBrokenTag(); return;
	}
	const auto* Attributes = ASC->GetSet<UNarrativeAttributeSetBase>();
	if (!IsValid(Attributes)) { return; }
	const uint64 ExpectedLife = Attributes->GetCombatLifeEpoch() + (Attributes->GetHealth() <= 0.f ? 1 : 0);
	const uint64 ActorEpoch = ASC->GetCombatActorInfoEpoch();
	ReviveRebindTimerHandle = World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
		[this, Request, ExpectedLife, ActorEpoch, WeakASC = TWeakObjectPtr<UNarrativeAbilitySystemComponent>(ASC),
		 WeakAttributes = TWeakObjectPtr<const UNarrativeAttributeSetBase>(Attributes)]()
		{
			auto* CurrentASC = WeakASC.Get(); const auto* CurrentAttributes = WeakAttributes.Get();
			if (Request != ReviveRebindGeneration || bRestoringCheckpoint || bEndingPlay || !IsValid(CurrentASC)
				|| !IsValid(CurrentAttributes) || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()
				|| CurrentASC->GetAvatarActor() != GetOwner()
				|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()) != CurrentASC
				|| CurrentASC->GetCombatActorInfoEpoch() != ActorEpoch
				|| CurrentASC->GetSet<UNarrativeAttributeSetBase>() != CurrentAttributes
				|| CurrentAttributes->GetCombatLifeEpoch() != ExpectedLife || CurrentAttributes->GetHealth() <= 0.f
				|| CurrentASC->IsDead()) { return; }
			// Narrative's ordinary revive initializes attributes in another death
			// observer. Rebind only after that notification has finished.
			ResetForCheckpoint();
		}));
}

void USovShieldComponent::HandleRechargeBlockedTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	static_cast<void>(CallbackTag);

	if (!ValidateBindingOrRetire() || bRestoringCheckpoint || !CanWriteShield())
	{
		return;
	}

	if (NewCount > 0)
	{
		StopRecharge();
	}
	else
	{
		TryStartRecharge();
	}
}

void USovShieldComponent::HandleDamageResolved(const FSovDamageResult& Result)
{
	if (!ValidateBindingOrRetire()) { return; }
	// Actual Shield decreases are already observed by the attribute delegate.
	// This path covers hits against an already-depleted Shield and explicit
	// recharge-reset packets, which otherwise have no attribute transition.
	if (!bRestoringCheckpoint && CanWriteShield()
		&& Result.TargetActor == GetOwner() && Result.IsCurrentTargetLife(AbilitySystemComponent)
		&& Result.bShouldRestartShieldRecharge
		&& Result.AppliedShieldDamage <= KINDA_SMALL_NUMBER
		&& Result.ConsumeNativeReceipt(this))
	{
		RecordShieldDamage();
	}
}

void USovShieldComponent::RecordShieldDamage()
{
	if (bRestoringCheckpoint || !CanWriteShield())
	{
		return;
	}

	bHasRecordedShieldDamage = true;
	bRechargeDelayElapsed = false;
	LastShieldDamageWorldTime = GetWorldTimeSeconds();
	StopRecharge();
	ScheduleRechargeDelay(FMath::Max(RechargeDelay, 0.0f));
}

void USovShieldComponent::ScheduleRechargeDelay(const float DelaySeconds)
{
	if (bRestoringCheckpoint || !CanWriteShield() || !FMath::IsFinite(DelaySeconds))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(RechargeDelayTimerHandle);

		if (DelaySeconds <= KINDA_SMALL_NUMBER)
		{
			HandleRechargeDelayElapsed();
			return;
		}

		TimerManager.SetTimer(
			RechargeDelayTimerHandle,
			this,
			&ThisClass::HandleRechargeDelayElapsed,
			DelaySeconds,
			false,
			DelaySeconds);
	}
}

void USovShieldComponent::HandleRechargeDelayElapsed()
{
	if (!ValidateBindingOrRetire()) { return; }
	if (bRestoringCheckpoint || !CanWriteShield()) { return; }
	bRechargeDelayElapsed = true;
	TryStartRecharge();
}

void USovShieldComponent::TryStartRecharge()
{
	if (bRestoringCheckpoint || !CanWriteShield()
		|| IsRechargeBlocked()
		|| !FMath::IsFinite(RechargeDelay)
		|| !FMath::IsFinite(RechargePercentPerSecond)
		|| !FMath::IsFinite(RechargeTimerInterval)
		|| RechargePercentPerSecond <= 0.0f
		|| RechargeTimerInterval <= 0.0f)
	{
		StopRecharge();
		return;
	}

	const float CurrentShield = GetShield();
	const float CurrentMaxShield = GetMaxShield();
	if (!FMath::IsFinite(CurrentShield) || !FMath::IsFinite(CurrentMaxShield)
		|| CurrentMaxShield <= KINDA_SMALL_NUMBER
		|| CurrentShield + KINDA_SMALL_NUMBER >= CurrentMaxShield)
	{
		StopRecharge();
		return;
	}

	if (!bRechargeDelayElapsed)
	{
		const float RemainingDelay = bHasRecordedShieldDamage
			? (LastShieldDamageWorldTime + FMath::Max(RechargeDelay, 0.0f)) - GetWorldTimeSeconds()
			: 0.0f;

		if (RemainingDelay > KINDA_SMALL_NUMBER)
		{
			ScheduleRechargeDelay(RemainingDelay);
			return;
		}

		bRechargeDelayElapsed = true;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (TimerManager.IsTimerActive(RechargeTimerHandle))
		{
			return;
		}

		const float EffectiveInterval = FMath::Max(RechargeTimerInterval, 0.01f);
		LastRechargeUpdateWorldTime = GetWorldTimeSeconds();
		TimerManager.SetTimer(
			RechargeTimerHandle,
			this,
			&ThisClass::HandleRechargeTimerElapsed,
			EffectiveInterval,
			true,
			EffectiveInterval);
	}
}

void USovShieldComponent::HandleRechargeTimerElapsed()
{
	if (!ValidateBindingOrRetire()) { return; }
	if (bRestoringCheckpoint || !CanWriteShield() || IsRechargeBlocked())
	{
		StopRecharge();
		return;
	}

	const float CurrentShield = GetShield();
	const float CurrentMaxShield = GetMaxShield();
	if (!FMath::IsFinite(CurrentShield) || !FMath::IsFinite(CurrentMaxShield)
		|| !FMath::IsFinite(RechargePercentPerSecond)
		|| CurrentMaxShield <= KINDA_SMALL_NUMBER
		|| CurrentShield + KINDA_SMALL_NUMBER >= CurrentMaxShield)
	{
		StopRecharge();
		return;
	}

	const float CurrentWorldTime = GetWorldTimeSeconds();
	const float RechargeSeconds = FMath::Max(
		CurrentWorldTime - LastRechargeUpdateWorldTime,
		0.0f);
	LastRechargeUpdateWorldTime = CurrentWorldTime;

	if (RechargeSeconds <= 0.0f)
	{
		return;
	}

	const float RechargeAmount = CurrentMaxShield
		* FMath::Max(RechargePercentPerSecond, 0.0f)
		* RechargeSeconds;
	const float NewShield = FMath::Min(CurrentShield + RechargeAmount, CurrentMaxShield);

	const uint64 Generation = BindingGeneration;
	AbilitySystemComponent->SetNumericAttributeBase(
		UNarrativeAttributeSetBase::GetShieldAttribute(),
		NewShield);
	if (!IsCurrentOperation(Generation) || !CanWriteShield()) { return; }

	if (NewShield + KINDA_SMALL_NUMBER >= CurrentMaxShield)
	{
		StopRecharge();
	}
}

void USovShieldComponent::StopRecharge()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RechargeTimerHandle);
	}
	LastRechargeUpdateWorldTime = 0.0f;
}

void USovShieldComponent::RefreshShieldBrokenState(
	const float CurrentShield,
	const bool bBroadcastBreak)
{
	if (!IsInitialized()) { return; }
	const uint64 Generation = BindingGeneration;
	const bool bNewShieldBroken = GetMaxShield() > KINDA_SMALL_NUMBER
		&& CurrentShield <= KINDA_SMALL_NUMBER;

	if (bNewShieldBroken == bShieldBroken)
	{
		return;
	}

	bShieldBroken = bNewShieldBroken;
	if (bShieldBroken)
	{
		ApplyShieldBrokenTag();
		// A client learns of the break from the replicated Shield attribute and cannot write it, but it still
		// presents the break; the authority keeps its existing live-owner requirement.
		if (IsCurrentOperation(Generation) && (CanWriteShield() || !GetOwner()->HasAuthority()) && bBroadcastBreak)
		{
			SpawnShieldBreakSystem();
			OnShieldBroken.Broadcast();
		}
	}
	else
	{
		RemoveShieldBrokenTag();
	}
}

void USovShieldComponent::BindCharacterVisual(
	ANarrativeCharacterVisual* NewCharacterVisual)
{
	if (BoundCharacterVisual == NewCharacterVisual)
	{
		return;
	}

	if (IsValid(BoundCharacterVisual))
	{
		BoundCharacterVisual->OnBaseAppearanceApplied.RemoveDynamic(
			this,
			&ThisClass::HandleBaseAppearanceApplied);
	}

	// ChangeAppearance can destroy and replace the entire runtime visual actor.
	// Release its meshes before rebinding so any prior overlays are restored.
	ClearShieldOverlayTargets();

	BoundCharacterVisual = NewCharacterVisual;
	if (IsValid(BoundCharacterVisual))
	{
		BoundCharacterVisual->OnBaseAppearanceApplied.AddUniqueDynamic(
			this,
			&ThisClass::HandleBaseAppearanceApplied);
	}
}

void USovShieldComponent::ScheduleShieldVisualRefresh()
{
	if (bShieldVisualRefreshPending)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		bShieldVisualRefreshPending = true;
		ShieldVisualRefreshTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(
				this,
				&ThisClass::HandleDeferredShieldVisualRefresh));
	}
	else
	{
		RefreshShieldVisuals();
	}
}

void USovShieldComponent::HandleDeferredShieldVisualRefresh()
{
	bShieldVisualRefreshPending = false;
	RefreshShieldVisuals();
}

bool USovShieldComponent::PrepareShieldOverlayMaterial()
{
	const bool bConfigurationChanged =
		AppliedShieldOverlayMaterial.Get() != ShieldOverlayMaterial.Get()
		|| AppliedShieldScalarParameterName != ShieldScalarParameterName;
	if (bConfigurationChanged)
	{
		ClearShieldOverlayTargets();
		ShieldOverlayMaterialInstance = nullptr;
		AppliedShieldOverlayMaterial = ShieldOverlayMaterial;
		AppliedShieldScalarParameterName = ShieldScalarParameterName;
		bWarnedInvalidShieldOverlayMaterial = false;
	}

	if (!IsValid(ShieldOverlayMaterial))
	{
		ClearShieldOverlayTargets();
		ShieldOverlayMaterialInstance = nullptr;
		return false;
	}

	if (!MaterialExposesShieldScalar(ShieldOverlayMaterial.Get()))
	{
		ClearShieldOverlayTargets();
		ShieldOverlayMaterialInstance = nullptr;
		if (!bWarnedInvalidShieldOverlayMaterial)
		{
			UE_LOG(
				LogSovShield,
				Warning,
				TEXT("%s cannot apply Shield Overlay Material %s: it does not expose scalar parameter %s."),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(ShieldOverlayMaterial.Get()),
				*ShieldScalarParameterName.ToString());
			bWarnedInvalidShieldOverlayMaterial = true;
		}
		return false;
	}

	if (!IsValid(ShieldOverlayMaterialInstance))
	{
		ShieldOverlayMaterialInstance = UMaterialInstanceDynamic::Create(
			ShieldOverlayMaterial.Get(),
			this);
	}

	if (IsValid(ShieldOverlayMaterialInstance))
	{
		ShieldOverlayMaterialInstance->SetScalarParameterValue(
			ShieldScalarParameterName,
			CurrentShieldVisualScalar);
		return true;
	}

	return false;
}

bool USovShieldComponent::ShouldDisplayShieldOverlay() const
{
	const float CurrentMaxShield = GetMaxShield();
	if (CurrentMaxShield <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	return !bHideShieldOverlayWhenBroken
		|| GetShield() > KINDA_SMALL_NUMBER;
}

void USovShieldComponent::ReconcileShieldOverlayVisibility()
{
	const int32 PreviousBindingCount = ShieldOverlayBindings.Num();
	PruneShieldOverlayBindings();
	const bool bNeedsDiscovery = ShieldOverlayBindings.IsEmpty()
		|| ShieldOverlayBindings.Num() != PreviousBindingCount;

	if (!IsValid(ShieldOverlayMaterial)
		|| !ShouldDisplayShieldOverlay())
	{
		ClearShieldOverlayTargets();
		return;
	}

	if (PrepareShieldOverlayMaterial()
		&& bAutoApplyShieldOverlayMaterial
		&& bNeedsDiscovery)
	{
		DiscoverShieldOverlayTargets();
	}
}

void USovShieldComponent::DiscoverShieldOverlayTargets()
{
	if (!IsValid(GetOwner())
		|| !IsValid(ShieldOverlayMaterialInstance)
		|| !ShouldDisplayShieldOverlay())
	{
		return;
	}

	TArray<AActor*> PresentationActors;
	PresentationActors.Add(GetOwner());
	if (IsValid(BoundCharacterVisual))
	{
		PresentationActors.AddUnique(BoundCharacterVisual);
	}

	for (AActor* PresentationActor : PresentationActors)
	{
		if (!IsValid(PresentationActor))
		{
			continue;
		}

		TInlineComponentArray<UMeshComponent*> MeshComponents;
		PresentationActor->GetComponents(MeshComponents);
		for (UMeshComponent* MeshComponent : MeshComponents)
		{
			// Narrative's Appearance Asset uses skeletal and static meshes. Limit
			// discovery to those component types so hair/groom and attached weapon
			// presentation are not accidentally coated.
			if (IsValid(MeshComponent)
				&& (MeshComponent->IsA<USkinnedMeshComponent>()
					|| MeshComponent->IsA<UStaticMeshComponent>()))
			{
				RegisterShieldOverlayTarget(MeshComponent);
			}
		}
	}
}

void USovShieldComponent::PruneShieldOverlayBindings()
{
	ShieldOverlayBindings.RemoveAll(
		[this](const FSovShieldOverlayBinding& Binding)
		{
			UMeshComponent* MeshComponent = Binding.MeshComponent.Get();
			return !IsValid(MeshComponent)
				|| !IsValid(ShieldOverlayMaterialInstance)
				|| MeshComponent->GetOverlayMaterial()
					!= ShieldOverlayMaterialInstance.Get();
		});
}

void USovShieldComponent::ClearShieldOverlayTargets()
{
	for (const FSovShieldOverlayBinding& Binding : ShieldOverlayBindings)
	{
		UMeshComponent* MeshComponent = Binding.MeshComponent.Get();
		if (IsValid(MeshComponent)
			&& IsValid(ShieldOverlayMaterialInstance)
			&& MeshComponent->GetOverlayMaterial()
				== ShieldOverlayMaterialInstance.Get())
		{
			MeshComponent->SetOverlayMaterial(Binding.PreviousOverlayMaterial.Get());
		}
	}

	ShieldOverlayBindings.Reset();
}

void USovShieldComponent::PruneShieldMaterialInstances()
{
	ShieldMaterialInstances.RemoveAll(
		[](const TObjectPtr<UMaterialInstanceDynamic>& MaterialInstance)
		{
			if (!IsValid(MaterialInstance))
			{
				return true;
			}

			const UMeshComponent* OwningMesh =
				Cast<UMeshComponent>(MaterialInstance->GetOuter());
			if (!IsValid(OwningMesh))
			{
				return true;
			}

			for (int32 MaterialIndex = 0;
				MaterialIndex < OwningMesh->GetNumMaterials();
				++MaterialIndex)
			{
				if (OwningMesh->GetMaterial(MaterialIndex) == MaterialInstance.Get())
				{
					return false;
				}
			}

			return true;
		});
}

void USovShieldComponent::UpdateShieldVisualScalar()
{
	const float PreviousScalar = CurrentShieldVisualScalar;
	const float CurrentMaxShield = GetMaxShield();
	const float CurrentShield = GetShield();

	if (CurrentMaxShield <= KINDA_SMALL_NUMBER)
	{
		CurrentShieldVisualScalar = FullShieldScalar;
	}
	else if (CurrentShield <= KINDA_SMALL_NUMBER)
	{
		CurrentShieldVisualScalar = BrokenShieldScalar;
	}
	else
	{
		const float Depletion = 1.0f - FMath::Clamp(
			CurrentShield / CurrentMaxShield,
			0.0f,
			1.0f);
		const float ResponseAlpha = FMath::Pow(
			Depletion,
			FMath::Max(ShieldScalarResponseExponent, 0.01f));
		CurrentShieldVisualScalar = FMath::Lerp(
			FullShieldScalar,
			NearBreakShieldScalar,
			ResponseAlpha);
	}

	PruneShieldMaterialInstances();
	for (UMaterialInstanceDynamic* MaterialInstance : ShieldMaterialInstances)
	{
		MaterialInstance->SetScalarParameterValue(
			ShieldScalarParameterName,
			CurrentShieldVisualScalar);
	}

	if (IsValid(ShieldOverlayMaterialInstance))
	{
		ShieldOverlayMaterialInstance->SetScalarParameterValue(
			ShieldScalarParameterName,
			CurrentShieldVisualScalar);
	}

	ReconcileShieldOverlayVisibility();

	if (!FMath::IsNearlyEqual(PreviousScalar, CurrentShieldVisualScalar))
	{
		OnShieldVisualScalarChanged.Broadcast(CurrentShieldVisualScalar);
	}
}

void USovShieldComponent::DiscoverShieldMaterialTargets()
{
	if (!IsValid(GetOwner()) || ShieldScalarParameterName.IsNone())
	{
		return;
	}

	TArray<AActor*> PresentationActors;
	PresentationActors.Add(GetOwner());
	if (IsValid(BoundCharacterVisual))
	{
		PresentationActors.AddUnique(BoundCharacterVisual);
	}

	for (AActor* PresentationActor : PresentationActors)
	{
		if (!IsValid(PresentationActor))
		{
			continue;
		}

		TInlineComponentArray<UMeshComponent*> MeshComponents;
		PresentationActor->GetComponents(MeshComponents);
		for (UMeshComponent* MeshComponent : MeshComponents)
		{
			if (!IsValid(MeshComponent))
			{
				continue;
			}

			for (int32 MaterialIndex = 0;
				MaterialIndex < MeshComponent->GetNumMaterials();
				++MaterialIndex)
			{
				RegisterShieldMaterialTarget(MeshComponent, MaterialIndex);
			}
		}
	}
}

void USovShieldComponent::SpawnShieldBreakSystem() const
{
	UWorld* World = GetWorld();
	if (!IsValid(ShieldBreakSystem)
		|| !IsValid(GetOwner())
		|| !IsValid(World)
		|| World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const FTransform SpawnTransform = ShieldBreakRelativeTransform
		* GetOwner()->GetActorTransform();
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		ShieldBreakSystem,
		SpawnTransform.GetLocation(),
		SpawnTransform.Rotator(),
		SpawnTransform.GetScale3D(),
		true,
		true,
		ENCPoolMethod::AutoRelease,
		true);
}

bool USovShieldComponent::MaterialExposesShieldScalar(
	const UMaterialInterface* Material) const
{
	if (!IsValid(Material) || ShieldScalarParameterName.IsNone())
	{
		return false;
	}

	TArray<FMaterialParameterInfo> ParameterInfos;
	TArray<FGuid> ParameterIds;
	Material->GetAllScalarParameterInfo(ParameterInfos, ParameterIds);
	return ParameterInfos.ContainsByPredicate(
		[this](const FMaterialParameterInfo& ParameterInfo)
		{
			return ParameterInfo.Name == ShieldScalarParameterName;
		});
}

void USovShieldComponent::ApplyShieldBrokenTag()
{
	if (!CanWriteShield()
		|| !ShieldBrokenTag.IsValid()
		|| bAppliedShieldBrokenTag)
	{
		return;
	}

	bAppliedShieldBrokenTag = true;
	AbilitySystemComponent->AddLooseGameplayTag(
		ShieldBrokenTag,
		1,
		EGameplayTagReplicationState::TagAndCountToAll);
}

void USovShieldComponent::RemoveShieldBrokenTag()
{
	if (!IsValid(AbilitySystemComponent)
		|| !ShieldBrokenTag.IsValid()
		|| !bAppliedShieldBrokenTag)
	{
		return;
	}

	bAppliedShieldBrokenTag = false;
	AbilitySystemComponent->RemoveLooseGameplayTag(
		ShieldBrokenTag,
		1,
		EGameplayTagReplicationState::TagAndCountToAll);
}

bool USovShieldComponent::CanWriteShield() const
{
	if (!IsInitialized() || !GetOwner()->HasAuthority()) { return false; }
	const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent);
	const float Health = BoundAttributes->GetHealth();
	return FMath::IsFinite(Health) && Health > 0.f && (!NarrativeASC || !NarrativeASC->IsDead())
		&& !AbilitySystemComponent->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
		&& !AbilitySystemComponent->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal);
}

float USovShieldComponent::GetWorldTimeSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void USovShieldComponent::HandleOwnerReadyEpochChanged(const int32 ReadyEpoch)
{
    const auto* ASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent);
    if (!bEndingPlay && IsValid(ASC) && ASC->GetAvatarActor() == GetOwner()
        && ASC->GetCharacterReadyEpoch() == ReadyEpoch && ReadyEpoch != BoundReadyEpoch)
    {
        ++CheckpointRestoreGeneration;
        InitializeWithAbilitySystem(AbilitySystemComponent);
    }
}
