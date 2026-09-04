// Copyright Narrative Tools 2024.

#include "GAS/NarrativeDamageExecCalc.h"

#include "ArsenalStatics.h"
#include "AbilitySystemComponent.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/SovCombatTransactionPolicy.h"
#include "GameplayEffect.h"
#include "Items/RangedWeaponItem.h"
#include "NarrativeGameplayTags.h"
#include "Settings/NarrativeCombatDeveloperSettings.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativePhysicalMaterial.h"

namespace NarrativeDamage
{
	struct FStatics
	{
		FGameplayEffectAttributeCaptureDefinition AttackDamageDef;
		FGameplayEffectAttributeCaptureDefinition AttackRatingDef;
		FGameplayEffectAttributeCaptureDefinition ArmorDef;
		FGameplayEffectAttributeCaptureDefinition DamageResistanceDef;

		FStatics()
		{
			AttackDamageDef = FGameplayEffectAttributeCaptureDefinition(
				UNarrativeAttributeSetBase::GetAttackDamageAttribute(),
				EGameplayEffectAttributeCaptureSource::Source,
				true);
			AttackRatingDef = FGameplayEffectAttributeCaptureDefinition(
				UNarrativeAttributeSetBase::GetAttackRatingAttribute(),
				EGameplayEffectAttributeCaptureSource::Source,
				true);
			ArmorDef = FGameplayEffectAttributeCaptureDefinition(
				UNarrativeAttributeSetBase::GetArmorAttribute(),
				EGameplayEffectAttributeCaptureSource::Target,
				false);
			DamageResistanceDef = FGameplayEffectAttributeCaptureDefinition(
				UNarrativeAttributeSetBase::GetDamageResistanceAttribute(),
				EGameplayEffectAttributeCaptureSource::Target,
				false);
		}
	};

	FStatics& Statics()
	{
		static FStatics Instance;
		return Instance;
	}

	float GetSetByCallerOrDefault(
		const FGameplayEffectSpec& Spec,
		const FGameplayTag& Tag,
		const float DefaultValue)
	{
		return Spec.GetSetByCallerMagnitude(Tag, false, DefaultValue);
	}

	const URangedWeaponItem* GetVariationWeaponForOrdinaryPrimaryFire(
		const FGameplayEffectSpec& Spec,
		const FGameplayTagContainer& EffectTags)
	{
		// Periodic damage, authored payloads, and non-hit transactions should never
		// inherit a weapon's primary-fire variation policy.
		if (!Spec.Def
			|| Spec.Def->DurationPolicy != EGameplayEffectDurationType::Instant)
		{
			return nullptr;
		}

		const FGameplayEffectContextHandle& Context = Spec.GetContext();
		const FHitResult* HitResult = Context.GetHitResult();
		if (!HitResult || !HitResult->bBlockingHit)
		{
			return nullptr;
		}

		const URangedWeaponItem* SourceWeapon = Cast<URangedWeaponItem>(Context.GetSourceObject());
		if (!IsValid(SourceWeapon) || !SourceWeapon->HasDamageVariation())
		{
			return nullptr;
		}

		const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
		if (EffectTags.HasTag(SovTags.Ability_Echo))
		{
			return nullptr;
		}

		const UNarrativeGameplayAbility* SourceAbility =
			Cast<UNarrativeGameplayAbility>(Context.GetAbility());
		if (!IsValid(SourceAbility))
		{
			return nullptr;
		}

		FGameplayTagContainer AbilityTags;
		AbilityTags.AppendTags(SourceAbility->GetAssetTags());
		if (AbilityTags.HasTag(SovTags.Ability_Echo))
		{
			return nullptr;
		}

		const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
		const bool bExplicitCinderlinePrimary =
			AbilityTags.HasTagExact(SovTags.Ability_Weapon_Cinderline_PrimaryFire)
			|| EffectTags.HasTagExact(SovTags.Ability_Weapon_Cinderline_PrimaryFire);
		const bool bGenericNarrativePrimary =
			SourceAbility->InputTag == NarrativeTags.Narrative_Input_Attack
			&& AbilityTags.HasTagExact(NarrativeTags.Ability_WeaponFire)
			&& AbilityTags.HasTagExact(NarrativeTags.Ability_DamageType_Ranged);
		return bExplicitCinderlinePrimary || bGenericNarrativePrimary
			? SourceWeapon
			: nullptr;
	}

