// Copyright Narrative Tools 2024.

#include "GAS/NarrativeDamageExecCalc.h"

#include "ArsenalStatics.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "Settings/NarrativeCombatDeveloperSettings.h"
#include "UnrealFramework/NarrativePhysicalMaterial.h"

namespace NarrativeDamage
{
	struct FStatics
	{
		FGameplayEffectAttributeCaptureDefinition AttackDamageDef;
		FGameplayEffectAttributeCaptureDefinition AttackRatingDef;
		FGameplayEffectAttributeCaptureDefinition ArmorDef;

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
		}
	};

	FStatics& Statics()
	{
		static FStatics Instance;
		return Instance;
	}
}

UNarrativeDamageExecCalc::UNarrativeDamageExecCalc()
{
	RelevantAttributesToCapture.Add(NarrativeDamage::Statics().AttackDamageDef);
	RelevantAttributesToCapture.Add(NarrativeDamage::Statics().AttackRatingDef);
	RelevantAttributesToCapture.Add(NarrativeDamage::Statics().ArmorDef);
}

void UNarrativeDamageExecCalc::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();

	AActor* SourceActor = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
	AActor* TargetActor = TargetASC ? TargetASC->GetAvatarActor() : nullptr;

	if (SourceActor && TargetActor && SourceActor != TargetActor)
	{
		if (const UNarrativeCombatDeveloperSettings* CombatSettings = GetDefault<UNarrativeCombatDeveloperSettings>())
		{
			if (!CombatSettings->bAllowFriendlyFire
				&& UArsenalStatics::GetAttitude(SourceActor, TargetActor) == ETeamAttitude::Friendly)
			{
				return;
			}
		}
	}

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = SourceTags;
	EvaluationParameters.TargetTags = TargetTags;

	float BaseDamage = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		NarrativeDamage::Statics().AttackDamageDef,
		EvaluationParameters,
		BaseDamage);

	// Narrative Pro's generic DealDamage path supplies damage through SetByCaller.
	// When present, it is the base value for this execution rather than an addition
	// to the captured AttackDamage attribute.
	const float SetByCallerDamage = Spec.GetSetByCallerMagnitude(
		FNarrativeGameplayTags::Get().SetByCaller_Damage,
		false,
		-1.f);

	if (SetByCallerDamage >= 0.f)
	{
		BaseDamage = SetByCallerDamage;
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
	AttackRating = FMath::Max(AttackRating, 0.f);

	float Armor = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
		NarrativeDamage::Statics().ArmorDef,
		EvaluationParameters,
		Armor);
	Armor = FMath::Max(Armor, 0.f);

	float HitZoneMultiplier = 1.f;
	if (const FHitResult* Hit = Spec.GetContext().GetHitResult())
	{
		if (const UNarrativePhysicalMaterial* PhysicalMaterial = Cast<UNarrativePhysicalMaterial>(Hit->PhysMaterial.Get()))
		{
			HitZoneMultiplier = FMath::Max(PhysicalMaterial->DamageMultiplier, 0.f);
		}
	}

	const float SourceMultiplier = 1.f + (AttackRating / 100.f);
	const float MitigationDivisor = 1.f + (Armor / 100.f);
	const float FinalDamage = FMath::Max(
		(BaseDamage * SourceMultiplier * HitZoneMultiplier) / MitigationDivisor,
		0.f);

	if (FinalDamage > KINDA_SMALL_NUMBER)
	{
		// Emit one pre-shield amount. The AttributeSet performs Shield absorption,
		// Health overflow, break/death transitions, and one damage notification.
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UNarrativeAttributeSetBase::GetDamageAttribute(),
			EGameplayModOp::Additive,
			FinalDamage));
	}
}
