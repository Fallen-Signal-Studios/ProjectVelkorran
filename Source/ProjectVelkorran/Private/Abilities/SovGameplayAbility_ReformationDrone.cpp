// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_ReformationDrone.h"

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
#include "Combat/SovDamageTargetSnapshot.h"
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
	if (bEndingAbility || bEndRequested) { return false; }
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
	if (!IsValid(ActorInfo->AbilitySystemComponent.Get())
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
	const uint64 Activation = ++DroneActivationSerial;
	UnbindDroneCancellation();
	bEndRequested = false;
	bPayloadStarted = false;
	bPayloadFinished = false;
	bAbilityStarted = false;
	bEndingAbility = false;
	CharacterOwner = ActorInfo
		? Cast<ANarrativeCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	DroneSourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	DroneSourceAvatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(DroneSourceASC.Get());
	DroneActorInfoEpoch = NarrativeASC ? NarrativeASC->GetCombatActorInfoEpoch() : 0;
	DroneReadyEpoch = NarrativeASC ? NarrativeASC->GetCharacterReadyEpoch() : 0;
	BindDroneCancellation(DroneSourceASC.Get());

	if (!ActorInfo
		|| !ActorInfo->IsNetAuthority()
		|| !IsValid(CharacterOwner.Get())
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!IsDroneActivationCurrent(Activation)) { return; }
	if (!CanContinueWeaponPayload()) { CancelDroneWeaponAbility(); return; }

	if (UWorld* World = GetWorld())
	{
		NextAllowedActivationTime = World->GetTimeSeconds()
			+ FMath::Max(CooldownDuration, 0.0f);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsDroneActivationCurrent(Activation))
	{
		return;
	}
	if (!CanContinueWeaponPayload()) { CancelDroneWeaponAbility(); return; }

	bAbilityStarted = true;
	StartAttackMontage();
	if (!IsDroneActivationCurrent(Activation))
	{
		return;
	}
	ReceiveDroneWeaponStarted();
	if (!IsDroneActivationCurrent(Activation))
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
				HandleAutomaticPayloadRelease();
			}
			else
			{
				World->GetTimerManager().SetTimer(
					PayloadReleaseTimerHandle,
					FTimerDelegate::CreateWeakLambda(this, [this, Activation]()
					{ if (IsDroneActivationCurrent(Activation)) { HandleAutomaticPayloadRelease(); } }),
					ReleaseDelay,
					false);
			}
		}
		if (!IsDroneActivationCurrent(Activation))
		{
			return;
		}

		World->GetTimerManager().SetTimer(
			MaximumDurationTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this, Activation]()
			{ if (IsDroneActivationCurrent(Activation)) { HandleMaximumDurationExpired(); } }),
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
	if (!IsEndAbilityValid(Handle, ActorInfo)) { return; }
	FenceDroneEndRequest();
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility,
			Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}
	if (bEndingAbility)
	{
		return;
	}
	bEndingAbility = true;
	UnbindDroneCancellation();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PayloadReleaseTimerHandle);
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
		World->GetTimerManager().ClearTimer(MaximumDurationTimerHandle);
	}

	MontageTask = nullptr;
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
	bEndRequested = false;
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
	if (!IsActive() || bEndingAbility || bPayloadFinished)
	{
		return false;
	}
	if (bPayloadStarted)
	{
		return CanContinueWeaponPayload();
	}
	if (!CanContinueWeaponPayload())
	{
		return false;
	}

	const uint64 Activation = DroneActivationSerial;
	bPayloadStarted = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PayloadReleaseTimerHandle);
	}
	ReceiveDroneWeaponPayloadReleased();
	return IsDroneActivationCurrent(Activation) && CanContinueWeaponPayload();
}

