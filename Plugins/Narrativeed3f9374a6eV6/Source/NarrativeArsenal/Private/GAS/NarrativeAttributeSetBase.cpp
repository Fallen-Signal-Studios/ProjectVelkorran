// Copyright Narrative Tools 2024.

#include "GAS/NarrativeAttributeSetBase.h"
#include "Settings/SovCampaignModifiers.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeDamageExecCalc.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "ArsenalStatics.h"
#include "GAS/SovCombatTypes.h"
#include "GAS/SovCombatTransactionPolicy.h"
#include "GAS/SovAttackReceiptSource.h"
#include "GAS/SovDamageSourcePolicy.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "UObject/StrongObjectPtr.h"
#include "GameplayTagContainer.h"
#include "NarrativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Settings/NarrativeCombatDeveloperSettings.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"

UNarrativeAttributeSetBase::UNarrativeAttributeSetBase()
{
	// Echo has a fixed campaign scale. Initial reserve is supplied by protagonist
	// initialization/checkpoint effects, so a new attribute set starts empty.
	InitMaxEcho(100.f);
	InitEcho(0.f);
}

void UNarrativeAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
		AdjustAttributeForMaxChange(Health, MaxHealth, NewValue, GetHealthAttribute());
	}
	else if (Attribute == GetMaxShieldAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
		AdjustAttributeForMaxChange(Shield, MaxShield, NewValue, GetShieldAttribute());
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
		AdjustAttributeForMaxChange(Stamina, MaxStamina, NewValue, GetStaminaAttribute());
	}
	else if (Attribute == GetMaxPoiseAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
		AdjustAttributeForMaxChange(Poise, MaxPoise, NewValue, GetPoiseAttribute());
	}
	else if (Attribute == GetMaxEchoAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);

		// Setting the initial 0..100 range must not grant a full Echo meter.
		if (GetMaxEcho() > KINDA_SMALL_NUMBER)
		{
			AdjustAttributeForMaxChange(Echo, MaxEcho, NewValue, GetEchoAttribute());
		}
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetShieldAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxShield());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	}
	else if (Attribute == GetPoiseAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxPoise());
	}
	else if (Attribute == GetEchoAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxEcho());
	}
	else if (Attribute == GetDamageResistanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, -100.f, 95.f);
	}
}

void UNarrativeAttributeSetBase::PostAttributeChange(
	const FGameplayAttribute& Attribute,
	float OldValue,
	float NewValue)
{
	if (Attribute == GetHealthAttribute() && OldValue <= 0.f && NewValue > 0.f) { ++CombatLifeEpoch; }
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetXPAttribute())
	{
		if (INarrativeCharacterOwner* CharacterOwner = Cast<INarrativeCharacterOwner>(GetOuter()))
		{
			if (ANarrativeCharacter* Character = CharacterOwner->GetNarrativeCharacter())
			{
				Character->OnXPChanged(OldValue, NewValue);
			}
		}
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
	else if (Attribute == GetMaxShieldAttribute())
	{
		SetShield(FMath::Clamp(GetShield(), 0.f, GetMaxShield()));
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.f, GetMaxStamina()));
	}
	else if (Attribute == GetMaxPoiseAttribute())
	{
		SetPoise(FMath::Clamp(GetPoise(), 0.f, GetMaxPoise()));
	}
	else if (Attribute == GetMaxEchoAttribute())
	{
		SetEcho(FMath::Clamp(GetEcho(), 0.f, GetMaxEcho()));
	}
}

bool UNarrativeAttributeSetBase::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	const bool bCombatPacket = Data.EvaluatedData.Attribute == GetDamageAttribute()
		|| Data.EvaluatedData.Attribute == GetPoiseDamageAttribute()
		|| Data.EvaluatedData.Attribute == GetControlRequestAttribute();
	if (bCombatPacket)
	{
		for (const auto& Magnitude : Data.EffectSpec.SetByCallerTagMagnitudes)
		{
			if (!FMath::IsFinite(Magnitude.Value)) { Data.EvaluatedData.Magnitude = 0.f; return false; }
		}
	}
	if (bCombatPacket && (CombatResolutionDepth >= 32 || !FMath::IsFinite(Data.EvaluatedData.Magnitude)
		|| UNarrativeDamageExecCalc::ShouldRejectTransaction(
			Data.EffectSpec.GetContext().GetOriginalInstigatorAbilitySystemComponent(),
			&Data.Target, Data.EffectSpec)))
	{
		Data.EvaluatedData.Magnitude = 0.f;
		return false;
	}

	// Heal is not a resurrection operation. Recovery/checkpoint owners restore resources explicitly.
	if (Data.EvaluatedData.Attribute == GetHealAttribute()
		&& (!FMath::IsFinite(Data.EvaluatedData.Magnitude) || GetHealth() <= 0.f))
	{ Data.EvaluatedData.Magnitude = 0.f; return false; }
	return true;
}

