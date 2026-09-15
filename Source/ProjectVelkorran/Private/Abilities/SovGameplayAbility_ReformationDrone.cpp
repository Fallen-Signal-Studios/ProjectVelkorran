// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_ReformationDrone.h"
#include "World/SovDestructibleCover.h"
#include "Engine/DamageEvents.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AIController.h"
#include "AISystem.h"
#include "Animation/AnimMontage.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Characters/SovDroneNPCBase.h"
#include "CollisionQueryParams.h"
#include "Combat/SovNativeDamageReceipt.h"
#include "Combat/SovProtectionInterceptReceipt.h"
#include "Combat/SovThreatTargeting.h"
#include "Components/SkeletalMeshComponent.h"
#include "Effects/SovGameplayEffect_ReformationDroneWeapons.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeGameplayTags.h"
#include "Navigation/PathFollowingComponent.h"
#include "Projectiles/SovReformationDroneRocketProjectile.h"
#include "Presentation/SovReformationDroneGunshotPresentation.h"
#include "Presentation/SovReformationDroneSelfDestructPresentation.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Weapons/NarrativeProjectile.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovReformationDroneAbility, Log, All);

namespace
{
	FGameplayTagContainer DroneInterruptions()
	{
		const auto& N = FNarrativeGameplayTags::Get();
		const auto& S = FSovGameplayTags::Get();
		FGameplayTagContainer Tags;
		Tags.AddTag(N.State_IsDead);
		Tags.AddTag(N.State_Interacting);
		Tags.AddTag(N.State_SequencerControlled);
		Tags.AddTag(N.State_Movement_Ragdoll);
		Tags.AddTag(N.State_Weapon_BlockFiring);
		Tags.AddTag(S.State_Fatal);
		Tags.AddTag(S.State_Poise_Broken);
		Tags.AddTag(S.State_Status_Frozen);
		Tags.AddTag(S.State_Status_DeviceDisabled);
		return Tags;
	}

	/**
	 * Narrative can route weapon traces through its runtime CharacterVisual or
	 * another owned presentation actor. Walk those ownership/provider links so
	 * a visible drone/body hit still resolves the authoritative character ASC.
	 */
	UAbilitySystemComponent* ResolveAbilitySystemFromActor(AActor* InActor)
	{
		AActor* Candidate = InActor;
		TSet<const AActor*> VisitedActors;
		for (int32 Depth = 0;
			IsValid(Candidate) && Depth < 6 && !VisitedActors.Contains(Candidate);
			++Depth)
		{
			VisitedActors.Add(Candidate);
			// Projectiles may expose their firing character through Narrative's
			// owner interface. They are valid physical blockers, never damage
			// proxies for the character that launched them.
			if (Candidate->IsA<ANarrativeProjectile>())
			{
				return nullptr;
			}
			if (UAbilitySystemComponent* AbilitySystem =
				UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate))
			{
				return AbilitySystem;
			}

			if (const INarrativeCharacterOwner* CharacterProvider =
				Cast<INarrativeCharacterOwner>(Candidate))
			{
				ANarrativeCharacter* NarrativeCharacter =
					CharacterProvider->GetNarrativeCharacter();
				if (IsValid(NarrativeCharacter)
					&& NarrativeCharacter != Candidate)
				{
					if (UAbilitySystemComponent* AbilitySystem =
						UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(
							NarrativeCharacter))
					{
						return AbilitySystem;
					}
				}
			}

			Candidate = Candidate->GetOwner();
		}

		return nullptr;
	}
}

USovGameplayAbility_ReformationDroneWeaponBase::
	USovGameplayAbility_ReformationDroneWeaponBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
	bRequiresAmmo = false;

	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	ActivationBlockedTags.AddTag(NarrativeTags.State_IsDead);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Interacting);
	ActivationBlockedTags.AddTag(NarrativeTags.State_SequencerControlled);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Movement_Ragdoll);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Weapon_BlockFiring);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Weapon_IsFiring);
	ActivationBlockedTags.AddTag(SovTags.State_Fatal);
	ActivationBlockedTags.AddTag(SovTags.State_Poise_Broken);
	ActivationBlockedTags.AddTag(SovTags.State_Status_Frozen);
	ActivationBlockedTags.AddTag(SovTags.State_Status_DeviceDisabled);
	ActivationOwnedTags.AddTag(NarrativeTags.State_Weapon_IsFiring);
}

bool USovGameplayAbility_ReformationDroneWeaponBase::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (bEndingAbility || bEndPending)
	{
		return false;
	}
	if (!Super::CanActivateAbility(
			Handle,
			ActorInfo,
			SourceTags,
			TargetTags,
			OptionalRelevantTags))
	{
		return false;
	}

	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(
				FNarrativeGameplayTags::Get().Ability_ActivateFail_Networking);
		}
		return false;
	}
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const auto* Attributes = IsValid(ASC) ? ASC->GetSet<UNarrativeAttributeSetBase>() : nullptr;
	if (!IsValid(ASC) || !IsValid(Attributes)
		|| ASC->GetAvatarActor() != ActorInfo->AvatarActor.Get()
		|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ActorInfo->AvatarActor.Get()) != ASC
		|| !FMath::IsFinite(Attributes->GetHealth()) || Attributes->GetHealth() <= 0.f
		|| !IsValid(Cast<ANarrativeCharacter>(ActorInfo->AvatarActor.Get()))
		|| !SovThreatTargeting::CanUseActorFocus(ActorInfo->AvatarActor.Get()))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(
				FNarrativeGameplayTags::Get().Ability_ActivateFail_TagsMissing);
		}
		return false;
	}

	if (!HasRequiredPayloadConfiguration())
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(
				FNarrativeGameplayTags::Get().Ability_ActivateFail_TagsBlocked);
		}
		return false;
	}

	const UWorld* World = GetWorld();
	if (IsValid(World)
		&& World->GetTimeSeconds() + KINDA_SMALL_NUMBER < NextAllowedActivationTime)
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(
				FNarrativeGameplayTags::Get().Ability_ActivateFail_Cooldown);
		}
		return false;
	}

	return true;
}

void USovGameplayAbility_ReformationDroneWeaponBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	TStrongObjectPtr<USovGameplayAbility_ReformationDroneWeaponBase> ActionLifetime(this);
	const uint64 Epoch = AdvanceWeaponActivationEpoch();
	bEndPending = false;
	bPayloadStarted = false;
	bPayloadFinished = false;
	bAbilityStarted = false;
	bEndingAbility = false;
	CharacterOwner = ActorInfo
		? Cast<ANarrativeCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	ActionASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	ActionAvatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	ActionWorld = ActionAvatar.IsValid() ? ActionAvatar->GetWorld() : nullptr;
	ActionAttributes = ActionASC.IsValid() ? ActionASC->GetSet<UNarrativeAttributeSetBase>() : nullptr;
	const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(ActionASC.Get());
	ActionActorInfoEpoch = NarrativeASC ? NarrativeASC->GetCombatActorInfoEpoch() : 0;
	ActionLifeEpoch = ActionAttributes.IsValid() ? ActionAttributes->GetCombatLifeEpoch() : 0;

	if (!ActorInfo
		|| !ActorInfo->IsNetAuthority()
		|| !IsValid(CharacterOwner.Get()) || !HasCurrentWeaponOwner())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	BindInterruptions();
	const bool bCommitted = CommitAbility(Handle, ActorInfo, ActivationInfo);
	if (!ValidateWeaponContinuation(Epoch)) { return; }
	if (!bCommitted)
	{
		CancelDroneWeaponAbility();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		NextAllowedActivationTime = World->GetTimeSeconds()
			+ FMath::Max(CooldownDuration, 0.0f);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!ValidateWeaponContinuation(Epoch))
	{
		return;
	}

	bAbilityStarted = true;
	StartAttackMontage();
	if (!ValidateWeaponContinuation(Epoch))
	{
		return;
	}
	ReceiveDroneWeaponStarted();
	if (!ValidateWeaponContinuation(Epoch))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		// An authored start hook is allowed to release immediately. Do not arm
		// the native fallback timer after that manual release has already begun.
		if (bAutoReleasePayload && !bPayloadStarted && !bPayloadFinished)
		{
			const float ReleaseDelay = FMath::Max(PayloadReleaseDelay, 0.0f);
			if (ReleaseDelay <= KINDA_SMALL_NUMBER)
			{
				HandleAutomaticPayloadRelease(Epoch);
			}
			else
			{
				World->GetTimerManager().SetTimer(
					PayloadReleaseTimerHandle,
					FTimerDelegate::CreateUObject(this, &ThisClass::HandleAutomaticPayloadRelease, Epoch),
					ReleaseDelay,
					false);
			}
		}
		if (!ValidateWeaponContinuation(Epoch))
		{
			return;
		}

		World->GetTimerManager().SetTimer(
			MaximumDurationTimerHandle,
			FTimerDelegate::CreateUObject(this, &ThisClass::HandleMaximumDurationExpired, Epoch),
			FMath::Max(MaximumActiveDuration, 0.1f),
			false);
	}
}

void USovGameplayAbility_ReformationDroneWeaponBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (bEndingAbility || Handle != CurrentSpecHandle || !IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}
	bEndPending = true;
	AdvanceWeaponActivationEpoch();
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility,
			Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}
	TStrongObjectPtr<USovGameplayAbility_ReformationDroneWeaponBase> ActionLifetime(this);
	bEndingAbility = true;
	UnbindInterruptions();

	if (UWorld* World = GetWeaponActionWorld())
	{
		World->GetTimerManager().ClearTimer(PayloadReleaseTimerHandle);
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
		World->GetTimerManager().ClearTimer(MaximumDurationTimerHandle);
	}

	UAbilityTask_PlayMontageAndWait* RetiredMontage = MontageTask.Get();
	MontageTask = nullptr;
	if (IsValid(RetiredMontage))
	{
		RetiredMontage->OnCompleted.RemoveAll(this);
		RetiredMontage->OnBlendOut.RemoveAll(this);
		RetiredMontage->OnInterrupted.RemoveAll(this);
		RetiredMontage->OnCancelled.RemoveAll(this);
		RetiredMontage->EndTask();
	}
	CleanupWeaponPayload();
	ActionASC.Reset();
	ActionAvatar.Reset();
	ActionWorld.Reset();
	ActionAttributes.Reset();
	const bool bShouldBroadcastEnd = bAbilityStarted;
	bAbilityStarted = false;
	bPayloadStarted = false;
	bPayloadFinished = false;
	if (bShouldBroadcastEnd)
	{
		ReceiveDroneWeaponEnded(bWasCancelled);
	}

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
	bEndingAbility = false;
	bEndPending = false;
}

void USovGameplayAbility_ReformationDroneWeaponBase::CleanupWeaponPayload() {}

uint64 USovGameplayAbility_ReformationDroneWeaponBase::AdvanceWeaponActivationEpoch()
{
	if (++WeaponActivationEpoch == 0) { ++WeaponActivationEpoch; }
	return WeaponActivationEpoch;
}

bool USovGameplayAbility_ReformationDroneWeaponBase::IsWeaponActivationCurrent(uint64 ExpectedEpoch) const
{
	return ExpectedEpoch != 0 && WeaponActivationEpoch == ExpectedEpoch && IsActive()
		&& !bEndingAbility && !bEndPending;
}

