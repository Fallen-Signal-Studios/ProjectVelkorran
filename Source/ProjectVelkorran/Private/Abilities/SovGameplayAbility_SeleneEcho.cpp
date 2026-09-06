// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_SeleneEcho.h"

#include "AbilitySystemComponent.h"
#include "Components/SovSeleneEchoGenerationComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameplayEffect.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "TimerManager.h"
#include "Weapons/NarrativeProjectile.h"

USovGameplayAbility_SeleneEchoBase::USovGameplayAbility_SeleneEchoBase()
{
	RequiredCharacterTag = FSovGameplayTags::Get().Character_Player_Selene;
}

USovGameplayAbility_SeleneStillpointGrenade::USovGameplayAbility_SeleneStillpointGrenade()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 35.0f;
	EchoCost = 35.0f;
	EchoSpendTag = Tags.Ability_Echo_Selene_StillpointGrenade;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Selene_StillpointGrenade;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability1;
	WeaponFamily = ESovSeleneEchoWeaponFamily::Universal;
	WeaponGatePolicy = ESovEchoWeaponGatePolicy::AnyAllowedWielded;
	AbilityDisplayName = NSLOCTEXT("SovSeleneEcho", "StillpointGrenadeName", "Stillpoint Grenade");
	AbilityDescription = NSLOCTEXT(
		"SovSeleneEcho",
		"StillpointGrenadeDescription",
		"Open a cryothermal Stasis field that locks down standard enemies and damages them while Frozen; resistant targets are Chilled instead.");
}

bool USovGameplayAbility_SeleneStillpointGrenade::HasRequiredPayloadConfiguration() const
{
	return GrenadeClass.Get()
		&& ChillEffectClass.Get()
		&& FreezeEffectClass.Get()
		&& FrozenDamageOverTimeEffectClass.Get()
		&& ResistantTargetDamageOverTimeEffectClass.Get()
		&& StasisRadius > KINDA_SMALL_NUMBER
		&& StasisDuration > KINDA_SMALL_NUMBER;
}

USovGameplayAbility_SeleneDispatch::USovGameplayAbility_SeleneDispatch()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 90.0f;
	EchoCost = 90.0f;
	EchoSpendTag = Tags.Ability_Echo_Selene_Dispatch;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Selene_Dispatch;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability3;
	WeaponFamily = ESovSeleneEchoWeaponFamily::Verity;
	// Dispatch is Selene's signature and summons Verity even while a firearm is active.
	bRequiresAllowedWeapon = false;
	MaximumActiveDuration = 7.0f;
	AbilityDisplayName = NSLOCTEXT("SovSeleneEcho", "DispatchName", "Dispatch");
	AbilityDescription = NSLOCTEXT(
		"SovSeleneEcho",
		"DispatchDescription",
		"Cast Verity through a steerable arc, then recall it on command or at the outbound limit; enemies can be struck once on each leg of the path.");
}

bool USovGameplayAbility_SeleneDispatch::HasRequiredPayloadConfiguration() const
{
	return ReturningVerityClass.Get()
		&& OutboundDamageEffectClass.Get()
		&& ReturnDamageEffectClass.Get()
		&& MaximumOutboundDuration > KINDA_SMALL_NUMBER
		&& MaximumOutboundDistance > KINDA_SMALL_NUMBER
		&& OutboundSpeed > KINDA_SMALL_NUMBER
		&& ReturnSpeed > KINDA_SMALL_NUMBER;
}

USovGameplayAbility_SeleneStaccatoZero::USovGameplayAbility_SeleneStaccatoZero()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 30.0f;
	EchoCost = 30.0f;
	EchoSpendTag = Tags.Ability_Echo_Selene_StaccatoZero;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Selene_StaccatoZero;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability2;
	WeaponFamily = ESovSeleneEchoWeaponFamily::Staccato;
	AbilityDisplayName = NSLOCTEXT("SovSeleneEcho", "StaccatoZeroName", "Staccato Zero");
	AbilityDescription = NSLOCTEXT(
		"SovSeleneEcho",
		"StaccatoZeroDescription",
		"Fire one overdriven precision shot with extreme direct damage and a deterministic Freeze against a valid target.");
}

