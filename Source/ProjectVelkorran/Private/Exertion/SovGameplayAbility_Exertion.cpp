// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Exertion/SovGameplayAbility_Exertion.h"
#include "Exertion/SovExertionComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemComponent.h"
#include "Character/NarrativeCharacterMovement.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/RootMotionSource.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeCombatAbility.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "TimerManager.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"

namespace
{
	FGameplayTagContainer InterruptTags()
	{
		FGameplayTagContainer Tags;
		const auto& N = FNarrativeGameplayTags::Get();
		const auto& S = FSovGameplayTags::Get();
		Tags.AddTag(N.State_IsDead); Tags.AddTag(N.State_Interacting); Tags.AddTag(N.State_SequencerControlled);
		Tags.AddTag(N.State_Movement_Ragdoll); Tags.AddTag(N.State_Weapon_Equipping);
		Tags.AddTag(S.State_Fatal); Tags.AddTag(S.State_Poise_Broken); Tags.AddTag(S.State_Status_Frozen);
		return Tags;
	}
	FActiveGameplayEffectHandle GrantWindow(UAbilitySystemComponent* ASC, const FGameplayTagContainer& Tags, float Duration)
	{
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(USovGameplayEffect_ExertionWindow::StaticClass(), 1.f, ASC->MakeEffectContext());
		if (!Spec.IsValid()) { return {}; }
		Spec.Data->DynamicGrantedTags.AppendTags(Tags);
		Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Duration, Duration);
		Spec.Data->SetDuration(Duration, true);
		return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

USovGameplayEffect_ExertionWindow::USovGameplayEffect_ExertionWindow()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	FSetByCallerFloat Duration;
	Duration.DataTag = FNarrativeGameplayTags::Get().SetByCaller_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);
}

USovGameplayAbility_Evade::USovGameplayAbility_Evade()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InputTag = FSovGameplayTags::Get().Input_Evade;
	SetAssetTags(FGameplayTagContainer(FSovGameplayTags::Get().Ability_Evade));
	ActivationBlockedTags.AppendTags(InterruptTags());
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_Busy);
	ActivationBlockedTags.AddTag(FSovGameplayTags::Get().State_Evading);
}

bool USovGameplayAbility_Evade::ResolveEvade(const FGameplayAbilityActorInfo* ActorInfo, FVector& Direction, float& Distance) const
{
	const auto* Character = ActorInfo ? Cast<ASovPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	const auto* Exertion = Character ? Character->GetExertionComponent() : nullptr;
	const auto* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const auto* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Character || !Exertion || !Capsule || !Movement || !ActorInfo->IsNetAuthority()
		|| !Character->IsCharacterReady() || !Movement->IsMovingOnGround() || Movement->HasRootMotionSources()
		|| !ActorInfo->AbilitySystemComponent.IsValid() || ActorInfo->AbilitySystemComponent->GetAvatarActor() != Character) { return false; }
	const FSovExertionProfile Profile = Exertion->GetProfile();
	if (!FMath::IsFinite(Profile.EvadeDistance) || Profile.EvadeDistance <= 12.f
		|| !FMath::IsFinite(Profile.EvadeDuration) || Profile.EvadeDuration <= 0.f
		|| !FMath::IsFinite(Profile.EvadeInvulnerability) || Profile.EvadeInvulnerability <= 0.f
		|| !Exertion->CanSpendExertion(Profile.EvadeCost)) { return false; }
	Direction = Character->GetLastMovementInputVector().GetSafeNormal2D();
	if (Direction.IsNearlyZero()) { Direction = Character->GetActorForwardVector().GetSafeNormal2D(); }
	if (Direction.IsNearlyZero() || Direction.ContainsNaN()) { return false; }
	Distance = Profile.EvadeDistance;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SovEvadeAdmission), false, Character);
	if (Character->GetCharacterVisual()) { Params.AddIgnoredActor(Character->GetCharacterVisual()); }
	FHitResult Hit;
	const FVector Start = Character->GetActorLocation();
	Character->GetWorld()->SweepSingleByChannel(Hit, Start, Start + Direction * Distance, Capsule->GetComponentQuat(),
		Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
		Params, FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
	if (Hit.bStartPenetrating) { return false; }
	if (Hit.bBlockingHit) { Distance = FMath::Max(0.f, Hit.Distance - 2.f); }
	return Distance >= 12.f;
}

