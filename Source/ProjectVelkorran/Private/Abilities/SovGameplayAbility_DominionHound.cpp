// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_DominionHound.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Effects/SovGameplayEffect_DominionHound.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Weapons/NarrativeProjectile.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovDominionHoundAbility, Log, All);

namespace
{
	UAbilitySystemComponent* ResolveAbilitySystemFromHoundHit(AActor* InActor)
	{
		AActor* Candidate = InActor;
		TSet<const AActor*> VisitedActors;
		for (int32 Depth = 0;
			IsValid(Candidate) && Depth < 6 && !VisitedActors.Contains(Candidate);
			++Depth)
		{
			VisitedActors.Add(Candidate);
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

	bool IsMovementBlockingSurface(const FHitResult& Hit)
	{
		if (!Hit.bBlockingHit)
		{
			return false;
		}

		const FVector ImpactNormal = FVector(Hit.ImpactNormal).GetSafeNormal();
		// A zero normal is common for a sphere that begins touching the floor.
		// Capsule movement still owns hard collision, so only a resolved steep
		// surface should stop the attack trace here.
		return !ImpactNormal.IsNearlyZero()
			&& FVector::DotProduct(ImpactNormal, FVector::UpVector) < 0.65f;
	}
}

USovGameplayAbility_DominionHoundAttackBase::
	USovGameplayAbility_DominionHoundAttackBase()
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

bool USovGameplayAbility_DominionHoundAttackBase::CanActivateAbility(
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

	UAbilitySystemComponent* SourceAbilitySystem =
		ActorInfo->AbilitySystemComponent.Get();
	AActor* SourceActor = ActorInfo->AvatarActor.Get();
	if (!IsValid(SourceAbilitySystem)
		|| !IsValid(Cast<ANarrativeCharacter>(SourceActor))
		|| !HasRequiredAttackConfiguration())
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(
				FNarrativeGameplayTags::Get().Ability_ActivateFail_TagsMissing);
		}
		return false;
	}

	const UWorld* World = GetWorld();
	if (IsValid(World)
		&& World->GetTimeSeconds() + KINDA_SMALL_NUMBER
			< NextAllowedActivationTime)
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(
				FNarrativeGameplayTags::Get().Ability_ActivateFail_Cooldown);
		}
		return false;
	}

	if (!IsValid(FindBestAttackTarget(SourceActor, SourceAbilitySystem)))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(
				FNarrativeGameplayTags::Get().Ability_ActivateFail_TagsMissing);
		}
		return false;
	}

	return true;
}

void USovGameplayAbility_DominionHoundAttackBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bPayloadStarted = false;
	bPayloadFinished = false;
	bAbilityStarted = false;
	bEndingAbility = false;
	HitTargets.Reset();
	AttackTarget.Reset();
	ActiveMontage = nullptr;
	CharacterOwner = ActorInfo
		? Cast<ANarrativeCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;

	UAbilitySystemComponent* SourceAbilitySystem = ActorInfo
		? ActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	AActor* SourceActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	AActor* TargetActor = FindBestAttackTarget(SourceActor, SourceAbilitySystem);
	if (!ActorInfo
		|| !ActorInfo->IsNetAuthority()
		|| !IsValid(CharacterOwner.Get())
		|| !IsValid(TargetActor))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AttackTarget = TargetActor;
	ConfigureActiveAttack(
		AttackMontage.Get(),
		MontagePlayRate,
		MontageStartSection,
		ImpactDelay,
		RecoveryAfterImpact);
	PrepareAttack();
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
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
	BindCancellationTags(SourceAbilitySystem);
	if (!CanContinueAttackPayload())
	{
		CancelHoundAttack();
		return;
	}

	bAbilityStarted = true;
	StartAttackMontage();
	ReceiveHoundAttackStarted(TargetActor);
	if (!IsActive())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const float SafeImpactDelay = FMath::Max(ActiveImpactDelay, 0.0f);
		if (SafeImpactDelay <= KINDA_SMALL_NUMBER)
		{
			HandleImpactTimer();
		}
		else
		{
			World->GetTimerManager().SetTimer(
				ImpactTimerHandle,
				this,
				&ThisClass::HandleImpactTimer,
				SafeImpactDelay,
				false);
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

void USovGameplayAbility_DominionHoundAttackBase::EndAbility(
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

	UnbindCancellationTags();
	StopOwnedMovement();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ImpactTimerHandle);
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
		World->GetTimerManager().ClearTimer(MaximumDurationTimerHandle);
	}

	MontageTask = nullptr;
	ActiveMontage = nullptr;
	AttackTarget.Reset();
	HitTargets.Reset();
	const bool bShouldBroadcastEnd = bAbilityStarted;
	bAbilityStarted = false;
	bPayloadStarted = false;
	bPayloadFinished = false;
	if (bShouldBroadcastEnd)
	{
		ReceiveHoundAttackEnded(bWasCancelled);
	}

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
	bEndingAbility = false;
}