bool USovGameplayAbility_SeleneStaccatoZero::HasRequiredPayloadConfiguration() const
{
	return EmpoweredShotDamageEffectClass.Get()
		&& FreezeEffectClass.Get()
		&& ResistantTargetChillEffectClass.Get()
		&& MaximumRange > KINDA_SMALL_NUMBER
		&& DamageMultiplier > KINDA_SMALL_NUMBER;
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
	WeaponFamily = ESovSeleneEchoWeaponFamily::Axiom;
	AbilityDisplayName = NSLOCTEXT("SovSeleneEcho", "AxiomNullPulseName", "Axiom Null Pulse");
	AbilityDescription = NSLOCTEXT(
		"SovSeleneEcho",
		"AxiomNullPulseDescription",
		"Charge Axiom and release a directed EMP pulse that collapses Shields, suppresses recharge, and disables eligible combat systems.");
}

void USovGameplayAbility_SeleneAxiomNullPulse::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bAuthorityCommandLinkPulseProcessed = false;
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	AuthorityChargeStartTimeSeconds = ActorInfo && ActorInfo->IsNetAuthority()
		&& IsValid(Avatar) && Avatar->GetWorld()
		? Avatar->GetWorld()->GetTimeSeconds()
		: 0.0;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive() || !ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const float ChargeDuration = GetAxiomFullChargeDuration();
		if (ChargeDuration <= KINDA_SMALL_NUMBER)
		{
			HandleAxiomFullChargeReached();
		}
		else
		{
			World->GetTimerManager().SetTimer(
				AxiomFullChargeTimerHandle,
				this,
				&ThisClass::HandleAxiomFullChargeReached,
				ChargeDuration,
				false);
		}
	}
}

void USovGameplayAbility_SeleneAxiomNullPulse::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AxiomFullChargeTimerHandle);
	}

	// A deliberate release before full charge still emits a server-validated
	// pulse. Interruptions, death, and other cancellation paths emit nothing.
	if (!bWasCancelled && ActorInfo && ActorInfo->IsNetAuthority()
		&& IsActive() && !bAuthorityCommandLinkPulseProcessed)
	{
		ReleaseAxiomNullPulseCommandLinks();
	}

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);

	// GAS can defer EndAbility while an ability scope lock is held. Preserve the
	// release ledger until the deferred end actually closes this activation;
	// otherwise the queued call could process the same pulse a second time.
	if (!IsActive())
	{
		AuthorityChargeStartTimeSeconds = 0.0;
		bAuthorityCommandLinkPulseProcessed = false;
	}
}

float USovGameplayAbility_SeleneAxiomNullPulse::GetAxiomPulseRange(
	const float ChargeAlpha) const
{
	return FMath::Lerp(
		FMath::Max(MinimumPulseRange, 0.0f),
		FMath::Max(MaximumPulseRange, MinimumPulseRange),
		FMath::Clamp(ChargeAlpha, 0.0f, 1.0f));
}

float USovGameplayAbility_SeleneAxiomNullPulse::GetAxiomPulseHalfAngleDegrees(
	const float ChargeAlpha) const
{
	return FMath::Lerp(
		FMath::Clamp(MinimumPulseHalfAngleDegrees, 0.0f, 90.0f),
		FMath::Clamp(
			MaximumPulseHalfAngleDegrees,
			MinimumPulseHalfAngleDegrees,
			90.0f),
		FMath::Clamp(ChargeAlpha, 0.0f, 1.0f));
}

int32 USovGameplayAbility_SeleneAxiomNullPulse::ReleaseAxiomNullPulseCommandLinks()
{
	if (bAuthorityCommandLinkPulseProcessed
		|| !CurrentActorInfo
		|| !CurrentActorInfo->IsNetAuthority()
		|| !IsActive())
	{
		return 0;
	}

	// Commit the per-activation ledger before invoking any link callbacks. A
	// Sever delegate may synchronously end or otherwise re-enter this ability.
	bAuthorityCommandLinkPulseProcessed = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AxiomFullChargeTimerHandle);
	}
	return ProcessAuthorityCommandLinkPulse(GetAuthorityChargeAlpha());
}

float USovGameplayAbility_SeleneAxiomNullPulse::GetAuthorityChargeAlpha() const
{
	const UWorld* World = GetWorld();
	const float ChargeDuration = GetAxiomFullChargeDuration();
	if (!World || ChargeDuration <= KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}

	return FMath::Clamp(
		static_cast<float>(
			(World->GetTimeSeconds() - AuthorityChargeStartTimeSeconds)
			/ ChargeDuration),
		0.0f,
		1.0f);
}

