// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Abilities/SovGameplayAbility_SeleneEcho.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Characters/SovDroneNPCBase.h"
#include "CollisionQueryParams.h"
#include "Combat/SovAxiomPulseMath.h"
#include "Components/SovSeleneEchoGenerationComponent.h"
#include "Effects/SovGameplayEffect_AxiomNullPulse.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeDamageExecCalc.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Items/WeaponItem.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Character/NarrativeCharacterVisual.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Weapons/NarrativeProjectile.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovAxiomPulse, Log, All);

namespace
{
	AActor* ResolveAxiomActor(AActor* Actor)
	{
		TSet<AActor*> Visited;
		for (int32 Depth = 0; IsValid(Actor) && Depth < 8 && !Visited.Contains(Actor); ++Depth)
		{
			Visited.Add(Actor);
			// Projectiles are payloads, not their owner as a second target.
			if (Actor->IsA<ANarrativeProjectile>()) { return nullptr; }
			if (Actor->FindComponentByClass<USovCommandLinkComponent>()) { return Actor; }
			if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor))
			{
				return IsValid(ASC->GetAvatarActor()) ? ASC->GetAvatarActor() : Actor;
			}
			if (const INarrativeCharacterOwner* Provider = Cast<INarrativeCharacterOwner>(Actor))
			{
				if (ANarrativeCharacter* Character = Provider->GetNarrativeCharacter()) { return Character; }
			}
			Actor = Actor->GetOwner();
		}
		return nullptr;
	}

	bool IsAxiomImmune(const UAbilitySystemComponent* ASC)
	{
		if (!IsValid(ASC)) { return false; }
		const FSovGameplayTags& Tags = FSovGameplayTags::Get();
		return ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable)
			|| ASC->HasMatchingGameplayTag(Tags.State_Invulnerable)
			|| ASC->HasMatchingGameplayTag(Tags.State_Damage_Immune)
			|| ASC->HasMatchingGameplayTag(Tags.Damage_Immunity_All)
			|| ASC->HasMatchingGameplayTag(Tags.Damage_Immunity_Disruption);
	}

	bool IsAxiomAlive(const UAbilitySystemComponent* ASC)
	{
		if (!IsValid(ASC)) { return true; }
		return !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
			&& !ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
			&& (!ASC->GetSet<UNarrativeAttributeSetBase>()
				|| ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > KINDA_SMALL_NUMBER);
	}

	void IgnoreAxiomSource(FCollisionQueryParams& Params, AActor* Avatar)
	{
		if (!IsValid(Avatar)) { return; }
		Params.AddIgnoredActor(Avatar);
		TArray<AActor*> Attached;
		Avatar->GetAttachedActors(Attached, true, true);
		Params.AddIgnoredActors(Attached);
		if (const ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(Avatar))
		{
			if (ANarrativeCharacterVisual* Visual = Character->GetCharacterVisual())
			{
				Params.AddIgnoredActor(Visual);
				Visual->GetAttachedActors(Attached, true, true);
				Params.AddIgnoredActors(Attached);
			}
		}
	}

	TSubclassOf<UGameplayEffect> SafeAxiomEffect(
		TSubclassOf<UGameplayEffect> Requested, TSubclassOf<UGameplayEffect> Native)
	{
		// Legacy effects may contain additional GE components or grants as well as
		// visible modifiers. Only the known, native shells may alter this transaction.
		// Keep serialized asset references intact, but fail safely to the native GE.
		return Requested == Native ? Requested : Native;
	}
}

USovGameplayAbility_SeleneAxiomNullPulse::USovGameplayAbility_SeleneAxiomNullPulse()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 30.0f;
	EchoCost = 30.0f;
	EchoSpendTag = Tags.Ability_Echo_Selene_AxiomNullPulse;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Selene_AxiomNullPulse;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability2;
	ActivationRequiredTags.AddTag(Tags.Character_Player_Selene);
	ActivationBlockedTags.AddTag(Tags.Character_Player_Tarrik);
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_Weapon_Equipping);
	WeaponFamily = ESovSeleneEchoWeaponFamily::Axiom;
	ShieldDisruptionDamageEffectClass = USovGameplayEffect_AxiomNullPulseDamage::StaticClass();
	ShieldRechargeBlockEffectClass = USovGameplayEffect_AxiomShieldSuppression::StaticClass();
	DeviceDisableEffectClass = USovGameplayEffect_AxiomDeviceDisable::StaticClass();
	AbilityDisplayName = NSLOCTEXT("SovSeleneEcho", "AxiomNullPulseName", "Axiom Null Pulse");
	AbilityDescription = NSLOCTEXT("SovSeleneEcho", "AxiomNullPulseDescription",
		"Charge Axiom and release a directed EMP pulse that collapses Shields, suppresses recharge, and disables eligible combat systems.");
}