bool USovGameplayAbility_Evade::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	FVector Direction; float Distance = 0.f;
	return !bEnding && !GetCostGameplayEffect() && Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& ResolveEvade(ActorInfo, Direction, Distance);
}

bool USovGameplayAbility_Evade::CanActivateAfterCombatCancel(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, UNarrativeCombatAbility* CancellingAttack, float PendingCancelCost) const
{
	FVector Direction; float Distance = 0.f; FGuid AttackId;
	const auto* Character = ActorInfo ? Cast<ASovPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!Character || !ASC || !IsValid(CancellingAttack) || !FMath::IsFinite(PendingCancelCost) || PendingCancelCost < 0.f
		|| !CancellingAttack->GetSovAttackIdentity(Character, AttackId) || bEnding || GetCostGameplayEffect()
		|| !ResolveEvade(ActorInfo, Direction, Distance) || !CheckCooldown(Handle, ActorInfo)) { return false; }
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	if (!Spec || Spec->PendingRemove || Spec->IsActive()
		|| (Spec->Ability != this && Spec->GetPrimaryInstance() != this)
		|| ASC->AreAbilityTagsBlocked(GetAssetTags())) { return false; }
	const FGameplayTag Busy = FNarrativeGameplayTags::Get().State_Busy;
	const int32 OwnedBusy = CancellingAttack->OwnsCombatActivationTag(Busy) ? 1 : 0;
	if (ASC->GetGameplayTagCount(Busy) > OwnedBusy) { return false; }
	FGameplayTagContainer OtherBlocked = ActivationBlockedTags;
	OtherBlocked.RemoveTag(Busy);
	if (ASC->HasAnyMatchingGameplayTags(OtherBlocked) || !ASC->HasAllMatchingGameplayTags(ActivationRequiredTags)) { return false; }
	return Character->GetExertionComponent()->CanSpendExertion(
		Character->GetExertionComponent()->GetProfile().EvadeCost + PendingCancelCost);
}

