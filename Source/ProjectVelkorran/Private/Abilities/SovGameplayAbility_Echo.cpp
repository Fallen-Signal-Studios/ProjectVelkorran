// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_Echo.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SovEchoComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Items/WeaponItem.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "TimerManager.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovEchoAbility, Log, All);

USovGameplayAbility_EchoBase::USovGameplayAbility_EchoBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	bRequiresAmmo = false;

	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	ActivationBlockedTags.AddTag(NarrativeTags.State_IsDead);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Busy);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Interacting);
	ActivationBlockedTags.AddTag(NarrativeTags.State_SequencerControlled);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Movement_Ragdoll);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Weapon_Equipping);
	ActivationBlockedTags.AddTag(SovTags.State_Fatal);
	ActivationBlockedTags.AddTag(SovTags.State_Guarding);
	ActivationBlockedTags.AddTag(SovTags.State_Guard_Broken);
	ActivationBlockedTags.AddTag(SovTags.State_Poise_Broken);
	ActivationBlockedTags.AddTag(SovTags.State_Status_Frozen);
	ActivationBlockedTags.AddTag(SovTags.State_EchoAbility_Active);
	ActivationOwnedTags.AddTag(NarrativeTags.State_Busy);
	ActivationOwnedTags.AddTag(SovTags.State_EchoAbility_Active);
}

bool USovGameplayAbility_EchoBase::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	LastActivationFailureReason.Reset();
	if (bEndingEcho) { return false; }
	FGameplayTagContainer LocalRelevantTags;
	FGameplayTagContainer* RelevantTags = OptionalRelevantTags
		? OptionalRelevantTags
		: &LocalRelevantTags;
	const bool bCanActivate = Super::CanActivateAbility(
		Handle,
		ActorInfo,
		SourceTags,
		TargetTags,
		RelevantTags);
	if (bCanActivate || !ActorInfo
		|| (!ActorInfo->IsLocallyControlled() && !ActorInfo->IsNetAuthority()))
	{
		return bCanActivate;
	}

	FGameplayTagContainer OwnedTags;
	if (const UAbilitySystemComponent* AbilitySystem =
		ActorInfo->AbilitySystemComponent.Get())
	{
		AbilitySystem->GetOwnedGameplayTags(OwnedTags);
	}

	const USovEchoComponent* EchoComponent = ResolveEchoComponent(ActorInfo);
	const float CurrentEcho = IsValid(EchoComponent) && EchoComponent->IsInitialized()
		? EchoComponent->GetEcho()
		: 0.0f;
	const float MaximumEcho = IsValid(EchoComponent) && EchoComponent->IsInitialized()
		? EchoComponent->GetMaxEcho()
		: 0.0f;
	const FString FailureReason = LastActivationFailureReason.IsEmpty()
		? TEXT("a GAS activation, tag, cooldown, or networking gate failed")
		: LastActivationFailureReason;

	UE_LOG(
		LogSovEchoAbility,
		Warning,
		TEXT("%s rejected Echo ability %s on input %s: %s. Echo %.2f/%.2f, threshold %.2f, cost %.2f. FailureTags=[%s] OwnedTags=[%s]"),
		*GetNameSafe(ActorInfo->AvatarActor.Get()),
		*GetNameSafe(this),
		*InputTag.ToString(),
		*FailureReason,
		CurrentEcho,
		MaximumEcho,
		GetMinimumEchoRequired(),
		GetEchoCost(),
		*RelevantTags->ToStringSimple(),
		*OwnedTags.ToStringSimple());

	return false;
}

bool USovGameplayAbility_EchoBase::CheckCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	return CheckEchoPresentationCost(Handle, ActorInfo, LastActivationFailureReason, OptionalRelevantTags);
}