bool USovGameplayAbility_SeleneAxiomNullPulse::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UWeaponItem* Weapon = ActorInfo ? Cast<UWeaponItem>(GetSourceObject(Handle, ActorInfo)) : nullptr;
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	if (!IsValid(ASC) || !IsValid(Weapon) || !Weapon->IsWielded()
		|| !ASC->HasMatchingGameplayTag(Tags.Character_Player_Selene)
		|| ASC->HasMatchingGameplayTag(Tags.Character_Player_Tarrik)
		|| ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Weapon_Equipping))
	{
		return false;
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

bool USovGameplayAbility_SeleneAxiomNullPulse::HasRequiredPayloadConfiguration() const
{
	const auto Positive = [](float Value) { return FMath::IsFinite(Value) && Value > KINDA_SMALL_NUMBER; };
	return Positive(FullChargeDuration) && Positive(MinimumPulseRange)
		&& Positive(MaximumPulseRange) && MaximumPulseRange >= MinimumPulseRange
		&& FMath::IsFinite(MinimumPulseHalfAngleDegrees) && MinimumPulseHalfAngleDegrees >= 0.0f
		&& FMath::IsFinite(MaximumPulseHalfAngleDegrees) && MaximumPulseHalfAngleDegrees <= 90.0f
		&& MaximumPulseHalfAngleDegrees >= MinimumPulseHalfAngleDegrees
		&& Positive(MinimumShieldSuppressionDuration) && Positive(MaximumShieldSuppressionDuration)
		&& MaximumShieldSuppressionDuration >= MinimumShieldSuppressionDuration
		&& FMath::IsFinite(PostPulseRecovery) && PostPulseRecovery >= 0.0f
		&& FMath::IsFinite(MaximumActiveDuration)
		&& MaximumActiveDuration > FullChargeDuration + PostPulseRecovery;
}

void USovGameplayAbility_SeleneAxiomNullPulse::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ClearAxiomTasksAndTimers();
	const uint32 Epoch = ++ActivationEpoch;
	ChargeStartWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	bNativeLifecycleReady = false;
	bPulseReleased = false;
	ReleasedChargeAlpha = 0.0f;
	AuthorizedCommandNode.Reset();
	ExpectedWeapon = ActorInfo ? Cast<UWeaponItem>(GetSourceObject(Handle, ActorInfo)) : nullptr;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (ActivationEpoch != Epoch || !IsActive()) { return; }
	bNativeLifecycleReady = true;
	if (!CanReleaseAxiomPulse()) { FinishEchoAbility(true); return; }

	if ((ShieldDisruptionDamageEffectClass && ShieldDisruptionDamageEffectClass != USovGameplayEffect_AxiomNullPulseDamage::StaticClass())
		|| (ShieldRechargeBlockEffectClass && ShieldRechargeBlockEffectClass != USovGameplayEffect_AxiomShieldSuppression::StaticClass())
		|| (DeviceDisableEffectClass && DeviceDisableEffectClass != USovGameplayEffect_AxiomDeviceDisable::StaticClass()))
	{
		UE_LOG(LogSovAxiomPulse, Warning, TEXT("%s uses legacy Axiom effect overrides. Native safe shells are used; migrate Blueprint gameplay to presentation-only hooks."), *GetNameSafe(this));
	}

	// WaitInputRelease replicates the input event, never a client charge magnitude.
	ReleaseTaskEpoch = Epoch;
	AxiomInputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	if (!AxiomInputReleaseTask) { FinishEchoAbility(true); return; }
	AxiomInputReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleAxiomInputReleased);
	AxiomInputReleaseTask->ReadyForActivation();
	if (!IsCurrentAxiomActivation(Epoch) || bPulseReleased) { return; }
	if (ActorInfo->IsNetAuthority())
	{
		const float Remaining = FMath::Max(FullChargeDuration - static_cast<float>(GetWorld()->GetTimeSeconds() - ChargeStartWorldTime), KINDA_SMALL_NUMBER);
		GetWorld()->GetTimerManager().SetTimer(AxiomFullChargeTimer,
			FTimerDelegate::CreateWeakLambda(this, [this, Epoch]()
			{
				if (IsCurrentAxiomActivation(Epoch)) { ReleaseAxiomNullPulseFromAim(); }
			}), Remaining, false);
	}
}