bool USovGameplayAbility_ReformationDroneWeaponBase::HasCurrentWeaponOwner() const
{
	const auto* NativeASC = Cast<UNarrativeAbilitySystemComponent>(ActionASC.Get());
	return CurrentActorInfo && CurrentActorInfo->IsNetAuthority()
		&& ActionAvatar.IsValid() && !ActionAvatar->IsActorBeingDestroyed() && ActionASC.IsValid()
		&& ActionWorld.IsValid() && ActionAvatar->GetWorld() == ActionWorld.Get() && GetWorld() == ActionWorld.Get()
		&& CurrentActorInfo->AvatarActor.Get() == ActionAvatar.Get()
		&& CurrentActorInfo->AbilitySystemComponent.Get() == ActionASC.Get()
		&& ActionASC->GetAvatarActor() == ActionAvatar.Get()
		&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ActionAvatar.Get()) == ActionASC.Get()
		&& (!NativeASC || NativeASC->GetCombatActorInfoEpoch() == ActionActorInfoEpoch)
		&& ActionAttributes.IsValid() && ActionASC->GetSet<UNarrativeAttributeSetBase>() == ActionAttributes.Get()
		&& ActionAttributes->GetCombatLifeEpoch() == ActionLifeEpoch
		&& FMath::IsFinite(ActionAttributes->GetHealth()) && ActionAttributes->GetHealth() > 0.f
		&& !ActionASC->HasAnyMatchingGameplayTags(DroneInterruptions());
}

bool USovGameplayAbility_ReformationDroneWeaponBase::ValidateWeaponContinuation(uint64 ExpectedEpoch)
{
	if (!IsWeaponActivationCurrent(ExpectedEpoch)) { return false; }
	if (HasCurrentWeaponOwner()) { return true; }
	CancelDroneWeaponAbility();
	return false;
}

void USovGameplayAbility_ReformationDroneWeaponBase::BindInterruptions()
{
	if (!ActionASC.IsValid()) { return; }
	for (const FGameplayTag Tag : DroneInterruptions())
	{
		InterruptionHandles.Add(Tag, ActionASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::AnyCountChange)
			.AddUObject(this, &ThisClass::HandleInterruption, WeaponActivationEpoch));
	}
	HealthChangedHandle = ActionASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute())
		.AddUObject(this, &ThisClass::HandleHealthChanged, WeaponActivationEpoch);
}

void USovGameplayAbility_ReformationDroneWeaponBase::UnbindInterruptions()
{
	if (ActionASC.IsValid())
	{
		for (const auto& Entry : InterruptionHandles)
		{
			ActionASC->RegisterGameplayTagEvent(Entry.Key, EGameplayTagEventType::AnyCountChange).Remove(Entry.Value);
		}
		ActionASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute()).Remove(HealthChangedHandle);
	}
	InterruptionHandles.Reset();
	HealthChangedHandle.Reset();
}

void USovGameplayAbility_ReformationDroneWeaponBase::HandleInterruption(FGameplayTag Tag, int32 Count, uint64 ExpectedEpoch)
{
	if (Count > 0 && IsWeaponActivationCurrent(ExpectedEpoch)) { CancelDroneWeaponAbility(); }
}

void USovGameplayAbility_ReformationDroneWeaponBase::HandleHealthChanged(const FOnAttributeChangeData& Change, uint64 ExpectedEpoch)
{
	if ((!FMath::IsFinite(Change.NewValue) || Change.NewValue <= 0.f) && IsWeaponActivationCurrent(ExpectedEpoch))
	{
		CancelDroneWeaponAbility();
	}
}

bool USovGameplayAbility_ReformationDroneWeaponBase::
	HasRequiredPayloadConfiguration() const
{
	return AbilityIdentityTag.IsValid()
		&& DamageEffectClass.Get()
		&& !DamageChannels.IsEmpty()
		&& !AttackClassifications.IsEmpty()
		&& FMath::IsFinite(MontagePlayRate)
		&& MontagePlayRate > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(PayloadReleaseDelay)
		&& PayloadReleaseDelay >= 0.0f
		&& FMath::IsFinite(PostFireRecovery)
		&& PostFireRecovery >= 0.0f
		&& FMath::IsFinite(CooldownDuration)
		&& CooldownDuration >= 0.0f
		&& FMath::IsFinite(MaximumActiveDuration)
		&& MaximumActiveDuration >= 0.1f
		&& !FallbackMuzzleOffset.ContainsNaN();
}

void USovGameplayAbility_ReformationDroneWeaponBase::ExecuteAutomaticPayload()
{
	NotifyPayloadFinished();
}

bool USovGameplayAbility_ReformationDroneWeaponBase::
	TryBeginWeaponPayloadRelease()
{
	const uint64 Epoch = GetWeaponActivationEpoch();
	if (!ValidateWeaponContinuation(Epoch) || bPayloadFinished)
	{
		return false;
	}
	if (bPayloadStarted)
	{
		return true;
	}
	if (!CanContinueWeaponPayload())
	{
		return false;
	}

	bPayloadStarted = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PayloadReleaseTimerHandle);
	}
	ReceiveDroneWeaponPayloadReleased();
	return ValidateWeaponContinuation(Epoch) && !bPayloadFinished;
}

void USovGameplayAbility_ReformationDroneWeaponBase::NotifyPayloadFinished()
{
	const uint64 Epoch = GetWeaponActivationEpoch();
	if (!ValidateWeaponContinuation(Epoch) || bPayloadFinished)
	{
		return;
	}
	if (!bPayloadStarted)
	{
		UE_LOG(
			LogSovReformationDroneAbility,
			Warning,
			TEXT("%s tried to finish a drone weapon payload before release began."),
			*GetNameSafe(this));
		CancelDroneWeaponAbility();
		return;
	}

	bPayloadFinished = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PayloadReleaseTimerHandle);
	}
	const float Recovery = FMath::Max(PostFireRecovery, 0.0f);
	if (Recovery <= KINDA_SMALL_NUMBER)
	{
		HandleRecoveryFinished(Epoch);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RecoveryTimerHandle,
			FTimerDelegate::CreateUObject(this, &ThisClass::HandleRecoveryFinished, Epoch),
			Recovery,
			false);
		return;
	}

	// GetWorld should exist for an active GAS ability, but never strand the
	// attack lane if teardown races a recovery request.
	HandleRecoveryFinished(Epoch);
}

void USovGameplayAbility_ReformationDroneWeaponBase::CancelDroneWeaponAbility()
{
	if (IsActive())
	{
		EndAbility(
			CurrentSpecHandle,
			CurrentActorInfo,
			CurrentActivationInfo,
			true,
			true);
	}
}

bool USovGameplayAbility_ReformationDroneWeaponBase::CanContinueWeaponPayload() const
{
	return IsWeaponActivationCurrent(WeaponActivationEpoch) && !bPayloadFinished && HasCurrentWeaponOwner();
}

FTransform USovGameplayAbility_ReformationDroneWeaponBase::ResolveMuzzleTransform(
	const int32 MuzzleIndex) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	FTransform MuzzleTransform = IsValid(Avatar)
		? Avatar->GetActorTransform()
		: FTransform::Identity;
	if (!IsValid(Avatar))
	{
		return MuzzleTransform;
	}

	const ANarrativeCharacter* NarrativeCharacter =
		Cast<ANarrativeCharacter>(Avatar);
	const USkeletalMeshComponent* DroneMesh = IsValid(NarrativeCharacter)
		? NarrativeCharacter->GetMesh()
		: nullptr;
	if (IsValid(DroneMesh) && MuzzleSocketNames.Num() > 0)
	{
		const int32 SafeIndex = FMath::Abs(MuzzleIndex) % MuzzleSocketNames.Num();
		const FName SocketName = MuzzleSocketNames[SafeIndex];
		if (!SocketName.IsNone() && DroneMesh->DoesSocketExist(SocketName))
		{
			return DroneMesh->GetSocketTransform(SocketName, RTS_World);
		}
	}

	MuzzleTransform.SetLocation(
		Avatar->GetActorTransform().TransformPositionNoScale(FallbackMuzzleOffset));
	MuzzleTransform.SetScale3D(FVector::OneVector);
	return MuzzleTransform;
}

FVector USovGameplayAbility_ReformationDroneWeaponBase::ResolveAuthorityAimPoint(
	const float TraceDistance)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar))
	{
		return FVector::ZeroVector;
	}

	FVector AimStart = Avatar->GetActorLocation();
	FRotator AimRotation = Avatar->GetActorRotation();
	Avatar->GetActorEyesViewPoint(AimStart, AimRotation);
	if (const AController* Controller = GetOwningController())
	{
		if (const AAIController* AIController = Cast<AAIController>(Controller))
		{
			const FVector FocalPoint = AIController->GetFocalPoint();
			if (FAISystem::IsValidLocation(FocalPoint))
			{
				const FVector ToFocus = FocalPoint - AimStart;
				if (!ToFocus.IsNearlyZero())
				{
					AimRotation = ToFocus.Rotation();
				}
				else
				{
					AimRotation = Controller->GetControlRotation();
				}
			}
			else
			{
				AimRotation = Controller->GetControlRotation();
			}
		}
		else
		{
			AimRotation = Controller->GetControlRotation();
		}
	}

	const FVector AimEnd = AimStart
		+ (AimRotation.Vector() * FMath::Max(TraceDistance, 0.0f));
	const TArray<FHitResult> AimHits = PerformTraceMulti(AimStart, AimEnd, 0.0f);
	const FHitResult* BlockingHit = AimHits.FindByPredicate(
		[](const FHitResult& Hit)
		{
			return Hit.bBlockingHit;
		});
	return BlockingHit ? FVector(BlockingHit->ImpactPoint) : AimEnd;
}