void USovGameplayAbility_ReformationDroneWeaponBase::NotifyPayloadFinished()
{
	if (!IsActive() || bEndingAbility || bPayloadFinished)
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
		HandleRecoveryFinished();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const uint64 Activation = DroneActivationSerial;
		World->GetTimerManager().SetTimer(
			RecoveryTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this, Activation]()
			{ if (IsDroneActivationCurrent(Activation)) { HandleRecoveryFinished(); } }),
			Recovery,
			false);
		return;
	}

	// GetWorld should exist for an active GAS ability, but never strand the
	// attack lane if teardown races a recovery request.
	HandleRecoveryFinished();
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
	if (!IsDroneActivationCurrent(DroneActivationSerial)
		|| bPayloadFinished
		|| !CurrentActorInfo
		|| !CurrentActorInfo->IsNetAuthority()
		|| !IsValid(CurrentActorInfo->AvatarActor.Get()))
	{
		return false;
	}

	const UAbilitySystemComponent* AbilitySystem =
		CurrentActorInfo->AbilitySystemComponent.Get();
	const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystem);
	if (!IsValid(AbilitySystem)
		|| AbilitySystem != DroneSourceASC.Get()
		|| AbilitySystem->GetAvatarActor() != DroneSourceAvatar.Get()
		|| CurrentActorInfo->AvatarActor.Get() != DroneSourceAvatar.Get()
		|| !IsValid(DroneSourceAvatar.Get()) || DroneSourceAvatar->IsActorBeingDestroyed()
		|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(DroneSourceAvatar.Get()) != AbilitySystem
		|| (NarrativeASC && (NarrativeASC->GetCombatActorInfoEpoch() != DroneActorInfoEpoch
			|| NarrativeASC->GetCharacterReadyEpoch() != DroneReadyEpoch))
		|| AbilitySystem->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) <= 0.f)
	{
		return false;
	}

	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	return !AbilitySystem->HasMatchingGameplayTag(NarrativeTags.State_IsDead)
		&& !AbilitySystem->HasMatchingGameplayTag(NarrativeTags.State_Interacting)
		&& !AbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_SequencerControlled)
		&& !AbilitySystem->HasMatchingGameplayTag(NarrativeTags.State_Movement_Ragdoll)
		&& !AbilitySystem->HasMatchingGameplayTag(NarrativeTags.State_Weapon_BlockFiring)
		&& !AbilitySystem->HasMatchingGameplayTag(SovTags.State_Fatal)
		&& !AbilitySystem->HasMatchingGameplayTag(SovTags.State_Poise_Broken)
		&& !AbilitySystem->HasMatchingGameplayTag(SovTags.State_Status_Frozen)
		&& !AbilitySystem->HasMatchingGameplayTag(SovTags.State_Status_DeviceDisabled);
}

bool USovGameplayAbility_ReformationDroneWeaponBase::IsDroneActivationCurrent(
	const uint64 ExpectedSerial) const
{
	return ExpectedSerial == DroneActivationSerial && IsActive() && !bEndingAbility && !bEndRequested;
}

void USovGameplayAbility_ReformationDroneWeaponBase::FenceDroneEndRequest()
{
	if (!bEndRequested) { bEndRequested = true; ++DroneActivationSerial; }
}

void USovGameplayAbility_ReformationDroneWeaponBase::BindDroneCancellation(UAbilitySystemComponent* AbilitySystem)
{
	if (!IsValid(AbilitySystem)) { return; }
	const uint64 Activation = DroneActivationSerial;
	for (const FGameplayTag& Tag : ActivationBlockedTags)
	{
		// This ability owns IsFiring itself. Every other blocking transition
		// retires a windup/burst even if the tag disappears in the same callback.
		if (ActivationOwnedTags.HasTagExact(Tag)) { continue; }
		const FDelegateHandle Handle = AbilitySystem->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddWeakLambda(this, [this, Activation](FGameplayTag, int32 Count)
			{ if (Count > 0 && IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); } });
		DroneCancellationHandles.Emplace(Tag, Handle);
	}
	DroneHealthHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute())
		.AddWeakLambda(this, [this, Activation](const FOnAttributeChangeData& Change)
		{ if (Change.NewValue <= 0.f && IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); } });
}