bool USovGameplayAbility_EchoBase::CheckEchoPresentationCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FString& OutReason, FGameplayTagContainer* OptionalRelevantTags) const
{
	OutReason.Reset();
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		OutReason = TEXT("the inherited GAS cost check failed");
		return false;
	}
	const auto* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const float Health = ASC ? ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) : 0.f;
	if (!FMath::IsFinite(Health) || Health <= 0.f)
	{
		OutReason = TEXT("the avatar has no living Health even if its death tag is not published yet");
		if (OptionalRelevantTags) { OptionalRelevantTags->AddTag(FNarrativeGameplayTags::Get().Ability_ActivateFail_TagsBlocked); }
		return false;
	}

	if (!HasRequiredPayloadConfiguration())
	{
		OutReason = TEXT("required payload configuration is incomplete");
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(
				FNarrativeGameplayTags::Get().Ability_ActivateFail_TagsBlocked);
		}
		return false;
	}
	if (!MeetsCharacterRequirement(ActorInfo))
	{
		OutReason = TEXT("the avatar does not satisfy the character identity requirement");
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(
				FNarrativeGameplayTags::Get().Ability_ActivateFail_TagsMissing);
		}
		return false;
	}
	if (!MeetsWeaponRequirement(Handle, ActorInfo))
	{
		OutReason = TEXT("the currently wielded or granting weapon is not allowed");
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(
				FNarrativeGameplayTags::Get().Ability_ActivateFail_TagsBlocked);
		}
		return false;
	}

	USovEchoComponent* EchoComponent = ResolveEchoComponent(ActorInfo);
	const float RequiredEcho = GetMinimumEchoRequired();
	const float Cost = GetEchoCost();
	if (!IsValid(EchoComponent))
	{
		OutReason = TEXT("the avatar has no Sovereign Echo component");
		AddEchoFailureTags(OptionalRelevantTags);
		return false;
	}
	if (!EchoComponent->IsInitialized())
	{
		OutReason = TEXT("the Sovereign Echo component could not bind to the current ASC");
		AddEchoFailureTags(OptionalRelevantTags);
		return false;
	}

	const float CurrentEcho = EchoComponent->GetEcho();
	if (CurrentEcho + KINDA_SMALL_NUMBER < RequiredEcho)
	{
		OutReason = FString::Printf(
			TEXT("current Echo %.2f is below the %.2f activation threshold"),
			CurrentEcho,
			RequiredEcho);
		AddEchoFailureTags(OptionalRelevantTags);
		return false;
	}
	if (!EchoComponent->CanAffordEcho(Cost))
	{
		OutReason = FString::Printf(
			TEXT("current Echo %.2f cannot pay the %.2f activation cost"),
			CurrentEcho,
			Cost);
		AddEchoFailureTags(OptionalRelevantTags);
		return false;
	}

	return true;
}

void USovGameplayAbility_EchoBase::ApplyCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// The replicated attribute corrects the predicting owner. Spending on both
	// sides would double-charge and is not prediction-key-backed.
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
		return;
	}
	const uint64 Epoch = EchoActivationEpoch;
	if (bAuthorityEchoSpendAttempted || !IsEchoActivationCurrent(Epoch))
	{
		return;
	}
	bAuthorityEchoSpendAttempted = true;

	TStrongObjectPtr<USovEchoComponent> EchoComponent(ResolveEchoComponent(ActorInfo));
	const float Cost = GetEchoCost();
	if (!IsEchoActivationCurrent(Epoch)) { return; }
	const bool bSpent = IsValid(EchoComponent.Get()) && EchoComponent->TrySpendEcho(Cost, EchoSpendTag);
	// A debit observer may end or replace this activation. Never write an old
	// payment result into the replacement instance or register stale target data.
	if (!IsEchoActivationCurrent(Epoch)) { return; }
	bAuthorityEchoSpendSucceeded = bSpent;
	if (!bAuthorityEchoSpendSucceeded)
	{
		UE_LOG(
			LogSovEchoAbility,
			Error,
			TEXT("%s passed CheckCost but could not spend %.2f Echo for %s."),
			*GetNameSafe(ActorInfo->AvatarActor.Get()),
			Cost,
			*GetNameSafe(this));
		return;
	}

	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
}

bool USovGameplayAbility_EchoBase::IsEchoActivationCurrent(uint64 Epoch) const
{
	const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(EchoActivationASC.Get());
	const float Health = EchoActivationASC.IsValid()
		? EchoActivationASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) : 0.f;
	return EchoActivationEpoch == Epoch && !bEchoEndPending && !bEndingEcho && IsActive()
		&& FMath::IsFinite(Health) && Health > 0.f
		&& CurrentActorInfo && CurrentSpecHandle == EchoActivationSpec
		&& EchoActivationAvatar.IsValid() && !EchoActivationAvatar->IsActorBeingDestroyed()
		&& EchoActivationASC.IsValid() && CurrentActorInfo->AvatarActor == EchoActivationAvatar
		&& EchoAttributes.IsValid() && EchoActivationASC->GetSet<UNarrativeAttributeSetBase>() == EchoAttributes.Get()
		&& EchoAttributes->GetCombatLifeEpoch() == EchoLifeEpoch
		&& CurrentActorInfo->AbilitySystemComponent == EchoActivationASC
		&& EchoActivationASC->GetAvatarActor() == EchoActivationAvatar.Get()
		&& (!NarrativeASC || NarrativeASC->GetCombatActorInfoEpoch() == EchoActorInfoEpoch)
		&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(EchoActivationAvatar.Get()) == EchoActivationASC.Get();
}