void USovGameplayAbility_SeleneAxiomNullPulse::HandleAxiomFullChargeReached()
{
	ReleaseAxiomNullPulseCommandLinks();

	// Full charge is the terminal point for this one-shot ability.  The
	// Blueprint child only owns cosmetic presentation, so close the native
	// lifecycle here instead of leaving Busy active until the watchdog cancels
	// the ability.  A Sever callback may have ended the ability re-entrantly.
	if (IsActive())
	{
		FinishEchoAbility(false);
	}
}

int32 USovGameplayAbility_SeleneAxiomNullPulse::ProcessAuthorityCommandLinkPulse(
	const float ChargeAlpha)
{
	AActor* Avatar = CurrentActorInfo
		? CurrentActorInfo->AvatarActor.Get()
		: nullptr;
	UWorld* World = IsValid(Avatar) ? Avatar->GetWorld() : nullptr;
	if (!World || !Avatar->HasAuthority())
	{
		return 0;
	}

	FVector PulseOrigin = Avatar->GetActorLocation();
	FRotator PulseRotation = Avatar->GetActorRotation();
	Avatar->GetActorEyesViewPoint(PulseOrigin, PulseRotation);
	if (AController* Controller = GetOwningController())
	{
		PulseRotation = Controller->GetControlRotation();
	}

	const float PulseRange = GetAxiomPulseRange(ChargeAlpha);
	const float PulseHalfAngle = GetAxiomPulseHalfAngleDegrees(ChargeAlpha);
	const FVector PulseDirection = PulseRotation.Vector();
	TArray<TWeakObjectPtr<AActor>> CommandNodes;

	// Command nodes are an authored, sparse encounter set. Iterating their
	// component owners is collision-independent (important for non-colliding
	// relays) and never trusts client target data. Snapshot before mutation so
	// callbacks may destroy or spawn actors without perturbing this pulse pass.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* CommandNode = *It;
		if (!IsValid(CommandNode) || CommandNode == Avatar
			|| !CommandNode->FindComponentByClass<USovCommandLinkComponent>()
			|| !IsLocationInsideDirectedPulse(
				PulseOrigin,
				PulseDirection,
				CommandNode->GetActorLocation(),
				PulseRange,
				PulseHalfAngle))
		{
			continue;
		}
		CommandNodes.AddUnique(CommandNode);
	}

	int32 NewlySeveredCount = 0;
	for (const TWeakObjectPtr<AActor>& CommandNodePtr : CommandNodes)
	{
		AActor* CommandNode = CommandNodePtr.Get();
		if (!IsValid(CommandNode))
		{
			continue;
		}
		FSovCommandLinkSeverResult Result;
		if (TrySeverAxiomCommandLink(CommandNode, Result)
			== ESovCommandLinkSeverResolution::NewlySevered)
		{
			++NewlySeveredCount;
		}
	}

	return NewlySeveredCount;
}