float USovGameplayAbility_DominionHoundAttackBase::
	GetAttackDamage_Implementation() const
{
	return FMath::Max(DamageAmount, 0.0f);
}

bool USovGameplayAbility_DominionHoundAttackBase::
	HasRequiredAttackConfiguration() const
{
	return InputTag.IsValid()
		&& AbilityIdentityTag.IsValid()
		&& DamageEffectClass.Get()
		&& !DamageChannels.IsEmpty()
		&& !AttackClassifications.IsEmpty()
		&& FMath::IsFinite(DamageAmount)
		&& DamageAmount > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(PoiseDamageAmount)
		&& PoiseDamageAmount >= 0.0f
		&& FMath::IsFinite(MinimumAttackRange)
		&& MinimumAttackRange >= 0.0f
		&& FMath::IsFinite(MaximumAttackRange)
		&& MaximumAttackRange > MinimumAttackRange
		&& FMath::IsFinite(TraceReach)
		&& TraceReach >= 0.0f
		&& FMath::IsFinite(TraceRadius)
		&& TraceRadius > KINDA_SMALL_NUMBER
		&& !FallbackTraceOffset.ContainsNaN()
		&& FMath::IsFinite(MontagePlayRate)
		&& MontagePlayRate > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(ImpactDelay)
		&& ImpactDelay >= 0.0f
		&& FMath::IsFinite(RecoveryAfterImpact)
		&& RecoveryAfterImpact >= 0.0f
		&& FMath::IsFinite(CooldownDuration)
		&& CooldownDuration >= 0.0f
		&& FMath::IsFinite(MaximumActiveDuration)
		&& MaximumActiveDuration >= 0.1f
		&& MaximumActiveDuration + KINDA_SMALL_NUMBER
			>= ImpactDelay + RecoveryAfterImpact;
}

void USovGameplayAbility_DominionHoundAttackBase::PrepareAttack()
{
	ConfigureActiveAttack(
		AttackMontage.Get(),
		MontagePlayRate,
		MontageStartSection,
		ImpactDelay,
		RecoveryAfterImpact);
}

void USovGameplayAbility_DominionHoundAttackBase::BeginAttackPayload()
{
	const FVector Start = ResolveAttackProbeLocation();
	const FVector End = Start
		+ ResolveAttackDirection(Start) * FMath::Max(TraceReach, 0.0f);
	bool bBlockedByWorld = false;
	ApplyAttackSweep(Start, End, TraceRadius, bBlockedByWorld);
	FinishAttackPayload();
}

void USovGameplayAbility_DominionHoundAttackBase::StopOwnedMovement()
{
}

bool USovGameplayAbility_DominionHoundAttackBase::TryBeginAttackPayload()
{
	if (!IsActive() || bEndingAbility || bPayloadFinished)
	{
		return false;
	}
	if (bPayloadStarted)
	{
		return true;
	}
	if (!CanContinueAttackPayload())
	{
		return false;
	}

	bPayloadStarted = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ImpactTimerHandle);
	}
	BeginAttackPayload();
	return IsActive();
}

void USovGameplayAbility_DominionHoundAttackBase::FinishAttackPayload()
{
	if (!IsActive() || bEndingAbility || bPayloadFinished)
	{
		return;
	}
	if (!bPayloadStarted)
	{
		CancelHoundAttack();
		return;
	}

	StopOwnedMovement();
	bPayloadFinished = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ImpactTimerHandle);
	}
	const float Recovery = FMath::Max(ActiveRecoveryAfterImpact, 0.0f);
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
	HandleRecoveryFinished();
}

void USovGameplayAbility_DominionHoundAttackBase::CancelHoundAttack()
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

bool USovGameplayAbility_DominionHoundAttackBase::
	CanContinueAttackPayload() const
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
		&& !AbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_Movement_Ragdoll)
		&& !AbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_Weapon_BlockFiring)
		&& !AbilitySystem->HasMatchingGameplayTag(SovTags.State_Fatal)
		&& !AbilitySystem->HasMatchingGameplayTag(SovTags.State_Poise_Broken)
		&& !AbilitySystem->HasMatchingGameplayTag(SovTags.State_Status_Frozen)
		&& !AbilitySystem->HasMatchingGameplayTag(
			SovTags.State_Status_DeviceDisabled);
}

void USovGameplayAbility_DominionHoundAttackBase::ConfigureActiveAttack(
	UAnimMontage* Montage,
	const float PlayRate,
	const FName StartSection,
	const float InImpactDelay,
	const float InRecoveryAfterImpact)
{
	ActiveMontage = Montage;
	ActiveMontagePlayRate = FMath::Max(PlayRate, 0.01f);
	ActiveMontageStartSection = StartSection;
	ActiveImpactDelay = FMath::Max(InImpactDelay, 0.0f);
	ActiveRecoveryAfterImpact = FMath::Max(InRecoveryAfterImpact, 0.0f);
}

