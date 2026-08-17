// Copyright Narrative Tools 2024.

#include "GAS/NarrativeDamageExecCalc.h"

#include "ArsenalStatics.h"
#include "AbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
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

void UNarrativeDamageExecCalc::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	AActor* SourceActor = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
	AActor* TargetActor = TargetASC ? TargetASC->GetAvatarActor() : nullptr;

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	FGameplayTagContainer EffectTags;
	Spec.GetAllAssetTags(EffectTags);

	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	const bool bFatalPolicy = EffectTags.HasTagExact(SovTags.Damage_Fatal);
	const bool bAlreadyResolved = bFatalPolicy
		|| EffectTags.HasTagExact(SovTags.Damage_AlreadyResolved);
	if (!bFatalPolicy
		&& TargetASC
		&& (TargetASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable)
			|| TargetASC->HasMatchingGameplayTag(SovTags.State_Invulnerable)
			|| TargetASC->HasMatchingGameplayTag(SovTags.State_Damage_Immune)
			|| NarrativeDamage::IsImmuneToChannels(TargetASC, EffectTags)))
	{
		return;
	}

	if (!bFatalPolicy && SourceActor && TargetActor && SourceActor != TargetActor)
	{
		const UNarrativeCombatDeveloperSettings* CombatSettings = GetDefault<UNarrativeCombatDeveloperSettings>();
		const bool bAllowsFriendlyFire = EffectTags.HasTagExact(SovTags.Damage_AllowFriendlyFire)
			|| (CombatSettings && CombatSettings->bAllowFriendlyFire);
		if (!bAllowsFriendlyFire
			&& UArsenalStatics::GetAttitude(SourceActor, TargetActor) == ETeamAttitude::Friendly)
		{
			return;
		}
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

	BaseDamage = FMath::Max(BaseDamage, 0.f);
	if (BaseDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

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

	if (ResolvedDamage > KINDA_SMALL_NUMBER)
	{
		// One pre-routing packet preserves deterministic guard, Shield, Health,
		// Poise, break, death, and telemetry ordering in the AttributeSet.
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UNarrativeAttributeSetBase::GetDamageAttribute(),
			EGameplayModOp::Additive,
			ResolvedDamage));
	}
}
