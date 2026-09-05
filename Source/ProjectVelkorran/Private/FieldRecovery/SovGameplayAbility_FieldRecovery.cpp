// Copyright Fallen Signal Studios. All Rights Reserved.
#include "FieldRecovery/SovGameplayAbility_FieldRecovery.h"
#include "FieldRecovery/SovFieldRecoveryComponent.h"
#include "FieldRecovery/SovFieldRecoveryPolicy.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Controller.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "TimerManager.h"
namespace
{
	FGameplayTagContainer Interruptions()
	{
		const auto& N = FNarrativeGameplayTags::Get(); const auto& S = FSovGameplayTags::Get();
		FGameplayTagContainer Tags;
		Tags.AddTag(N.State_IsDead); Tags.AddTag(N.State_SequencerControlled); Tags.AddTag(N.State_Interacting);
		Tags.AddTag(N.State_Weapon_Equipping); Tags.AddTag(N.State_Movement_Ragdoll); Tags.AddTag(S.State_Fatal);
		Tags.AddTag(S.State_Poise_Broken); Tags.AddTag(S.State_Guard_Broken); Tags.AddTag(S.State_Status_Frozen);
		Tags.AddTag(S.State_Traversal); return Tags;
	}
}
USovGameplayAbility_FieldRecovery::USovGameplayAbility_FieldRecovery()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InputTag = FSovGameplayTags::Get().Input_FieldRecovery;
	SetAssetTags(FGameplayTagContainer(FSovGameplayTags::Get().Ability_FieldRecovery));
	ActivationOwnedTags.AddTag(FNarrativeGameplayTags::Get().State_Busy);
	ActivationOwnedTags.AddTag(FSovGameplayTags::Get().State_FieldRecovery);
	ActivationBlockedTags.AppendTags(Interruptions());
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_Busy);
	ActivationBlockedTags.AddTag(FSovGameplayTags::Get().State_Guarding);
	ActivationBlockedTags.AddTag(FSovGameplayTags::Get().State_Deflecting);
	ActivationBlockedTags.AddTag(FSovGameplayTags::Get().State_EchoAbility_Active);
}
bool USovGameplayAbility_FieldRecovery::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
	const auto* Player = Info ? Cast<ASovPlayerCharacterBase>(Info->AvatarActor.Get()) : nullptr;
	return !bEnding && Info && Info->IsNetAuthority() && Player && !GetCostGameplayEffect()
		&& Player->GetFieldRecoveryComponent() && Player->GetFieldRecoveryComponent()->CanUse()
		&& Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags);
}
bool USovGameplayAbility_FieldRecovery::ContextValid() const
{
	return IsActive() && !bEnding && !bEndPending && CurrentActorInfo && BoundASC.IsValid() && BoundAvatar.IsValid() && Recovery.IsValid()
		&& CurrentActorInfo->AbilitySystemComponent.Get() == BoundASC.Get() && CurrentActorInfo->AvatarActor.Get() == BoundAvatar.Get()
		&& BoundASC->GetAvatarActor() == BoundAvatar.Get() && Recovery->HasLiveOwner() && Recovery->StateEpoch == RecoveryStateEpoch
		&& BoundASC->GetCharacterReadyEpoch() == ReadyEpoch && Cast<APawn>(BoundAvatar.Get())->GetController() == BoundController.Get()
		&& !BoundASC->HasAnyMatchingGameplayTags(Interruptions());
}
void USovGameplayAbility_FieldRecovery::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	const uint64 ThisEpoch = ++Epoch;
	bCompleted = false; CompletedHeal = 0.f; bPresented = false; bEndPending = false; ChargeUse.Invalidate();
	Super::ActivateAbility(Handle, Info, Activation, Event);
	if (!IsActive() || ThisEpoch != Epoch) { return; }
	auto* Player = Info ? Cast<ASovPlayerCharacterBase>(Info->AvatarActor.Get()) : nullptr;
	BoundAvatar = Player; BoundASC = Info ? Cast<UNarrativeAbilitySystemComponent>(Info->AbilitySystemComponent.Get()) : nullptr;
	BoundController = Player ? Player->GetController() : nullptr; ReadyEpoch = BoundASC.IsValid() ? BoundASC->GetCharacterReadyEpoch() : 0;
	Recovery = Player ? Player->GetFieldRecoveryComponent() : nullptr;
	RecoveryStateEpoch = Recovery.IsValid() ? Recovery->StateEpoch : 0;
	if (!ContextValid() || !CommitAbility(Handle, Info, Activation) || !ContextValid() || ThisEpoch != Epoch)
	{ if (ThisEpoch == Epoch && IsActive()) { EndAbility(Handle, Info, Activation, true, true); } return; }
	BoundASC->OnDamageResolvedAsTarget.AddUniqueDynamic(this, &ThisClass::HandleDamage);
	HealthChangedHandle = BoundASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute())
		.AddUObject(this, &ThisClass::HandleHealthChanged);
	for (const auto& Tag : Interruptions())
	{ InterruptHandles.Add(Tag, BoundASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleInterrupt)); }
	const FGuid NewUse = Recovery->BeginUse(this);
	if (ThisEpoch != Epoch) { return; }
	if (!ContextValid())
	{
		if (Recovery.IsValid()) { Recovery->CancelUse(this, NewUse); }
		if (IsActive()) { EndAbility(Handle, Info, Activation, true, true); } return;
	}
	ChargeUse = NewUse;
	if (!ChargeUse.IsValid()) { EndAbility(Handle, Info, Activation, true, true); return; }
	if (RecoveryMontage)
	{
		const float Rate = RecoveryMontage->GetPlayLength() / Recovery->CommittedDuration;
		if (!FMath::IsFinite(Rate) || Rate <= 0.f) { EndAbility(Handle, Info, Activation, true, true); return; }
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, RecoveryMontage, Rate, NAME_None, true);
		if (!MontageTask) { EndAbility(Handle, Info, Activation, true, true); return; }
		MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleMontageInterrupted);
		MontageTask->ReadyForActivation();
	}
	if (ThisEpoch != Epoch) { return; }
	if (!ContextValid()) { if (IsActive()) { EndAbility(Handle, Info, Activation, true, true); } return; }
	GetWorld()->GetTimerManager().SetTimer(PollTimer, this, &ThisClass::Poll, 0.02f, true);
	bPresented = true; OnRecoveryStarted(Recovery->UseSeconds);
}
void USovGameplayAbility_FieldRecovery::Poll()
{
	if (!ContextValid() || !Recovery->IsCurrentUse(this, ChargeUse))
	{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
	if (!SovFieldRecoveryPolicy::Completed(GetWorld()->GetTimeSeconds(), Recovery->StartedAt, Recovery->CommittedDuration)) { return; }
	const uint64 ThisEpoch = Epoch;
	float Healed = 0.f;
	const bool bHealed = Recovery->CompleteUse(this, ChargeUse, Healed);
	if (ThisEpoch != Epoch || !IsActive()) { return; }
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bHealed);
}
void USovGameplayAbility_FieldRecovery::HandleInterrupt(FGameplayTag Tag, int32 Count)
{
	static_cast<void>(Tag);
	if (Count > 0 && IsActive()) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); }
}
void USovGameplayAbility_FieldRecovery::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue < Data.OldValue && IsActive()) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); }
}
void USovGameplayAbility_FieldRecovery::HandleDamage(const FSovDamageResult& Result)
{
	if (IsActive() && Result.TargetActor == BoundAvatar.Get() && Result.TransactionId.IsValid()
		&& (Result.AppliedHealthDamage > 0.f || Result.AppliedShieldDamage > 0.f || Result.AppliedPoiseDamage > 0.f))
	{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); }
}
void USovGameplayAbility_FieldRecovery::HandleMontageInterrupted()
{
	if (IsActive()) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); }
}
void USovGameplayAbility_FieldRecovery::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, bool bReplicate, bool bCancelled)
{
	if (bEnding || !IsEndAbilityValid(Handle, Info)) { return; }
	bEndPending = true;
	if (ScopeLockCount > 0)
	{
		++Epoch;
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility, Handle, Info, Activation, bReplicate, bCancelled)); return;
	}
	TGuardValue<bool> Ending(bEnding, true); ++Epoch;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(PollTimer); }
	if (MontageTask)
	{
		MontageTask->OnInterrupted.RemoveDynamic(this, &ThisClass::HandleMontageInterrupted);
		MontageTask->OnCancelled.RemoveDynamic(this, &ThisClass::HandleMontageInterrupted);
		MontageTask->EndTask(); MontageTask = nullptr;
	}
	if (Recovery.IsValid()) { Recovery->CancelUse(this, ChargeUse); }
	if (BoundASC.IsValid())
	{
		BoundASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::HandleDamage);
		BoundASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute()).Remove(HealthChangedHandle);
		for (const auto& Pair : InterruptHandles)
		{ BoundASC->RegisterGameplayTagEvent(Pair.Key, EGameplayTagEventType::NewOrRemoved).Remove(Pair.Value); }
		if (RecoveryMontage && BoundASC->GetAnimatingAbility() == this) { BoundASC->CurrentMontageStop(); }
	}
	InterruptHandles.Reset(); HealthChangedHandle.Reset(); ChargeUse.Invalidate();
	const bool bCanNotify = Recovery.IsValid() && Recovery->StateEpoch == RecoveryStateEpoch && BoundASC.IsValid()
		&& BoundAvatar.IsValid() && BoundASC->GetAvatarActor() == BoundAvatar.Get()
		&& BoundASC->GetCharacterReadyEpoch() == ReadyEpoch && Cast<APawn>(BoundAvatar.Get())->GetController() == BoundController.Get()
		&& CurrentActorInfo && CurrentActorInfo->AvatarActor.Get() == BoundAvatar.Get();
	Recovery.Reset(); BoundASC.Reset(); BoundAvatar.Reset(); BoundController.Reset();
	const bool bNotify = bPresented; bPresented = false;
	if (bNotify && bCanNotify) { OnRecoveryFinished(bCompleted, CompletedHeal); }
	Super::EndAbility(Handle, Info, Activation, bReplicate, bCancelled);
}