bool USovGameplayAbility_ReformationDroneWeaponBase::ApplyPointDamage(
	const FHitResult& Hit,
	const float Damage,
	const float PoiseDamage,
	USovProtectionInterceptReceipt* ProtectionReceipt) const
{
	const uint64 Epoch = GetWeaponActivationEpoch();
	if (!CanContinueWeaponPayload())
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
	AActor* SourceActor = CurrentActorInfo->AvatarActor.Get();
	UAbilitySystemComponent* TargetASC = ResolveAbilitySystemFromActor(Hit.GetActor());
	// Environmental cover has no character ASC. Admit only an explicitly authored
	// owner, after the same activation gate; never send both scenery and GAS damage.
	if (ASovDestructibleCover* Cover = Cast<ASovDestructibleCover>(Hit.GetActor()))
	{
		if (!IsValid(SourceASC) || !IsValid(SourceActor) || !Hit.bBlockingHit
			|| !CurrentActorInfo->IsNetAuthority() || !IsWeaponActivationCurrent(Epoch)
			|| !CanContinueWeaponPayload()) { return false; }
		const FPointDamageEvent Event(Damage, Hit,
			(FVector(Hit.ImpactPoint) - SourceActor->GetActorLocation()).GetSafeNormal(), nullptr);
		return Cover->TakeDamage(Damage, Event, nullptr, SourceActor) > 0.f;
	}
	if (!IsValid(SourceASC)
		|| !IsValid(SourceActor)
		|| !IsValid(TargetASC)
		|| !Hit.bBlockingHit
		|| !DamageEffectClass.Get()
		|| Damage <= KINDA_SMALL_NUMBER
		|| !IsHostileTarget(TargetASC)
		|| !IsTargetAlive(TargetASC))
	{
		return false;
	}
	if (!IsWeaponActivationCurrent(Epoch) || !CanContinueWeaponPayload()) { return false; }

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.SetAbility(this);
	Context.AddInstigator(SourceActor, SourceActor);
	UObject* SourceObject = GetCurrentSourceObject();
	if (!IsValid(SourceObject) || SourceObject->IsA<UGameplayAbility>())
	{
		SourceObject = SourceActor;
	}
	Context.AddSourceObject(SourceObject);
	Context.AddHitResult(Hit, true);
	Context.AddOrigin(FVector(Hit.ImpactPoint));

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		DamageEffectClass,
		static_cast<float>(GetAbilityLevel()),
		Context);
	if (!IsWeaponActivationCurrent(Epoch) || !CanContinueWeaponPayload()) { return false; }
	FGameplayEffectSpec* DamageSpec = SpecHandle.Data.Get();
	if (!DamageSpec)
	{
		return false;
	}

	DamageSpec->AddDynamicAssetTag(AbilityIdentityTag);
	for (const FGameplayTag& Tag : DamageChannels)
	{
		DamageSpec->AddDynamicAssetTag(Tag);
	}
	for (const FGameplayTag& Tag : AttackClassifications)
	{
		DamageSpec->AddDynamicAssetTag(Tag);
	}
	DamageSpec->SetSetByCallerMagnitude(
		FNarrativeGameplayTags::Get().SetByCaller_Damage,
		FMath::Max(Damage, 0.0f));
	if (PoiseDamage > KINDA_SMALL_NUMBER)
	{
		DamageSpec->SetSetByCallerMagnitude(
			FSovGameplayTags::Get().SetByCaller_Damage_PoiseDamage,
			PoiseDamage);
	}

	UNarrativeAbilitySystemComponent* NarrativeSource = Cast<UNarrativeAbilitySystemComponent>(SourceASC);
	TStrongObjectPtr<USovNativeDamageReceipt> Receipt(NewObject<USovNativeDamageReceipt>());
	Receipt->ExpectedTarget = TargetASC->GetAvatarActor(); Receipt->ExpectedContext = DamageSpec->GetContext().Get();
	if (NarrativeSource)
	{
		NarrativeSource->OnDamageResolvedAsSource.AddDynamic(Receipt.Get(), &USovNativeDamageReceipt::ReceiveResult);
		if (ProtectionReceipt && ProtectionReceipt->ArmForDamage(DamageSpec->GetContext(), SourceASC, TargetASC))
		{
			NarrativeSource->OnDamageResolvedAsSource.AddDynamic(ProtectionReceipt, &USovProtectionInterceptReceipt::ReceiveResult);
		}
	}
	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
	if (NarrativeSource)
	{
		NarrativeSource->OnDamageResolvedAsSource.RemoveDynamic(Receipt.Get(), &USovNativeDamageReceipt::ReceiveResult);
		if (ProtectionReceipt)
		{
			NarrativeSource->OnDamageResolvedAsSource.RemoveDynamic(ProtectionReceipt, &USovProtectionInterceptReceipt::ReceiveResult);
			ProtectionReceipt->Disarm();
		}
	}
	return Receipt->bAppliedDamage;
}

bool USovGameplayAbility_ReformationDroneWeaponBase::IsHostileTarget(
	const UAbilitySystemComponent* TargetAbilitySystem) const
{
	const UAbilitySystemComponent* SourceASC = CurrentActorInfo
		? CurrentActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	AActor* SourceActor = CurrentActorInfo
		? CurrentActorInfo->AvatarActor.Get()
		: nullptr;
	AActor* TargetActor = IsValid(TargetAbilitySystem)
		? TargetAbilitySystem->GetAvatarActor()
		: nullptr;
	const INarrativeTeamAgentInterface* SourceTeam =
		Cast<const INarrativeTeamAgentInterface>(SourceActor);
	return IsValid(SourceASC)
		&& IsValid(SourceActor)
		&& IsValid(TargetAbilitySystem)
		&& IsValid(TargetActor)
		&& TargetAbilitySystem != SourceASC
		&& TargetAbilitySystem->GetSet<UNarrativeAttributeSetBase>()
		&& SourceTeam
		&& SourceTeam->GetTeamAttitudeTowards(*TargetActor) == ETeamAttitude::Hostile;
}

bool USovGameplayAbility_ReformationDroneWeaponBase::IsTargetAlive(
	const UAbilitySystemComponent* TargetAbilitySystem) const
{
	if (!IsValid(TargetAbilitySystem))
	{
		return false;
	}
	if (const auto* Attributes = TargetAbilitySystem->GetSet<UNarrativeAttributeSetBase>())
	{
		// Attribute publication can precede Narrative's death-state publication.
		if (!FMath::IsFinite(Attributes->GetHealth()) || Attributes->GetHealth() <= 0.f) { return false; }
	}
	if (const UNarrativeAbilitySystemComponent* NarrativeASC =
		Cast<UNarrativeAbilitySystemComponent>(TargetAbilitySystem))
	{
		return !NarrativeASC->IsDead();
	}
	return !TargetAbilitySystem->HasMatchingGameplayTag(
			FNarrativeGameplayTags::Get().State_IsDead)
		&& !TargetAbilitySystem->HasMatchingGameplayTag(
			FSovGameplayTags::Get().State_Fatal);
}

void USovGameplayAbility_ReformationDroneWeaponBase::
	HandleAutomaticPayloadRelease(uint64 ExpectedEpoch)
{
	if (!ValidateWeaponContinuation(ExpectedEpoch)) { return; }
	if (!TryBeginWeaponPayloadRelease())
	{
		if (IsWeaponActivationCurrent(ExpectedEpoch) && !bPayloadFinished) { CancelDroneWeaponAbility(); }
		return;
	}
	if (!ValidateWeaponContinuation(ExpectedEpoch) || bPayloadFinished)
	{
		return;
	}
	ExecuteAutomaticPayload();
}

void USovGameplayAbility_ReformationDroneWeaponBase::HandleRecoveryFinished(uint64 ExpectedEpoch)
{
	if (ValidateWeaponContinuation(ExpectedEpoch))
	{
		EndAbility(
			CurrentSpecHandle,
			CurrentActorInfo,
			CurrentActivationInfo,
			true,
			false);
	}
}

void USovGameplayAbility_ReformationDroneWeaponBase::
	HandleMaximumDurationExpired(uint64 ExpectedEpoch)
{
	if (!ValidateWeaponContinuation(ExpectedEpoch)) { return; }
	UE_LOG(
		LogSovReformationDroneAbility,
		Warning,
		TEXT("%s cancelled %s after its %.2f second lifecycle watchdog expired."),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(this),
		MaximumActiveDuration);
	CancelDroneWeaponAbility();
}

void USovGameplayAbility_ReformationDroneWeaponBase::HandleMontageCompleted()
{
	MontageTask = nullptr;
}

void USovGameplayAbility_ReformationDroneWeaponBase::HandleMontageInterrupted()
{
	MontageTask = nullptr;
	// Animation is presentation, not gameplay authority. An incompatible or
	// absent server AnimInstance must not suppress the native payload timer.
}