FVector USovGameplayAbility_DominionHoundAttackBase::ResolveAttackDirection(
	const FVector& FromLocation) const
{
	if (const AActor* TargetActor = AttackTarget.Get())
	{
		const FVector ToTarget = TargetActor->GetActorLocation() - FromLocation;
		if (!ToTarget.IsNearlyZero())
		{
			return ToTarget.GetSafeNormal();
		}
	}

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	return IsValid(Avatar)
		? Avatar->GetActorForwardVector()
		: FVector::ForwardVector;
}

FVector USovGameplayAbility_DominionHoundAttackBase::
	ResolveAttackProbeLocation() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar))
	{
		return FVector::ZeroVector;
	}

	const ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(Avatar);
	const USkeletalMeshComponent* Mesh = IsValid(Character)
		? Character->GetMesh()
		: nullptr;
	if (IsValid(Mesh)
		&& !TraceSocketName.IsNone()
		&& Mesh->DoesSocketExist(TraceSocketName))
	{
		return Mesh->GetSocketLocation(TraceSocketName);
	}

	return Avatar->GetActorTransform().TransformPositionNoScale(
		FallbackTraceOffset);
}

bool USovGameplayAbility_DominionHoundAttackBase::ApplyAttackSweep(
	const FVector& Start,
	const FVector& End,
	const float Radius,
	bool& bOutBlockedByWorld)
{
	bOutBlockedByWorld = false;
	TArray<FHitResult> Hits = PerformTraceMulti(
		Start,
		End,
		FMath::Max(Radius, 0.0f));
	Hits.Sort([](const FHitResult& Left, const FHitResult& Right)
	{
		return Left.Distance < Right.Distance;
	});

	AActor* SourceActor = GetAvatarActorFromActorInfo();
	UAbilitySystemComponent* SourceAbilitySystem = CurrentActorInfo
		? CurrentActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	for (const FHitResult& Hit : Hits)
	{
		UAbilitySystemComponent* TargetAbilitySystem =
			ResolveAbilitySystemFromHoundHit(Hit.GetActor());
		if (IsValidAttackTarget(
				SourceActor,
				SourceAbilitySystem,
				TargetAbilitySystem))
		{
			const TWeakObjectPtr<UAbilitySystemComponent> TargetKey(
				TargetAbilitySystem);
			if (HitTargets.Contains(TargetKey))
			{
				continue;
			}

			// Record before applying. Damage callbacks are synchronous and may
			// re-enter, kill, deflect, or otherwise mutate either participant.
			HitTargets.Add(TargetKey);
			const bool bAppliedDamage = ApplyPointDamage(
				Hit,
				TargetAbilitySystem);
			if (bAppliedDamage && IsActive() && !bEndingAbility)
			{
				ReceiveHoundAttackImpact();
			}
			return bAppliedDamage;
		}

		if (IsMovementBlockingSurface(Hit))
		{
			bOutBlockedByWorld = true;
			break;
		}
	}

	return false;
}

AActor* USovGameplayAbility_DominionHoundAttackBase::FindBestAttackTarget(
	AActor* SourceActor,
	UAbilitySystemComponent* SourceAbilitySystem) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| !IsValid(SourceActor)
		|| !IsValid(SourceAbilitySystem))
	{
		return nullptr;
	}

	auto ResolveValidTarget =
		[this, SourceActor, SourceAbilitySystem](AActor* Candidate) -> AActor*
	{
		UAbilitySystemComponent* CandidateAbilitySystem =
			ResolveAbilitySystemFromHoundHit(Candidate);
		AActor* TargetAvatar = IsValid(CandidateAbilitySystem)
			? CandidateAbilitySystem->GetAvatarActor()
			: nullptr;
		const APawn* TargetPawn = Cast<APawn>(TargetAvatar);
		if (!IsValid(TargetAvatar)
			|| (bOnlyAcquirePlayerControlledTargets
				&& (!IsValid(TargetPawn) || !TargetPawn->IsPlayerControlled()))
			|| !IsValidAttackTarget(
				SourceActor,
				SourceAbilitySystem,
				CandidateAbilitySystem))
		{
			return nullptr;
		}

		const float DistanceSquared = FVector::DistSquared(
			SourceActor->GetActorLocation(),
			TargetAvatar->GetActorLocation());
		return DistanceSquared + KINDA_SMALL_NUMBER
				>= FMath::Square(FMath::Max(MinimumAttackRange, 0.0f))
			&& DistanceSquared
				<= FMath::Square(FMath::Max(MaximumAttackRange, 0.0f))
			? TargetAvatar
			: nullptr;
	};

	if (const AAIController* AIController =
		Cast<AAIController>(GetOwningController()))
	{
		if (AActor* FocusTarget = ResolveValidTarget(
			AIController->GetFocusActor()))
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
			SourceActor->GetActorLocation(),
			Candidate->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestTarget = Candidate;
		}
	}

	return NearestTarget;
}