void USovGameplayAbility_SeleneAxiomNullPulse::EndAbility(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo)) { return; }
	++ActivationEpoch;
	bNativeLifecycleReady = false;
	if (ScopeLockCount > 0)
	{
		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}
	AuthorizedCommandNode.Reset();
	ExpectedWeapon.Reset();
	ClearAxiomTasksAndTimers();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USovGameplayAbility_SeleneAxiomNullPulse::ClearAxiomTasksAndTimers()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AxiomFullChargeTimer);
		GetWorld()->GetTimerManager().ClearTimer(AxiomRecoveryTimer);
	}
	if (AxiomInputReleaseTask)
	{
		AxiomInputReleaseTask->OnRelease.RemoveAll(this);
		AxiomInputReleaseTask->EndTask();
		AxiomInputReleaseTask = nullptr;
	}
	ReleaseTaskEpoch = 0;
}

bool USovGameplayAbility_SeleneAxiomNullPulse::IsCurrentAxiomActivation(uint32 Epoch) const
{
	return ActivationEpoch == Epoch && IsActive() && bNativeLifecycleReady;
}

bool USovGameplayAbility_SeleneAxiomNullPulse::ContinueAxiomRelease(uint32 Epoch)
{
	if (!IsCurrentAxiomActivation(Epoch)) { return false; }
	if (!CanReleaseAxiomPulse())
	{
		// A synchronous target callback may change possession/equipment without
		// ending GAS. Stop the remaining payload and clean up this activation now.
		FinishEchoAbility(true);
		return false;
	}
	return true;
}

float USovGameplayAbility_SeleneAxiomNullPulse::GetAxiomChargeAlpha() const
{
	if (bPulseReleased) { return ReleasedChargeAlpha; }
	return IsActive() && GetWorld()
		? SovAxiomPulse::ChargeAlpha(static_cast<float>(GetWorld()->GetTimeSeconds() - ChargeStartWorldTime), FullChargeDuration)
		: 0.0f;
}

bool USovGameplayAbility_SeleneAxiomNullPulse::CanReleaseAxiomPulse() const
{
	if (!CurrentActorInfo || !IsCurrentEchoExecutionValid() || !bNativeLifecycleReady || !GetWorld()
		|| !HasRequiredPayloadConfiguration()) { return false; }
	const UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	ANarrativeCharacter* Avatar = Cast<ANarrativeCharacter>(CurrentActorInfo->AvatarActor.Get());
	const UWeaponItem* Weapon = ExpectedWeapon.Get();
	if (!IsValid(ASC) || !IsValid(Avatar) || !IsValid(Weapon) || !Weapon->IsWielded()
		|| ASC->GetAvatarActor() != Avatar
		|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Avatar) != ASC
		|| !IsAxiomAlive(ASC)
		|| (Avatar->GetWeapon(true) != Weapon && Avatar->GetWeapon(false) != Weapon)
		|| !MeetsWeaponRequirement(CurrentSpecHandle, CurrentActorInfo)) { return false; }
	const FSovGameplayTags& Sov = FSovGameplayTags::Get();
	const FNarrativeGameplayTags& Narrative = FNarrativeGameplayTags::Get();
	FGameplayTagContainer Blocking;
	Blocking.AddTag(Narrative.State_IsDead);
	Blocking.AddTag(Narrative.State_Interacting);
	Blocking.AddTag(Narrative.State_SequencerControlled);
	Blocking.AddTag(Narrative.State_Movement_Ragdoll);
	Blocking.AddTag(Narrative.State_Weapon_Equipping);
	Blocking.AddTag(Sov.State_Fatal);
	Blocking.AddTag(Sov.State_Poise_Broken);
	Blocking.AddTag(Sov.State_Guard_Broken);
	Blocking.AddTag(Sov.State_Status_Frozen);
	Blocking.AddTag(Sov.State_Guarding);
	Blocking.AddTag(Sov.Character_Player_Tarrik);
	return ASC->HasMatchingGameplayTag(Sov.Character_Player_Selene)
		&& !ASC->HasAnyMatchingGameplayTags(Blocking)
		&& ASC->GetGameplayTagCount(Narrative.State_Busy) <= 1;
}