void UNarrativeAttributeSetBase::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SovCombat_ResolveAttributePacket);
	Super::PostGameplayEffectExecute(Data);

	const FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	UNarrativeAbilitySystemComponent* SourceASC = Cast<UNarrativeAbilitySystemComponent>(
		Context.GetOriginalInstigatorAbilitySystemComponent());
	UNarrativeAbilitySystemComponent* TargetASC = Cast<UNarrativeAbilitySystemComponent>(&Data.Target);

	AActor* TargetActor = Data.Target.AbilityActorInfo.IsValid()
		? Data.Target.AbilityActorInfo->AvatarActor.Get()
		: nullptr;

	AActor* SourceActor = SourceASC && SourceASC->AbilityActorInfo.IsValid()
		? SourceASC->AbilityActorInfo->AvatarActor.Get()
		: nullptr;

	// Allocation lifetime and gameplay ownership are separate. Delegates may
	// destroy, restore or rebind their actors while this synchronous call is live.
	TStrongObjectPtr<UNarrativeAttributeSetBase> AttributeLifetime(this);
	TStrongObjectPtr<UNarrativeAbilitySystemComponent> TargetASCLifetime(TargetASC);
	TStrongObjectPtr<UNarrativeAbilitySystemComponent> SourceASCLifetime(SourceASC);
	TStrongObjectPtr<AActor> TargetActorLifetime(TargetActor);
	TStrongObjectPtr<AActor> SourceActorLifetime(SourceActor);
	const uint64 PacketLifeEpoch = CombatLifeEpoch;
	const uint64 TargetActorInfoEpoch = TargetASC ? TargetASC->GetCombatActorInfoEpoch() : 0;
	const uint64 SourceActorInfoEpoch = SourceASC ? SourceASC->GetCombatActorInfoEpoch() : 0;
	const auto* SourceAttributes = SourceASC ? SourceASC->GetSet<UNarrativeAttributeSetBase>() : nullptr;
	TStrongObjectPtr<const UNarrativeAttributeSetBase> SourceAttributeLifetime(SourceAttributes);
	const uint64 SourceLifeEpoch = SourceAttributes ? SourceAttributes->GetCombatLifeEpoch() : 0;
	const auto StillOwnsTarget = [this, PacketLifeEpoch, TargetActorInfoEpoch, TargetASC, TargetActor]()
	{
		return IsValid(this) && CombatLifeEpoch == PacketLifeEpoch
			&& IsValid(TargetASC) && IsValid(TargetActor) && !TargetActor->IsActorBeingDestroyed()
			&& TargetASC->GetAvatarActor() == TargetActor
			&& TargetASC->GetCombatActorInfoEpoch() == TargetActorInfoEpoch
			&& TargetASC->GetSet<UNarrativeAttributeSetBase>() == this
			&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor) == TargetASC;
	};
	const auto StillOwnsSource = [SourceASC, SourceActor, SourceActorInfoEpoch, SourceAttributes, SourceLifeEpoch]()
	{
		return IsValid(SourceASC) && IsValid(SourceActor) && !SourceActor->IsActorBeingDestroyed()
			&& SourceASC->GetAvatarActor() == SourceActor
			&& SourceASC->GetCombatActorInfoEpoch() == SourceActorInfoEpoch
			&& (!SourceAttributes || (IsValid(SourceAttributes)
				&& SourceASC->GetSet<UNarrativeAttributeSetBase>() == SourceAttributes
				&& SourceAttributes->GetCombatLifeEpoch() == SourceLifeEpoch))
			&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor) == SourceASC;
	};

	AController* SourceController = nullptr;
	if (ANarrativeCharacter* SourceCharacter = Cast<ANarrativeCharacter>(SourceActor))
	{
		SourceController = SourceCharacter->GetController();
	}
	else if (SourceASC && SourceASC->AbilityActorInfo.IsValid())
	{
		SourceController = SourceASC->AbilityActorInfo->PlayerController.Get();
	}
	TStrongObjectPtr<AController> SourceControllerLifetime(SourceController);

	const auto NotifyAppliedDamage = [
		SourceASC,
		TargetASC,
		SourceActor,
		TargetActor,
		SourceController, &StillOwnsTarget, &StillOwnsSource](
			const float AppliedDamage,
			const FGameplayEffectSpec& NotificationSpec)
	{
		if (AppliedDamage <= KINDA_SMALL_NUMBER || !StillOwnsTarget() || !StillOwnsSource())
		{
			return;
		}

		if (ANarrativePlayerController* PlayerController = Cast<ANarrativePlayerController>(SourceController))
		{
			const APawn* SourcePawn = Cast<APawn>(SourceActor);
			if (IsValid(PlayerController) && !PlayerController->IsActorBeingDestroyed()
				&& (!SourcePawn || SourcePawn->GetController() == PlayerController)
				&& TargetActor != SourceActor)
			{
				PlayerController->NotifyDealtDamage(TargetActor, AppliedDamage);
			}
		}

		if (StillOwnsSource() && StillOwnsTarget() && SourceASC != TargetASC)
		{
			TargetASC->DamagedBy(SourceASC, AppliedDamage, NotificationSpec);
			if (StillOwnsSource() && StillOwnsTarget())
			{ SourceASC->DealtDamage(TargetASC, AppliedDamage, NotificationSpec); }
		}
	};

	const bool bBodyPacket = Data.EvaluatedData.Attribute == GetDamageAttribute();
	const bool bPoisePacket = Data.EvaluatedData.Attribute == GetPoiseDamageAttribute();
	const bool bControlPacket = Data.EvaluatedData.Attribute == GetControlRequestAttribute();
	if (bBodyPacket || bPoisePacket || bControlPacket)
	{
		SovCombatTransaction::FScopedCallbackBudget WorkScope(SovCombatTransaction::ThreadCallbackBudget());
		TGuardValue<uint32> ResolutionScope(CombatResolutionDepth, CombatResolutionDepth + 1);
		FGameplayTagContainer TargetTagsAtEntry;
		Data.Target.GetOwnedGameplayTags(TargetTagsAtEntry);
		const bool bPeriodicDelivery = Data.EffectSpec.GetPeriod() > 0.f;
		const float IncomingDamage = bBodyPacket ? FMath::Max(GetDamage(), 0.f) : 0.f;
		const float IncomingPoise = bPoisePacket ? FMath::Max(GetPoiseDamage(), 0.f) : 0.f;
		const float IncomingControl = bControlPacket ? FMath::Max(GetControlRequest(), 0.f) : 0.f;
		// Clear only our own packet before any callback can synchronously apply another.
		if (bBodyPacket) { SetDamage(0.f); }
		if (bPoisePacket) { SetPoiseDamage(0.f); }
		if (bControlPacket) { SetControlRequest(0.f); }
		if (!WorkScope.IsAdmitted() || !StillOwnsTarget() || GetHealth() <= 0.f
			|| IncomingDamage + IncomingPoise + IncomingControl <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		FGameplayTagContainer EffectAssetTags;
		Data.EffectSpec.GetAllAssetTags(EffectAssetTags);
		const FSovGameplayTags& Tags = FSovGameplayTags::Get();
		const UNarrativeCombatDeveloperSettings* CombatSettings = GetDefault<UNarrativeCombatDeveloperSettings>();
		AActor* DamageInstigator = SourceActor
			? SourceActor
			: Context.GetOriginalInstigator();
		AActor* DamageCauser = Context.GetEffectCauser()
			? Context.GetEffectCauser()
			: DamageInstigator;
		TStrongObjectPtr<AActor> DamageInstigatorLifetime(DamageInstigator);
		TStrongObjectPtr<AActor> DamageCauserLifetime(DamageCauser);

		FSovDamageResult Result;
		Result.TransactionId = FGuid::NewGuid();
		Result.bCanonicalFatal = EffectAssetTags.HasTagExact(Tags.Damage_Fatal);
		Result.bPeriodicDamage = bPeriodicDelivery;
		Result.TargetTagsBeforeDamage = MoveTemp(TargetTagsAtEntry);
		Result.SourceActor = DamageInstigator;
		Result.TargetActor = TargetActor;
		Result.InitializeNativeReceipt(this);
		if (IsValid(DamageInstigator) && DamageInstigator->HasAuthority())
		{
			const UObject* ReceiptCandidates[] = {Context.GetSourceObject(), Context.GetEffectCauser()};
			for (const UObject* Candidate : ReceiptCandidates)
			{
				const ISovAttackReceiptSource* Receipt = Cast<ISovAttackReceiptSource>(Candidate);
				FGuid CandidateId;
				if (Receipt && Receipt->GetSovAttackIdentity(DamageInstigator, CandidateId) && CandidateId.IsValid())
				{
					Result.AttackId = CandidateId;
					break;
				}
			}
		}

		Result.BaseDamage = FMath::Max(
			Data.EffectSpec.GetSetByCallerMagnitude(
				FNarrativeGameplayTags::Get().SetByCaller_Damage,
				false,
				IncomingDamage),
			0.f);
		Result.ResolvedDamage = IncomingDamage;
		Result.EffectContext = Context;
		Result.AcceptedChannelFraction = UNarrativeDamageExecCalc::GetAcceptedChannelFraction(TargetASC, Data.EffectSpec, &Result.RejectedDamageChannels);
		Result.bFromEchoAbility = EffectAssetTags.HasTag(Tags.Ability_Echo);
		if (const UGameplayAbility* SourceAbility = Context.GetAbility())
		{
			Result.bFromEchoAbility = Result.bFromEchoAbility
				|| SourceAbility->GetAssetTags().HasTag(Tags.Ability_Echo);
		}
		if (const FHitResult* Hit = Context.GetHitResult())
		{
			Result.HitZone = Hit->BoneName;
		}

		const FGameplayTag DamageChannels[] = {
			Tags.Damage_Channel_Kinetic,
			Tags.Damage_Channel_Edge,
			Tags.Damage_Channel_Thermal,
			Tags.Damage_Channel_Echo,
			Tags.Damage_Channel_Disruption,
			Tags.Damage_Channel_Corruption,
			Tags.Damage_Channel_Environmental};
		for (const FGameplayTag& Tag : DamageChannels)
		{
			if (EffectAssetTags.HasTagExact(Tag))
			{
				Result.DamageChannels.AddTag(Tag);
			}
		}

		const FGameplayTag AttackClassifications[] = {
			Tags.Damage_GuardClass_Standard,
			Tags.Damage_GuardClass_Heavy,
			Tags.Damage_GuardClass_Unblockable,
			Tags.Damage_Source_GuardCounter,
			Tags.Damage_Heavy,
			Tags.Damage_Unblockable};
		for (const FGameplayTag& Tag : AttackClassifications)
		{
			if (EffectAssetTags.HasTagExact(Tag))
			{
				Result.AttackClassifications.AddTag(Tag);
			}
		}

		for (const FGameplayTag& Tag : EffectAssetTags)
		{
			if (Tag != Tags.Status_Apply && Tag.MatchesTag(Tags.Status_Apply))
			{
				Result.RequestedStatusTags.AddTag(Tag);
			}
		}
		FGameplayTagContainer SourceAbilityTags;
		for (const FGameplayTag& Tag : EffectAssetTags)
		{
			if (Tag.MatchesTag(Tags.Ability))
			{
				SourceAbilityTags.AddTag(Tag);
			}
		}
		if (const UGameplayAbility* SourceAbility = Context.GetAbility())
		{
			for (const FGameplayTag& Tag : SourceAbility->GetAssetTags())
			{
				if (Tag.MatchesTag(Tags.Ability))
				{
					SourceAbilityTags.AddTag(Tag);
				}
			}
		}

		const auto SendSovEvent = [TargetActor, DamageInstigator, &Context, &StillOwnsTarget](
			const FGameplayTag& EventTag,
			const float Magnitude,
			const FGameplayTagContainer* PayloadTargetTags)
		{
			if (!StillOwnsTarget() || !EventTag.IsValid())
			{
				return;
			}

			FGameplayEventData Payload;
			Payload.EventTag = EventTag;
			Payload.Instigator = DamageInstigator;
			Payload.Target = TargetActor;
			Payload.ContextHandle = Context;
			Payload.EventMagnitude = Magnitude;
			if (PayloadTargetTags)
			{
				Payload.TargetTags = *PayloadTargetTags;
			}
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, EventTag, Payload);
		};

		float RoutedDamage = IncomingDamage;
		float RoutedPoiseDamage = bPoisePacket ? IncomingPoise : bControlPacket ? 0.f : Data.EffectSpec.GetSetByCallerMagnitude(
			Tags.SetByCaller_Damage_PoiseDamage,
			false,
			-1.f);
		if (RoutedPoiseDamage < 0.f)
		{
			const float DefaultPoiseCoefficient =
				EffectAssetTags.HasTagExact(Tags.Damage_Poise) ? 1.f : 0.f;
			const float PoiseCoefficient = FMath::Max(
				Data.EffectSpec.GetSetByCallerMagnitude(
					Tags.SetByCaller_Damage_PoiseCoefficient,
					false,
					DefaultPoiseCoefficient),
				0.f);
			RoutedPoiseDamage = SovCombatTransaction::BoundedProduct(IncomingDamage, PoiseCoefficient);
		}

		RoutedPoiseDamage = FMath::IsFinite(RoutedPoiseDamage) ? FMath::Max(RoutedPoiseDamage, 0.f) : 0.f;
		if (bPoisePacket || (bBodyPacket && Data.EffectSpec.GetSetByCallerMagnitude(Tags.SetByCaller_Damage_PoiseDamage, false, -1.f) >= 0.f))
		{
			RoutedPoiseDamage *= Result.AcceptedChannelFraction;
		}
		Result.RequestedPoiseDamage = RoutedPoiseDamage;
		// Local difficulty adjusts hostile body pressure only, after deriving independent Poise.
		const APawn* TargetPawn = Cast<APawn>(TargetActor);
		const APlayerController* LocalPlayer = TargetPawn ? Cast<APlayerController>(TargetPawn->GetController()) : nullptr;
		if (LocalPlayer && LocalPlayer->IsLocalController() && SourceActor && SourceActor != TargetActor
			&& !EffectAssetTags.HasTagExact(Tags.Damage_Fatal)
			&& UArsenalStatics::GetAttitude(SourceActor, TargetActor) == ETeamAttitude::Hostile)
		{
			if (const auto* Settings = UNarrativeGameUserSettings::GetSovPlayerSettings(TargetActor))
			{
				const float Scale = Settings->GetIncomingDamageScale();
				RoutedDamage = SovCombatTransaction::BoundedProduct(RoutedDamage, FMath::IsFinite(Scale) ? FMath::Clamp(Scale, 0.f, 10.f) : 1.f);
				Result.ResolvedDamage = RoutedDamage;
			}
		}


        if (const auto* Settings = UNarrativeGameUserSettings::GetSovSettings())
        {
            const uint8 Mask = Settings->GetCampaignModifiers();
            if (Mask & (SovCampaignModifiers::Ascendant | SovCampaignModifiers::GlassCannon))
            {
                const bool EnemyToAlly = UNarrativeGameUserSettings::IsCampaignEnemy(SourceActor)
                    && UNarrativeGameUserSettings::IsCampaignAlly(TargetActor);
                const bool AllyToEnemy = UNarrativeGameUserSettings::IsCampaignAlly(SourceActor)
                    && UNarrativeGameUserSettings::IsCampaignEnemy(TargetActor);
                const float Scale = SovCampaignModifiers::BodyDamage(Mask, EnemyToAlly, AllyToEnemy,
                    EffectAssetTags.HasTagExact(Tags.Damage_Fatal));
                RoutedDamage = SovCombatTransaction::BoundedProduct(RoutedDamage, Scale);
                Result.ResolvedDamage = RoutedDamage;
            }
        }
		// Action-state defenses are evaluated after mathematical mitigation and
		// before Shield/Health routing. Deflection is a short all-or-nothing
		// precision window; it never enters Tarrik's sustained Guard policy.
		const bool bIsGuarding = Data.Target.HasMatchingGameplayTag(Tags.State_Guarding);
		const bool bPerfectWindow = Data.Target.HasMatchingGameplayTag(Tags.State_PerfectGuard);
		const bool bIsDeflecting = Data.Target.HasMatchingGameplayTag(Tags.State_Deflecting);
		const bool bHeavyAttack = EffectAssetTags.HasTagExact(Tags.Damage_GuardClass_Heavy)
			|| EffectAssetTags.HasTagExact(Tags.Damage_Heavy);
		const bool bUnblockableAttack = EffectAssetTags.HasTagExact(Tags.Damage_GuardClass_Unblockable)
			|| EffectAssetTags.HasTagExact(Tags.Damage_Unblockable)
			|| EffectAssetTags.HasTagExact(Tags.Damage_Fatal);
		const bool bBypassesGuard = bUnblockableAttack
			|| EffectAssetTags.HasTagExact(Tags.Damage_BypassGuard);
		const bool bNonInstantDamage = !Data.EffectSpec.Def
			|| Data.EffectSpec.Def->DurationPolicy
				!= EGameplayEffectDurationType::Instant;
		const bool bBypassesDeflection = bUnblockableAttack
			|| bNonInstantDamage
			|| EffectAssetTags.HasTagExact(Tags.Damage_BypassDeflection)
			|| EffectAssetTags.HasTagExact(Tags.Damage_Channel_Environmental);

		AActor* DirectionSource = DamageCauser;
		const auto IsInsideDefenseArc = [TargetActor, DirectionSource](const float HalfAngleDegrees)
		{
			if (!TargetActor || !DirectionSource || TargetActor == DirectionSource)
			{
				return false;
			}

			const FVector ToSource = (DirectionSource->GetActorLocation() - TargetActor->GetActorLocation()).GetSafeNormal2D();
			const FVector DefenseForward = TargetActor->GetActorForwardVector().GetSafeNormal2D();
			const float ClampedHalfAngle = FMath::Clamp(HalfAngleDegrees, 0.f, 180.f);
			return ToSource.IsNearlyZero()
				|| FVector::DotProduct(DefenseForward, ToSource)
					>= FMath::Cos(FMath::DegreesToRadians(ClampedHalfAngle));
		};

		const float GuardHalfAngle = CombatSettings
			? CombatSettings->GuardHalfAngleDegrees
			: 70.f;
		const float DeflectionHalfAngle = CombatSettings
			? CombatSettings->DeflectionHalfAngleDegrees
			: 65.f;
		const bool bInsideGuardArc = bIsGuarding && IsInsideDefenseArc(GuardHalfAngle);
		const bool bInsideDeflectionArc = bIsDeflecting && IsInsideDefenseArc(DeflectionHalfAngle);

		// A valid Deflection wins if a character is accidentally configured with
		// both defense states. This keeps the narrow window out of Guard's Stamina,
		// mitigation, break, counter, and reward paths.
		const float OldStamina = FMath::Max(GetStamina(), 0.f);
		const bool bDeflectionCandidate = bIsDeflecting
			&& bInsideDeflectionArc
			&& !bBypassesDeflection
			&& OldStamina > KINDA_SMALL_NUMBER;
		const bool bGuardCandidate = !bDeflectionCandidate
			&& bIsGuarding
			&& bInsideGuardArc
			&& !bBypassesGuard;
		if (bDeflectionCandidate)
		{
			const float DeflectionCost = bHeavyAttack
				? (CombatSettings
					? FMath::Max(CombatSettings->HeavyDeflectionStaminaDamage, 0.f)
					: 18.f)
				: (CombatSettings
					? FMath::Max(CombatSettings->DeflectionStaminaDamage, 0.f)
					: 8.f);
			Result.AppliedStaminaDamage = FMath::Min(OldStamina, DeflectionCost);

			Result.DefenseKind = ESovDefenseKind::Deflection;
			Result.bDeflected = true;
			Result.bPerfectDefense = true;
			RoutedDamage = 0.f;
			RoutedPoiseDamage = 0.f;
		}
		else
		{
			Result.bPerfectDefense = bGuardCandidate
				&& bPerfectWindow
				&& OldStamina > KINDA_SMALL_NUMBER;
			if (Result.bPerfectDefense)
			{
				const float PerfectGuardCost = CombatSettings
					? FMath::Max(CombatSettings->PerfectGuardStaminaDamage, 0.f)
					: 5.f;
				Result.AppliedStaminaDamage = FMath::Min(OldStamina, PerfectGuardCost);

				Result.DefenseKind = ESovDefenseKind::Guard;
				Result.bGuarded = true;
				RoutedDamage = 0.f;
				RoutedPoiseDamage = 0.f;
				// The timing remains successful, but spending the last Stamina ends
				// Guard through the normal replicated broken-state pipeline.
				Result.bGuardBroken = OldStamina - Result.AppliedStaminaDamage <= KINDA_SMALL_NUMBER;
			}
			else if (bGuardCandidate)
			{
				const float MinimumGuardCost = CombatSettings ? FMath::Max(CombatSettings->MinimumGuardStaminaDamage, 0.f) : 8.f;
				const float MaximumGuardCost = CombatSettings
					? FMath::Max(CombatSettings->MaximumGuardStaminaDamage, MinimumGuardCost)
					: 20.f;
				const float DefaultGuardCost = FMath::Clamp(
					IncomingDamage * (CombatSettings ? FMath::Max(CombatSettings->GuardStaminaDamageScalar, 0.f) : 0.5f),
					MinimumGuardCost,
					MaximumGuardCost);
				const float RequestedGuardCost = FMath::Max(
					Data.EffectSpec.GetSetByCallerMagnitude(
						Tags.SetByCaller_Damage_GuardStaminaDamage,
						false,
						DefaultGuardCost),
					0.f);
				const bool bCanPayGuardCost = RequestedGuardCost <= KINDA_SMALL_NUMBER
					|| OldStamina - RequestedGuardCost > KINDA_SMALL_NUMBER;

				Result.AppliedStaminaDamage = FMath::Min(OldStamina, RequestedGuardCost);

				if (!bHeavyAttack && bCanPayGuardCost)
				{
					Result.DefenseKind = ESovDefenseKind::Guard;
					Result.bGuarded = true;
					RoutedDamage = SovCombatTransaction::BoundedProduct(RoutedDamage, CombatSettings
						? FMath::Clamp(CombatSettings->GuardDamageMultiplier, 0.f, 1.f)
						: 0.25f);
					RoutedPoiseDamage = SovCombatTransaction::BoundedProduct(RoutedPoiseDamage, CombatSettings
						? FMath::Clamp(CombatSettings->GuardPoiseMultiplier, 0.f, 1.f)
						: 0.25f);
				}
				else
				{
					Result.bGuardBroken = true;
				}
			}
		}

		if (!StillOwnsTarget()) { return; }
		Result.AppliedStaminaDamage = FMath::Min(FMath::Max(GetStamina(), 0.f), Result.AppliedStaminaDamage);
		if (Result.AppliedStaminaDamage > 0.f)
		{ SetStamina(FMath::Clamp(GetStamina() - Result.AppliedStaminaDamage, 0.f, GetMaxStamina())); }
		if (!StillOwnsTarget()) { return; }

		const float OldShield = FMath::Max(GetShield(), 0.f);
		const float HealthAtRouting = FMath::Max(GetHealth(), 0.f);
		const float PoiseAtRouting = FMath::Max(GetPoise(), 0.f);

		const bool bFatalPolicy = EffectAssetTags.HasTagExact(Tags.Damage_Fatal);
		if (bFatalPolicy || EffectAssetTags.HasTagExact(Tags.Damage_BypassShield))
		{
			Result.ShieldBypassRatio = 1.f;
		}
		else if (EffectAssetTags.HasTagExact(Tags.Damage_BypassShield_Partial))
		{
			const float DefaultPartialBypass = CombatSettings
				? FMath::Clamp(CombatSettings->DefaultPartialShieldBypassRatio, 0.f, 1.f)
				: 0.5f;
			Result.ShieldBypassRatio = FMath::Clamp(
				Data.EffectSpec.GetSetByCallerMagnitude(
					Tags.SetByCaller_Damage_ShieldBypassRatio,
					false,
					DefaultPartialBypass),
				0.f,
				1.f);
			if (!FMath::IsFinite(Result.ShieldBypassRatio)) { Result.ShieldBypassRatio = 0.f; }
		}

		const float ShieldCoefficient = FMath::Max(
			Data.EffectSpec.GetSetByCallerMagnitude(
				Tags.SetByCaller_Damage_ShieldCoefficient,
				false,
				1.f),
			0.f);
		const float HealthCoefficient = bFatalPolicy
			? 1.f
			: FMath::Max(
				Data.EffectSpec.GetSetByCallerMagnitude(
					Tags.SetByCaller_Damage_HealthCoefficient,
					false,
					1.f),
				0.f);
		const double ShieldEligibleBase = static_cast<double>(RoutedDamage) * (1.0 - Result.ShieldBypassRatio);
		const double RequestedShieldDamage = ShieldEligibleBase * ShieldCoefficient;
		Result.RequestedShieldDamage = SovCombatTransaction::BoundedProduct(ShieldEligibleBase, ShieldCoefficient);
		Result.bShieldWasTargeted = GetMaxShield() > KINDA_SMALL_NUMBER
			&& Result.ShieldBypassRatio < 1.f
			&& Result.RequestedShieldDamage > KINDA_SMALL_NUMBER;
		Result.AppliedShieldDamage = FMath::Min(OldShield, Result.RequestedShieldDamage);

		double OverflowBase = 0.0;
		if (OldShield <= KINDA_SMALL_NUMBER)
		{
			OverflowBase = ShieldEligibleBase;
		}
		else if (ShieldCoefficient > KINDA_SMALL_NUMBER
			&& RequestedShieldDamage > OldShield)
		{
			OverflowBase =
				(RequestedShieldDamage - OldShield) / ShieldCoefficient;
		}
		const float RequestedHealthDamage = SovCombatTransaction::BoundedProduct(
			(static_cast<double>(RoutedDamage) * Result.ShieldBypassRatio) + OverflowBase, HealthCoefficient);
		Result.AppliedHealthDamage = FMath::Min(HealthAtRouting, RequestedHealthDamage);
		Result.HealthOverkillDamage = FMath::Max(RequestedHealthDamage - HealthAtRouting, 0.f);
		float HealthDamageLimit = RequestedHealthDamage;

		bool bSourceAllowsStatus = true;
		if (IsValid(DamageInstigator) && DamageInstigator->HasAuthority())
		{
			TInlineComponentArray<UActorComponent*> SourcePolicies(DamageInstigator);
			for (UActorComponent* Component : SourcePolicies)
			{
				const auto* Policy = IsValid(Component) ? Cast<ISovDamageSourcePolicy>(Component) : nullptr;
				if (!Policy) { continue; }
				const float ShieldLimit = Result.AppliedShieldDamage, HealthLimit = Result.AppliedHealthDamage;
				const float PoiseLimit = FMath::Min(RoutedPoiseDamage, PoiseAtRouting);
				float LimitedShield = ShieldLimit, LimitedHealth = HealthLimit, LimitedPoise = PoiseLimit;
				bSourceAllowsStatus &= Policy->LimitSovDamage(TargetActor, Context, LimitedShield, LimitedHealth, LimitedPoise);
				// Native policies can reduce their own contribution, never amplify damage or heal.
				Result.AppliedShieldDamage = FMath::IsFinite(LimitedShield) ? FMath::Clamp(LimitedShield, 0.f, ShieldLimit) : 0.f;
				Result.AppliedHealthDamage = FMath::IsFinite(LimitedHealth) ? FMath::Clamp(LimitedHealth, 0.f, HealthLimit) : 0.f;
				RoutedPoiseDamage = FMath::IsFinite(LimitedPoise) ? FMath::Clamp(LimitedPoise, 0.f, PoiseLimit) : 0.f;
				// A policy approves this concrete contribution, even if it did not
				// reduce the routing snapshot. A later Shield callback may heal the
				// target but cannot enlarge the source's approved Health budget.
				HealthDamageLimit = Result.AppliedHealthDamage;
				if (Result.AppliedHealthDamage < HealthLimit)
				{ Result.HealthOverkillDamage = 0.f; }
				if (Result.AppliedShieldDamage <= 0.f && ShieldLimit > 0.f) { Result.bShieldWasTargeted = false; }
			}
		}

		// Attribute setters can run synchronous GAS delegates. Each remaining debit
		// therefore starts from the current resource, never a snapshot taken before
		// another setter. Keep typed results local to this packet: a nested kill or
		// heal must not be counted as this packet's applied damage/fatal transition.
		if (!StillOwnsTarget()) { return; }
		const float ShieldBeforeCommit = FMath::Max(GetShield(), 0.f);
		Result.AppliedShieldDamage = FMath::Min(ShieldBeforeCommit, Result.AppliedShieldDamage);
		Result.bShieldBroken = ShieldBeforeCommit > 0.f && Result.AppliedShieldDamage >= ShieldBeforeCommit;
		if (Result.AppliedShieldDamage > 0.f)
		{ SetShield(FMath::Clamp(ShieldBeforeCommit - Result.AppliedShieldDamage, 0.f, GetMaxShield())); }
		Result.bShouldRestartShieldRecharge = EffectAssetTags.HasTagExact(Tags.Damage_RestartShieldRecharge)
			|| Result.bShieldWasTargeted;
		if (!StillOwnsTarget()) { return; }
		const float HealthBeforeCommit = FMath::Max(GetHealth(), 0.f);
		Result.AppliedHealthDamage = FMath::Min(HealthBeforeCommit, HealthDamageLimit);
		Result.HealthOverkillDamage = FMath::Max(HealthDamageLimit - HealthBeforeCommit, 0.f);
		Result.bFatal = HealthBeforeCommit > 0.f && Result.AppliedHealthDamage >= HealthBeforeCommit;
		if (Result.AppliedHealthDamage > 0.f)
		{ SetHealth(FMath::Clamp(HealthBeforeCommit - Result.AppliedHealthDamage, 0.f, GetMaxHealth())); }
		if (!StillOwnsTarget()) { return; }
		const float PoiseBeforeCommit = FMath::Max(GetPoise(), 0.f);
		if (GetHealth() > 0.f && RoutedPoiseDamage > KINDA_SMALL_NUMBER && PoiseBeforeCommit > 0.f)
		{
			const bool bPoiseCannotBreak = Data.Target.HasMatchingGameplayTag(Tags.State_Poise_Recovering)
				|| Data.Target.HasMatchingGameplayTag(Tags.State_Poise_SuperArmor)
				|| Data.Target.HasMatchingGameplayTag(Tags.State_InterruptProtected);
			const float PoiseFloor = bPoiseCannotBreak
				? FMath::Min(PoiseBeforeCommit, FMath::Min(GetMaxPoise(), FMath::Max(GetMaxPoise() * 0.01f, 1.f)))
				: 0.f;
			const float NewPoise = FMath::Clamp(PoiseBeforeCommit - RoutedPoiseDamage, PoiseFloor, GetMaxPoise());
			Result.AppliedPoiseDamage = FMath::Max(PoiseBeforeCommit - NewPoise, 0.f);
			Result.bPoiseBroken = NewPoise <= 0.f;
			SetPoise(NewPoise);
		}
		const float AppliedDamage = SovCombatTransaction::BoundedProduct(
			static_cast<double>(Result.AppliedShieldDamage) + Result.AppliedHealthDamage, 1.0);
		// All explicit gameplay notifications run after resource writes. Nested
		// same-target hits remain synchronous, preserving native payload receipts.

		if (StillOwnsTarget() && Result.bGuardBroken)
		{
			OnGuardBroken.Broadcast(
				DamageInstigator,
				DamageCauser,
				Data.EffectSpec,
				Result.AppliedStaminaDamage);
		}

		if (Result.bGuarded && !Result.bPerfectDefense)
		{ SendSovEvent(Tags.Event_Guard_Blocked, IncomingDamage, nullptr); }
		if (StillOwnsTarget() && Result.bShieldBroken)
		{
			OnShieldBroken.Broadcast(DamageInstigator, DamageCauser, Data.EffectSpec, Result.AppliedShieldDamage);
			SendSovEvent(Tags.Event_Shield_Broken, Result.AppliedShieldDamage, nullptr);
		}

		if (StillOwnsTarget() && Result.bPoiseBroken)
		{
			OnPoiseBroken.Broadcast(
				DamageInstigator,
				DamageCauser,
				Data.EffectSpec,
				Result.AppliedPoiseDamage);
			SendSovEvent(Tags.Event_Poise_Broken, Result.AppliedPoiseDamage, nullptr);
		}
		// Filter semantic status requests before any gameplay listener sees them. Parent immunity
		// is exact: a Freeze-only immunity must not silently become immunity to every status.
		FGameplayTagContainer CurrentTargetTags;
		Data.Target.GetOwnedGameplayTags(CurrentTargetTags);
		if (!StillOwnsTarget() || GetHealth() <= 0.f || !bSourceAllowsStatus || CurrentTargetTags.HasTagExact(Tags.Status_Immunity)
			|| CurrentTargetTags.HasTag(Tags.Status_Immunity_All)) { Result.RequestedStatusTags.Reset(); }
		else
		{
			if (Data.Target.HasMatchingGameplayTag(Tags.Status_Immunity_Chill)) { Result.RequestedStatusTags.RemoveTag(Tags.Status_Apply_Chill); }
			if (Data.Target.HasMatchingGameplayTag(Tags.Status_Immunity_Exposed)) { Result.RequestedStatusTags.RemoveTag(Tags.Status_Apply_Exposed); }
			if (Data.Target.HasMatchingGameplayTag(Tags.Status_Immunity_Corruption)) { Result.RequestedStatusTags.RemoveTag(Tags.Status_Apply_Corruption); }
			if (Data.Target.HasMatchingGameplayTag(Tags.Status_Immunity_Burn)) { Result.RequestedStatusTags.RemoveTag(Tags.Status_Apply_Burn); }
			if (Data.Target.HasMatchingGameplayTag(Tags.Status_Immunity_DeviceDisable)) { Result.RequestedStatusTags.RemoveTag(Tags.Status_Apply_DeviceDisabled); }
			if (Data.Target.HasMatchingGameplayTag(Tags.Status_Immunity_Freeze)
				|| Data.Target.HasMatchingGameplayTag(Tags.State_InterruptProtected)
				|| Data.Target.HasMatchingGameplayTag(Tags.State_Poise_Recovering)
				|| Data.Target.HasMatchingGameplayTag(Tags.State_Poise_SuperArmor))
			{ Result.RequestedStatusTags.RemoveTag(Tags.Status_Apply_Freeze); }
		}
		const float RequestedStatusMagnitude = Data.EffectSpec.GetSetByCallerMagnitude(
			Tags.SetByCaller_Status_Magnitude, false, 1.f);
		Result.bStatusApplicationRequested = StillOwnsTarget() && GetHealth() > 0.f
			&& !Result.bFatal && SovCombatTransaction::AcceptStatusRequest(
			!Result.RequestedStatusTags.IsEmpty() && FMath::IsFinite(RequestedStatusMagnitude)
				&& RequestedStatusMagnitude > KINDA_SMALL_NUMBER,
			Result.bPerfectDefense, Result.bGuarded,
			bControlPacket, AppliedDamage + Result.AppliedPoiseDamage, KINDA_SMALL_NUMBER);
		if (Result.bStatusApplicationRequested)
		{
			const float RawStatusMagnitude = Data.EffectSpec.GetSetByCallerMagnitude(
				Tags.SetByCaller_Status_Magnitude,
				false,
				1.f);
			const float StatusMagnitude = FMath::IsFinite(RawStatusMagnitude)
				? FMath::Max(RawStatusMagnitude, 0.f)
				: 0.f;
			const float RawStatusDuration = Data.EffectSpec.GetSetByCallerMagnitude(
				Tags.SetByCaller_Status_Duration,
				false,
				0.f);
			const float StatusDuration = FMath::IsFinite(RawStatusDuration)
				? FMath::Max(RawStatusDuration, 0.f)
				: 0.f;
			const float RawEffectLevel = Data.EffectSpec.GetLevel();
			const float StatusEffectLevel = FMath::IsFinite(RawEffectLevel)
				? FMath::Max(RawEffectLevel, 0.f)
				: 1.f;

			if (TargetASC && TargetActor && !EffectAssetTags.HasTagExact(Tags.Status_Application_NativeOwned))
			{
				// The container stores only explicitly authored descendants of
				// Sov.Status.Apply. Emit each exact leaf as its own typed request;
				// consumers deduplicate with (RequestId, StatusTag).
				for (const FGameplayTag& RequestedStatusTag : Result.RequestedStatusTags)
				{
					// Each leaf can invoke callbacks that retire this life or avatar.
					if (!StillOwnsTarget() || GetHealth() <= 0.f) { break; }
					FSovStatusApplicationRequest Request;
					Request.RequestId = Result.TransactionId;
					Request.StatusTag = RequestedStatusTag;
					Request.SourceActor = DamageInstigator;
					Request.TargetActor = TargetActor;
					Request.Magnitude = StatusMagnitude;
					Request.Duration = StatusDuration;
					Request.EffectLevel = StatusEffectLevel;
					Request.Context = Context;
					Request.SourceAbilityTags = SourceAbilityTags;
					Request.bRequiresAppliedDamage = !bControlPacket;
					Request.OriginatingDamageReceipt = Result.NativeConsumptionReceipt;
					TargetASC->StatusApplicationRequested(Request);
				}
			}

			// Preserve the original aggregate gameplay-event contract while
			// project listeners migrate to the lossless typed delegate.
			if (StillOwnsTarget() && GetHealth() > 0.f)
			{
				SendSovEvent(
					Tags.Event_Status_ApplicationRequested,
					StatusMagnitude,
					&Result.RequestedStatusTags);
			}
		}

		// Only this packet's still-current zero crossing owns death and kill rewards.
		Result.bFatal = Result.bFatal && StillOwnsTarget() && GetHealth() <= 0.f;
		if (Result.bFatal)
		{
			OnOutOfHealth.Broadcast(
				DamageInstigator,
				DamageCauser,
				Data.EffectSpec,
				AppliedDamage);

			Result.bFatal = Result.bFatal && StillOwnsTarget() && GetHealth() <= 0.f;
			if (Result.bFatal && StillOwnsSource() && SourceASC != TargetASC)
			{
				FGameplayEventData EventData;
				EventData.EventTag = FNarrativeGameplayTags::Get().GameplayEvent_KilledEnemy;
				EventData.Instigator = DamageInstigator;
				EventData.Target = GetOwningActor();

				SourceASC->HandleGameplayEvent(EventData.EventTag, &EventData);
			}
		}

		Result.bFatal = Result.bFatal && StillOwnsTarget() && GetHealth() <= 0.f;
		if (StillOwnsTarget())
		{
			TargetASC->DamageResolvedAsTarget(Result);
		}
		// Target-result listeners may restore the same avatar before source-side
		// kill/Echo consumers run. Do not carry fatal ownership into that new life.
		Result.bFatal = Result.bFatal && StillOwnsTarget() && GetHealth() <= 0.f;
		if (StillOwnsSource() && SourceASC != TargetASC)
		{
			SourceASC->DamageResolvedAsSource(Result);
		}
		if (Result.DefenseKind == ESovDefenseKind::Deflection)
		{
			// Keep the legacy notification contract immutable while exposing an
			// exact callback-local result tag to any presentation path that elects
			// to observe zero-damage defense transactions.
			FGameplayEffectSpec DeflectedNotificationSpec(Data.EffectSpec);
			DeflectedNotificationSpec.AddDynamicAssetTag(Tags.Damage_Result_Deflected);
			NotifyAppliedDamage(AppliedDamage, DeflectedNotificationSpec);
		}
		else if (Result.bGuarded)
		{
			// Preserve Narrative's legacy damage notification while giving its
			// presentation graph an exact, per-hit way to suppress a normal flinch.
			// The authored damage spec is immutable here, so tag a callback-local copy.
			FGameplayEffectSpec GuardedNotificationSpec(Data.EffectSpec);
			GuardedNotificationSpec.AddDynamicAssetTag(Tags.Damage_Result_Guarded);
			NotifyAppliedDamage(AppliedDamage, GuardedNotificationSpec);
		}
		else
		{
			NotifyAppliedDamage(AppliedDamage, Data.EffectSpec);
		}
		SendSovEvent(Tags.Event_Damage_Resolved, AppliedDamage, nullptr);

		return;
	}

	if (Data.EvaluatedData.Attribute == GetHealAttribute())
	{
		const float RequestedHeal = FMath::Max(GetHeal(), 0.f);
		SetHeal(0.f);

		if (!StillOwnsTarget() || GetHealth() <= 0.f || !FMath::IsFinite(RequestedHeal)) { return; }
		const float OldHealth = GetHealth();
		const float AppliedHeal = FMath::Min(RequestedHeal, FMath::Max(GetMaxHealth() - OldHealth, 0.f));
		SetHealth(FMath::Clamp(OldHealth + AppliedHeal, 0.f, GetMaxHealth()));

		if (AppliedHeal > 0.f && StillOwnsTarget() && StillOwnsSource())
		{
			TargetASC->HealedBy(SourceASC, AppliedHeal, Data.EffectSpec);
		}

		return;
	}

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetShieldAttribute())
	{
		SetShield(FMath::Clamp(GetShield(), 0.f, GetMaxShield()));
	}
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.f, GetMaxStamina()));
	}
	else if (Data.EvaluatedData.Attribute == GetPoiseAttribute())
	{
		SetPoise(FMath::Clamp(GetPoise(), 0.f, GetMaxPoise()));
	}
	else if (Data.EvaluatedData.Attribute == GetEchoAttribute())
	{
		SetEcho(FMath::Clamp(GetEcho(), 0.f, GetMaxEcho()));
	}
	else if (Data.EvaluatedData.Attribute == GetDamageResistanceAttribute())
	{
		SetDamageResistance(FMath::Clamp(GetDamageResistance(), -100.f, 95.f));
	}
	else if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		SetMaxHealth(FMath::Max(GetMaxHealth(), 0.f));
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetMaxShieldAttribute())
	{
		SetMaxShield(FMath::Max(GetMaxShield(), 0.f));
		SetShield(FMath::Clamp(GetShield(), 0.f, GetMaxShield()));
	}
	else if (Data.EvaluatedData.Attribute == GetMaxStaminaAttribute())
	{
		SetMaxStamina(FMath::Max(GetMaxStamina(), 0.f));
		SetStamina(FMath::Clamp(GetStamina(), 0.f, GetMaxStamina()));
	}
	else if (Data.EvaluatedData.Attribute == GetMaxPoiseAttribute())
	{
		SetMaxPoise(FMath::Max(GetMaxPoise(), 0.f));
		SetPoise(FMath::Clamp(GetPoise(), 0.f, GetMaxPoise()));
	}
	else if (Data.EvaluatedData.Attribute == GetMaxEchoAttribute())
	{
		SetMaxEcho(FMath::Max(GetMaxEcho(), 0.f));
		SetEcho(FMath::Clamp(GetEcho(), 0.f, GetMaxEcho()));
	}
}

void UNarrativeAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, XP, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, MaxShield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, Poise, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, MaxPoise, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, Echo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, MaxEcho, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, StaminaRegenRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, Armor, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, DamageResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, AttackRating, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, StealthRating, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, AttackDamage, COND_None, REPNOTIFY_Always);
}

void UNarrativeAttributeSetBase::AdjustAttributeForMaxChange(
	FGameplayAttributeData& AffectedAttribute,
	const FGameplayAttributeData& MaxAttribute,
	float NewMaxValue,
	const FGameplayAttribute& AffectedAttributeProperty)
{
	UAbilitySystemComponent* AbilityComponent = GetOwningAbilitySystemComponent();
	const float CurrentMaxValue = MaxAttribute.GetCurrentValue();

	if (FMath::IsNearlyEqual(CurrentMaxValue, NewMaxValue) || !AbilityComponent)
	{
		return;
	}

	const float CurrentValue = AffectedAttribute.GetCurrentValue();
	const float NewDelta = CurrentMaxValue > 0.f
		? (CurrentValue * NewMaxValue / CurrentMaxValue) - CurrentValue
		: NewMaxValue;

	AbilityComponent->ApplyModToAttributeUnsafe(
		AffectedAttributeProperty,
		EGameplayModOp::Additive,
		NewDelta);
}