void USovGameplayAbility_ReformationDroneWeaponBase::StartAttackMontage()
{
	if (!IsValid(AttackMontage.Get()))
	{
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		TEXT("DroneWeaponMontage"),
		AttackMontage.Get(),
		FMath::Max(MontagePlayRate, 0.01f),
		MontageStartSection,
		true);
	if (!IsValid(MontageTask.Get()))
	{
		UE_LOG(
			LogSovReformationDroneAbility,
			Warning,
			TEXT("%s could not create its authored drone weapon montage task."),
			*GetNameSafe(this));
		return;
	}

	MontageTask->OnCompleted.AddDynamic(
		this,
		&ThisClass::HandleMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(
		this,
		&ThisClass::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(
		this,
		&ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(
		this,
		&ThisClass::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();
}

USovGameplayAbility_ReformationDroneGunfire::
	USovGameplayAbility_ReformationDroneGunfire()
{
	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	InputTag = NarrativeTags.Narrative_Input_Attack;
	AbilityIdentityTag = SovTags.Ability_NPC_ReformationDrone_Gunfire;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(AbilityIdentityTag);
	AssetTags.AddTag(NarrativeTags.Ability_WeaponFire);
	AssetTags.AddTag(NarrativeTags.Ability_DamageType_Ranged);
	SetAssetTags(AssetTags);
	DamageChannels.AddTag(SovTags.Damage_Channel_Kinetic);
	AttackClassifications.AddTag(SovTags.Damage_GuardClass_Standard);
	DamageEffectClass = USovGameplayEffect_ReformationDroneDamage::StaticClass();
	GunshotPresentationClass = ASovReformationDroneGunshotPresentation::StaticClass();
	MuzzleSocketNames.Add(TEXT("Muzzle_Gun"));
	FallbackMuzzleOffset = FVector(105.0f, 0.0f, 35.0f);
	PayloadReleaseDelay = 0.08f;
	PostFireRecovery = 0.2f;
	CooldownDuration = 0.75f;
	MaximumActiveDuration = 3.0f;
	DefaultBotAttackFrequency = 0.9f;
	DefaultBotAttackRange = 4500.0f;
}

void USovGameplayAbility_ReformationDroneGunfire::FireGunBurstFromAim()
{
	const uint64 Epoch = GetWeaponActivationEpoch();
	if (!IsActive()
		|| !CurrentActorInfo
		|| !CurrentActorInfo->IsNetAuthority()
		|| bBurstStarted)
	{
		return;
	}
	if (!HasRequiredPayloadConfiguration()
		|| !TryBeginWeaponPayloadRelease())
	{
		if (IsWeaponActivationCurrent(Epoch) && !HasWeaponPayloadFinished()) { CancelDroneWeaponAbility(); }
		return;
	}
	// The release event may itself call this function. Respect that inner call
	// instead of resetting and firing a duplicate burst on the outer stack.
	if (!ValidateWeaponContinuation(Epoch) || HasWeaponPayloadFinished() || bBurstStarted)
	{
		return;
	}

	bBurstStarted = true;
	++BurstEpoch;
	ShotsFired = 0;
	FireNextBurstShot(Epoch);
}

void USovGameplayAbility_ReformationDroneGunfire::CleanupWeaponPayload()
{
	++BurstEpoch;
	if (UWorld* World = GetWeaponActionWorld())
	{
		World->GetTimerManager().ClearTimer(BurstTimerHandle);
	}
	ShotsFired = 0;
	bBurstStarted = false;
	Super::CleanupWeaponPayload();
}

bool USovGameplayAbility_ReformationDroneGunfire::
	HasRequiredPayloadConfiguration() const
{
	return Super::HasRequiredPayloadConfiguration()
		&& FMath::IsFinite(DamagePerShot)
		&& DamagePerShot > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(PoiseDamagePerShot)
		&& PoiseDamagePerShot >= 0.0f
		&& BurstShotCount >= 1
		&& BurstShotCount <= 30
		&& FMath::IsFinite(TimeBetweenShots)
		&& TimeBetweenShots > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(MaximumRange)
		&& MaximumRange > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(TraceRadius)
		&& TraceRadius >= 0.0f
		&& FMath::IsFinite(SpreadDegrees)
		&& SpreadDegrees >= 0.0f
		&& SpreadDegrees <= 45.0f
		&& MaximumActiveDuration + KINDA_SMALL_NUMBER >=
			((bAutoReleasePayload ? PayloadReleaseDelay : 0.0f)
				+ (FMath::Max(BurstShotCount - 1, 0) * TimeBetweenShots)
				+ PostFireRecovery);
}

void USovGameplayAbility_ReformationDroneGunfire::ExecuteAutomaticPayload()
{
	FireGunBurstFromAim();
}

float USovGameplayAbility_ReformationDroneGunfire::
	GetAttackDamage_Implementation() const
{
	return FMath::Max(DamagePerShot, 0.0f);
}

void USovGameplayAbility_ReformationDroneGunfire::FireNextBurstShot(uint64 ExpectedEpoch)
{
	if (!ValidateWeaponContinuation(ExpectedEpoch)) { return; }
	if (!CanContinueWeaponPayload()
		|| !SovThreatTargeting::CanUseActorFocus(GetAvatarActorFromActorInfo())
		|| !bBurstStarted
		|| ShotsFired >= BurstShotCount)
	{
		if (ShotsFired >= BurstShotCount)
		{
			NotifyPayloadFinished();
		}
		else
		{
			CancelDroneWeaponAbility();
		}
		return;
	}

	const uint32 ExpectedBurstEpoch = BurstEpoch;
	const int32 ShotIndex = ShotsFired;
	ANarrativeCharacter* SourceCharacter = Cast<ANarrativeCharacter>(GetAvatarActorFromActorInfo());
	AAIController* SourceController = SourceCharacter ? Cast<AAIController>(SourceCharacter->GetController()) : nullptr;
	AActor* IntendedFocus = SourceController ? SourceController->GetFocusActor() : nullptr;
	const FTransform MuzzleTransform = ResolveMuzzleTransform(ShotIndex);
	const FVector TraceStart = MuzzleTransform.GetLocation();
	const FVector AimPoint = ResolveAuthorityAimPoint(MaximumRange);
	FVector ShotDirection = (AimPoint - TraceStart).GetSafeNormal();
	if (ShotDirection.IsNearlyZero())
	{
		ShotDirection = MuzzleTransform.GetRotation().GetForwardVector();
	}
	if (SpreadDegrees > KINDA_SMALL_NUMBER)
	{
		ShotDirection = FMath::VRandCone(
			ShotDirection,
			FMath::DegreesToRadians(SpreadDegrees * 0.5f));
	}

	const FVector UnblockedEnd = TraceStart + (ShotDirection * MaximumRange);
	const TArray<FHitResult> Hits = PerformTraceMulti(
		TraceStart,
		UnblockedEnd,
		FMath::Max(TraceRadius, 0.0f));
	const FHitResult* BlockingHit = Hits.FindByPredicate(
		[](const FHitResult& Hit)
		{
			return Hit.bBlockingHit;
		});
	const FVector TraceEnd = BlockingHit
		? FVector(BlockingHit->ImpactPoint)
		: UnblockedEnd;
	TStrongObjectPtr<USovProtectionInterceptReceipt> ProtectionReceipt(BlockingHit
		? USovProtectionInterceptReceipt::TryCreateForDroneShot(SourceCharacter, IntendedFocus, *BlockingHit,
			TraceStart, UnblockedEnd, FMath::Max(TraceRadius, 0.f)) : nullptr);
	if (!ValidateWeaponContinuation(ExpectedEpoch) || BurstEpoch != ExpectedBurstEpoch) { return; }
	const bool bDamagedTarget = BlockingHit
		&& ApplyPointDamage(*BlockingHit, DamagePerShot, PoiseDamagePerShot, ProtectionReceipt.Get());
	// Typed damage/reward callbacks may end this ability, start a new activation,
	// or switch the avatar. The old shot must not schedule that activation's burst.
	if (BurstEpoch != ExpectedBurstEpoch || !ValidateWeaponContinuation(ExpectedEpoch) || !CanContinueWeaponPayload()
		|| GetAvatarActorFromActorInfo() != SourceCharacter) { return; }
	SpawnGunshotPresentation(
		TraceStart,
		TraceEnd,
		BlockingHit,
		bDamagedTarget,
		ShotIndex);
	if (!ValidateWeaponContinuation(ExpectedEpoch) || BurstEpoch != ExpectedBurstEpoch || !CanContinueWeaponPayload()) { return; }

	++ShotsFired;
	if (ShotsFired >= BurstShotCount)
	{
		NotifyPayloadFinished();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BurstTimerHandle,
			FTimerDelegate::CreateUObject(this, &ThisClass::FireNextBurstShot, ExpectedEpoch),
			FMath::Max(TimeBetweenShots, 0.01f),
			false);
		return;
	}

	CancelDroneWeaponAbility();
}

void USovGameplayAbility_ReformationDroneGunfire::SpawnGunshotPresentation(
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const FHitResult* Hit,
	const bool bDamagedTarget,
	const int32 ShotIndex) const
{
	UWorld* World = GetWorld();
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(World) || !IsValid(Avatar) || !GunshotPresentationClass.Get())
	{
		return;
	}

	FVector ImpactNormal = -((TraceEnd - TraceStart).GetSafeNormal());
	AActor* HitActor = nullptr;
	FName HitBone = NAME_None;
	EPhysicalSurface ImpactSurfaceType = SurfaceType_Default;
	const bool bBlockingHit = Hit && Hit->bBlockingHit;
	if (bBlockingHit)
	{
		ImpactNormal = FVector(Hit->ImpactNormal).GetSafeNormal();
		HitActor = Hit->GetActor();
		HitBone = Hit->BoneName;
		ImpactSurfaceType = UGameplayStatics::GetSurfaceType(*Hit);
	}
	if (ImpactNormal.IsNearlyZero())
	{
		ImpactNormal = FVector::UpVector;
	}

	const FTransform SpawnTransform(
		(TraceEnd - TraceStart).GetSafeNormal().ToOrientationQuat(),
		TraceStart,
		FVector::OneVector);
	ASovReformationDroneGunshotPresentation* Presentation =
		World->SpawnActorDeferred<ASovReformationDroneGunshotPresentation>(
			GunshotPresentationClass,
			SpawnTransform,
			Avatar,
			Cast<APawn>(Avatar),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Presentation))
	{
		return;
	}

	Presentation->InitializeGunshotPresentation(
		TraceStart,
		TraceEnd,
		ImpactNormal,
		HitActor,
		HitBone,
		ImpactSurfaceType,
		bBlockingHit,
		bDamagedTarget,
		ShotIndex);
	UGameplayStatics::FinishSpawningActor(Presentation, SpawnTransform);
}

USovGameplayAbility_ReformationDroneRocketLauncher::
	USovGameplayAbility_ReformationDroneRocketLauncher()
{
	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	InputTag = NarrativeTags.Narrative_Input_AltAttack;
	AbilityIdentityTag = SovTags.Ability_NPC_ReformationDrone_RocketLauncher;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(AbilityIdentityTag);
	AssetTags.AddTag(NarrativeTags.Ability_WeaponFire);
	AssetTags.AddTag(NarrativeTags.Ability_DamageType_Ranged);
	SetAssetTags(AssetTags);
	DamageChannels.AddTag(SovTags.Damage_Channel_Kinetic);
	DamageChannels.AddTag(SovTags.Damage_Channel_Thermal);
	AttackClassifications.AddTag(SovTags.Damage_GuardClass_Heavy);
	DamageEffectClass = USovGameplayEffect_ReformationDroneDamage::StaticClass();
	RocketClass = ASovReformationDroneRocketProjectile::StaticClass();
	MuzzleSocketNames.Add(TEXT("Muzzle_Rocket_L"));
	MuzzleSocketNames.Add(TEXT("Muzzle_Rocket_R"));
	FallbackMuzzleOffset = FVector(90.0f, 0.0f, 20.0f);
	PayloadReleaseDelay = 0.28f;
	PostFireRecovery = 0.45f;
	CooldownDuration = 4.0f;
	MaximumActiveDuration = 3.0f;
	DefaultBotAttackFrequency = 5.0f;
	DefaultBotAttackRange = 6000.0f;
}

void USovGameplayAbility_ReformationDroneRocketLauncher::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bRocketReleaseAttempted = false;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

bool USovGameplayAbility_ReformationDroneRocketLauncher::
	HasRequiredPayloadConfiguration() const
{
	return Super::HasRequiredPayloadConfiguration()
		&& ResolveRocketClass().Get()
		&& FMath::IsFinite(ExplosionRadius)
		&& ExplosionRadius > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(ExplosionDamage)
		&& ExplosionDamage > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(ExplosionPoiseDamage)
		&& ExplosionPoiseDamage >= 0.0f
		&& FMath::IsFinite(MinimumExplosionDamageFraction)
		&& MinimumExplosionDamageFraction >= 0.0f
		&& MinimumExplosionDamageFraction <= 1.0f
		&& FMath::IsFinite(RocketSpeed)
		&& RocketSpeed > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(RocketAimTraceDistance)
		&& RocketAimTraceDistance > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(RocketGravityScale)
		&& RocketGravityScale >= 0.0f
		&& FMath::IsFinite(RocketCollisionRadius)
		&& RocketCollisionRadius >= 1.0f
		&& FMath::IsFinite(RocketFlightDuration)
		&& RocketFlightDuration >= 0.1f
		&& FMath::IsFinite(MaximumRocketSpeed)
		&& MaximumRocketSpeed + KINDA_SMALL_NUMBER >= RocketSpeed
		&& FMath::IsFinite(MaximumRocketSpawnDistance)
		&& MaximumRocketSpawnDistance > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(HomingAccelerationMagnitude)
		&& (!bEnableHoming || HomingAccelerationMagnitude > KINDA_SMALL_NUMBER)
		&& MaximumActiveDuration + KINDA_SMALL_NUMBER >=
			((bAutoReleasePayload ? PayloadReleaseDelay : 0.0f)
				+ PostFireRecovery);
}

void USovGameplayAbility_ReformationDroneRocketLauncher::ExecuteAutomaticPayload()
{
	LaunchRocketFromAim();
}

float USovGameplayAbility_ReformationDroneRocketLauncher::
	GetAttackDamage_Implementation() const
{
	return FMath::Max(ExplosionDamage, 0.0f);
}

ASovReformationDroneRocketProjectile*
USovGameplayAbility_ReformationDroneRocketLauncher::LaunchRocketFromAim()
{
	const uint64 Epoch = GetWeaponActivationEpoch();
	if (!IsActive()
		|| !CurrentActorInfo
		|| !CurrentActorInfo->IsNetAuthority()
		|| bRocketReleaseAttempted)
	{
		return nullptr;
	}

	if (!HasRequiredPayloadConfiguration()
		|| !TryBeginWeaponPayloadRelease())
	{
		if (IsWeaponActivationCurrent(Epoch) && !HasWeaponPayloadFinished()) { CancelDroneWeaponAbility(); }
		return nullptr;
	}
	if (!ValidateWeaponContinuation(Epoch) || HasWeaponPayloadFinished() || bRocketReleaseAttempted)
	{
		return nullptr;
	}

	const int32 MuzzleIndex = NextMuzzleIndex;
	if (!SovThreatTargeting::CanUseActorFocus(GetAvatarActorFromActorInfo()))
	{
		CancelDroneWeaponAbility(); return nullptr;
	}
	const FTransform MuzzleTransform = ResolveMuzzleTransform(MuzzleIndex);
	const FVector AimPoint = ResolveAuthorityAimPoint(RocketAimTraceDistance);
	FVector LaunchDirection = (AimPoint - MuzzleTransform.GetLocation()).GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		LaunchDirection = MuzzleTransform.GetRotation().GetForwardVector();
	}
	const FTransform SpawnTransform(
		LaunchDirection.ToOrientationQuat(),
		MuzzleTransform.GetLocation(),
		FVector::OneVector);
	AActor* HomingTarget = ResolveHomingTarget();
	if (!ValidateWeaponContinuation(Epoch)) { return nullptr; }
	return LaunchRocket(
		SpawnTransform,
		LaunchDirection * RocketSpeed,
		HomingTarget);
}