void USovGameplayAbility_SeleneAxiomNullPulse::HandleAxiomInputReleased(float TimeHeld)
{
	static_cast<void>(TimeHeld);
	if (IsCurrentAxiomActivation(ReleaseTaskEpoch) && CurrentActorInfo->IsNetAuthority())
	{
		ReleaseAxiomNullPulseFromAim();
	}
}

bool USovGameplayAbility_SeleneAxiomNullPulse::HasAxiomLineOfSight(AActor* Target, const FVector& TargetPoint) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Target) || !IsValid(Avatar) || !GetWorld()) { return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovAxiomPulseVisibility), false);
	IgnoreAxiomSource(Query, Avatar);
	// Skip only our own ownership/visual proxies. Other enemies can obstruct a lane.
	for (int32 Attempt = 0; Attempt < 12; ++Attempt)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, ReleaseOrigin, TargetPoint, ECC_Visibility, Query)) { return true; }
		AActor* HitActor = Hit.GetActor();
		AActor* Canonical = ResolveAxiomActor(HitActor);
		if (Canonical == Target || HitActor == Target) { return true; }
		if (IsValid(HitActor) && (Canonical == Avatar || HitActor->IsOwnedBy(Avatar)))
		{
			Query.AddIgnoredActor(HitActor);
			continue;
		}
		return false;
	}
	return false;
}

bool USovGameplayAbility_SeleneAxiomNullPulse::IsAxiomTargetInPulse(AActor* Target) const
{
	if (!IsValid(Target) || Target->GetWorld() != GetWorld() || Target == GetAvatarActorFromActorInfo()) { return false; }
	const auto IsVisiblePointInsidePulse = [this, Target](const FVector& Point)
	{
		const FVector Offset = Point - ReleaseOrigin;
		return SovAxiomPulse::ContainsPoint(Offset.X, Offset.Y, Offset.Z,
			ReleaseDirection.X, ReleaseDirection.Y, ReleaseDirection.Z, ReleasedRange, ReleasedHalfAngle)
			&& HasAxiomLineOfSight(Target, Point);
	};
	const FVector Center = Target->GetActorLocation();
	if (IsVisiblePointInsidePulse(Center)) { return true; }
	if (const ACharacter* Character = Cast<ACharacter>(Target))
	{
		FVector Eyes;
		FRotator EyeRotation;
		Character->GetActorEyesViewPoint(Eyes, EyeRotation);
		// A horizontal near-range shot must not miss solely because the cone
		// originates at the shooter's eyes and the target capsule center is lower.
		// Each candidate point passes BOTH angle/range and visibility itself.
		if (!Eyes.ContainsNaN() && FVector::DistSquared(Eyes, Center) <= FMath::Square(400.0f))
		{
			return IsVisiblePointInsidePulse(Eyes);
		}
	}
	return false;
}

bool USovGameplayAbility_SeleneAxiomNullPulse::IsAxiomTargetEligible(
	AActor* Target, UAbilitySystemComponent* TargetASC) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar) || !IsValid(Target) || Target == Avatar || Target->IsOwnedBy(Avatar)
		|| Target->IsActorBeingDestroyed() || !IsAxiomAlive(TargetASC) || IsAxiomImmune(TargetASC)) { return false; }
	if (!TargetASC)
	{
		// The link validates live participant hostility; an unowned world prop is not a device.
		return Target->FindComponentByClass<USovCommandLinkComponent>() != nullptr;
	}
	const INarrativeTeamAgentInterface* Team = Cast<INarrativeTeamAgentInterface>(Avatar);
	return Team && Team->GetTeamAttitudeTowards(*Target) == ETeamAttitude::Hostile;
}