float USovGameplayAbility_EchoBase::GetCurrentEcho() const
{
	const USovEchoComponent* EchoComponent = GetEchoComponent();
	return IsValid(EchoComponent) ? EchoComponent->GetEcho() : 0.0f;
}

USovEchoComponent* USovGameplayAbility_EchoBase::GetEchoComponent() const
{
	return ResolveEchoComponent(CurrentActorInfo);
}

FGameplayAbilityTargetDataHandle USovGameplayAbility_EchoBase::GetAuthorityAimTargetData(
	const FCombatTraceData& TraceData)
{
	FGameplayAbilityTargetDataHandle TargetData;
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return TargetData;
	}

	AActor* Avatar = CurrentActorInfo->AvatarActor.Get();
	if (!IsValid(Avatar))
	{
		return TargetData;
	}

	FVector Start = Avatar->GetActorLocation();
	FRotator ViewRotation = Avatar->GetActorRotation();
	Avatar->GetActorEyesViewPoint(Start, ViewRotation);
	if (AController* Controller = GetOwningController())
	{
		ViewRotation = Controller->GetControlRotation();
	}

	const float TraceDistance = FMath::Max(TraceData.TraceDistance, 0.0f);
	const FVector End = Start + (ViewRotation.Vector() * TraceDistance);
	TArray<FHitResult> Hits = PerformTraceMulti(Start, End, FMath::Max(TraceData.TraceRadius, 0.0f));
	if (!TraceData.bTraceMulti && Hits.Num() > 1)
	{
		Hits.SetNum(1);
	}

	for (const FHitResult& Hit : Hits)
	{
		TargetData.Add(new FGameplayAbilityTargetData_SingleTargetHit(Hit));
	}
	return TargetData;
}

void USovGameplayAbility_EchoBase::FinishEchoAbility(const bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(
			CurrentSpecHandle,
			CurrentActorInfo,
			CurrentActivationInfo,
			true,
			bWasCancelled);
	}
}

void USovGameplayAbility_EchoBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	TStrongObjectPtr<USovGameplayAbility_EchoBase> ActionLifetime(this);
	const uint64 Epoch = ++EchoActivationEpoch;
	bEchoEndPending = false;
	EchoActivationAvatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	EchoActivationASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(EchoActivationASC.Get());
	EchoActorInfoEpoch = NarrativeASC ? NarrativeASC->GetCombatActorInfoEpoch() : 0;
	EchoAttributes = EchoActivationASC.IsValid() ? EchoActivationASC->GetSet<UNarrativeAttributeSetBase>() : nullptr;
	EchoLifeEpoch = EchoAttributes.IsValid() ? EchoAttributes->GetCombatLifeEpoch() : 0;
	EchoActivationSpec = Handle;
	bEchoAbilityStarted = false;
	bAuthorityEchoSpendAttempted = false;
	bAuthorityEchoSpendSucceeded = ActorInfo && !ActorInfo->IsNetAuthority();
	const auto ContinueActivation = [this, Epoch]()
	{
		if (IsEchoActivationCurrent(Epoch)) { return true; }
		if (EchoActivationEpoch == Epoch && IsActive()) { FinishEchoAbility(true); }
		return false;
	};
	const bool bCommitted = ActorInfo && CommitAbility(Handle, ActorInfo, ActivationInfo);
	if (!ContinueActivation()) { return; }
	if (!bCommitted)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (ActorInfo->IsNetAuthority() && !bAuthorityEchoSpendSucceeded)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Close payment-time interruption before Narrative can invoke authored startup.
	BindCancellationTags(ActorInfo->AbilitySystemComponent.Get());
	if (!ContinueActivation()) { return; }

	// Narrative binds its target-data callback and invokes the generic Blueprint
	// ActivateAbility event here. Payment has already succeeded, so even an
	// accidentally implemented K2 event cannot execute an unpaid payload.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!ContinueActivation()) { return; }

	bEchoAbilityStarted = true;
	if (MaximumActiveDuration > KINDA_SMALL_NUMBER)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				MaximumDurationTimerHandle,
				FTimerDelegate::CreateWeakLambda(this, [this, Epoch]()
				{ if (IsEchoActivationCurrent(Epoch)) { HandleMaximumDurationExpired(); } }),
				MaximumActiveDuration,
				false);
		}
	}

	if (ActorInfo->IsLocallyControlled() || ActorInfo->IsNetAuthority())
	{
		ReceiveEchoAbilityStarted(ActorInfo->IsNetAuthority());
	}
	if (!ContinueActivation()) { return; }
	if (ActorInfo->IsLocallyControlled())
	{
		ReceiveEchoAbilityLocalPresentation();
	}
	if (!ContinueActivation()) { return; }
	if (ActorInfo->IsNetAuthority())
	{
		ReceiveEchoAbilityAuthorityCommitted(GetEchoCost());
		ContinueActivation();
	}
}