bool USovGameplayAbility_DominionHoundAttackBase::IsValidAttackTarget(
	AActor* SourceActor,
	UAbilitySystemComponent* SourceAbilitySystem,
	UAbilitySystemComponent* TargetAbilitySystem) const
{
	AActor* TargetActor = IsValid(TargetAbilitySystem)
		? TargetAbilitySystem->GetAvatarActor()
		: nullptr;
	const INarrativeTeamAgentInterface* SourceTeam =
		Cast<const INarrativeTeamAgentInterface>(SourceActor);
	if (!IsValid(SourceActor)
		|| !IsValid(SourceAbilitySystem)
		|| !IsValid(TargetAbilitySystem)
		|| !IsValid(TargetActor)
		|| TargetAbilitySystem == SourceAbilitySystem
		|| !TargetAbilitySystem->GetSet<UNarrativeAttributeSetBase>()
		|| !SourceTeam
		|| SourceTeam->GetTeamAttitudeTowards(*TargetActor)
			!= ETeamAttitude::Hostile)
	{
		return false;
	}

	const UNarrativeAbilitySystemComponent* NarrativeAbilitySystem =
		Cast<UNarrativeAbilitySystemComponent>(TargetAbilitySystem);
	return (!IsValid(NarrativeAbilitySystem)
			|| !NarrativeAbilitySystem->IsDead())
		&& !TargetAbilitySystem->HasMatchingGameplayTag(
			FNarrativeGameplayTags::Get().State_IsDead)
		&& !TargetAbilitySystem->HasMatchingGameplayTag(
			FSovGameplayTags::Get().State_Fatal);
}

bool USovGameplayAbility_DominionHoundAttackBase::ApplyPointDamage(
	const FHitResult& Hit,
	UAbilitySystemComponent* TargetAbilitySystem)
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return false;
	}

	UAbilitySystemComponent* SourceAbilitySystem =
		CurrentActorInfo->AbilitySystemComponent.Get();
	AActor* SourceActor = CurrentActorInfo->AvatarActor.Get();
	if (!IsValid(SourceAbilitySystem)
		|| !IsValid(SourceActor)
		|| !IsValid(TargetAbilitySystem)
		|| !DamageEffectClass.Get()
		|| DamageAmount <= KINDA_SMALL_NUMBER
		|| !IsValidAttackTarget(
			SourceActor,
			SourceAbilitySystem,
			TargetAbilitySystem))
	{
		return false;
	}

	FGameplayEffectContextHandle Context =
		SourceAbilitySystem->MakeEffectContext();
	Context.AddInstigator(SourceActor, SourceActor);
	UObject* SourceObject = GetCurrentSourceObject();
	if (!IsValid(SourceObject) || SourceObject->IsA<UGameplayAbility>())
	{
		SourceObject = SourceActor;
	}
	Context.AddSourceObject(SourceObject);
	Context.AddHitResult(Hit, true);
	Context.AddOrigin(FVector(Hit.ImpactPoint));

	FGameplayEffectSpecHandle SpecHandle =
		SourceAbilitySystem->MakeOutgoingSpec(
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
		FMath::Max(DamageAmount, 0.0f));
	if (PoiseDamageAmount > KINDA_SMALL_NUMBER)
	{
		DamageSpec->SetSetByCallerMagnitude(
			FSovGameplayTags::Get().SetByCaller_Damage_PoiseDamage,
			PoiseDamageAmount);
	}

	SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(
		*DamageSpec,
		TargetAbilitySystem);
	return true;
}