void USovGameplayAbility_ReformationDroneWeaponBase::UnbindDroneCancellation()
{
	if (UAbilitySystemComponent* ASC = DroneSourceASC.Get())
	{
		for (const auto& Binding : DroneCancellationHandles)
		{
			ASC->RegisterGameplayTagEvent(Binding.Key, EGameplayTagEventType::NewOrRemoved).Remove(Binding.Value);
		}
		ASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute()).Remove(DroneHealthHandle);
	}
	DroneCancellationHandles.Reset();
	DroneHealthHandle.Reset();
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
	const uint64 Activation = DroneActivationSerial;
	if (!CanContinueWeaponPayload())
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
	AActor* SourceActor = CurrentActorInfo->AvatarActor.Get();
	UAbilitySystemComponent* TargetASC = ResolveAbilitySystemFromActor(Hit.GetActor());
	AActor* TargetAvatar = IsValid(TargetASC) ? TargetASC->GetAvatarActor() : nullptr;
	const FSovDamageTargetSnapshot Target(TargetASC);
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
	if (!IsDroneActivationCurrent(Activation) || !CanContinueWeaponPayload()
		|| !IsValid(TargetAvatar) || !Target.IsCurrent()) { return false; }

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
	Receipt->bRequireNativeProof = true;
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
	HandleAutomaticPayloadRelease()
{
	const uint64 Activation = DroneActivationSerial;
	if (!TryBeginWeaponPayloadRelease())
	{
		if (IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); }
		return;
	}
	if (!IsDroneActivationCurrent(Activation) || bPayloadFinished)
	{
		return;
	}
	ExecuteAutomaticPayload();
}

void USovGameplayAbility_ReformationDroneWeaponBase::HandleRecoveryFinished()
{
	if (IsActive())
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
	HandleMaximumDurationExpired()
{
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
	const uint64 Activation = GetDroneActivationSerial();
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
		if (IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); }
		return;
	}
	// The release event may itself call this function. Respect that inner call
	// instead of resetting and firing a duplicate burst on the outer stack.
	if (!IsDroneActivationCurrent(Activation) || !CanContinueWeaponPayload() || HasWeaponPayloadFinished() || bBurstStarted)
	{
		return;
	}

	bBurstStarted = true;
	++BurstEpoch;
	ShotsFired = 0;
	FireNextBurstShot();
}

void USovGameplayAbility_ReformationDroneGunfire::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo)) { return; }
	FenceDroneEndRequest();
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility,
			Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}

	++BurstEpoch;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BurstTimerHandle);
	}
	ShotsFired = 0;
	bBurstStarted = false;
	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
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