void USovGameplayAbility_EchoBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (bEndingEcho || !IsEndAbilityValid(Handle, ActorInfo)) { return; }
	++EchoActivationEpoch;
	bEchoEndPending = true;
	if (ScopeLockCount > 0)
	{
		// Narrative retires native dispatch immediately and queues virtual teardown.
		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}
	TStrongObjectPtr<USovGameplayAbility_EchoBase> ActionLifetime(this);
	TGuardValue<bool> Ending(bEndingEcho, true);
	UnbindCancellationTags();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MaximumDurationTimerHandle);
	}

	const bool bShouldBroadcastEnd = bEchoAbilityStarted
		&& ActorInfo
		&& (ActorInfo->IsLocallyControlled() || ActorInfo->IsNetAuthority());
	bEchoAbilityStarted = false;
	bAuthorityEchoSpendAttempted = false;
	bAuthorityEchoSpendSucceeded = false;
	EchoActivationAvatar.Reset();
	EchoActivationASC.Reset();
	EchoAttributes.Reset();
	EchoActivationSpec = FGameplayAbilitySpecHandle();
	if (bShouldBroadcastEnd)
	{
		ReceiveEchoAbilityEnded(bWasCancelled);
	}

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

USovEchoComponent* USovGameplayAbility_EchoBase::ResolveEchoComponent(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	USovEchoComponent* EchoComponent = Avatar
		? Avatar->FindComponentByClass<USovEchoComponent>()
		: nullptr;
	if (IsValid(EchoComponent) && ActorInfo)
	{
		// The ASC lives on Narrative's PlayerState and may be replaced across
		// possession/respawn. Rebinding here makes cost checks resilient to a
		// missed or delayed character-readiness callback on either network side.
		EchoComponent->InitializeWithAbilitySystem(
			ActorInfo->AbilitySystemComponent.Get());
	}
	return EchoComponent;
}

bool USovGameplayAbility_EchoBase::MeetsWeaponRequirement(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!bRequiresAllowedWeapon)
	{
		return true;
	}

	// Weapon-gated abilities fail closed until their concrete Blueprint child
	// declares the item classes that are valid for that payload.
	if (AllowedWeaponClasses.IsEmpty())
	{
		return false;
	}

	const auto IsAllowedWeapon = [this](const UWeaponItem* Weapon)
	{
		if (!IsValid(Weapon) || !Weapon->IsWielded())
		{
			return false;
		}

		return AllowedWeaponClasses.ContainsByPredicate(
			[Weapon](const TSubclassOf<UWeaponItem>& AllowedWeaponClass)
			{
				return AllowedWeaponClass
					&& Weapon->IsA(AllowedWeaponClass.Get());
			});
	};

	const ANarrativeCharacter* Character = ActorInfo
		? Cast<ANarrativeCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	UWeaponItem* MainhandWeapon = Character ? Character->GetWeapon(true) : nullptr;
	UWeaponItem* OffhandWeapon = Character ? Character->GetWeapon(false) : nullptr;
	if (WeaponGatePolicy == ESovEchoWeaponGatePolicy::AnyAllowedWielded)
	{
		return IsAllowedWeapon(MainhandWeapon) || IsAllowedWeapon(OffhandWeapon);
	}

	// Weapon-specific abilities are normally granted by the weapon itself.
	// Requiring that source object to still be wielded prevents a stale spec
	// from firing after an equipment transition.
	const UWeaponItem* SourceWeapon = Cast<UWeaponItem>(GetSourceObject(Handle, ActorInfo));
	return IsAllowedWeapon(SourceWeapon)
		&& (SourceWeapon == MainhandWeapon || SourceWeapon == OffhandWeapon);
}

