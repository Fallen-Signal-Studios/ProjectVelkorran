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
	if (!IsValid(InAbilitySystemComponent))
	{
		return false;
	}

	if (AbilitySystemComponent == InAbilitySystemComponent
		&& ShieldChangedDelegateHandle.IsValid()
		&& MaxShieldChangedDelegateHandle.IsValid())
	{
		return true;
	}

	if (!InAbilitySystemComponent->GetSet<UNarrativeAttributeSetBase>())
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

	UninitializeFromAbilitySystem();
	AbilitySystemComponent = InAbilitySystemComponent;
	bWarnedMissingAttributeSet = false;

	ShieldBrokenTag = FSovGameplayTags::Get().State_Shield_Broken;
	RechargeBlockedTag = FSovGameplayTags::Get().State_Shield_RechargeBlocked;

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
	}

	bHasRecordedShieldDamage = false;
	bRechargeDelayElapsed = true;
	LastShieldDamageWorldTime = GetWorldTimeSeconds();
	LastRechargeUpdateWorldTime = LastShieldDamageWorldTime;
	RefreshShieldBrokenState(GetShield(), false);
	RefreshShieldVisuals();

	TryStartRecharge();
	return true;
}

bool USovShieldComponent::IsInitialized() const
{
	return IsValid(AbilitySystemComponent);
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
		if (OwnerASC != AbilitySystemComponent)
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
	StopRecharge();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RechargeDelayTimerHandle);
	}

	if (!IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent = nullptr;
		ShieldChangedDelegateHandle.Reset();
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
	}

	RemoveShieldBrokenTag();

	AbilitySystemComponent = nullptr;
	ShieldChangedDelegateHandle.Reset();
	MaxShieldChangedDelegateHandle.Reset();
	RechargeBlockedTagChangedDelegateHandle.Reset();
	ShieldBrokenTag = FGameplayTag();
	RechargeBlockedTag = FGameplayTag();
	bShieldBroken = false;
	bHasRecordedShieldDamage = false;
	bRechargeDelayElapsed = false;
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
	const float OldShield = FMath::Max(ChangeData.OldValue, 0.0f);
	const float NewShield = FMath::Clamp(ChangeData.NewValue, 0.0f, GetMaxShield());

	if (CanWriteShield() && NewShield + KINDA_SMALL_NUMBER < OldShield)
	{
		RecordShieldDamage();
	}

	UpdateShieldVisualScalar();
	RefreshShieldBrokenState(NewShield, true);
	OnShieldChanged.Broadcast(OldShield, NewShield, GetMaxShield());

	if (!CanWriteShield())
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
	const float CurrentShield = GetShield();
	const float CurrentMaxShield = FMath::Max(ChangeData.NewValue, 0.0f);

	RefreshShieldBrokenState(CurrentShield, false);
	UpdateShieldVisualScalar();
	OnShieldChanged.Broadcast(CurrentShield, CurrentShield, CurrentMaxShield);

	if (!CanWriteShield() || CurrentShield + KINDA_SMALL_NUMBER >= CurrentMaxShield)
	{
		StopRecharge();
		return;
	}

	TryStartRecharge();
}

void USovShieldComponent::HandleRechargeBlockedTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	static_cast<void>(CallbackTag);

	if (!CanWriteShield())
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
	// Actual Shield decreases are already observed by the attribute delegate.
	// This path covers hits against an already-depleted Shield and explicit
	// recharge-reset packets, which otherwise have no attribute transition.
	if (CanWriteShield()
		&& Result.bShouldRestartShieldRecharge
		&& Result.AppliedShieldDamage <= KINDA_SMALL_NUMBER)
	{
		RecordShieldDamage();
	}
}

void USovShieldComponent::RecordShieldDamage()
{
	if (!CanWriteShield())
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
	if (!CanWriteShield())
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
	bRechargeDelayElapsed = true;
	TryStartRecharge();
}

void USovShieldComponent::TryStartRecharge()
{
	if (!CanWriteShield()
		|| IsRechargeBlocked()
		|| RechargePercentPerSecond <= 0.0f
		|| RechargeTimerInterval <= 0.0f)
	{
		StopRecharge();
		return;
	}

	const float CurrentShield = GetShield();
	const float CurrentMaxShield = GetMaxShield();
	if (CurrentMaxShield <= KINDA_SMALL_NUMBER
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
	if (!CanWriteShield() || IsRechargeBlocked())
	{
		StopRecharge();
		return;
	}

	const float CurrentShield = GetShield();
	const float CurrentMaxShield = GetMaxShield();
	if (CurrentMaxShield <= KINDA_SMALL_NUMBER
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

	AbilitySystemComponent->SetNumericAttributeBase(
		UNarrativeAttributeSetBase::GetShieldAttribute(),
		NewShield);

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
		if (bBroadcastBreak)
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

	AbilitySystemComponent->AddLooseGameplayTag(
		ShieldBrokenTag,
		1,
		EGameplayTagReplicationState::TagAndCountToAll);
	bAppliedShieldBrokenTag = true;
}

void USovShieldComponent::RemoveShieldBrokenTag()
{
	if (!IsValid(AbilitySystemComponent)
		|| !ShieldBrokenTag.IsValid()
		|| !bAppliedShieldBrokenTag)
	{
		return;
	}

	AbilitySystemComponent->RemoveLooseGameplayTag(
		ShieldBrokenTag,
		1,
		EGameplayTagReplicationState::TagAndCountToAll);
	bAppliedShieldBrokenTag = false;
}

bool USovShieldComponent::CanWriteShield() const
{
	return IsInitialized() && IsValid(GetOwner()) && GetOwner()->HasAuthority();
}

float USovShieldComponent::GetWorldTimeSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0f;
}