void USovGameplayAbility_ReformationDroneGunfire::FireNextBurstShot()
{
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
	const uint64 Activation = GetDroneActivationSerial();
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
	if (!IsDroneActivationCurrent(Activation) || !CanContinueWeaponPayload()) { return; }
	const bool bDamagedTarget = BlockingHit
		&& ApplyPointDamage(*BlockingHit, DamagePerShot, PoiseDamagePerShot, ProtectionReceipt.Get());
	// Typed damage/reward callbacks may end this ability, start a new activation,
	// or switch the avatar. The old shot must not schedule that activation's burst.
	if (BurstEpoch != ExpectedBurstEpoch || !CanContinueWeaponPayload()
		|| GetAvatarActorFromActorInfo() != SourceCharacter) { return; }
	SpawnGunshotPresentation(
		TraceStart,
		TraceEnd,
		BlockingHit,
		bDamagedTarget,
		ShotIndex);
	if (!IsDroneActivationCurrent(Activation) || BurstEpoch != ExpectedBurstEpoch || !CanContinueWeaponPayload()) { return; }

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
			FTimerDelegate::CreateWeakLambda(this, [this, Activation, ExpectedBurstEpoch]()
			{ if (IsDroneActivationCurrent(Activation) && BurstEpoch == ExpectedBurstEpoch) { FireNextBurstShot(); } }),
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
	const uint64 Activation = GetDroneActivationSerial();
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
		if (IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); }
		return nullptr;
	}
	if (!IsDroneActivationCurrent(Activation) || !CanContinueWeaponPayload() || HasWeaponPayloadFinished() || bRocketReleaseAttempted)
	{
		return nullptr;
	}

	const int32 MuzzleIndex = NextMuzzleIndex;
	if (!SovThreatTargeting::CanUseActorFocus(GetAvatarActorFromActorInfo()))
	{
		if (IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); } return nullptr;
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
	if (!IsDroneActivationCurrent(Activation) || !CanContinueWeaponPayload()) { return nullptr; }
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
	const uint64 Activation = GetDroneActivationSerial();
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
		if (IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); }
		return nullptr;
	}
	// A Blueprint release event is allowed to launch the rocket. If it did,
	// this outer/manual call must not launch a second projectile.
	if (!IsDroneActivationCurrent(Activation) || !CanContinueWeaponPayload() || HasWeaponPayloadFinished() || bRocketReleaseAttempted)
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
		if (IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); }
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
		if (IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); }
		return nullptr;
	}

	ASovReformationDroneRocketProjectile* Rocket =
		World->SpawnActorDeferred<ASovReformationDroneRocketProjectile>(
			ResolveRocketClass(),
			ServerSpawnTransform,
			Avatar,
			Cast<APawn>(Avatar),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Rocket))
	{
		if (IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); }
		return nullptr;
	}
	if (!IsDroneActivationCurrent(Activation) || !CanContinueWeaponPayload())
	{
		Rocket->Destroy(); return nullptr;
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

	if (!IsDroneActivationCurrent(Activation) || !CanContinueWeaponPayload())
	{
		Rocket->Destroy(); return nullptr;
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
	UGameplayStatics::FinishSpawningActor(Rocket, ServerSpawnTransform);
	if (!IsValid(Rocket) || Rocket->IsActorBeingDestroyed())
	{
		if (IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); }
		return nullptr;
	}
	if (!IsDroneActivationCurrent(Activation) || !CanContinueWeaponPayload())
	{
		Rocket->Destroy(); return nullptr;
	}

	++NextMuzzleIndex;
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
	PursuitStartTime = 0.0;
	LastMoveRequestTime = 0.0;
	bPursuitStarted = false;
	bWarningStarted = false;
	bDetonationCommitted = false;
	bEndingSelfDestructAbility = false;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void USovGameplayAbility_ReformationDroneSelfDestruct::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo)) { return; }
	FenceDroneEndRequest();
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility,
			Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}

	if (bEndingSelfDestructAbility)
	{
		return;
	}
	TGuardValue<bool> EndGuard(bEndingSelfDestructAbility, true);
	const bool bOwnedMovement = bPursuitStarted || bWarningStarted;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PursuitUpdateTimerHandle);
		World->GetTimerManager().ClearTimer(DetonationWarningTimerHandle);
	}
	if (bOwnedMovement)
	{
		AbortOwnedPursuitMove();
	}

	const bool bCommittedExplosion = bDetonationCommitted;
	if (!bCommittedExplosion && IsValid(ActivePresentation.Get()))
	{
		ActivePresentation->CancelPresentation();
	}
	ActivePresentation = nullptr;
	PursuitTarget.Reset();
	OwnedPursuitMoveRequestId = FAIRequestID::InvalidRequest;
	PursuitStartTime = 0.0;
	LastMoveRequestTime = 0.0;
	bPursuitStarted = false;
	bWarningStarted = false;
	bDetonationCommitted = false;

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
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
	const uint64 Activation = GetDroneActivationSerial();
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
		if (IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); }
		return;
	}
	// The payload-released Blueprint event may have called this function
	// recursively. Respect that inner run instead of starting a second one.
	if (!IsDroneActivationCurrent(Activation) || !CanContinueWeaponPayload()
		|| HasWeaponPayloadFinished()
		|| bPursuitStarted)
	{
		return;
	}

	AActor* SourceDrone = CurrentActorInfo->AvatarActor.Get();
	AActor* TargetActor = ResolvePursuitTarget();
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
		if (IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); }
		return;
	}

	bPursuitStarted = true;
	PursuitTarget = TargetActor;
	PursuitStartTime = World->GetTimeSeconds();
	ASovReformationDroneSelfDestructPresentation* NewPresentation = SpawnPresentation(SourceDrone);
	if (!IsDroneActivationCurrent(Activation) || !CanContinueWeaponPayload())
	{
		if (IsValid(NewPresentation)) { NewPresentation->CancelPresentation(); }
		return;
	}
	ActivePresentation = NewPresentation;
	ReceivePursuitAcquired(TargetActor);
	if (!IsDroneActivationCurrent(Activation) || bWarningStarted || bDetonationCommitted)
	{
		return;
	}
	if (!CanContinueWeaponPayload() || !SovThreatTargeting::CanTrack(SourceDrone, TargetActor))
	{
		if (IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); }
		return;
	}
	// Arm validation before MoveTo: an AlreadyAtGoal result enters the
	// warning synchronously and still needs prompt fuse interruption checks.
	World->GetTimerManager().SetTimer(
		PursuitUpdateTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, Activation]()
		{ if (IsDroneActivationCurrent(Activation)) { UpdatePursuit(); } }),
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
		if (!IsDroneActivationCurrent(Activation))
		{
			return;
		}
		EnterDetonationWarning();
		return;
	}

	const bool bMoveRequested = RequestPursuitMove(TargetActor);
	if (!IsDroneActivationCurrent(Activation))
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
		if (IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); }
		return;
	}
	if (bWarningStarted)
	{
		return;
	}
}