void UNarrativeAttributeSetBase::OnRep_XP(const FGameplayAttributeData& OldXP)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, XP, OldXP);
}

void UNarrativeAttributeSetBase::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, Health, OldHealth);
}

void UNarrativeAttributeSetBase::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, MaxHealth, OldMaxHealth);
}

void UNarrativeAttributeSetBase::OnRep_Shield(const FGameplayAttributeData& OldShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, Shield, OldShield);
}

void UNarrativeAttributeSetBase::OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, MaxShield, OldMaxShield);
}

void UNarrativeAttributeSetBase::OnRep_Stamina(const FGameplayAttributeData& OldStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, Stamina, OldStamina);
}

void UNarrativeAttributeSetBase::OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, MaxStamina, OldMaxStamina);
}

void UNarrativeAttributeSetBase::OnRep_Poise(const FGameplayAttributeData& OldPoise)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, Poise, OldPoise);
}

void UNarrativeAttributeSetBase::OnRep_MaxPoise(const FGameplayAttributeData& OldMaxPoise)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, MaxPoise, OldMaxPoise);
}

void UNarrativeAttributeSetBase::OnRep_Echo(const FGameplayAttributeData& OldEcho)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, Echo, OldEcho);
}

void UNarrativeAttributeSetBase::OnRep_MaxEcho(const FGameplayAttributeData& OldMaxEcho)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, MaxEcho, OldMaxEcho);
}

void UNarrativeAttributeSetBase::OnRep_StaminaRegenRate(const FGameplayAttributeData& OldStaminaRegenRate)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, StaminaRegenRate, OldStaminaRegenRate);
}

void UNarrativeAttributeSetBase::OnRep_Armor(const FGameplayAttributeData& OldArmor)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, Armor, OldArmor);
}

void UNarrativeAttributeSetBase::OnRep_DamageResistance(const FGameplayAttributeData& OldDamageResistance)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, DamageResistance, OldDamageResistance);
}

void UNarrativeAttributeSetBase::OnRep_AttackRating(const FGameplayAttributeData& OldAttackRating)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, AttackRating, OldAttackRating);
}

void UNarrativeAttributeSetBase::OnRep_StealthRating(const FGameplayAttributeData& OldStealthRating)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, StealthRating, OldStealthRating);
}

void UNarrativeAttributeSetBase::OnRep_AttackDamage(const FGameplayAttributeData& OldAttackDamage)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, AttackDamage, OldAttackDamage);
}

UNarrativeCharacterAttributeSet::UNarrativeCharacterAttributeSet()
{
}