ASovReformationDroneRocketProjectile*
USovGameplayAbility_ReformationDroneRocketLauncher::LaunchRocket(
	const FTransform& SpawnTransform,
	FVector InitialVelocity,
	AActor* HomingTarget)
{
	TStrongObjectPtr<USovGameplayAbility_ReformationDroneRocketLauncher> ActionLifetime(this);
	const uint64 Epoch = GetWeaponActivationEpoch();
	if (!IsActive()
		|| !CurrentActorInfo
		|| !CurrentActorInfo->IsNetAuthority()
		|| bRocketReleaseAttempted)
	{
		return nullptr;
	}
	if (!HasRequiredPayloadConfiguration()
		|| !TryBeginWeaponPayloadRelease())
	{
		if (IsWeaponActivationCurrent(Epoch) && !HasWeaponPayloadFinished()) { CancelDroneWeaponAbility(); }
		return nullptr;
	}
	// A Blueprint release event is allowed to launch the rocket. If it did,
	// this outer/manual call must not launch a second projectile.
	if (!ValidateWeaponContinuation(Epoch) || HasWeaponPayloadFinished() || bRocketReleaseAttempted)
	{
		return nullptr;
	}
	bRocketReleaseAttempted = true;

	AActor* Avatar = CurrentActorInfo->AvatarActor.Get();
	UWorld* World = GetWorld();
	if (!IsValid(Avatar)
		|| !IsValid(World)
		|| SpawnTransform.ContainsNaN()
		|| InitialVelocity.ContainsNaN()
		|| FVector::DistSquared(Avatar->GetActorLocation(), SpawnTransform.GetLocation())
			> FMath::Square(MaximumRocketSpawnDistance))
	{
		UE_LOG(
			LogSovReformationDroneAbility,
			Error,
			TEXT("%s rejected an invalid Reformation drone rocket release."),
			*GetNameSafe(Avatar));
		CancelDroneWeaponAbility();
		return nullptr;
	}

	FTransform ServerSpawnTransform = SpawnTransform;
	ServerSpawnTransform.NormalizeRotation();
	ServerSpawnTransform.SetScale3D(FVector::OneVector);
	if (InitialVelocity.IsNearlyZero())
	{
		InitialVelocity = ServerSpawnTransform.GetRotation().GetForwardVector()
			* RocketSpeed;
	}
	InitialVelocity = InitialVelocity.GetClampedToMaxSize(MaximumRocketSpeed);
	if (InitialVelocity.IsNearlyZero())
	{
		CancelDroneWeaponAbility();
		return nullptr;
	}

	ASovReformationDroneRocketProjectile* Rocket =
		World->SpawnActorDeferred<ASovReformationDroneRocketProjectile>(
			ResolveRocketClass(),
			ServerSpawnTransform,
			Avatar,
			Cast<APawn>(Avatar),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!ValidateWeaponContinuation(Epoch))
	{
		if (IsValid(Rocket)) { Rocket->Destroy(); }
		return nullptr;
	}
	if (!IsValid(Rocket))
	{
		CancelDroneWeaponAbility();
		return nullptr;
	}

	UObject* SourceObject = GetCurrentSourceObject();
	if (!IsValid(SourceObject) || SourceObject->IsA<UGameplayAbility>())
	{
		SourceObject = Avatar;
	}
	AActor* ValidatedHomingTarget = nullptr;
	if (bEnableHoming && IsValid(HomingTarget))
	{
		UAbilitySystemComponent* HomingTargetASC =
			ResolveAbilitySystemFromActor(HomingTarget);
		if (IsHostileTarget(HomingTargetASC) && IsTargetAlive(HomingTargetASC)
			&& SovThreatTargeting::CanTrack(Avatar, HomingTargetASC->GetAvatarActor()))
		{
			ValidatedHomingTarget = HomingTargetASC->GetAvatarActor();
		}
	}
	if (!ValidateWeaponContinuation(Epoch))
	{
		if (IsValid(Rocket)) { Rocket->Destroy(); }
		return nullptr;
	}

	Rocket->InitializeRocket(
		CurrentActorInfo->AbilitySystemComponent.Get(),
		Avatar,
		SourceObject,
		DamageEffectClass,
		AbilityIdentityTag,
		DamageChannels,
		AttackClassifications,
		static_cast<float>(GetAbilityLevel()),
		InitialVelocity,
		RocketGravityScale,
		RocketCollisionRadius,
		RocketFlightDuration,
		ExplosionRadius,
		ExplosionDamage,
		ExplosionPoiseDamage,
		MinimumExplosionDamageFraction,
		bExplosionRequiresLineOfSight,
		ValidatedHomingTarget,
		bEnableHoming ? HomingAccelerationMagnitude : 0.0f);
	if (!ValidateWeaponContinuation(Epoch))
	{
		if (IsValid(Rocket)) { Rocket->Destroy(); }
		return nullptr;
	}
	// Finishing spawn commits the independently owned projectile. Later source
	// interruption does not recall it, but cannot finish/rearm a replacement action.
	++NextMuzzleIndex;
	UGameplayStatics::FinishSpawningActor(Rocket, ServerSpawnTransform);
	if (!ValidateWeaponContinuation(Epoch))
	{
		return IsValid(Rocket) && !Rocket->IsActorBeingDestroyed() ? Rocket : nullptr;
	}
	if (!IsValid(Rocket) || Rocket->IsActorBeingDestroyed())
	{
		CancelDroneWeaponAbility();
		return nullptr;
	}

	NotifyPayloadFinished();
	return Rocket;
}

TSubclassOf<ASovReformationDroneRocketProjectile>
USovGameplayAbility_ReformationDroneRocketLauncher::ResolveRocketClass() const
{
	if (RocketClass.Get())
	{
		return RocketClass;
	}

	return ASovReformationDroneRocketProjectile::StaticClass();
}

AActor* USovGameplayAbility_ReformationDroneRocketLauncher::
	ResolveHomingTarget() const
{
	const AAIController* AIController = Cast<AAIController>(GetOwningController());
	AActor* FocusActor = IsValid(AIController) ? AIController->GetFocusActor() : nullptr;
	if (!IsValid(FocusActor))
	{
		return nullptr;
	}

	UAbilitySystemComponent* TargetASC = ResolveAbilitySystemFromActor(FocusActor);
	return IsHostileTarget(TargetASC) && IsTargetAlive(TargetASC)
		&& SovThreatTargeting::CanTrack(GetAvatarActorFromActorInfo(), TargetASC->GetAvatarActor())
		? TargetASC->GetAvatarActor()
		: nullptr;
}

USovGameplayAbility_ReformationDroneSelfDestruct::
	USovGameplayAbility_ReformationDroneSelfDestruct()
{
	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	InputTag = NarrativeTags.Narrative_Input_Attack;
	AbilityIdentityTag = SovTags.Ability_NPC_ReformationDrone_SelfDestruct;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(AbilityIdentityTag);
	AssetTags.AddTag(NarrativeTags.Ability_DamageType_Melee);
	AssetTags.AddTag(NarrativeTags.Ability_DamageType_Heavy);
	SetAssetTags(AssetTags);

	DamageChannels.AddTag(SovTags.Damage_Channel_Kinetic);
	DamageChannels.AddTag(SovTags.Damage_Channel_Thermal);
	AttackClassifications.AddTag(SovTags.Damage_GuardClass_Heavy);
	DamageEffectClass = USovGameplayEffect_ReformationDroneDamage::StaticClass();
	PresentationClass =
		ASovReformationDroneSelfDestructPresentation::StaticClass();

	PayloadReleaseDelay = 0.05f;
	PostFireRecovery = 0.0f;
	CooldownDuration = 0.5f;
	MaximumActiveDuration = 11.0f;
	DefaultBotAttackFrequency = 10.0f;
	DefaultBotAttackRange = 3500.0f;
}

void USovGameplayAbility_ReformationDroneSelfDestruct::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	PursuitTarget.Reset();
	ActivePresentation = nullptr;
	OwnedPursuitMoveRequestId = FAIRequestID::InvalidRequest;
	OwnedPathFollowing.Reset();
	PursuitStartTime = 0.0;
	LastMoveRequestTime = 0.0;
	bPursuitStarted = false;
	bWarningStarted = false;
	bDetonationCommitted = false;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void USovGameplayAbility_ReformationDroneSelfDestruct::CleanupWeaponPayload()
{
	const bool bOwnedMovement = bPursuitStarted || bWarningStarted;
	if (UWorld* World = GetWeaponActionWorld())
	{
		World->GetTimerManager().ClearTimer(PursuitUpdateTimerHandle);
		World->GetTimerManager().ClearTimer(DetonationWarningTimerHandle);
	}
	const bool bCommittedExplosion = bDetonationCommitted;
	ASovReformationDroneSelfDestructPresentation* RetiredPresentation = ActivePresentation.Get();
	ActivePresentation = nullptr;
	PursuitTarget.Reset();
	PursuitStartTime = 0.0;
	LastMoveRequestTime = 0.0;
	bPursuitStarted = false;
	bWarningStarted = false;
	bDetonationCommitted = false;
	if (bOwnedMovement) { AbortOwnedPursuitMove(); }
	if (!bCommittedExplosion && IsValid(RetiredPresentation)) { RetiredPresentation->CancelPresentation(); }
	Super::CleanupWeaponPayload();
}

bool USovGameplayAbility_ReformationDroneSelfDestruct::
	HasRequiredPayloadConfiguration() const
{
	const float ExpectedLifecycle =
		(bAutoReleasePayload ? PayloadReleaseDelay : 0.0f)
		+ MaximumPursuitDuration
		+ PursuitUpdateInterval
		+ DetonationWarningDuration
		+ PostFireRecovery
		+ 0.05f;
	return Super::HasRequiredPayloadConfiguration()
		&& ResolvePresentationClass().Get()
		&& FMath::IsFinite(TargetAcquisitionRange)
		&& TargetAcquisitionRange > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(DetonationTriggerRadius)
		&& DetonationTriggerRadius >= 1.0f
		&& FMath::IsFinite(MoveAcceptanceRadius)
		&& MoveAcceptanceRadius >= 0.0f
		&& FMath::IsFinite(PursuitUpdateInterval)
		&& PursuitUpdateInterval >= 0.02f
		&& FMath::IsFinite(MoveRetryInterval)
		&& MoveRetryInterval >= 0.1f
		&& FMath::IsFinite(MaximumPursuitDuration)
		&& MaximumPursuitDuration >= 0.1f
		&& FMath::IsFinite(DetonationWarningDuration)
		&& DetonationWarningDuration >= 0.05f
		&& FMath::IsFinite(ExplosionRadius)
		&& ExplosionRadius >= 1.0f
		&& FMath::IsFinite(ExplosionDamage)
		&& ExplosionDamage > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(ExplosionPoiseDamage)
		&& ExplosionPoiseDamage >= 0.0f
		&& FMath::IsFinite(MinimumExplosionDamageFraction)
		&& MinimumExplosionDamageFraction >= 0.0f
		&& MinimumExplosionDamageFraction <= 1.0f
		&& MaximumActiveDuration + KINDA_SMALL_NUMBER >= ExpectedLifecycle;
}

void USovGameplayAbility_ReformationDroneSelfDestruct::
	ExecuteAutomaticPayload()
{
	StartSelfDestructRun();
}

