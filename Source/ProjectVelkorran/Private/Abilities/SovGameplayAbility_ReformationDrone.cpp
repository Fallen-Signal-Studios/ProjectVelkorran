// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_ReformationDrone.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AIController.h"
#include "AISystem.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Effects/SovGameplayEffect_ReformationDroneWeapons.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeGameplayTags.h"
#include "Projectiles/SovReformationDroneRocketProjectile.h"
#include "Presentation/SovReformationDroneGunshotPresentation.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Weapons/NarrativeProjectile.h"

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
		|| !IsValid(Cast<ANarrativeCharacter>(ActorInfo->AvatarActor.Get())))
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
	bPayloadStarted = false;
	bPayloadFinished = false;
	bAbilityStarted = false;
	bEndingAbility = false;
	CharacterOwner = ActorInfo
		? Cast<ANarrativeCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;

	if (!ActorInfo
		|| !ActorInfo->IsNetAuthority()
		|| !IsValid(CharacterOwner.Get())
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		NextAllowedActivationTime = World->GetTimeSeconds()
			+ FMath::Max(CooldownDuration, 0.0f);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	bAbilityStarted = true;
	StartAttackMontage();
	if (!IsActive())
	{
		return;
	}
	ReceiveDroneWeaponStarted();
	if (!IsActive())
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
					this,
					&ThisClass::HandleAutomaticPayloadRelease,
					ReleaseDelay,
					false);
			}
		}
		if (!IsActive())
		{
			return;
		}

		World->GetTimerManager().SetTimer(
			MaximumDurationTimerHandle,
			this,
			&ThisClass::HandleMaximumDurationExpired,
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
	if (bEndingAbility)
	{
		return;
	}
	bEndingAbility = true;

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
	return IsActive();
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
		World->GetTimerManager().SetTimer(
			RecoveryTimerHandle,
			this,
			&ThisClass::HandleRecoveryFinished,
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
	if (!IsActive()
		|| bEndingAbility
		|| bPayloadFinished
		|| !CurrentActorInfo
		|| !CurrentActorInfo->IsNetAuthority()
		|| !IsValid(CurrentActorInfo->AvatarActor.Get()))
	{
		return false;
	}

	const UAbilitySystemComponent* AbilitySystem =
		CurrentActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(AbilitySystem))
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
	const float PoiseDamage) const
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
	AActor* SourceActor = CurrentActorInfo->AvatarActor.Get();
	UAbilitySystemComponent* TargetASC = ResolveAbilitySystemFromActor(Hit.GetActor());
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

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
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

	const float OldShield = TargetASC->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetShieldAttribute());
	const float OldHealth = TargetASC->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetHealthAttribute());
	const float OldPoise = TargetASC->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetPoiseAttribute());
	const float OldStamina = TargetASC->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetStaminaAttribute());
	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);

	return TargetASC->GetNumericAttribute(
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
	if (!TryBeginWeaponPayloadRelease())
	{
		CancelDroneWeaponAbility();
		return;
	}
	if (!IsActive() || bPayloadFinished)
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
		CancelDroneWeaponAbility();
		return;
	}
	// The release event may itself call this function. Respect that inner call
	// instead of resetting and firing a duplicate burst on the outer stack.
	if (!IsActive() || HasWeaponPayloadFinished() || bBurstStarted)
	{
		return;
	}

	bBurstStarted = true;
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

	const int32 ShotIndex = ShotsFired;
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
	const bool bDamagedTarget = BlockingHit
		&& ApplyPointDamage(*BlockingHit, DamagePerShot, PoiseDamagePerShot);
	SpawnGunshotPresentation(
		TraceStart,
		TraceEnd,
		BlockingHit,
		bDamagedTarget,
		ShotIndex);

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
			this,
			&ThisClass::FireNextBurstShot,
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
		CancelDroneWeaponAbility();
		return nullptr;
	}
	if (!IsActive() || HasWeaponPayloadFinished() || bRocketReleaseAttempted)
	{
		return nullptr;
	}

	const int32 MuzzleIndex = NextMuzzleIndex;
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
	return LaunchRocket(
		SpawnTransform,
		LaunchDirection * RocketSpeed,
		ResolveHomingTarget());
}

ASovReformationDroneRocketProjectile*
USovGameplayAbility_ReformationDroneRocketLauncher::LaunchRocket(
	const FTransform& SpawnTransform,
	FVector InitialVelocity,
	AActor* HomingTarget)
{
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
		CancelDroneWeaponAbility();
		return nullptr;
	}
	// A Blueprint release event is allowed to launch the rocket. If it did,
	// this outer/manual call must not launch a second projectile.
	if (!IsActive() || HasWeaponPayloadFinished() || bRocketReleaseAttempted)
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
		if (IsHostileTarget(HomingTargetASC) && IsTargetAlive(HomingTargetASC))
		{
			ValidatedHomingTarget = HomingTargetASC->GetAvatarActor();
		}
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
		CancelDroneWeaponAbility();
		return nullptr;
	}

	NotifyPayloadFinished();
	++NextMuzzleIndex;
	return Rocket;
}

TSubclassOf<ASovReformationDroneRocketProjectile>
USovGameplayAbility_ReformationDroneRocketLauncher::ResolveRocketClass() const
{
	return RocketClass.Get()
		? RocketClass
		: ASovReformationDroneRocketProjectile::StaticClass();
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
		? TargetASC->GetAvatarActor()
		: nullptr;
}