bool USovGameplayAbility_SeleneAxiomNullPulse::IsLocationInsideDirectedPulse(
	const FVector& PulseOrigin,
	const FVector& PulseDirection,
	const FVector& CandidateLocation,
	const float PulseRange,
	const float PulseHalfAngleDegrees)
{
	if (PulseOrigin.ContainsNaN() || PulseDirection.ContainsNaN()
		|| CandidateLocation.ContainsNaN() || !FMath::IsFinite(PulseRange)
		|| !FMath::IsFinite(PulseHalfAngleDegrees)
		|| PulseRange <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector SafeDirection = PulseDirection.GetSafeNormal();
	if (SafeDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector ToCandidate = CandidateLocation - PulseOrigin;
	const double DistanceSquared = ToCandidate.SizeSquared();
	if (DistanceSquared > FMath::Square(static_cast<double>(PulseRange)))
	{
		return false;
	}
	if (DistanceSquared <= UE_DOUBLE_SMALL_NUMBER)
	{
		return true;
	}

	const double MinimumDot = FMath::Cos(FMath::DegreesToRadians(
		FMath::Clamp(PulseHalfAngleDegrees, 0.0f, 90.0f)));
	return FVector::DotProduct(SafeDirection, ToCandidate.GetSafeNormal())
		+ KINDA_SMALL_NUMBER >= MinimumDot;
}

#if WITH_AUTOMATION_TESTS
bool USovGameplayAbility_SeleneAxiomNullPulse::IsLocationInsideAxiomPulse(
	const FVector& PulseOrigin,
	const FVector& PulseDirection,
	const FVector& CandidateLocation,
	const float PulseRange,
	const float PulseHalfAngleDegrees)
{
	return IsLocationInsideDirectedPulse(
		PulseOrigin,
		PulseDirection,
		CandidateLocation,
		PulseRange,
		PulseHalfAngleDegrees);
}
#endif

ESovCommandLinkSeverResolution
USovGameplayAbility_SeleneAxiomNullPulse::TrySeverAxiomCommandLink(
	AActor* CommandNode,
	FSovCommandLinkSeverResult& OutResult)
{
	OutResult = FSovCommandLinkSeverResult();
	if (!CurrentActorInfo
		|| !CurrentActorInfo->IsNetAuthority()
		|| !IsActive())
	{
		return ESovCommandLinkSeverResolution::Invalid;
	}

	AActor* Avatar = CurrentActorInfo->AvatarActor.Get();
	UAbilitySystemComponent* SourceAbilitySystem =
		CurrentActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(Avatar)
		|| !IsValid(SourceAbilitySystem)
		|| !IsValid(CommandNode)
		|| CommandNode == Avatar
		|| CommandNode->GetWorld() != Avatar->GetWorld()
		|| MaximumPulseRange <= KINDA_SMALL_NUMBER
		|| FVector::DistSquared(
				Avatar->GetActorLocation(),
				CommandNode->GetActorLocation())
			> FMath::Square(MaximumPulseRange))
	{
		return ESovCommandLinkSeverResolution::Invalid;
	}

	USovCommandLinkComponent* CommandLink =
		CommandNode->FindComponentByClass<USovCommandLinkComponent>();
	if (!IsValid(CommandLink))
	{
		return ESovCommandLinkSeverResolution::Invalid;
	}

	const ESovCommandLinkSeverResolution Resolution =
		CommandLink->TrySeverCommandLink(Avatar, OutResult);
	if (Resolution == ESovCommandLinkSeverResolution::NewlySevered)
	{
		if (USovSeleneEchoGenerationComponent* EchoGeneration =
			Avatar->FindComponentByClass<USovSeleneEchoGenerationComponent>())
		{
			EchoGeneration->ConsumeCommandLinkSever(OutResult);
		}
	}

	return Resolution;
}

bool USovGameplayAbility_SeleneAxiomNullPulse::HasRequiredPayloadConfiguration() const
{
	return ShieldDisruptionDamageEffectClass.Get()
		&& ShieldRechargeBlockEffectClass.Get()
		&& FullChargeDuration > KINDA_SMALL_NUMBER
		&& MinimumPulseRange > KINDA_SMALL_NUMBER
		&& MaximumPulseRange >= MinimumPulseRange
		&& MaximumPulseHalfAngleDegrees >= MinimumPulseHalfAngleDegrees
		&& MaximumShieldSuppressionDuration >= MinimumShieldSuppressionDuration;
}

USovGameplayAbility_SeleneVeritysWake::USovGameplayAbility_SeleneVeritysWake()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 30.0f;
	EchoCost = 30.0f;
	EchoSpendTag = Tags.Ability_Echo_Selene_VeritysWake;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Selene_VeritysWake;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability2;
	WeaponFamily = ESovSeleneEchoWeaponFamily::Verity;
	AbilityDisplayName = NSLOCTEXT("SovSeleneEcho", "VeritysWakeName", "Verity's Wake");
	AbilityDescription = NSLOCTEXT(
		"SovSeleneEcho",
		"VeritysWakeDescription",
		"Drive a fast frost wave down a combat lane, dealing Echo damage and Chill; centerline or already-Chilled targets Freeze and suffer Frost damage over time.");
}

bool USovGameplayAbility_SeleneVeritysWake::HasRequiredPayloadConfiguration() const
{
	return WaveClass.Get()
		&& WaveDamageEffectClass.Get()
		&& ChillEffectClass.Get()
		&& FreezeEffectClass.Get()
		&& FrostDamageOverTimeEffectClass.Get()
		&& WaveRange > KINDA_SMALL_NUMBER
		&& WaveWidth > KINDA_SMALL_NUMBER;
}