float USovGameplayAbility_ReformationDroneSelfDestruct::
	GetAttackDamage_Implementation() const
{
	return FMath::Max(ExplosionDamage, 0.0f);
}

void USovGameplayAbility_ReformationDroneSelfDestruct::
	StartSelfDestructRun()
{
	const uint64 Epoch = GetWeaponActivationEpoch();
	if (!IsActive()
		|| !CurrentActorInfo
		|| !CurrentActorInfo->IsNetAuthority()
		|| bPursuitStarted)
	{
		return;
	}
	if (!HasRequiredPayloadConfiguration()
		|| !TryBeginWeaponPayloadRelease())
	{
		if (IsWeaponActivationCurrent(Epoch) && !HasWeaponPayloadFinished()) { CancelDroneWeaponAbility(); }
		return;
	}
	// The payload-released Blueprint event may have called this function
	// recursively. Respect that inner run instead of starting a second one.
	if (!ValidateWeaponContinuation(Epoch)
		|| HasWeaponPayloadFinished()
		|| bPursuitStarted)
	{
		return;
	}

	AActor* SourceDrone = CurrentActorInfo->AvatarActor.Get();
	AActor* TargetActor = ResolvePursuitTarget();
	if (!ValidateWeaponContinuation(Epoch)) { return; }
	UWorld* World = GetWorld();
	if (!IsValid(SourceDrone)
		|| !IsValid(TargetActor)
		|| !IsValid(World))
	{
		UE_LOG(
			LogSovReformationDroneAbility,
			Warning,
			TEXT("%s could not begin self destruct: no living hostile target was available."),
			*GetNameSafe(SourceDrone));
		CancelDroneWeaponAbility();
		return;
	}

	bPursuitStarted = true;
	PursuitTarget = TargetActor;
	PursuitStartTime = World->GetTimeSeconds();
	ASovReformationDroneSelfDestructPresentation* NewPresentation = SpawnPresentation(SourceDrone);
	if (!ValidateWeaponContinuation(Epoch))
	{
		if (IsValid(NewPresentation)) { NewPresentation->CancelPresentation(); }
		return;
	}
	ActivePresentation = NewPresentation;
	ReceivePursuitAcquired(TargetActor);
	if (!ValidateWeaponContinuation(Epoch) || bWarningStarted || bDetonationCommitted)
	{
		return;
	}
	const bool bCanTrackTarget = SovThreatTargeting::CanTrack(SourceDrone, TargetActor);
	if (!ValidateWeaponContinuation(Epoch)) { return; }
	if (!CanContinueWeaponPayload() || !bCanTrackTarget)
	{
		CancelDroneWeaponAbility();
		return;
	}
	// Arm validation before MoveTo: an AlreadyAtGoal result enters the
	// warning synchronously and still needs prompt fuse interruption checks.
	World->GetTimerManager().SetTimer(
		PursuitUpdateTimerHandle,
		FTimerDelegate::CreateUObject(this, &ThisClass::UpdatePursuit, Epoch),
		FMath::Max(PursuitUpdateInterval, 0.02f),
		true);
	if (FVector::DistSquared(
		SourceDrone->GetActorLocation(),
		TargetActor->GetActorLocation())
		<= FMath::Square(FMath::Max(DetonationTriggerRadius, 1.0f)))
	{
		// Best-effort claim of the AI movement lane stops an existing StateTree
		// path, but an in-range drone never depends on navigation succeeding.
		RequestPursuitMove(TargetActor);
		if (!ValidateWeaponContinuation(Epoch))
		{
			return;
		}
		EnterDetonationWarning();
		return;
	}

	const bool bMoveRequested = RequestPursuitMove(TargetActor);
	if (!ValidateWeaponContinuation(Epoch))
	{
		return;
	}
	if (!bMoveRequested)
	{
		UE_LOG(
			LogSovReformationDroneAbility,
			Warning,
			TEXT("%s could not request self-destruct pursuit movement. Check its AIController, NavMovement, and path configuration."),
			*GetNameSafe(SourceDrone));
		CancelDroneWeaponAbility();
		return;
	}
	if (bWarningStarted)
	{
		return;
	}
}

void USovGameplayAbility_ReformationDroneSelfDestruct::UpdatePursuit(uint64 ExpectedEpoch)
{
	if (!ValidateWeaponContinuation(ExpectedEpoch)
		|| !bPursuitStarted
		|| bDetonationCommitted)
	{
		return;
	}
	if (!CanContinueWeaponPayload())
	{
		CancelDroneWeaponAbility();
		return;
	}
	// Continue validating interrupt states while the fuse is active, but do not
	// keep requiring the original target after the drone has armed.
	if (bWarningStarted)
	{
		// An external StateTree/BT move has taken the movement lane back. Treat
		// that as an interruption without aborting the external request.
		if (const AAIController* AIController =
			Cast<AAIController>(GetOwningController());
			IsValid(AIController)
			&& AIController->GetMoveStatus() != EPathFollowingStatus::Idle)
		{
			CancelDroneWeaponAbility();
		}
		return;
	}
	AActor* SourceDrone = GetAvatarActorFromActorInfo();
	AActor* TargetActor = PursuitTarget.Get();
	UAbilitySystemComponent* TargetASC =
		ResolveAbilitySystemFromActor(TargetActor);
	UWorld* World = GetWorld();
	if (!IsValid(SourceDrone)
		|| !IsValid(TargetActor)
		|| !IsValid(World)
		|| !IsHostileTarget(TargetASC)
		|| !IsTargetAlive(TargetASC)
		|| !SovThreatTargeting::CanTrack(SourceDrone, TargetActor))
	{
		if (IsWeaponActivationCurrent(ExpectedEpoch)) { CancelDroneWeaponAbility(); }
		return;
	}
	if (!ValidateWeaponContinuation(ExpectedEpoch)) { return; }

	const double Now = World->GetTimeSeconds();
	if (Now - PursuitStartTime >= MaximumPursuitDuration)
	{
		if (bDetonateWhenPursuitTimesOut)
		{
			EnterDetonationWarning();
		}
		else
		{
			CancelDroneWeaponAbility();
		}
		return;
	}

	if (FVector::DistSquared(
		SourceDrone->GetActorLocation(),
		TargetActor->GetActorLocation())
		<= FMath::Square(FMath::Max(DetonationTriggerRadius, 1.0f)))
	{
		EnterDetonationWarning();
		return;
	}

	AAIController* AIController = Cast<AAIController>(GetOwningController());
	UPathFollowingComponent* PathFollowing = IsValid(AIController)
		? AIController->GetPathFollowingComponent()
		: nullptr;
	if (!IsValid(AIController) || !IsValid(PathFollowing))
	{
		CancelDroneWeaponAbility();
		return;
	}
	const bool bMoveIsActive =
		AIController->GetMoveStatus() != EPathFollowingStatus::Idle;
	const bool bOwnedMoveIsCurrent = OwnedPathFollowing.Get() == PathFollowing && OwnedPursuitMoveRequestId.IsValid()
		&& PathFollowing->GetCurrentRequestId().IsEquivalent(
			OwnedPursuitMoveRequestId);
	if (bMoveIsActive && !bOwnedMoveIsCurrent)
	{
		// Another AI task replaced the pursuit. Cancel this ability without
		// disturbing the new path-following owner.
		CancelDroneWeaponAbility();
		return;
	}
	if (!bMoveIsActive && Now - LastMoveRequestTime >= MoveRetryInterval)
	{
		const bool bRequested = RequestPursuitMove(TargetActor);
		if (ValidateWeaponContinuation(ExpectedEpoch) && !bRequested) { CancelDroneWeaponAbility(); }
	}
}

void USovGameplayAbility_ReformationDroneSelfDestruct::
	HandleDetonationWarningExpired(uint64 ExpectedEpoch)
{
	if (ValidateWeaponContinuation(ExpectedEpoch)) { CommitDetonation(); }
}

AActor* USovGameplayAbility_ReformationDroneSelfDestruct::
	ResolvePursuitTarget() const
{
	UWorld* World = GetWorld();
	AActor* SourceDrone = GetAvatarActorFromActorInfo();
	if (!IsValid(World) || !IsValid(SourceDrone))
	{
		return nullptr;
	}

	auto ResolveValidTarget = [this, SourceDrone](AActor* Candidate) -> AActor*
	{
		UAbilitySystemComponent* CandidateASC =
			ResolveAbilitySystemFromActor(Candidate);
		AActor* TargetAvatar = IsValid(CandidateASC)
			? CandidateASC->GetAvatarActor()
			: nullptr;
		const APawn* TargetPawn = Cast<APawn>(TargetAvatar);
		return IsValid(TargetAvatar)
			&& (!bOnlyAcquirePlayerControlledTargets
				|| (IsValid(TargetPawn) && TargetPawn->IsPlayerControlled()))
			&& FVector::DistSquared(
				SourceDrone->GetActorLocation(),
				TargetAvatar->GetActorLocation())
				<= FMath::Square(FMath::Max(TargetAcquisitionRange, 0.0f))
			&& IsHostileTarget(CandidateASC)
			&& IsTargetAlive(CandidateASC)
			&& SovThreatTargeting::CanTrack(SourceDrone, TargetAvatar)
			? TargetAvatar
			: nullptr;
	};

	if (const AAIController* AIController =
		Cast<AAIController>(GetOwningController()))
	{
		if (AActor* FocusTarget =
			ResolveValidTarget(AIController->GetFocusActor()))
		{
			return FocusTarget;
		}
	}

	AActor* NearestTarget = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<ANarrativeCharacter> It(World); It; ++It)
	{
		AActor* Candidate = ResolveValidTarget(*It);
		if (!IsValid(Candidate))
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared(
			SourceDrone->GetActorLocation(),
			Candidate->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestTarget = Candidate;
		}
	}
	return NearestTarget;
}

bool USovGameplayAbility_ReformationDroneSelfDestruct::RequestPursuitMove(
	AActor* TargetActor)
{
	const uint64 Epoch = GetWeaponActivationEpoch();
	if (!ValidateWeaponContinuation(Epoch)) { return false; }
	AAIController* AIController = Cast<AAIController>(GetOwningController());
	if (!IsValid(AIController) || !IsValid(TargetActor)
		|| !SovThreatTargeting::CanTrack(GetAvatarActorFromActorInfo(), TargetActor))
	{
		return false;
	}
	if (!ValidateWeaponContinuation(Epoch)) { return false; }

	if (const UWorld* World = GetWorld())
	{
		LastMoveRequestTime = World->GetTimeSeconds();
	}
	FAIMoveRequest MoveRequest(TargetActor);
	MoveRequest.SetAcceptanceRadius(FMath::Max(MoveAcceptanceRadius, 0.0f));
	MoveRequest.SetUsePathfinding(bUsePathfinding);
	MoveRequest.SetAllowPartialPath(bAllowPartialPath);
	MoveRequest.SetCanStrafe(true);
	MoveRequest.SetReachTestIncludesAgentRadius(false);
	TWeakObjectPtr<UPathFollowingComponent> RequestedPath = AIController->GetPathFollowingComponent();
	const FPathFollowingRequestResult MoveResult =
		AIController->MoveTo(MoveRequest, nullptr);
	if (!ValidateWeaponContinuation(Epoch))
	{
		// Never store an old MoveTo result in a restarted action. Abort only the
		// exact path request returned to this stack, not the new action's move.
		if (MoveResult.MoveId.IsValid() && RequestedPath.IsValid()
			&& RequestedPath->GetCurrentRequestId().IsEquivalent(MoveResult.MoveId))
		{
			RequestedPath->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished,
				MoveResult.MoveId, EPathFollowingVelocityMode::Reset);
		}
		return false;
	}
	OwnedPursuitMoveRequestId = MoveResult.MoveId;
	OwnedPathFollowing = RequestedPath;
	if (MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		OwnedPursuitMoveRequestId = FAIRequestID::InvalidRequest;
		EnterDetonationWarning();
		return true;
	}
	if (MoveResult.Code != EPathFollowingRequestResult::RequestSuccessful
		|| !OwnedPursuitMoveRequestId.IsValid())
	{
		OwnedPursuitMoveRequestId = FAIRequestID::InvalidRequest;
		return false;
	}
	return true;
}