	float GetDeterministicHitDistance(
		const FHitResult& HitResult,
		const AActor* SourceActor)
	{
		if (HitResult.Distance > KINDA_SMALL_NUMBER)
		{
			return HitResult.Distance;
		}

		const FVector ImpactPoint = HitResult.ImpactPoint.IsNearlyZero()
			? HitResult.Location
			: HitResult.ImpactPoint;
		if (!HitResult.TraceStart.Equals(ImpactPoint))
		{
			return FVector::Distance(HitResult.TraceStart, ImpactPoint);
		}

		return SourceActor
			? FVector::Distance(SourceActor->GetActorLocation(), ImpactPoint)
			: 0.f;
	}

	bool IsImmuneToChannels(
		const UAbilitySystemComponent* TargetASC,
		const FGameplayTagContainer& EffectTags)
	{
		if (!TargetASC)
		{
			return false;
		}

		const FSovGameplayTags& Tags = FSovGameplayTags::Get();
		if (TargetASC->HasMatchingGameplayTag(Tags.Damage_Immunity_All))
		{
			return true;
		}

		// Channels are combinable. Until attacks carry per-channel weights, reject
		// the transaction only when every declared channel is immune. Conditional
		// resistance modifiers still evaluate against the full channel container.
		bool bHasDeclaredChannel = false;
		bool bAllDeclaredChannelsImmune = true;
		const auto AccumulateChannelImmunity = [
			TargetASC,
			&EffectTags,
			&bHasDeclaredChannel,
			&bAllDeclaredChannelsImmune](
				const FGameplayTag& ChannelTag,
				const FGameplayTag& ImmunityTag)
		{
			if (EffectTags.HasTagExact(ChannelTag))
			{
				bHasDeclaredChannel = true;
				bAllDeclaredChannelsImmune &=
					TargetASC->HasMatchingGameplayTag(ImmunityTag);
			}
		};

		AccumulateChannelImmunity(Tags.Damage_Channel_Kinetic, Tags.Damage_Immunity_Kinetic);
		AccumulateChannelImmunity(Tags.Damage_Channel_Edge, Tags.Damage_Immunity_Edge);
		AccumulateChannelImmunity(Tags.Damage_Channel_Thermal, Tags.Damage_Immunity_Thermal);
		AccumulateChannelImmunity(Tags.Damage_Channel_Echo, Tags.Damage_Immunity_Echo);
		AccumulateChannelImmunity(Tags.Damage_Channel_Disruption, Tags.Damage_Immunity_Disruption);
		AccumulateChannelImmunity(Tags.Damage_Channel_Corruption, Tags.Damage_Immunity_Corruption);
		AccumulateChannelImmunity(Tags.Damage_Channel_Environmental, Tags.Damage_Immunity_Environmental);
		return bHasDeclaredChannel && bAllDeclaredChannelsImmune;
	}
}

UNarrativeDamageExecCalc::UNarrativeDamageExecCalc()
{
	RelevantAttributesToCapture.Add(NarrativeDamage::Statics().AttackDamageDef);
	RelevantAttributesToCapture.Add(NarrativeDamage::Statics().AttackRatingDef);
	RelevantAttributesToCapture.Add(NarrativeDamage::Statics().ArmorDef);
	RelevantAttributesToCapture.Add(NarrativeDamage::Statics().DamageResistanceDef);
}