void USovGameplayAbility_DominionHoundAttackBase::StartAttackMontage()
{
	if (!IsValid(ActiveMontage.Get()))
	{
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::
		CreatePlayMontageAndWaitProxy(
			this,
			TEXT("DominionHoundAttackMontage"),
			ActiveMontage.Get(),
			FMath::Max(ActiveMontagePlayRate, 0.01f),
			ActiveMontageStartSection,
			true);
	if (!IsValid(MontageTask.Get()))
	{
		UE_LOG(
			LogSovDominionHoundAbility,
			Warning,
			TEXT("%s could not create its hound attack montage task."),
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

void USovGameplayAbility_DominionHoundAttackBase::HandleImpactTimer()
{
	if (!TryBeginAttackPayload())
	{
		CancelHoundAttack();
	}
}

void USovGameplayAbility_DominionHoundAttackBase::HandleRecoveryFinished()
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

void USovGameplayAbility_DominionHoundAttackBase::
	HandleMaximumDurationExpired()
{
	UE_LOG(
		LogSovDominionHoundAbility,
		Warning,
		TEXT("%s cancelled %s after its %.2f second lifecycle watchdog expired."),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(this),
		MaximumActiveDuration);
	CancelHoundAttack();
}

void USovGameplayAbility_DominionHoundAttackBase::HandleMontageCompleted()
{
	MontageTask = nullptr;
}

void USovGameplayAbility_DominionHoundAttackBase::HandleMontageInterrupted()
{
	MontageTask = nullptr;
	// Gameplay timing and movement remain native authority responsibilities.
}

void USovGameplayAbility_DominionHoundAttackBase::BindCancellationTags(
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
	const FGameplayTag TagsToWatch[] = {
		NarrativeTags.State_IsDead,
		NarrativeTags.State_Interacting,
		NarrativeTags.State_SequencerControlled,
		NarrativeTags.State_Movement_Ragdoll,
		NarrativeTags.State_Weapon_BlockFiring,
		SovTags.State_Fatal,
		SovTags.State_Poise_Broken,
		SovTags.State_Status_Frozen,
		SovTags.State_Status_DeviceDisabled};
	for (const FGameplayTag& Tag : TagsToWatch)
	{
		FDelegateHandle Handle = BoundAbilitySystem
			->RegisterGameplayTagEvent(
				Tag,
				EGameplayTagEventType::NewOrRemoved)
			.AddUObject(
				this,
				&ThisClass::HandleCancellationTagChanged);
		CancellationTagHandles.Emplace(Tag, Handle);
	}
}

void USovGameplayAbility_DominionHoundAttackBase::UnbindCancellationTags()
{
	if (IsValid(BoundAbilitySystem))
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Binding :
			CancellationTagHandles)
		{
			BoundAbilitySystem
				->RegisterGameplayTagEvent(
					Binding.Key,
					EGameplayTagEventType::NewOrRemoved)
				.Remove(Binding.Value);
		}
	}

	CancellationTagHandles.Reset();
	BoundAbilitySystem = nullptr;
}

void USovGameplayAbility_DominionHoundAttackBase::
	HandleCancellationTagChanged(
		const FGameplayTag CallbackTag,
		const int32 NewCount)
{
	static_cast<void>(CallbackTag);
	if (NewCount > 0 && IsActive())
	{
		CancelHoundAttack();
	}
}

USovGameplayAbility_DominionHoundBite::
	USovGameplayAbility_DominionHoundBite()
{
	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	InputTag = NarrativeTags.Narrative_Input_Attack;
	AbilityIdentityTag = SovTags.Ability_NPC_DominionHound_Bite;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(AbilityIdentityTag);
	AssetTags.AddTag(NarrativeTags.Ability_MeleeAttack);
	AssetTags.AddTag(NarrativeTags.Ability_DamageType_Melee);
	SetAssetTags(AssetTags);

	DamageChannels.AddTag(SovTags.Damage_Channel_Kinetic);
	DamageChannels.AddTag(SovTags.Damage_Channel_Edge);
	AttackClassifications.AddTag(SovTags.Damage_GuardClass_Standard);
	DamageEffectClass = USovGameplayEffect_DominionHoundDamage::StaticClass();
	DamageAmount = 16.0f;
	PoiseDamageAmount = 12.0f;
	MinimumAttackRange = 0.0f;
	MaximumAttackRange = 400.0f;
	TraceSocketName = TEXT("MouthSocket");
	FallbackTraceOffset = FVector(85.0f, 0.0f, 55.0f);
	TraceReach = 120.0f;
	TraceRadius = 55.0f;
	CooldownDuration = 0.8f;
	MaximumActiveDuration = 1.5f;
	DefaultBotAttackFrequency = 0.9f;
	DefaultBotAttackRange = 350.0f;

	BiteVariants.SetNum(3);
	BiteVariants[0].ImpactDelay = 0.20f;
	BiteVariants[0].RecoveryAfterImpact = 0.40f;
	BiteVariants[1].ImpactDelay = 0.24f;
	BiteVariants[1].RecoveryAfterImpact = 0.44f;
	BiteVariants[2].ImpactDelay = 0.22f;
	BiteVariants[2].RecoveryAfterImpact = 0.42f;
}

bool USovGameplayAbility_DominionHoundBite::
	HasRequiredAttackConfiguration() const
{
	if (!Super::HasRequiredAttackConfiguration() || BiteVariants.Num() != 3)
	{
		return false;
	}

	for (const FSovDominionHoundBiteVariant& Variant : BiteVariants)
	{
		if (!FMath::IsFinite(Variant.PlayRate)
			|| Variant.PlayRate <= KINDA_SMALL_NUMBER
			|| !FMath::IsFinite(Variant.ImpactDelay)
			|| Variant.ImpactDelay < 0.0f
			|| !FMath::IsFinite(Variant.RecoveryAfterImpact)
			|| Variant.RecoveryAfterImpact < 0.0f
			|| MaximumActiveDuration + KINDA_SMALL_NUMBER
				< Variant.ImpactDelay + Variant.RecoveryAfterImpact)
		{
			return false;
		}
	}

	return true;
}

void USovGameplayAbility_DominionHoundBite::PrepareAttack()
{
	TArray<int32> CandidateIndices;
	for (int32 Index = 0; Index < BiteVariants.Num(); ++Index)
	{
		if (IsValid(BiteVariants[Index].Montage.Get()))
		{
			CandidateIndices.Add(Index);
		}
	}
	if (CandidateIndices.IsEmpty())
	{
		for (int32 Index = 0; Index < BiteVariants.Num(); ++Index)
		{
			CandidateIndices.Add(Index);
		}
	}
	if (CandidateIndices.Num() > 1)
	{
		CandidateIndices.Remove(LastSelectedVariantIndex);
	}

	const int32 ChosenCandidate = CandidateIndices.IsEmpty()
		? 0
		: FMath::RandRange(0, CandidateIndices.Num() - 1);
	const int32 ChosenIndex = CandidateIndices.IsValidIndex(ChosenCandidate)
		? CandidateIndices[ChosenCandidate]
		: 0;
	LastSelectedVariantIndex = ChosenIndex;
	const FSovDominionHoundBiteVariant& Variant = BiteVariants[ChosenIndex];
	ConfigureActiveAttack(
		Variant.Montage.Get(),
		Variant.PlayRate,
		Variant.StartSection,
		Variant.ImpactDelay,
		Variant.RecoveryAfterImpact);
}

USovGameplayAbility_DominionHoundHornCharge::
	USovGameplayAbility_DominionHoundHornCharge()
{
	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	InputTag = NarrativeTags.Narrative_Input_Ability1;
	AbilityIdentityTag = SovTags.Ability_NPC_DominionHound_HornCharge;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(AbilityIdentityTag);
	AssetTags.AddTag(NarrativeTags.Ability_MeleeAttack);
	AssetTags.AddTag(NarrativeTags.Ability_DamageType_Melee);
	AssetTags.AddTag(NarrativeTags.Ability_DamageType_Heavy);
	SetAssetTags(AssetTags);
	ActivationOwnedTags.AddTag(NarrativeTags.State_Movement_PostponePathUpdates);

	DamageChannels.AddTag(SovTags.Damage_Channel_Kinetic);
	DamageChannels.AddTag(SovTags.Damage_Channel_Edge);
	AttackClassifications.AddTag(SovTags.Damage_GuardClass_Heavy);
	DamageEffectClass = USovGameplayEffect_DominionHoundDamage::StaticClass();
	DamageAmount = 30.0f;
	PoiseDamageAmount = 38.0f;
	MinimumAttackRange = 375.0f;
	MaximumAttackRange = 1800.0f;
	TraceSocketName = TEXT("HornSocket");
	FallbackTraceOffset = FVector(110.0f, 0.0f, 60.0f);
	TraceReach = 150.0f;
	TraceRadius = 65.0f;
	ImpactDelay = 0.45f;
	RecoveryAfterImpact = 0.65f;
	CooldownDuration = 3.0f;
	MaximumActiveDuration = 2.25f;
	DefaultBotAttackFrequency = 3.2f;
	DefaultBotAttackRange = 1650.0f;
}

bool USovGameplayAbility_DominionHoundHornCharge::
	HasRequiredAttackConfiguration() const
{
	return Super::HasRequiredAttackConfiguration()
		&& FMath::IsFinite(ChargeSpeed)
		&& ChargeSpeed >= 0.0f
		&& (!bUseNativeMovement || ChargeSpeed > KINDA_SMALL_NUMBER)
		&& FMath::IsFinite(MaximumChargeDuration)
		&& MaximumChargeDuration >= 0.05f
		&& FMath::IsFinite(MaximumChargeDistance)
		&& MaximumChargeDistance >= 0.0f
		&& (!bUseNativeMovement
			|| MaximumChargeDistance > KINDA_SMALL_NUMBER)
		&& FMath::IsFinite(MovementSweepInterval)
		&& MovementSweepInterval >= 0.005f
		&& MaximumActiveDuration + KINDA_SMALL_NUMBER
			>= ImpactDelay + MaximumChargeDuration + RecoveryAfterImpact;
}

void USovGameplayAbility_DominionHoundHornCharge::BeginAttackPayload()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UWorld* World = GetWorld();
	if (!IsValid(Character) || !IsValid(World))
	{
		CancelHoundAttack();
		return;
	}

	ChargeDirection = ResolveAttackDirection(Character->GetActorLocation());
	ChargeDirection.Z = 0.0f;
	ChargeDirection = ChargeDirection.GetSafeNormal();
	if (ChargeDirection.IsNearlyZero())
	{
		ChargeDirection = Character->GetActorForwardVector().GetSafeNormal2D();
	}
	Character->SetActorRotation(ChargeDirection.Rotation());
	ChargeStartLocation = Character->GetActorLocation();
	PreviousProbeLocation = ResolveAttackProbeLocation();
	ChargeStartTime = World->GetTimeSeconds();

	if (bUseNativeMovement)
	{
		UCharacterMovementComponent* Movement =
			Character->GetCharacterMovement();
		if (!IsValid(Movement))
		{
			CancelHoundAttack();
			return;
		}
		if (!Movement->IsMovingOnGround())
		{
			CancelHoundAttack();
			return;
		}
		if (AAIController* AIController =
			Cast<AAIController>(GetOwningController()))
		{
			AIController->StopMovement();
		}
		OwnedMovementComponent = Movement;
		SavedMaxWalkSpeed = Movement->MaxWalkSpeed;
		Movement->MaxWalkSpeed = FMath::Max(ChargeSpeed, 1.0f);
		Movement->StopMovementImmediately();
		Movement->Velocity = ChargeDirection * FMath::Max(ChargeSpeed, 0.0f);
		bOwnsChargeMovement = true;
	}

	World->GetTimerManager().SetTimer(
		ChargeUpdateTimerHandle,
		this,
		&ThisClass::UpdateCharge,
		FMath::Max(MovementSweepInterval, 0.005f),
		true);
	UpdateCharge();
}

void USovGameplayAbility_DominionHoundHornCharge::StopOwnedMovement()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeUpdateTimerHandle);
	}

	if (bOwnsChargeMovement)
	{
		if (UCharacterMovementComponent* Movement =
			OwnedMovementComponent.Get())
		{
			Movement->StopMovementImmediately();
			Movement->MaxWalkSpeed = SavedMaxWalkSpeed;
		}
	}
	OwnedMovementComponent.Reset();
	bOwnsChargeMovement = false;
	ChargeStartTime = 0.0;
}