void USovGameplayAbility_Evade::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const uint32 Epoch = ++EvadeEpoch;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	FVector Direction; float Distance = 0.f;
	if (!IsActive() || Epoch != EvadeEpoch) { return; }
	if (!ResolveEvade(ActorInfo, Direction, Distance) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return;
	}
	if (!IsActive() || Epoch != EvadeEpoch) { return; }
	EvadingCharacter = Cast<ASovPlayerCharacterBase>(ActorInfo->AvatarActor.Get());
	EvadingASC = ActorInfo->AbilitySystemComponent;
	if (!HasValidSource()) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	const FSovExertionProfile Profile = EvadingCharacter->GetExertionComponent()->GetProfile();
	if (!EvadingCharacter->GetExertionComponent()->TrySpendExertion(Profile.EvadeCost)
		|| Epoch != EvadeEpoch || !HasValidSource())
	{
		if (Epoch == EvadeEpoch && IsActive()) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); }
		return;
	}
	UAbilitySystemComponent* PaidASC = EvadingASC.Get();
	FGameplayTagContainer BusyTags(FNarrativeGameplayTags::Get().State_Busy);
	BusyTags.AddTag(FSovGameplayTags::Get().State_Evading);
	const FActiveGameplayEffectHandle NewBusy = GrantWindow(PaidASC, BusyTags, Profile.EvadeDuration);
	if (Epoch != EvadeEpoch || !HasValidSource()) { PaidASC->RemoveActiveGameplayEffect(NewBusy); return; }
	BusyHandle = NewBusy;
	if (!BusyHandle.IsValid()) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	const auto* Settings = UNarrativeGameUserSettings::GetSovPlayerSettings(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	const float WindowScale = Settings ? Settings->GetDefenseWindowScale() : 1.f;
	const float Window = FMath::Min(Profile.EvadeInvulnerability * WindowScale, Profile.EvadeDuration);
	const FActiveGameplayEffectHandle NewInvulnerability = GrantWindow(PaidASC,
		FGameplayTagContainer(FSovGameplayTags::Get().State_Invulnerable), Window);
	if (Epoch != EvadeEpoch || !HasValidSource()) { PaidASC->RemoveActiveGameplayEffect(NewInvulnerability); return; }
	InvulnerabilityHandle = NewInvulnerability;
	if (!InvulnerabilityHandle.IsValid()) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	for (const FGameplayTag& Tag : InterruptTags())
	{
		InterruptHandles.Add(Tag, PaidASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleInterruptTag));
	}
	if (PaidASC->HasAnyMatchingGameplayTags(InterruptTags())) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	auto* Movement = EvadingCharacter->GetCharacterMovement();
	if (auto* NarrativeMovement = Cast<UNarrativeCharacterMovement>(Movement)) { NarrativeMovement->StopSprinting(); }
	bPreviousOrientToMovement = Movement->bOrientRotationToMovement;
	bPreviousControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
	bOwnedMovementRotation = true;
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = false;
	if (EvadingCharacter->GetProtagonistIdentityTag() == FSovGameplayTags::Get().Character_Player_Selene)
	{
		EvadingCharacter->SetActorRotation(Direction.Rotation());
	}
	TSharedPtr<FRootMotionSource_ConstantForce> Motion = MakeShared<FRootMotionSource_ConstantForce>();
	Motion->InstanceName = FName(*FString::Printf(TEXT("SovEvade_%u"), Epoch));
	Motion->AccumulateMode = ERootMotionAccumulateMode::Override;
	Motion->Priority = 1000;
	Motion->Force = Direction * (Distance / Profile.EvadeDuration);
	Motion->Duration = Profile.EvadeDuration;
	Motion->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	Motion->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	RootMotionSourceID = Movement->ApplyRootMotionSource(Motion);
	bOwnsRootMotionSource = true;
	EndWorldTime = GetWorld()->GetTimeSeconds() + Profile.EvadeDuration;
	GetWorld()->GetTimerManager().SetTimer(MonitorHandle, this, &ThisClass::MonitorEvade, 0.01f, true);
	bPresented = true;
	ReceiveEvadeStarted(Direction, Profile.EvadeDuration);
}

bool USovGameplayAbility_Evade::HasValidSource() const
{
	return IsActive() && EvadingCharacter.IsValid() && EvadingASC.IsValid() && CurrentActorInfo
		&& EvadingCharacter->IsCharacterReady() && CurrentActorInfo->AvatarActor.Get() == EvadingCharacter.Get()
		&& CurrentActorInfo->AbilitySystemComponent.Get() == EvadingASC.Get()
		&& EvadingASC->GetAvatarActor() == EvadingCharacter.Get()
		&& EvadingASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > KINDA_SMALL_NUMBER;
}

void USovGameplayAbility_Evade::MonitorEvade()
{
	if (!HasValidSource() || EvadingASC->HasAnyMatchingGameplayTags(InterruptTags()))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
	else if (GetWorld()->GetTimeSeconds() >= EndWorldTime)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void USovGameplayAbility_Evade::HandleInterruptTag(FGameplayTag Tag, int32 Count)
{
	static_cast<void>(Tag);
	if (Count > 0 && IsActive()) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); }
}