void USovGameplayAbility_ReformationDroneSelfDestruct::UpdatePursuit()
{
	const uint64 Activation = GetDroneActivationSerial();
	if (!IsActive()
		|| !bPursuitStarted
		|| bDetonationCommitted)
	{
		return;
	}
	if (!CanContinueWeaponPayload())
	{
		if (IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); }
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
		CancelDroneWeaponAbility();
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (!IsDroneActivationCurrent(Activation) || !CanContinueWeaponPayload()) { return; }
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
	const bool bOwnedMoveIsCurrent = OwnedPursuitMoveRequestId.IsValid()
		&& PathFollowing->GetCurrentRequestId().IsEquivalent(
			OwnedPursuitMoveRequestId);
	if (bMoveIsActive && !bOwnedMoveIsCurrent)
	{
		// Another AI task replaced the pursuit. Cancel this ability without
		// disturbing the new path-following owner.
		CancelDroneWeaponAbility();
		return;
	}
	if (!bMoveIsActive
		&& Now - LastMoveRequestTime >= MoveRetryInterval
		&& !RequestPursuitMove(TargetActor))
	{
		if (IsDroneActivationCurrent(Activation)) { CancelDroneWeaponAbility(); }
	}
}

void USovGameplayAbility_ReformationDroneSelfDestruct::
	HandleDetonationWarningExpired()
{
	CommitDetonation();
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
	const uint64 Activation = GetDroneActivationSerial();
	AAIController* AIController = Cast<AAIController>(GetOwningController());
	if (!IsValid(AIController) || !IsValid(TargetActor)
		|| !SovThreatTargeting::CanTrack(GetAvatarActorFromActorInfo(), TargetActor))
	{
		return false;
	}

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
	const FPathFollowingRequestResult MoveResult =
		AIController->MoveTo(MoveRequest, nullptr);
	if (!IsDroneActivationCurrent(Activation) || !CanContinueWeaponPayload()
		|| !SovThreatTargeting::CanTrack(GetAvatarActorFromActorInfo(), TargetActor))
	{
		// Replacing a previous AI request may synchronously notify StateTree/BT.
		// If that ended the ability, do not leave the newly issued move running.
		UPathFollowingComponent* Following = AIController->GetPathFollowingComponent();
		if (IsValid(Following) && MoveResult.MoveId.IsValid()
			&& Following->GetCurrentRequestId().IsEquivalent(MoveResult.MoveId))
		{
			Following->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished,
				MoveResult.MoveId, EPathFollowingVelocityMode::Reset);
		}
		return false;
	}
	OwnedPursuitMoveRequestId = MoveResult.MoveId;
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
	const uint64 Activation = GetDroneActivationSerial();
	if (!IsActive()
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
	if (!IsDroneActivationCurrent(Activation) || bDetonationCommitted)
	{
		return;
	}

	if (IsValid(ActivePresentation.Get()))
	{
		ActivePresentation->EnterWarning(DetonationWarningDuration);
	}
	if (!IsDroneActivationCurrent(Activation) || !CanContinueWeaponPayload()) { return; }
	ReceiveDetonationWarningBegan(DetonationWarningDuration);
	if (!IsDroneActivationCurrent(Activation) || bDetonationCommitted)
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
		FTimerDelegate::CreateWeakLambda(this, [this, Activation]()
		{ if (IsDroneActivationCurrent(Activation)) { HandleDetonationWarningExpired(); } }),
		FMath::Max(DetonationWarningDuration, 0.05f),
		false);
}