void USovGameplayAbility_DominionHoundHornCharge::UpdateCharge()
{
	if (!CanContinueAttackPayload())
	{
		CancelHoundAttack();
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!IsValid(Avatar) || !IsValid(World))
	{
		CancelHoundAttack();
		return;
	}

	if (bUseNativeMovement)
	{
		UCharacterMovementComponent* Movement =
			OwnedMovementComponent.Get();
		if (!bOwnsChargeMovement || !IsValid(Movement))
		{
			CancelHoundAttack();
			return;
		}
		if (Movement->IsFalling())
		{
			FinishAttackPayload();
			return;
		}
		Movement->Velocity = ChargeDirection * FMath::Max(ChargeSpeed, 0.0f);
	}

	const FVector CurrentProbeLocation = ResolveAttackProbeLocation();
	bool bBlockedByWorld = false;
	const bool bHitTarget = ApplyAttackSweep(
		PreviousProbeLocation,
		CurrentProbeLocation
			+ ChargeDirection * FMath::Max(TraceReach, 0.0f),
		TraceRadius,
		bBlockedByWorld);
	PreviousProbeLocation = CurrentProbeLocation;

	const float Elapsed = static_cast<float>(
		World->GetTimeSeconds() - ChargeStartTime);
	const float Travelled = FVector::Dist2D(
		ChargeStartLocation,
		Avatar->GetActorLocation());
	if (bHitTarget
		|| bBlockedByWorld
		|| Elapsed + KINDA_SMALL_NUMBER >= MaximumChargeDuration
		|| Travelled + KINDA_SMALL_NUMBER >= MaximumChargeDistance)
	{
		FinishAttackPayload();
	}
}