bool USovGameplayAbility_SeleneAxiomNullPulse::ApplyAxiomShieldCollapse(UAbilitySystemComponent* TargetASC)
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || !bPulseReleased
		|| !CanReleaseAxiomPulse() || !IsValid(TargetASC)
		|| !IsAxiomTargetEligible(TargetASC->GetAvatarActor(), TargetASC)) { return false; }
	UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
	if (!TargetASC->GetSet<UNarrativeAttributeSetBase>()) { return false; }
	const float Shield = TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute());
	if (!FMath::IsFinite(Shield) || Shield <= KINDA_SMALL_NUMBER) { return false; }
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.SetAbility(this);
	Context.AddSourceObject(ExpectedWeapon.Get());
	Context.AddOrigin(ReleaseOrigin);
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(
		SafeAxiomEffect(ShieldDisruptionDamageEffectClass, USovGameplayEffect_AxiomNullPulseDamage::StaticClass()), GetAbilityLevel(), Context);
	if (!Spec.IsValid()) { return false; }
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	Spec.Data->AddDynamicAssetTag(EchoSpendTag);
	Spec.Data->AddDynamicAssetTag(Tags.Damage_Channel_Disruption);
	Spec.Data->AddDynamicAssetTag(Tags.Damage_AlreadyResolved);
	Spec.Data->AddDynamicAssetTag(Tags.Damage_BypassGuard);
	Spec.Data->AddDynamicAssetTag(Tags.Damage_BypassDeflection);
	Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, Shield);
	Spec.Data->SetSetByCallerMagnitude(Tags.SetByCaller_Damage_ShieldCoefficient, 1.0f);
	Spec.Data->SetSetByCallerMagnitude(Tags.SetByCaller_Damage_HealthCoefficient, 0.0f);
	Spec.Data->SetSetByCallerMagnitude(Tags.SetByCaller_Damage_PoiseDamage, 0.0f);
	Spec.Data->SetSetByCallerMagnitude(Tags.SetByCaller_Damage_ShieldBypassRatio, 0.0f);
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	return IsValid(TargetASC) && TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()) < Shield;
}

bool USovGameplayAbility_SeleneAxiomNullPulse::ApplyAxiomDurationEffect(
	UAbilitySystemComponent* TargetASC, TSubclassOf<UGameplayEffect> EffectClass,
	FGameplayTag GrantedTag, float Duration)
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || !bPulseReleased
		|| !CanReleaseAxiomPulse() || !IsValid(TargetASC) || !GrantedTag.IsValid()
		|| !FMath::IsFinite(Duration) || Duration <= KINDA_SMALL_NUMBER
		|| !IsAxiomTargetEligible(TargetASC->GetAvatarActor(), TargetASC)) { return false; }
	UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.SetAbility(this);
	Context.AddSourceObject(ExpectedWeapon.Get());
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(EffectClass, GetAbilityLevel(), Context);
	if (!Spec.IsValid()) { return false; }
	Spec.Data->AddDynamicAssetTag(EchoSpendTag);
	Spec.Data->DynamicGrantedTags.AddTag(GrantedTag);
	Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Duration, Duration);
	// Explicit duration also supports a fresh native shell before capture/recalculation.
	Spec.Data->SetDuration(Duration, true);
	const FActiveGameplayEffectHandle Applied = SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	return Applied.IsValid() && IsValid(TargetASC) && TargetASC->HasMatchingGameplayTag(GrantedTag);
}

bool USovGameplayAbility_SeleneAxiomNullPulse::IsAxiomDeviceEligible(
	AActor* Target, UAbilitySystemComponent* TargetASC) const
{
	if (!IsValid(Target) || !IsValid(TargetASC)) { return false; }
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	FGameplayTagContainer OwnedTags;
	TargetASC->GetOwnedGameplayTags(OwnedTags);
	if (TargetASC->HasMatchingGameplayTag(Tags.Status_Immunity_DeviceDisable)
		|| OwnedTags.HasTagExact(Tags.Status_Immunity)
		|| (!bAllowBossDeviceDisable && TargetASC->HasMatchingGameplayTag(Tags.Character_Enemy_Boss))) { return false; }
	return Target->IsA<ASovDroneNPCBase>() || AdditionalDisableTargetClasses.ContainsByPredicate(
		[Target](const TSubclassOf<AActor>& Class) { return Class && Target->IsA(Class); });
}