void USovGameplayAbility_ReformationDroneSelfDestruct::CommitDetonation()
{
	if (!IsActive()
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
	const uint64 Activation = GetDroneActivationSerial();
	const auto* NarrativeSource = Cast<UNarrativeAbilitySystemComponent>(SourceASC);
	const uint64 SourceEpoch = NarrativeSource ? NarrativeSource->GetCombatActorInfoEpoch() : 0;
	const int32 ReadyEpoch = NarrativeSource ? NarrativeSource->GetCharacterReadyEpoch() : 0;
	const FSovDamageTargetSnapshot CommittedSource(SourceASC);
	AbortOwnedPursuitMove();
	if (!IsDroneActivationCurrent(Activation) || !CanContinueWeaponPayload()) { return; }
	bDetonationCommitted = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PursuitUpdateTimerHandle);
		World->GetTimerManager().ClearTimer(DetonationWarningTimerHandle);
	}

	const FVector ExplosionLocation = SourceDrone->GetActorLocation();
	ASovReformationDroneSelfDestructPresentation* Presentation =
		ActivePresentation.Get();
	const bool bDamagedAnyTarget = ApplyExplosionDamage(ExplosionLocation, Presentation);
	if (IsValid(Presentation))
	{
		Presentation->FinalizeDetonation(bDamagedAnyTarget);
	}
	// The committed blast may outlive death/cancellation, but it never kills a
	// replacement avatar or crosses a checkpoint restoration on this ASC.
	if (!CommittedSource.IsCurrent() || !IsValid(SourceASC) || !IsValid(SourceDrone) || SourceASC->GetAvatarActor() != SourceDrone
		|| (NarrativeSource && (NarrativeSource->GetCombatActorInfoEpoch() != SourceEpoch
			|| NarrativeSource->GetCharacterReadyEpoch() != ReadyEpoch))) { return; }
	if (!IsTargetAlive(SourceASC))
	{
		// A reactive effect may have killed the drone during the outward pass.
		// Narrative normally ends the ability synchronously, but finish it here
		// as a fallback so no firing tag or watchdog remains active.
		if (IsDroneActivationCurrent(Activation))
		{
			NotifyPayloadFinished();
		}
		return;
	}
	// Narrative death cancels this ability synchronously. Keep this as the final
	// operation so damage credit and presentation state are already committed.
	// Suppress the optional generic death blast explicitly. The drone base also
	// inspects committed presentation state as a compatibility fallback, but the
	// gameplay contract must not depend on that presentation actor still existing.
	if (ASovDroneNPCBase* ProjectDrone = Cast<ASovDroneNPCBase>(SourceDrone))
	{
		ProjectDrone->SuppressNextDeathExplosion();
	}
	const bool bSourceDied = ApplyFatalSelfDamage(
		SourceASC,
		SourceDrone,
		SelfDamageEffectLevel,
		ExplosionLocation);
	if (!bSourceDied)
	{
		if (ASovDroneNPCBase* ProjectDrone =
			Cast<ASovDroneNPCBase>(SourceDrone))
		{
			ProjectDrone->ClearDeathExplosionSuppression();
		}
		if (IsDroneActivationCurrent(Activation))
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
	if (IsDroneActivationCurrent(Activation))
	{
		NotifyPayloadFinished();
	}
}