USovGameplayAbility_DominionHoundPounce::
	USovGameplayAbility_DominionHoundPounce()
{
	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	InputTag = NarrativeTags.Narrative_Input_Ability2;
	AbilityIdentityTag = SovTags.Ability_NPC_DominionHound_Pounce;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(AbilityIdentityTag);
	AssetTags.AddTag(NarrativeTags.Ability_MeleeAttack);
	AssetTags.AddTag(NarrativeTags.Ability_DamageType_Melee);
	AssetTags.AddTag(NarrativeTags.Ability_DamageType_Heavy);
	SetAssetTags(AssetTags);
	ActivationOwnedTags.AddTag(NarrativeTags.State_Movement_PostponePathUpdates);

	DamageChannels.AddTag(SovTags.Damage_Channel_Kinetic);
	AttackClassifications.AddTag(SovTags.Damage_GuardClass_Heavy);
	DamageEffectClass = USovGameplayEffect_DominionHoundDamage::StaticClass();
	DamageAmount = 26.0f;
	PoiseDamageAmount = 32.0f;
	MinimumAttackRange = 500.0f;
	MaximumAttackRange = 1600.0f;
	TraceSocketName = TEXT("MouthSocket");
	FallbackTraceOffset = FVector(90.0f, 0.0f, 60.0f);
	TraceReach = 120.0f;
	TraceRadius = 75.0f;
	ImpactDelay = 0.40f;
	RecoveryAfterImpact = 0.70f;
	CooldownDuration = 4.5f;
	MaximumActiveDuration = 2.25f;
	DefaultBotAttackFrequency = 4.8f;
	DefaultBotAttackRange = 1500.0f;
}