bool UNarrativeDamageExecCalc::ShouldRejectTransaction(
	const UAbilitySystemComponent* SourceASC,
	const UAbilitySystemComponent* TargetASC,
	const FGameplayEffectSpec& Spec)
{
	AActor* SourceActor = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
	AActor* TargetActor = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
	FGameplayTagContainer EffectTags;
	Spec.GetAllAssetTags(EffectTags);
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	const bool bFatalPolicy = EffectTags.HasTagExact(SovTags.Damage_Fatal);
	if (!bFatalPolicy
		&& TargetASC
		&& (TargetASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable)
			|| TargetASC->HasMatchingGameplayTag(SovTags.State_Invulnerable)
			|| TargetASC->HasMatchingGameplayTag(SovTags.State_Damage_Immune)
			|| NarrativeDamage::IsImmuneToChannels(TargetASC, EffectTags)))
	{
		return true;
	}

	if (!bFatalPolicy && SourceActor && TargetActor && SourceActor != TargetActor)
	{
		const UNarrativeCombatDeveloperSettings* CombatSettings = GetDefault<UNarrativeCombatDeveloperSettings>();
		const bool bAllowsFriendlyFire = EffectTags.HasTagExact(SovTags.Damage_AllowFriendlyFire)
			|| (CombatSettings && CombatSettings->bAllowFriendlyFire);
		if (!bAllowsFriendlyFire
			&& UArsenalStatics::GetAttitude(SourceActor, TargetActor) == ETeamAttitude::Friendly)
		{
			return true;
		}
	}

	return false;
}