void USovGameplayAbility_ReformationDroneSelfDestruct::
	EnterDetonationWarning()
{
	const uint64 Epoch = GetWeaponActivationEpoch();
	if (!ValidateWeaponContinuation(Epoch)
		|| !bPursuitStarted
		|| bWarningStarted
		|| bDetonationCommitted)
	{
		return;
	}
	bWarningStarted = true;
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		CancelDroneWeaponAbility();
		return;
	}
	AbortOwnedPursuitMove();
	if (!ValidateWeaponContinuation(Epoch) || bDetonationCommitted)
	{
		return;
	}

	if (IsValid(ActivePresentation.Get()))
	{
		ActivePresentation->EnterWarning(DetonationWarningDuration);
	}
	if (!ValidateWeaponContinuation(Epoch) || bDetonationCommitted) { return; }
	ReceiveDetonationWarningBegan(DetonationWarningDuration);
	if (!ValidateWeaponContinuation(Epoch) || bDetonationCommitted)
	{
		return;
	}
	if (!CanContinueWeaponPayload())
	{
		CancelDroneWeaponAbility();
		return;
	}

	World->GetTimerManager().SetTimer(
		DetonationWarningTimerHandle,
		FTimerDelegate::CreateUObject(this, &ThisClass::HandleDetonationWarningExpired, Epoch),
		FMath::Max(DetonationWarningDuration, 0.05f),
		false);
}

void USovGameplayAbility_ReformationDroneSelfDestruct::CommitDetonation()
{
	const uint64 Epoch = GetWeaponActivationEpoch();
	TStrongObjectPtr<USovGameplayAbility_ReformationDroneSelfDestruct> ActionLifetime(this);
	if (!ValidateWeaponContinuation(Epoch)
		|| !bWarningStarted
		|| bDetonationCommitted)
	{
		return;
	}
	if (!CanContinueWeaponPayload())
	{
		CancelDroneWeaponAbility();
		return;
	}

	AActor* SourceDrone = GetAvatarActorFromActorInfo();
	if (!IsValid(SourceDrone))
	{
		CancelDroneWeaponAbility();
		return;
	}
	UAbilitySystemComponent* SourceASC = CurrentActorInfo
		? CurrentActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	if (!IsValid(SourceASC)
		|| !SourceASC->GetSet<UNarrativeAttributeSetBase>())
	{
		UE_LOG(
			LogSovReformationDroneAbility,
			Error,
			TEXT("%s cannot self destruct without a Narrative combat AttributeSet."),
			*GetNameSafe(SourceDrone));
		CancelDroneWeaponAbility();
		return;
	}
	const float SelfDamageEffectLevel =
		static_cast<float>(GetAbilityLevel());
	const FCommittedExplosion Payload{DamageEffectClass, AbilityIdentityTag, DamageChannels,
		AttackClassifications, ExplosionRadius, ExplosionDamage, ExplosionPoiseDamage,
		MinimumExplosionDamageFraction, bExplosionRequiresLineOfSight};
	const TWeakObjectPtr<UNarrativeAbilitySystemComponent> NativeSource = Cast<UNarrativeAbilitySystemComponent>(SourceASC);
	const uint64 SourceActorInfoEpoch = NativeSource.IsValid() ? NativeSource->GetCombatActorInfoEpoch() : 0;
	const TWeakObjectPtr<const UNarrativeAttributeSetBase> SourceAttributes = SourceASC->GetSet<UNarrativeAttributeSetBase>();
	const uint64 SourceLifeEpoch = SourceAttributes->GetCombatLifeEpoch();
	const auto IsCommittedSourceCurrent = [SourceASC, SourceDrone, NativeSource, SourceActorInfoEpoch, SourceAttributes, SourceLifeEpoch]()
	{
		return IsValid(SourceASC) && IsValid(SourceDrone) && !SourceDrone->IsActorBeingDestroyed()
			&& SourceASC->GetAvatarActor() == SourceDrone
			&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceDrone) == SourceASC
			&& (!NativeSource.IsValid() || NativeSource->GetCombatActorInfoEpoch() == SourceActorInfoEpoch)
			&& SourceAttributes.IsValid() && SourceASC->GetSet<UNarrativeAttributeSetBase>() == SourceAttributes.Get()
			&& SourceAttributes->GetCombatLifeEpoch() == SourceLifeEpoch;
	};
	// Movement callbacks are still pre-commit and may interrupt or replace us.
	AbortOwnedPursuitMove();
	if (!ValidateWeaponContinuation(Epoch)) { return; }
	bDetonationCommitted = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PursuitUpdateTimerHandle);
		World->GetTimerManager().ClearTimer(DetonationWarningTimerHandle);
	}

	const FVector ExplosionLocation = SourceDrone->GetActorLocation();
	ASovReformationDroneSelfDestructPresentation* Presentation =
		ActivePresentation.Get();
	if (IsValid(Presentation))
	{
		// Freeze immutable state before any damage callback can synchronously
		// kill the source and cancel this ability.
		Presentation->PrepareDetonation(
			ExplosionLocation,
			Payload.Radius);
	}
	const bool bDamagedAnyTarget = IsCommittedSourceCurrent()
		&& ApplyExplosionDamage(ExplosionLocation, SourceASC, SourceDrone, SelfDamageEffectLevel, Payload);
	if (IsValid(Presentation))
	{
		Presentation->FinalizeDetonation(bDamagedAnyTarget);
	}
	// The blast is committed, not permission to kill a restored life or a new
	// avatar after outward damage/presentation callbacks. Never end its ability.
	if (!IsCommittedSourceCurrent())
	{
		if (IsWeaponActivationCurrent(Epoch)) { CancelDroneWeaponAbility(); }
		return;
	}
	if (!IsTargetAlive(SourceASC))
	{
		// A reactive effect may have killed the drone during the outward pass.
		// Narrative normally ends the ability synchronously, but finish it here
		// as a fallback so no firing tag or watchdog remains active.
		if (IsWeaponActivationCurrent(Epoch))
		{
			NotifyPayloadFinished();
		}
		return;
	}
	// Narrative death cancels this ability synchronously. Keep this as the final
	// operation so damage credit and presentation state are already committed.
	const bool bSourceDied = ApplyFatalSelfDamage(
		SourceASC,
		SourceDrone,
		SelfDamageEffectLevel,
		ExplosionLocation,
		Payload.EffectClass,
		Payload.Identity,
		Payload.Channels);
	if (!IsCommittedSourceCurrent())
	{
		if (IsWeaponActivationCurrent(Epoch)) { CancelDroneWeaponAbility(); }
		return;
	}
	if (!bSourceDied)
	{
		if (ASovDroneNPCBase* ProjectDrone =
			Cast<ASovDroneNPCBase>(SourceDrone))
		{
			ProjectDrone->ClearDeathExplosionSuppression();
		}
		if (IsWeaponActivationCurrent(Epoch))
		{
			UE_LOG(
				LogSovReformationDroneAbility,
				Error,
				TEXT("%s committed a self-destruct explosion but did not enter Narrative death."),
				*GetNameSafe(SourceDrone));
			CancelDroneWeaponAbility();
		}
		return;
	}
	// This is normally already inactive because Narrative death cancels active
	// abilities. Keep the native lifecycle correct if a character-specific
	// death implementation defers that cancellation.
	if (IsWeaponActivationCurrent(Epoch))
	{
		NotifyPayloadFinished();
	}
}

void USovGameplayAbility_ReformationDroneSelfDestruct::
	AbortOwnedPursuitMove()
{
	UPathFollowingComponent* PathFollowing = OwnedPathFollowing.Get();
	const FAIRequestID RequestToAbort = OwnedPursuitMoveRequestId;
	OwnedPursuitMoveRequestId = FAIRequestID::InvalidRequest;
	OwnedPathFollowing.Reset();
	if (RequestToAbort.IsValid()
		&& IsValid(PathFollowing)
		&& PathFollowing->GetCurrentRequestId().IsEquivalent(
			RequestToAbort))
	{
		PathFollowing->AbortMove(
			*this,
			FPathFollowingResultFlags::OwnerFinished,
			RequestToAbort,
			EPathFollowingVelocityMode::Reset);
	}
}