void USovGameplayAbility_Evade::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bEnding || !IsEndAbilityValid(Handle, ActorInfo)) { return; }
	if (ScopeLockCount > 0)
	{
		++EvadeEpoch;
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility, Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}
	TGuardValue<bool> Ending(bEnding, true);
	++EvadeEpoch;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(MonitorHandle); }
	if (EvadingASC.IsValid())
	{
		for (const auto& Pair : InterruptHandles)
		{
			EvadingASC->RegisterGameplayTagEvent(Pair.Key, EGameplayTagEventType::NewOrRemoved).Remove(Pair.Value);
		}
		EvadingASC->RemoveActiveGameplayEffect(InvulnerabilityHandle);
		if (EvadingASC.IsValid()) { EvadingASC->RemoveActiveGameplayEffect(BusyHandle); }
	}
	InterruptHandles.Reset(); InvulnerabilityHandle.Invalidate(); BusyHandle.Invalidate();
	if (EvadingCharacter.IsValid())
	{
		auto* Movement = EvadingCharacter->GetCharacterMovement();
		if (bOwnsRootMotionSource) { Movement->RemoveRootMotionSourceByID(RootMotionSourceID); }
		if (bOwnedMovementRotation)
		{
			if (!Movement->bOrientRotationToMovement) { Movement->bOrientRotationToMovement = bPreviousOrientToMovement; }
			if (!Movement->bUseControllerDesiredRotation) { Movement->bUseControllerDesiredRotation = bPreviousControllerDesiredRotation; }
		}
	}
	RootMotionSourceID = 0; bOwnsRootMotionSource = false; bOwnedMovementRotation = false;
	EvadingCharacter.Reset(); EvadingASC.Reset();
	const bool bNotify = bPresented; bPresented = false;
	if (bNotify) { ReceiveEvadeEnded(bWasCancelled); }
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

USovGameplayAbility_Sprint::USovGameplayAbility_Sprint()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Sprint;
	ActivationBlockedTags.AppendTags(InterruptTags());
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_Busy);
}

bool USovGameplayAbility_Sprint::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const auto* Character = ActorInfo ? Cast<ASovPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	return !bEnding && !GetCostGameplayEffect() && Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& Character && Character->GetExertionComponent() && Character->GetExertionComponent()->CanSprint();
}

void USovGameplayAbility_Sprint::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) { return; }
	SprintingCharacter = ActorInfo ? Cast<ASovPlayerCharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!SprintingCharacter.IsValid() || !CommitAbility(Handle, ActorInfo, ActivationInfo) || !IsActive())
	{
		if (IsActive()) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); } return;
	}
	auto* Movement = Cast<UNarrativeCharacterMovement>(SprintingCharacter->GetCharacterMovement());
	if (!Movement || !SprintingCharacter->GetExertionComponent()->CanSprint()) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	Movement->StartSprinting();
	if (!Movement->bWantsSprint) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	GetWorld()->GetTimerManager().SetTimer(MonitorHandle, this, &ThisClass::MonitorSprint, 0.02f, true);
	ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	if (!ReleaseTask) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleReleased);
	ReleaseTask->ReadyForActivation();
}

void USovGameplayAbility_Sprint::HandleReleased(float HeldTime)
{
	static_cast<void>(HeldTime);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USovGameplayAbility_Sprint::MonitorSprint()
{
	if (!SprintingCharacter.IsValid() || !CurrentActorInfo || CurrentActorInfo->AvatarActor.Get() != SprintingCharacter.Get()
		|| !SprintingCharacter->GetExertionComponent()->CanSprint()
		|| !Cast<UNarrativeCharacterMovement>(SprintingCharacter->GetCharacterMovement())->bWantsSprint)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void USovGameplayAbility_Sprint::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bEnding || !IsEndAbilityValid(Handle, ActorInfo)) { return; }
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility, Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}
	TGuardValue<bool> Ending(bEnding, true);
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(MonitorHandle); }
	if (ReleaseTask) { ReleaseTask->OnRelease.RemoveDynamic(this, &ThisClass::HandleReleased); ReleaseTask->EndTask(); ReleaseTask = nullptr; }
	if (SprintingCharacter.IsValid())
	{
		if (auto* Movement = Cast<UNarrativeCharacterMovement>(SprintingCharacter->GetCharacterMovement())) { Movement->StopSprinting(); }
	}
	SprintingCharacter.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