void UNarrativeDamageExecCalc::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	AActor* SourceActor = SourceASC ? SourceASC->GetAvatarActor() : nullptr;

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	FGameplayTagContainer EffectTags;
	Spec.GetAllAssetTags(EffectTags);

	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	const bool bFatalPolicy = EffectTags.HasTagExact(SovTags.Damage_Fatal);
	const bool bAlreadyResolved = bFatalPolicy
		|| EffectTags.HasTagExact(SovTags.Damage_AlreadyResolved);
	if (ShouldRejectTransaction(SourceASC, TargetASC, Spec))
	{
		return;
	}

	FGameplayTagContainer EvaluationSourceTags;
	if (const FGameplayTagContainer* CapturedSourceTags = Spec.CapturedSourceTags.GetAggregatedTags())
	{
		EvaluationSourceTags.AppendTags(*CapturedSourceTags);
	}
	EvaluationSourceTags.AppendTags(EffectTags);

	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = &EvaluationSourceTags;
	EvaluationParameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float BaseDamage = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		NarrativeDamage::Statics().AttackDamageDef,
		EvaluationParameters,
		BaseDamage);

	const float AuthoredBaseDamage = Spec.GetSetByCallerMagnitude(
		FNarrativeGameplayTags::Get().SetByCaller_Damage,
		false,
		-1.f);
	if (AuthoredBaseDamage >= 0.f)
	{
		BaseDamage = AuthoredBaseDamage;
	}
	else if (!bAlreadyResolved)
	{
		// Variation replaces only the source weapon's fixed contribution, preserving
		// any other captured AttackDamage modifiers. Explicit SetByCaller damage is
		// deliberately excluded so Echo attacks, explosions, and authored specials
		// remain stable.
		if (const URangedWeaponItem* VariationWeapon =
			NarrativeDamage::GetVariationWeaponForOrdinaryPrimaryFire(Spec, EffectTags))
		{
			if (const FHitResult* HitResult = Spec.GetContext().GetHitResult())
			{
				const float FixedWeaponDamage = VariationWeapon->GetAttackDamage();
				const float VariedWeaponDamage = VariationWeapon->ResolveAttackDamageForDistance(
					NarrativeDamage::GetDeterministicHitDistance(*HitResult, SourceActor));
				BaseDamage = FMath::Max(
					BaseDamage - FixedWeaponDamage + VariedWeaponDamage,
					0.f);
			}
		}
	}

	if (!FMath::IsFinite(BaseDamage))
	{
		return;
	}
	BaseDamage = FMath::Max(BaseDamage, 0.f);

	float AttackRating = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		NarrativeDamage::Statics().AttackRatingDef,
		EvaluationParameters,
		AttackRating);

	float Armor = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		NarrativeDamage::Statics().ArmorDef,
		EvaluationParameters,
		Armor);

	float Resistance = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		NarrativeDamage::Statics().DamageResistanceDef,
		EvaluationParameters,
		Resistance);

	float HitZoneMultiplier = 1.f;
	if (const FHitResult* Hit = Spec.GetContext().GetHitResult())
	{
		if (const UNarrativePhysicalMaterial* PhysicalMaterial = Cast<UNarrativePhysicalMaterial>(Hit->PhysMaterial.Get()))
		{
			HitZoneMultiplier = FMath::Max(PhysicalMaterial->DamageMultiplier, 0.f);
		}
	}
	HitZoneMultiplier = FMath::Max(
		NarrativeDamage::GetSetByCallerOrDefault(
			Spec,
			SovTags.SetByCaller_Damage_HitZoneModifier,
			HitZoneMultiplier),
		0.f);

	const float AbilityScalar = FMath::Max(
		NarrativeDamage::GetSetByCallerOrDefault(Spec, SovTags.SetByCaller_Damage_AbilityScalar, 1.f),
		0.f);
	const float AttackRatingMultiplier = 1.f + (FMath::Max(AttackRating, 0.f) / 100.f);
	const float ExplicitSourceModifier = FMath::Max(
		NarrativeDamage::GetSetByCallerOrDefault(Spec, SovTags.SetByCaller_Damage_SourceModifier, 1.f),
		0.f);
	const float DifficultyScalar = FMath::Max(
		NarrativeDamage::GetSetByCallerOrDefault(Spec, SovTags.SetByCaller_Damage_DifficultyScalar, 1.f),
		0.f);

	const float ArmorMultiplier = EffectTags.HasTagExact(SovTags.Damage_IgnoreArmor)
		? 1.f
		: 1.f / (1.f + (FMath::Max(Armor, 0.f) / 100.f));
	const float ResistanceMultiplier = EffectTags.HasTagExact(SovTags.Damage_IgnoreResistance)
		? 1.f
		: 1.f - (FMath::Clamp(Resistance, -100.f, 95.f) / 100.f);
	const float AuthoredMitigationMultiplier = FMath::Max(
		NarrativeDamage::GetSetByCallerOrDefault(Spec, SovTags.SetByCaller_Damage_MitigationMultiplier, 1.f),
		0.f);

	const UNarrativeCombatDeveloperSettings* CombatSettings = GetDefault<UNarrativeCombatDeveloperSettings>();
	const float MinimumMultiplier = CombatSettings ? FMath::Max(CombatSettings->MinimumDamageMultiplier, 0.f) : 0.f;
	const float MaximumMultiplier = CombatSettings
		? FMath::Max(CombatSettings->MaximumDamageMultiplier, MinimumMultiplier)
		: TNumericLimits<float>::Max();
	const float MitigationMultiplier = FMath::Clamp(
		ArmorMultiplier * ResistanceMultiplier * AuthoredMitigationMultiplier,
		MinimumMultiplier,
		MaximumMultiplier);

	const float ResolvedDamage = bAlreadyResolved
		? BaseDamage
		: FMath::Max(
			BaseDamage
			* AbilityScalar
			* AttackRatingMultiplier
			* ExplicitSourceModifier
			* HitZoneMultiplier
			* DifficultyScalar
			* MitigationMultiplier,
			0.f);

	const float ExplicitPoise = Spec.GetSetByCallerMagnitude(SovTags.SetByCaller_Damage_PoiseDamage, false, 0.f);
	bool bHasStatusRequest = false;
	for (const FGameplayTag& Tag : EffectTags)
	{
		bHasStatusRequest |= Tag != SovTags.Status_Apply && Tag.MatchesTag(SovTags.Status_Apply);
	}
	const auto Packet = SovCombatTransaction::SelectPacket(ResolvedDamage, ExplicitPoise, bHasStatusRequest,
		Spec.GetSetByCallerMagnitude(SovTags.SetByCaller_Status_Magnitude, false, 1.f), KINDA_SMALL_NUMBER);
	FGameplayAttribute Attribute;
	float Magnitude = 0.f;
	switch (Packet)
	{
	case SovCombatTransaction::EPacket::Body:
		Attribute = UNarrativeAttributeSetBase::GetDamageAttribute(); Magnitude = ResolvedDamage; break;
	case SovCombatTransaction::EPacket::Poise:
		Attribute = UNarrativeAttributeSetBase::GetPoiseDamageAttribute(); Magnitude = ExplicitPoise; break;
	case SovCombatTransaction::EPacket::Control:
		Attribute = UNarrativeAttributeSetBase::GetControlRequestAttribute(); Magnitude = 1.f; break;
	default: return;
	}
	// One pre-routing packet preserves the existing defense, break and telemetry ordering.
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(Attribute, EGameplayModOp::Additive, Magnitude));
}