ESovCommandLinkSeverResolution USovGameplayAbility_SeleneAxiomNullPulse::TrySeverAxiomCommandLink(
	AActor* CommandNode, FSovCommandLinkSeverResult& OutResult)
{
	OutResult = FSovCommandLinkSeverResult();
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || !bPulseReleased
		|| !CanReleaseAxiomPulse() || !IsValid(CommandNode) || AuthorizedCommandNode.Get() != CommandNode) { return ESovCommandLinkSeverResolution::Invalid; }
	AuthorizedCommandNode.Reset(); // consume before callbacks can re-enter
	UAbilitySystemComponent* NodeASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(CommandNode);
	if (!IsAxiomTargetEligible(CommandNode, NodeASC) || !IsAxiomTargetInPulse(CommandNode)) { return ESovCommandLinkSeverResolution::Invalid; }
	// A command actor can own independent links. Keep a fixed, deterministic
	// candidate list; a severed first component must not hide another active link.
	struct FCandidate
	{
		TWeakObjectPtr<USovCommandLinkComponent> Link;
		TWeakObjectPtr<AActor> Source;
		FGuid InstanceId;
		FName LinkId;
		FName ComponentName;
	};
	TArray<USovCommandLinkComponent*> Components;
	CommandNode->GetComponents(Components);
	TArray<FCandidate> Candidates;
	for (USovCommandLinkComponent* Link : Components)
	{
		if (IsValid(Link) && Link->IsRegistered() && Link->GetOwner() == CommandNode
			&& Link->GetWorld() == GetWorld() && Link->IsCommandLinkActive()
			&& Link->GetLinkInstanceId().IsValid() && !Link->GetLinkId().IsNone())
		{
			Candidates.Add({Link, Link->GetCommandSource(), Link->GetLinkInstanceId(), Link->GetLinkId(), Link->GetFName()});
		}
	}
	Candidates.Sort([](const FCandidate& Left, const FCandidate& Right)
	{
		return Left.LinkId == Right.LinkId ? Left.ComponentName.LexicalLess(Right.ComponentName) : Left.LinkId.LexicalLess(Right.LinkId);
	});
	const uint32 Epoch = ActivationEpoch;
	const TWeakObjectPtr<AActor> OriginalNode = CommandNode;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	const TWeakObjectPtr<AActor> OriginalAvatar = Avatar;
	const TWeakObjectPtr<UAbilitySystemComponent> OriginalASC = CurrentActorInfo->AbilitySystemComponent;
	ESovCommandLinkSeverResolution Result = ESovCommandLinkSeverResolution::Inactive;
	for (const FCandidate& Candidate : Candidates)
	{
		if (!ContinueAxiomRelease(Epoch) || !OriginalNode.IsValid()
			|| OriginalNode->IsActorBeingDestroyed() || OriginalNode->GetWorld() != GetWorld()) { return ESovCommandLinkSeverResolution::Invalid; }
		USovCommandLinkComponent* Link = Candidate.Link.Get();
		const auto IsCurrentCandidate = [&]()
		{
			return OriginalNode.IsValid() && !OriginalNode->IsActorBeingDestroyed()
				&& Candidate.Source.IsValid() && !Candidate.Source->IsActorBeingDestroyed()
				&& Candidate.Source->GetWorld() == GetWorld()
				&& IsValid(Link) && Link->IsRegistered() && Link->GetOwner() == OriginalNode.Get()
				&& OriginalNode->GetComponents().Contains(Link) && Link->GetWorld() == GetWorld()
				&& Link->IsCommandLinkActive() && Link->GetLinkInstanceId() == Candidate.InstanceId
				&& Link->GetLinkId() == Candidate.LinkId && Link->GetCommandSource() == Candidate.Source.Get();
		};
		if (!IsCurrentCandidate()) { continue; }
		NodeASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OriginalNode.Get());
		if (!IsAxiomTargetEligible(OriginalNode.Get(), NodeASC) || !IsAxiomTargetInPulse(OriginalNode.Get())
			|| !ContinueAxiomRelease(Epoch) || !IsCurrentCandidate()) { return ESovCommandLinkSeverResolution::Invalid; }
		UAbilitySystemComponent* CommandASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate.Source.Get());
		if (!ContinueAxiomRelease(Epoch) || !IsCurrentCandidate()) { return ESovCommandLinkSeverResolution::Invalid; }
		if (!IsAxiomAlive(CommandASC) || IsAxiomImmune(CommandASC)) { Result = ESovCommandLinkSeverResolution::Immune; continue; }
		// The native link remains the sole owner of immunity/hostility admission,
		// mutation, transaction identity and notifications. Rejections mint no proof.
		FSovCommandLinkSeverResult CandidateResult;
		Result = Link->TrySeverCommandLink(OriginalAvatar.Get(), CandidateResult);
		if (Result == ESovCommandLinkSeverResolution::NewlySevered)
		{
			OutResult = MoveTemp(CandidateResult);
			break; // At most one successful link, including callback cancellation.
		}
	}
	// Sever is already committed. A cosmetic/link listener cancelling this ability
	// must not erase its earned reward, but possession/death must never transfer it.
	if (Result == ESovCommandLinkSeverResolution::NewlySevered
		&& OriginalAvatar.IsValid() && OriginalASC.IsValid() && IsAxiomAlive(OriginalASC.Get())
		&& OriginalASC->GetAvatarActor() == OriginalAvatar.Get()
		&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OriginalAvatar.Get()) == OriginalASC.Get())
	{
		if (USovSeleneEchoGenerationComponent* Generator = OriginalAvatar->FindComponentByClass<USovSeleneEchoGenerationComponent>())
		{
			Generator->ConsumeCommandLinkSever(OutResult);
		}
	}
	return Result;
}