bool USovGameplayAbility_DominionHoundPounce::
	HasRequiredAttackConfiguration() const
{
	return Super::HasRequiredAttackConfiguration()
		&& FMath::IsFinite(PounceFlightDuration)
		&& PounceFlightDuration >= 0.1f
		&& FMath::IsFinite(MaximumLaunchSpeed)
		&& MaximumLaunchSpeed >= 0.0f
		&& (!bUseNativeMovement || MaximumLaunchSpeed > KINDA_SMALL_NUMBER)
		&& FMath::IsFinite(TargetHeightOffset)
		&& FMath::IsFinite(MovementSweepInterval)
		&& MovementSweepInterval >= 0.005f
		&& MaximumActiveDuration + KINDA_SMALL_NUMBER
			>= ImpactDelay + PounceFlightDuration + RecoveryAfterImpact;
}

void USovGameplayAbility_DominionHoundPounce::BeginAttackPayload()
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	AActor* Target = GetAttackTarget();
	UWorld* World = GetWorld();
	if (!IsValid(Character) || !IsValid(Target) || !IsValid(World))
	{
		CancelHoundAttack();
		return;
	}

	const FVector StartLocation = Character->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation()
		+ FVector(0.0f, 0.0f, TargetHeightOffset);
	PounceDirection = TargetLocation - StartLocation;
	PounceDirection.Z = 0.0f;
	PounceDirection = PounceDirection.GetSafeNormal();
	if (PounceDirection.IsNearlyZero())
	{
		PounceDirection = Character->GetActorForwardVector().GetSafeNormal2D();
	}
	Character->SetActorRotation(PounceDirection.Rotation());
	PreviousProbeLocation = ResolveAttackProbeLocation();
	PounceStartTime = World->GetTimeSeconds();

	if (bUseNativeMovement)
	{
		UCharacterMovementComponent* Movement =
			Character->GetCharacterMovement();
		if (!IsValid(Movement))
		{
			CancelHoundAttack();
			return;
		}
		if (AAIController* AIController =
			Cast<AAIController>(GetOwningController()))
		{
			AIController->StopMovement();
		}
		OwnedMovementComponent = Movement;
		SavedAirControl = Movement->AirControl;
		Movement->AirControl = 0.0f;
		Movement->StopMovementImmediately();

		const float FlightTime = FMath::Max(PounceFlightDuration, 0.1f);
		FVector LaunchVelocity = (TargetLocation - StartLocation) / FlightTime;
		LaunchVelocity.Z -= 0.5f * Movement->GetGravityZ() * FlightTime;
		LaunchVelocity = LaunchVelocity.GetClampedToMaxSize(
			FMath::Max(MaximumLaunchSpeed, 1.0f));
		Character->LaunchCharacter(LaunchVelocity, true, true);
		bOwnsPounceMovement = true;
	}

	World->GetTimerManager().SetTimer(
		PounceUpdateTimerHandle,
		this,
		&ThisClass::UpdatePounce,
		FMath::Max(MovementSweepInterval, 0.005f),
		true);
	UpdatePounce();
}

void USovGameplayAbility_DominionHoundPounce::StopOwnedMovement()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PounceUpdateTimerHandle);
	}

	if (bOwnsPounceMovement)
	{
		if (ACharacter* Character =
			Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			// LaunchCharacter queues velocity until CharacterMovement ticks.
			// Clear that queue as well as live velocity when cancellation races
			// the launch frame.
			Character->LaunchCharacter(FVector::ZeroVector, true, true);
		}
		if (UCharacterMovementComponent* Movement =
			OwnedMovementComponent.Get())
		{
			Movement->StopMovementImmediately();
			Movement->AirControl = SavedAirControl;
		}
	}
	OwnedMovementComponent.Reset();
	bOwnsPounceMovement = false;
	PounceStartTime = 0.0;
}

void USovGameplayAbility_DominionHoundPounce::UpdatePounce()
{
	if (!CanContinueAttackPayload())
	{
		CancelHoundAttack();
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(GetAvatarActorFromActorInfo()))
	{
		CancelHoundAttack();
		return;
	}
	if (bUseNativeMovement
		&& (!bOwnsPounceMovement || !OwnedMovementComponent.IsValid()))
	{
		CancelHoundAttack();
		return;
	}

	const FVector CurrentProbeLocation = ResolveAttackProbeLocation();
	bool bBlockedByWorld = false;
	const bool bHitTarget = ApplyAttackSweep(
		PreviousProbeLocation,
		CurrentProbeLocation
			+ PounceDirection * FMath::Max(TraceReach, 0.0f),
		TraceRadius,
		bBlockedByWorld);
	PreviousProbeLocation = CurrentProbeLocation;

	const float Elapsed = static_cast<float>(
		World->GetTimeSeconds() - PounceStartTime);
	if (bHitTarget
		|| bBlockedByWorld
		|| Elapsed + KINDA_SMALL_NUMBER >= PounceFlightDuration)
	{
		FinishAttackPayload();
	}
}