void USovGameplayAbility_ReformationDroneSelfDestruct::
	AbortOwnedPursuitMove()
{
	AAIController* AIController = Cast<AAIController>(GetOwningController());
	UPathFollowingComponent* PathFollowing = IsValid(AIController)
		? AIController->GetPathFollowingComponent()
		: nullptr;
	const FAIRequestID RequestToAbort = OwnedPursuitMoveRequestId;
	OwnedPursuitMoveRequestId = FAIRequestID::InvalidRequest;
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
	const FVector& ExplosionLocation, ASovReformationDroneSelfDestructPresentation* Presentation) const
{
	UWorld* World = GetWorld();
	UAbilitySystemComponent* SourceASC = CurrentActorInfo
		? CurrentActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	AActor* SourceActor = CurrentActorInfo
		? CurrentActorInfo->AvatarActor.Get()
		: nullptr;
	if (!IsValid(World)
		|| !IsValid(SourceASC)
		|| !IsValid(SourceActor)
		|| !DamageEffectClass.Get()
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
	const float EffectLevel = static_cast<float>(GetAbilityLevel());
	const auto ReleaseExplosionRadius = ExplosionRadius;
	const auto ReleaseExplosionDamage = ExplosionDamage;
	const auto ReleaseExplosionPoiseDamage = ExplosionPoiseDamage;
	const auto ReleaseMinimumExplosionDamageFraction = MinimumExplosionDamageFraction;
	const auto bReleaseRequiresLineOfSight = bExplosionRequiresLineOfSight;
	const auto ReleaseDamageEffectClass = DamageEffectClass;
	const auto ReleaseAbilityIdentityTag = AbilityIdentityTag;
	const auto ReleaseDamageChannels = DamageChannels;
	const auto ReleaseAttackClassifications = AttackClassifications;
	const auto* NarrativeSource = Cast<UNarrativeAbilitySystemComponent>(SourceASC);
	const uint64 SourceEpoch = NarrativeSource ? NarrativeSource->GetCombatActorInfoEpoch() : 0;
	const int32 ReadyEpoch = NarrativeSource ? NarrativeSource->GetCharacterReadyEpoch() : 0;
	const FSovDamageTargetSnapshot CommittedSource(SourceASC);
	auto IsSourceCurrent = [SourceASC, SourceActor, NarrativeSource, SourceEpoch, ReadyEpoch, CommittedSource]()
	{
		return CommittedSource.IsCurrent() && IsValid(SourceASC) && IsValid(SourceActor) && SourceASC->GetAvatarActor() == SourceActor
			&& (!NarrativeSource || (NarrativeSource->GetCombatActorInfoEpoch() == SourceEpoch
				&& NarrativeSource->GetCharacterReadyEpoch() == ReadyEpoch));
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
		FCollisionShape::MakeSphere(FMath::Max(ReleaseExplosionRadius, 1.0f)),
		QueryParams);

	TSet<UAbilitySystemComponent*> UniqueTargets;
	TArray<FSovDamageTargetSnapshot> Targets;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (UAbilitySystemComponent* TargetASC =
			ResolveAbilitySystemFromActor(Overlap.GetActor()))
		{
			if (!UniqueTargets.Contains(TargetASC)) { UniqueTargets.Add(TargetASC); Targets.Emplace(TargetASC); }
		}
	}

	UObject* SourceObject = GetCurrentSourceObject();
	if (!IsValid(SourceObject) || SourceObject->IsA<UGameplayAbility>())
	{
		SourceObject = SourceActor;
	}
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	if (IsValid(Presentation)) { Presentation->PrepareDetonation(ExplosionLocation, ReleaseExplosionRadius); }
	bool bDamagedAnyTarget = false;
	for (const FSovDamageTargetSnapshot& Target : Targets)
	{
		if (!IsSourceCurrent()) { break; }
		if (!Target.IsCurrent()) { continue; }
		UAbilitySystemComponent* TargetASC = Target.AbilitySystem.Get();
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
			|| (bReleaseRequiresLineOfSight
				&& !HasExplosionLineOfSight(
					SourceActor,
					TargetActor,
					TargetASC,
					ExplosionLocation)))
		{
			continue;
		}

		if (!IsSourceCurrent() || !Target.IsCurrent()) { continue; }

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(SourceActor, SourceActor);
		Context.AddSourceObject(SourceObject);
		Context.AddOrigin(ExplosionLocation);
		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
			ReleaseDamageEffectClass,
			EffectLevel,
			Context);
		FGameplayEffectSpec* DamageSpec = SpecHandle.Data.Get();
		if (!DamageSpec)
		{
			continue;
		}

		const float DistanceAlpha = FMath::Clamp(
			FVector::Distance(
				ExplosionLocation,
				TargetActor->GetActorLocation())
				/ FMath::Max(ReleaseExplosionRadius, 1.0f),
			0.0f,
			1.0f);
		const float FalloffScalar = FMath::Lerp(
			1.0f,
			ReleaseMinimumExplosionDamageFraction,
			DistanceAlpha);
		DamageSpec->AddDynamicAssetTag(ReleaseAbilityIdentityTag);
		for (const FGameplayTag& Tag : ReleaseDamageChannels)
		{
			DamageSpec->AddDynamicAssetTag(Tag);
		}
		for (const FGameplayTag& Tag : ReleaseAttackClassifications)
		{
			DamageSpec->AddDynamicAssetTag(Tag);
		}
		DamageSpec->SetSetByCallerMagnitude(
			FNarrativeGameplayTags::Get().SetByCaller_Damage,
			ReleaseExplosionDamage);
		DamageSpec->SetSetByCallerMagnitude(
			SovTags.SetByCaller_Damage_SourceModifier,
			FalloffScalar);
		if (ReleaseExplosionPoiseDamage > KINDA_SMALL_NUMBER)
		{
			DamageSpec->SetSetByCallerMagnitude(
				SovTags.SetByCaller_Damage_PoiseDamage,
				ReleaseExplosionPoiseDamage * FalloffScalar);
		}

		TStrongObjectPtr<USovNativeDamageReceipt> Receipt(NewObject<USovNativeDamageReceipt>());
		Receipt->bRequireNativeProof = true;
		Receipt->ExpectedTarget = TargetActor;
		Receipt->ExpectedContext = DamageSpec->GetContext().Get();
		auto* ReceiptSource = Cast<UNarrativeAbilitySystemComponent>(SourceASC);
		if (ReceiptSource) { ReceiptSource->OnDamageResolvedAsSource.AddDynamic(Receipt.Get(), &USovNativeDamageReceipt::ReceiveResult); }
		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
		if (IsValid(ReceiptSource)) { ReceiptSource->OnDamageResolvedAsSource.RemoveDynamic(Receipt.Get(), &USovNativeDamageReceipt::ReceiveResult); }
		bDamagedAnyTarget |= Receipt->bAppliedDamage;
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
		const FVector& ExplosionLocation) const
{
	if (!IsValid(SourceASC)
		|| !IsValid(SourceActor)
		|| !DamageEffectClass.Get())
	{
		UE_LOG(
			LogSovReformationDroneAbility,
			Error,
			TEXT("Self destruct committed without a valid source ASC, avatar, or damage effect."));
		return false;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(SourceActor, SourceActor);
	Context.AddSourceObject(SourceActor);
	Context.AddOrigin(ExplosionLocation);
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		DamageEffectClass,
		EffectLevel,
		Context);
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
	FatalSpec->AddDynamicAssetTag(AbilityIdentityTag);
	FatalSpec->AddDynamicAssetTag(SovTags.Damage_Fatal);
	for (const FGameplayTag& Tag : DamageChannels)
	{
		FatalSpec->AddDynamicAssetTag(Tag);
	}
	const float CurrentHealth = SourceASC->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetHealthAttribute());
	FatalSpec->SetSetByCallerMagnitude(
		FNarrativeGameplayTags::Get().SetByCaller_Damage,
		FMath::Max(CurrentHealth + 1.0f, 1.0f));
	SourceASC->ApplyGameplayEffectSpecToSelf(*FatalSpec);
	return !IsTargetAlive(SourceASC);
}

ASovReformationDroneSelfDestructPresentation*
USovGameplayAbility_ReformationDroneSelfDestruct::SpawnPresentation(
	AActor* SourceDrone) const
{
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
	Presentation->InitializeForDrone(SourceDrone, DetonationWarningDuration);
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