bool USovGameplayAbility_SeleneAxiomNullPulse::ReleaseAxiomNullPulseFromAim()
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || bPulseReleased || !bNativeLifecycleReady || !IsActive()) { return false; }
	if (!CanReleaseAxiomPulse()) { FinishEchoAbility(true); return false; }
	const uint32 Epoch = ActivationEpoch;
	ReleasedChargeAlpha = GetAxiomChargeAlpha();
	ReleasedRange = SovAxiomPulse::Lerp(MinimumPulseRange, MaximumPulseRange, ReleasedChargeAlpha);
	ReleasedHalfAngle = SovAxiomPulse::Lerp(MinimumPulseHalfAngleDegrees, MaximumPulseHalfAngleDegrees, ReleasedChargeAlpha);
	AActor* Avatar = GetAvatarActorFromActorInfo();
	FRotator Rotation;
	Avatar->GetActorEyesViewPoint(ReleaseOrigin, Rotation);
	if (const AController* Controller = GetOwningController()) { Rotation = Controller->GetControlRotation(); }
	ReleaseDirection = Rotation.Vector().GetSafeNormal();
	if (ReleaseOrigin.ContainsNaN() || ReleaseDirection.ContainsNaN() || ReleaseDirection.IsNearlyZero()
		|| FVector::DistSquared(ReleaseOrigin, Avatar->GetActorLocation()) > FMath::Square(400.0f))
	{
		FinishEchoAbility(true);
		return false;
	}
	bPulseReleased = true; // ownership precedes every target/effect/Blueprint callback
	ClearAxiomTasksAndTimers();
	// Broadphase for the release cone. IsAxiomTargetEligible admits exactly two kinds of
	// target: an actor with a live hostile ASC, or an actor carrying a USovCommandLinkComponent
	// ("an unowned world prop is not a device"). Neither can be reached only through
	// ECC_WorldStatic or ECC_PhysicsBody: static level geometry is never a live NPC or an
	// authored command source, and a physics body is either a ragdoll - already rejected by
	// IsAxiomAlive - or an inert prop. Querying them cost a full-radius sweep of every wall,
	// floor and piece of debris within MaximumPulseRange, plus ResolveAxiomActor's owner walk
	// per result, which is a visible hitch on a dressed level and would corrupt any FPS
	// measurement of the slice's signature ability.
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovAxiomPulseOverlap), false);
	IgnoreAxiomSource(Query, Avatar);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, ReleaseOrigin, FQuat::Identity,
		Objects, FCollisionShape::MakeSphere(ReleasedRange), Query);
	TSet<TWeakObjectPtr<AActor>> Targets;
	// One overlap actor can contribute many primitives. Resolve each distinct actor once so the
	// owner walk, component search and ASC lookup do not repeat per capsule, mesh and prop.
	TSet<AActor*> ResolvedOverlapActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OverlapActor = Overlap.GetActor();
		if (!IsValid(OverlapActor)) { continue; }
		bool bAlreadyResolved = false;
		ResolvedOverlapActors.Add(OverlapActor, &bAlreadyResolved);
		if (bAlreadyResolved) { continue; }
		if (AActor* Target = ResolveAxiomActor(OverlapActor)) { Targets.Add(Target); }
	}
	const float Suppression = SovAxiomPulse::Lerp(MinimumShieldSuppressionDuration, MaximumShieldSuppressionDuration, ReleasedChargeAlpha);
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	int32 ShieldTargets = 0, DisabledDevices = 0, SeveredLinks = 0;
	for (const TWeakObjectPtr<AActor>& WeakTarget : Targets)
	{
		if (!ContinueAxiomRelease(Epoch)) { return true; }
		AActor* Target = WeakTarget.Get();
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!IsAxiomTargetEligible(Target, TargetASC) || !IsAxiomTargetInPulse(Target)) { continue; }
		if (TargetASC)
		{
			if (ApplyAxiomShieldCollapse(TargetASC)) { ++ShieldTargets; }
			if (!ContinueAxiomRelease(Epoch)) { return true; }
			if (!IsValid(TargetASC) || !IsAxiomTargetEligible(Target, TargetASC)) { continue; }
			if (TargetASC->GetSet<UNarrativeAttributeSetBase>()
				&& TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxShieldAttribute()) > KINDA_SMALL_NUMBER)
			{
				ApplyAxiomDurationEffect(TargetASC,
					SafeAxiomEffect(ShieldRechargeBlockEffectClass, USovGameplayEffect_AxiomShieldSuppression::StaticClass()), Tags.State_Shield_RechargeBlocked, Suppression);
			}
			if (!ContinueAxiomRelease(Epoch)) { return true; }
			if (!IsValid(TargetASC) || !IsAxiomTargetEligible(Target, TargetASC)) { continue; }
			if (IsAxiomDeviceEligible(Target, TargetASC) && ApplyAxiomDurationEffect(TargetASC,
				SafeAxiomEffect(DeviceDisableEffectClass, USovGameplayEffect_AxiomDeviceDisable::StaticClass()), Tags.State_Status_DeviceDisabled, Suppression))
			{
				++DisabledDevices;
				if (!ContinueAxiomRelease(Epoch)) { return true; }
				FGameplayTagContainer DroneAttacks;
				DroneAttacks.AddTag(Tags.Ability_NPC_ReformationDrone_Gunfire);
				DroneAttacks.AddTag(Tags.Ability_NPC_ReformationDrone_RocketLauncher);
				DroneAttacks.AddTag(Tags.Ability_NPC_ReformationDrone_SelfDestruct);
				TargetASC->CancelAbilities(&DroneAttacks);
			}
			if (!ContinueAxiomRelease(Epoch)) { return true; }
		}
		AuthorizedCommandNode = Target;
		FSovCommandLinkSeverResult SeverResult;
		if (TrySeverAxiomCommandLink(Target, SeverResult) == ESovCommandLinkSeverResolution::NewlySevered) { ++SeveredLinks; }
		if (!ContinueAxiomRelease(Epoch)) { return true; }
		AuthorizedCommandNode.Reset();
	}
	if (ContinueAxiomRelease(Epoch))
	{
		ReceiveAxiomPulseReleased(ReleaseOrigin, ReleaseDirection, ReleasedChargeAlpha, ReleasedRange, ReleasedHalfAngle,
			ShieldTargets, DisabledDevices, SeveredLinks);
		if (ContinueAxiomRelease(Epoch)) { StartAxiomRecovery(Epoch); }
	}
	return true;
}

void USovGameplayAbility_SeleneAxiomNullPulse::StartAxiomRecovery(uint32 Epoch)
{
	if (PostPulseRecovery <= KINDA_SMALL_NUMBER) { FinishEchoAbility(false); return; }
	GetWorld()->GetTimerManager().SetTimer(AxiomRecoveryTimer,
		FTimerDelegate::CreateWeakLambda(this, [this, Epoch]()
		{
			if (IsCurrentAxiomActivation(Epoch)) { FinishEchoAbility(false); }
		}), PostPulseRecovery, false);
}