bool USovGameplayAbility_ReformationDroneSelfDestruct::ApplyExplosionDamage(
	const FVector& ExplosionLocation, UAbilitySystemComponent* SourceASC,
	AActor* SourceActor, float EffectLevel, const FCommittedExplosion& Payload) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| !IsValid(SourceASC)
		|| !IsValid(SourceActor)
		|| !Payload.EffectClass.Get()
		|| ExplosionLocation.ContainsNaN())
	{
		return false;
	}
	const INarrativeTeamAgentInterface* SourceTeam =
		Cast<const INarrativeTeamAgentInterface>(SourceActor);
	if (!SourceTeam)
	{
		return false;
	}
	// Damage application may synchronously kill the drone (for example through
	// reactive effects) and end this instanced ability. Capture every value that
	// depends on live ability state before invoking the first target callback.
	const TWeakObjectPtr<UNarrativeAbilitySystemComponent> NativeSource = Cast<UNarrativeAbilitySystemComponent>(SourceASC);
	const uint64 SourceActorInfoEpoch = NativeSource.IsValid() ? NativeSource->GetCombatActorInfoEpoch() : 0;
	const TWeakObjectPtr<const UNarrativeAttributeSetBase> SourceAttributes = SourceASC->GetSet<UNarrativeAttributeSetBase>();
	if (!SourceAttributes.IsValid()) { return false; }
	const uint64 SourceLifeEpoch = SourceAttributes->GetCombatLifeEpoch();
	const auto IsSourceCurrent = [SourceASC, SourceActor, NativeSource, SourceActorInfoEpoch, SourceAttributes, SourceLifeEpoch]()
	{
		return IsValid(SourceASC) && IsValid(SourceActor) && !SourceActor->IsActorBeingDestroyed()
			&& SourceASC->GetAvatarActor() == SourceActor
			&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor) == SourceASC
			&& (!NativeSource.IsValid() || NativeSource->GetCombatActorInfoEpoch() == SourceActorInfoEpoch)
			&& SourceAttributes.IsValid() && SourceASC->GetSet<UNarrativeAttributeSetBase>() == SourceAttributes.Get()
			&& SourceAttributes->GetCombatLifeEpoch() == SourceLifeEpoch;
	};

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SovReformationDroneSelfDestructExplosion),
		false,
		SourceActor);
	QueryParams.AddIgnoredActor(SourceActor);
	if (IsValid(ActivePresentation.Get()))
	{
		QueryParams.AddIgnoredActor(ActivePresentation.Get());
	}

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		ExplosionLocation,
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(FMath::Max(Payload.Radius, 1.0f)),
		QueryParams);

	TSet<UAbilitySystemComponent*> UniqueTargets;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (UAbilitySystemComponent* TargetASC =
			ResolveAbilitySystemFromActor(Overlap.GetActor()))
		{
			UniqueTargets.Add(TargetASC);
		}
	}

	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	bool bDamagedAnyTarget = false;
	for (UAbilitySystemComponent* TargetASC : UniqueTargets)
	{
		if (!IsSourceCurrent()) { break; }
		AActor* TargetActor = IsValid(TargetASC)
			? TargetASC->GetAvatarActor()
			: nullptr;
		if (!IsValid(TargetASC)
			|| !IsValid(TargetActor)
			|| TargetASC == SourceASC
			|| !TargetASC->GetSet<UNarrativeAttributeSetBase>()
			|| !IsTargetAlive(TargetASC)
			|| SourceTeam->GetTeamAttitudeTowards(*TargetActor)
				!= ETeamAttitude::Hostile
			|| (Payload.bRequiresLOS
				&& !HasExplosionLineOfSight(
					SourceActor,
					TargetActor,
					TargetASC,
					ExplosionLocation)))
		{
			continue;
		}
		if (!IsSourceCurrent()) { break; }
		if (!IsValid(TargetASC) || !IsValid(TargetActor) || TargetASC->GetAvatarActor() != TargetActor) { continue; }

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(SourceActor, SourceActor);
		Context.AddSourceObject(SourceActor);
		Context.AddOrigin(ExplosionLocation);
		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
			Payload.EffectClass,
			EffectLevel,
			Context);
		if (!IsSourceCurrent()) { break; }
		FGameplayEffectSpec* DamageSpec = SpecHandle.Data.Get();
		if (!DamageSpec)
		{
			continue;
		}

		const float DistanceAlpha = FMath::Clamp(
			FVector::Distance(
				ExplosionLocation,
				TargetActor->GetActorLocation())
				/ FMath::Max(Payload.Radius, 1.0f),
			0.0f,
			1.0f);
		const float FalloffScalar = FMath::Lerp(
			1.0f,
			Payload.MinimumFraction,
			DistanceAlpha);
		DamageSpec->AddDynamicAssetTag(Payload.Identity);
		for (const FGameplayTag& Tag : Payload.Channels)
		{
			DamageSpec->AddDynamicAssetTag(Tag);
		}
		for (const FGameplayTag& Tag : Payload.Classifications)
		{
			DamageSpec->AddDynamicAssetTag(Tag);
		}
		DamageSpec->SetSetByCallerMagnitude(
			FNarrativeGameplayTags::Get().SetByCaller_Damage,
			Payload.Damage);
		DamageSpec->SetSetByCallerMagnitude(
			SovTags.SetByCaller_Damage_SourceModifier,
			FalloffScalar);
		if (Payload.Poise > KINDA_SMALL_NUMBER)
		{
			DamageSpec->SetSetByCallerMagnitude(
				SovTags.SetByCaller_Damage_PoiseDamage,
				Payload.Poise * FalloffScalar);
		}

		const float OldShield = TargetASC->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetShieldAttribute());
		const float OldHealth = TargetASC->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetHealthAttribute());
		const float OldPoise = TargetASC->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetPoiseAttribute());
		const float OldStamina = TargetASC->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetStaminaAttribute());
		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
		if (!IsValid(TargetASC) || TargetASC->GetAvatarActor() != TargetActor) { continue; }
		bDamagedAnyTarget = bDamagedAnyTarget
			|| TargetASC->GetNumericAttribute(
				UNarrativeAttributeSetBase::GetShieldAttribute())
				< OldShield - KINDA_SMALL_NUMBER
			|| TargetASC->GetNumericAttribute(
				UNarrativeAttributeSetBase::GetHealthAttribute())
				< OldHealth - KINDA_SMALL_NUMBER
			|| TargetASC->GetNumericAttribute(
				UNarrativeAttributeSetBase::GetPoiseAttribute())
				< OldPoise - KINDA_SMALL_NUMBER
			|| TargetASC->GetNumericAttribute(
				UNarrativeAttributeSetBase::GetStaminaAttribute())
				< OldStamina - KINDA_SMALL_NUMBER;
	}
	return bDamagedAnyTarget;
}

bool USovGameplayAbility_ReformationDroneSelfDestruct::
	HasExplosionLineOfSight(
		AActor* SourceActor,
		AActor* TargetActor,
		UAbilitySystemComponent* TargetAbilitySystem,
		const FVector& ExplosionLocation) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| !IsValid(SourceActor)
		|| !IsValid(TargetActor)
		|| !IsValid(TargetAbilitySystem))
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SovReformationDroneSelfDestructLineOfSight),
		false,
		SourceActor);
	QueryParams.AddIgnoredActor(SourceActor);
	if (IsValid(ActivePresentation.Get()))
	{
		QueryParams.AddIgnoredActor(ActivePresentation.Get());
	}
	if (const ANarrativeCharacter* NarrativeCharacter =
		Cast<ANarrativeCharacter>(SourceActor))
	{
		if (ANarrativeCharacterVisual* CharacterVisual =
			NarrativeCharacter->GetCharacterVisual())
		{
			QueryParams.AddIgnoredActor(CharacterVisual);
		}
	}

	FHitResult BlockingHit;
	if (!World->LineTraceSingleByObjectType(
		BlockingHit,
		ExplosionLocation + (FVector::UpVector * 2.0f),
		TargetActor->GetActorLocation(),
		ObjectQuery,
		QueryParams))
	{
		return true;
	}
	AActor* BlockingActor = BlockingHit.GetActor();
	return BlockingActor == TargetActor
		|| (IsValid(BlockingActor) && BlockingActor->IsOwnedBy(TargetActor))
		|| (IsValid(BlockingActor) && TargetActor->IsOwnedBy(BlockingActor))
		|| ResolveAbilitySystemFromActor(BlockingActor)
			== TargetAbilitySystem;
}

bool USovGameplayAbility_ReformationDroneSelfDestruct::
	ApplyFatalSelfDamage(
		UAbilitySystemComponent* SourceASC,
		AActor* SourceActor,
		const float EffectLevel,
		const FVector& ExplosionLocation,
		TSubclassOf<UGameplayEffect> CommittedEffectClass,
		FGameplayTag CommittedIdentity,
		const FGameplayTagContainer& CommittedChannels) const
{
	if (!IsValid(SourceASC)
		|| !IsValid(SourceActor)
		|| !CommittedEffectClass.Get())
	{
		UE_LOG(
			LogSovReformationDroneAbility,
			Error,
			TEXT("Self destruct committed without a valid source ASC, avatar, or damage effect."));
		return false;
	}
	const TWeakObjectPtr<UNarrativeAbilitySystemComponent> NativeSource = Cast<UNarrativeAbilitySystemComponent>(SourceASC);
	const uint64 SourceActorInfoEpoch = NativeSource.IsValid() ? NativeSource->GetCombatActorInfoEpoch() : 0;
	const TWeakObjectPtr<const UNarrativeAttributeSetBase> SourceAttributes = SourceASC->GetSet<UNarrativeAttributeSetBase>();
	if (!SourceAttributes.IsValid()) { return false; }
	const uint64 SourceLifeEpoch = SourceAttributes->GetCombatLifeEpoch();
	const auto IsSourceCurrent = [SourceASC, SourceActor, NativeSource, SourceActorInfoEpoch, SourceAttributes, SourceLifeEpoch]()
	{
		return IsValid(SourceASC) && IsValid(SourceActor) && !SourceActor->IsActorBeingDestroyed()
			&& SourceASC->GetAvatarActor() == SourceActor
			&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor) == SourceASC
			&& (!NativeSource.IsValid() || NativeSource->GetCombatActorInfoEpoch() == SourceActorInfoEpoch)
			&& SourceAttributes.IsValid() && SourceASC->GetSet<UNarrativeAttributeSetBase>() == SourceAttributes.Get()
			&& SourceAttributes->GetCombatLifeEpoch() == SourceLifeEpoch;
	};
	if (!IsSourceCurrent()) { return false; }

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(SourceActor, SourceActor);
	Context.AddSourceObject(SourceActor);
	Context.AddOrigin(ExplosionLocation);
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		CommittedEffectClass,
		EffectLevel,
		Context);
	if (!IsSourceCurrent()) { return false; }
	FGameplayEffectSpec* FatalSpec = SpecHandle.Data.Get();
	if (!FatalSpec)
	{
		UE_LOG(
			LogSovReformationDroneAbility,
			Error,
			TEXT("%s could not create its fatal self-destruct damage spec."),
			*GetNameSafe(SourceActor));
		return false;
	}

	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	FatalSpec->AddDynamicAssetTag(CommittedIdentity);
	FatalSpec->AddDynamicAssetTag(SovTags.Damage_Fatal);
	for (const FGameplayTag& Tag : CommittedChannels)
	{
		FatalSpec->AddDynamicAssetTag(Tag);
	}
	const float CurrentHealth = SourceASC->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetHealthAttribute());
	FatalSpec->SetSetByCallerMagnitude(
		FNarrativeGameplayTags::Get().SetByCaller_Damage,
		FMath::Max(CurrentHealth + 1.0f, 1.0f));
	// Claim suppression only after the callback-capable spec construction and
	// its exact-owner validation. A rejected spec must not suppress a later,
	// unrelated death on a retained drone. No authored callback lies between
	// this claim and submitting the fatal effect.
	if (ASovDroneNPCBase* ProjectDrone = Cast<ASovDroneNPCBase>(SourceActor))
	{
		ProjectDrone->SuppressNextDeathExplosion();
	}
	SourceASC->ApplyGameplayEffectSpecToSelf(*FatalSpec);
	return !IsTargetAlive(SourceASC);
}

ASovReformationDroneSelfDestructPresentation*
USovGameplayAbility_ReformationDroneSelfDestruct::SpawnPresentation(
	AActor* SourceDrone) const
{
	const uint64 Epoch = GetWeaponActivationEpoch();
	UWorld* World = GetWorld();
	const TSubclassOf<ASovReformationDroneSelfDestructPresentation>
		ResolvedClass = ResolvePresentationClass();
	if (!IsValid(World) || !IsValid(SourceDrone) || !ResolvedClass.Get())
	{
		return nullptr;
	}

	const FTransform SpawnTransform = SourceDrone->GetActorTransform();
	ASovReformationDroneSelfDestructPresentation* Presentation =
		World->SpawnActorDeferred<ASovReformationDroneSelfDestructPresentation>(
			ResolvedClass,
			SpawnTransform,
			SourceDrone,
			Cast<APawn>(SourceDrone),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Presentation))
	{
		return nullptr;
	}
	if (!IsWeaponActivationCurrent(Epoch) || !CanContinueWeaponPayload())
	{
		Presentation->Destroy();
		return nullptr;
	}
	Presentation->InitializeForDrone(SourceDrone, DetonationWarningDuration);
	if (!IsWeaponActivationCurrent(Epoch) || !CanContinueWeaponPayload())
	{
		Presentation->Destroy();
		return nullptr;
	}
	UGameplayStatics::FinishSpawningActor(Presentation, SpawnTransform);
	return IsValid(Presentation) && !Presentation->IsActorBeingDestroyed()
		? Presentation
		: nullptr;
}

TSubclassOf<ASovReformationDroneSelfDestructPresentation>
USovGameplayAbility_ReformationDroneSelfDestruct::
	ResolvePresentationClass() const
{
	if (PresentationClass.Get())
	{
		return PresentationClass;
	}
	return ASovReformationDroneSelfDestructPresentation::StaticClass();
}