bool USovGameplayAbility_EchoBase::MeetsCharacterRequirement(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!RequiredCharacterTag.IsValid())
	{
		return true;
	}

	const UAbilitySystemComponent* AbilitySystem = ActorInfo
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	if (!IsValid(AbilitySystem))
	{
		return false;
	}
	if (AbilitySystem->HasMatchingGameplayTag(RequiredCharacterTag))
	{
		return true;
	}

	// Preserve existing Tarrik content until Player Definitions adopt the new
	// identity contract. Once any player identity is present, mismatches fail.
	return !AbilitySystem->HasMatchingGameplayTag(
		FSovGameplayTags::Get().Character_Player);
}

void USovGameplayAbility_EchoBase::AddEchoFailureTags(
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!OptionalRelevantTags)
	{
		return;
	}

	OptionalRelevantTags->AddTag(FNarrativeGameplayTags::Get().Ability_ActivateFail_Cost);
	OptionalRelevantTags->AddTag(FSovGameplayTags::Get().Ability_ActivateFail_Echo);
}

void USovGameplayAbility_EchoBase::BindCancellationTags(
	UAbilitySystemComponent* AbilitySystem)
{
	UnbindCancellationTags();
	if (!IsValid(AbilitySystem))
	{
		return;
	}

	BoundAbilitySystem = AbilitySystem;
	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	FGameplayTagContainer TagsToObserve;
	TagsToObserve.AddTag(NarrativeTags.State_IsDead);
	TagsToObserve.AddTag(NarrativeTags.State_Interacting);
	TagsToObserve.AddTag(NarrativeTags.State_SequencerControlled);
	TagsToObserve.AddTag(NarrativeTags.State_Movement_Ragdoll);
	TagsToObserve.AddTag(NarrativeTags.State_Weapon_Equipping);
	TagsToObserve.AddTag(SovTags.State_Fatal);
	TagsToObserve.AddTag(SovTags.State_Guard_Broken);
	TagsToObserve.AddTag(SovTags.State_Poise_Broken);
	TagsToObserve.AddTag(SovTags.State_Status_Frozen);

	for (const FGameplayTag& Tag : TagsToObserve)
	{
		FDelegateHandle Handle = BoundAbilitySystem
			->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleCancellationTagChanged);
		CancellationTagHandles.Add(Tag, Handle);
	}
	const FGameplayTag BusyTag = NarrativeTags.State_Busy;
	BusyTagChangedHandle = BoundAbilitySystem
		->RegisterGameplayTagEvent(BusyTag, EGameplayTagEventType::AnyCountChange)
		.AddUObject(this, &ThisClass::HandleCancellationTagChanged);

	// Close the race where an interrupt state arrived after activation checks
	// but before callbacks were registered. One Busy contribution is ours.
	if (BoundAbilitySystem->HasAnyMatchingGameplayTags(TagsToObserve)
		|| BoundAbilitySystem->GetGameplayTagCount(BusyTag) > 1)
	{
		FinishEchoAbility(true);
	}
}

void USovGameplayAbility_EchoBase::UnbindCancellationTags()
{
	if (IsValid(BoundAbilitySystem))
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Entry : CancellationTagHandles)
		{
			BoundAbilitySystem
				->RegisterGameplayTagEvent(Entry.Key, EGameplayTagEventType::NewOrRemoved)
				.Remove(Entry.Value);
		}
		BoundAbilitySystem
			->RegisterGameplayTagEvent(
				FNarrativeGameplayTags::Get().State_Busy,
				EGameplayTagEventType::AnyCountChange)
			.Remove(BusyTagChangedHandle);
	}

	CancellationTagHandles.Reset();
	BusyTagChangedHandle.Reset();
	BoundAbilitySystem = nullptr;
}

void USovGameplayAbility_EchoBase::HandleCancellationTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	const bool bExternalBusy = CallbackTag == FNarrativeGameplayTags::Get().State_Busy
		&& NewCount > 1;
	if (bExternalBusy
		|| (CallbackTag != FNarrativeGameplayTags::Get().State_Busy && NewCount > 0))
	{
		FinishEchoAbility(true);
	}
}

void USovGameplayAbility_EchoBase::HandleMaximumDurationExpired()
{
	FinishEchoAbility(true);
}
