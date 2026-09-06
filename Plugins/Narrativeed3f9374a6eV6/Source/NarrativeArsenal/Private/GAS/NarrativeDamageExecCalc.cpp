// Copyright Narrative Tools 2024.

#include "GAS/NarrativeDamageExecCalc.h"

#include "ArsenalStatics.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "UObject/StrongObjectPtr.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/SovCombatTransactionPolicy.h"
#include "GAS/SovDamageChannelPolicy.h"
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

	struct FChannel { FGameplayTag Channel; FGameplayTag Immunity; };
	TArray<FChannel, TInlineAllocator<7>> Channels()
	{
		const auto& T = FSovGameplayTags::Get();
		return {{T.Damage_Channel_Kinetic, T.Damage_Immunity_Kinetic}, {T.Damage_Channel_Edge, T.Damage_Immunity_Edge},
			{T.Damage_Channel_Thermal, T.Damage_Immunity_Thermal}, {T.Damage_Channel_Echo, T.Damage_Immunity_Echo},
			{T.Damage_Channel_Disruption, T.Damage_Immunity_Disruption}, {T.Damage_Channel_Corruption, T.Damage_Immunity_Corruption},
			{T.Damage_Channel_Environmental, T.Damage_Immunity_Environmental}};
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
	// Team queries can execute authored code, including nested damage. Bound
	// admission itself, before a packet reaches the AttributeSet resolver.
	SovCombatTransaction::FScopedCallbackBudget AdmissionScope(SovCombatTransaction::ThreadCallbackBudget());
	if (!AdmissionScope.IsAdmitted() || !IsValid(TargetASC)) { return true; }
	AActor* SourceActor = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
	AActor* TargetActor = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
	const UNarrativeAttributeSetBase* TargetAttributes = TargetASC->GetSet<UNarrativeAttributeSetBase>();
	const uint64 TargetLifeEpoch = TargetAttributes ? TargetAttributes->GetCombatLifeEpoch() : 0;
	const auto* SourceAttributes = SourceASC ? SourceASC->GetSet<UNarrativeAttributeSetBase>() : nullptr;
	const uint64 SourceLifeEpoch = SourceAttributes ? SourceAttributes->GetCombatLifeEpoch() : 0;
	const auto* NarrativeTargetASC = Cast<UNarrativeAbilitySystemComponent>(TargetASC);
	const auto* NarrativeSourceASC = Cast<UNarrativeAbilitySystemComponent>(SourceASC);
	const uint64 TargetActorInfoEpoch = NarrativeTargetASC ? NarrativeTargetASC->GetCombatActorInfoEpoch() : 0;
	const uint64 SourceActorInfoEpoch = NarrativeSourceASC ? NarrativeSourceASC->GetCombatActorInfoEpoch() : 0;
	TStrongObjectPtr<const UAbilitySystemComponent> SourceLifetime(SourceASC);
	TStrongObjectPtr<const UAbilitySystemComponent> TargetLifetime(TargetASC);
	TStrongObjectPtr<const UNarrativeAttributeSetBase> AttributeLifetime(TargetAttributes);
	TStrongObjectPtr<const UNarrativeAttributeSetBase> SourceAttributeLifetime(SourceAttributes);
	TStrongObjectPtr<AActor> SourceActorLifetime(SourceActor);
	TStrongObjectPtr<AActor> TargetActorLifetime(TargetActor);
	FGameplayTagContainer EffectTags;
	Spec.GetAllAssetTags(EffectTags);
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	const bool bFatalPolicy = EffectTags.HasTagExact(SovTags.Damage_Fatal);
	if (!bFatalPolicy
		&& TargetASC
		&& (TargetASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable)
			|| TargetASC->HasMatchingGameplayTag(SovTags.State_Invulnerable)
			|| TargetASC->HasMatchingGameplayTag(SovTags.State_Damage_Immune)
			|| GetAcceptedChannelFraction(TargetASC, Spec) <= 0.f))
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

	// A team callback must not admit the old packet into a restored life, a
	// different avatar, or an AttributeSet that was removed during admission.
	return !IsValid(TargetASC) || !IsValid(TargetActor) || TargetActor->IsActorBeingDestroyed()
		|| TargetASC->GetAvatarActor() != TargetActor
		|| (NarrativeTargetASC && NarrativeTargetASC->GetCombatActorInfoEpoch() != TargetActorInfoEpoch)
		|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor) != TargetASC
		|| (TargetAttributes && (!IsValid(TargetAttributes)
			|| TargetASC->GetSet<UNarrativeAttributeSetBase>() != TargetAttributes
			|| TargetAttributes->GetCombatLifeEpoch() != TargetLifeEpoch || TargetAttributes->GetHealth() <= 0.f))
		|| (SourceASC && (!IsValid(SourceASC) || !IsValid(SourceActor) || SourceActor->IsActorBeingDestroyed()
			|| SourceASC->GetAvatarActor() != SourceActor
			|| (NarrativeSourceASC && NarrativeSourceASC->GetCombatActorInfoEpoch() != SourceActorInfoEpoch)
			|| (SourceAttributes && (!IsValid(SourceAttributes)
				|| SourceASC->GetSet<UNarrativeAttributeSetBase>() != SourceAttributes
				|| SourceAttributes->GetCombatLifeEpoch() != SourceLifeEpoch))
			|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor) != SourceASC));
}

float UNarrativeDamageExecCalc::GetAcceptedChannelFraction(const UAbilitySystemComponent* TargetASC,
	const FGameplayEffectSpec& Spec, FGameplayTagContainer* OutRejectedChannels)
{
	FGameplayTagContainer AssetTags;
	Spec.GetAllAssetTags(AssetTags);
	const bool bImmuneToAll=TargetASC && TargetASC->HasMatchingGameplayTag(FSovGameplayTags::Get().Damage_Immunity_All);
	SovDamageChannels::FPortion Portions[7];
	int32 Count = 0;
	for (const auto& Channel : NarrativeDamage::Channels())
	{
		if (!AssetTags.HasTagExact(Channel.Channel)) { continue; }
		const bool bImmune = bImmuneToAll || (TargetASC && TargetASC->HasMatchingGameplayTag(Channel.Immunity));
		const float Weight = Spec.GetSetByCallerMagnitude(Channel.Channel, false, 1.f);
		Portions[Count++] = {Weight, bImmune, 1.};
		if (OutRejectedChannels && (bImmune || Weight <= 0.f || !FMath::IsFinite(Weight))) { OutRejectedChannels->AddTag(Channel.Channel); }
	}
	// Untagged legacy packets retain ordinary damage behavior, but cannot bypass global immunity.
	return bImmuneToAll ? 0.f : static_cast<float>(SovDamageChannels::Resolve(Portions, Count));
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
	float PrimaryVariationDelta = 0.f;
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
				PrimaryVariationDelta = VariedWeaponDamage - FixedWeaponDamage;
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
	const float ExplicitSourceModifier = FMath::Max(
		NarrativeDamage::GetSetByCallerOrDefault(Spec, SovTags.SetByCaller_Damage_SourceModifier, 1.f),
		0.f);
	const float DifficultyScalar = FMath::Max(
		NarrativeDamage::GetSetByCallerOrDefault(Spec, SovTags.SetByCaller_Damage_DifficultyScalar, 1.f),
		0.f);

	const float AuthoredMitigationMultiplier = FMath::Max(
		NarrativeDamage::GetSetByCallerOrDefault(Spec, SovTags.SetByCaller_Damage_MitigationMultiplier, 1.f), 0.f);
	const UNarrativeCombatDeveloperSettings* CombatSettings = GetDefault<UNarrativeCombatDeveloperSettings>();
	const float MinimumMultiplier = CombatSettings ? FMath::Max(CombatSettings->MinimumDamageMultiplier, 0.f) : 0.f;
	const float MaximumMultiplier = CombatSettings ? FMath::Max(CombatSettings->MaximumDamageMultiplier, MinimumMultiplier) : TNumericLimits<float>::Max();
	const auto EvaluateMultiplier = [&]()
	{
		float AttackRating = 0.f, Armor = 0.f, Resistance = 0.f;
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(NarrativeDamage::Statics().AttackRatingDef, EvaluationParameters, AttackRating);
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(NarrativeDamage::Statics().ArmorDef, EvaluationParameters, Armor);
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(NarrativeDamage::Statics().DamageResistanceDef, EvaluationParameters, Resistance);
		if (!FMath::IsFinite(AttackRating) || !FMath::IsFinite(Armor) || !FMath::IsFinite(Resistance)) { return 0.f; }
		const float ArmorMultiplier = EffectTags.HasTagExact(SovTags.Damage_IgnoreArmor) ? 1.f : 1.f / (1.f + FMath::Max(Armor, 0.f) / 100.f);
		const float ResistanceMultiplier = EffectTags.HasTagExact(SovTags.Damage_IgnoreResistance) ? 1.f : 1.f - FMath::Clamp(Resistance, -100.f, 95.f) / 100.f;
		float ChannelBaseDamage = BaseDamage;
		if (AuthoredBaseDamage < 0.f)
		{
			ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(NarrativeDamage::Statics().AttackDamageDef, EvaluationParameters, ChannelBaseDamage);
			ChannelBaseDamage = FMath::Max(ChannelBaseDamage + PrimaryVariationDelta, 0.f);
		}
		if (!FMath::IsFinite(ChannelBaseDamage)) { return 0.f; }
		return ChannelBaseDamage * (1.f + FMath::Max(AttackRating, 0.f) / 100.f) * FMath::Clamp(ArmorMultiplier * ResistanceMultiplier * AuthoredMitigationMultiplier, MinimumMultiplier, MaximumMultiplier);
	};
	SovDamageChannels::FPortion Portions[7];
	int32 Count = 0;
	for (const auto& Channel : NarrativeDamage::Channels()) { EvaluationSourceTags.RemoveTag(Channel.Channel); }
	for (const auto& Channel : NarrativeDamage::Channels())
	{
		if (!EffectTags.HasTagExact(Channel.Channel)) { continue; }
		EvaluationSourceTags.AddTag(Channel.Channel);
		Portions[Count++] = {Spec.GetSetByCallerMagnitude(Channel.Channel, false, 1.f),
			TargetASC && TargetASC->HasMatchingGameplayTag(Channel.Immunity), EvaluateMultiplier()};
		EvaluationSourceTags.RemoveTag(Channel.Channel);
	}
	const float ChannelDamage = Count ? static_cast<float>(SovDamageChannels::Resolve(Portions, Count)) : EvaluateMultiplier();

	const float ResolvedDamage = bAlreadyResolved
		? BaseDamage * (bFatalPolicy ? 1.f : GetAcceptedChannelFraction(TargetASC, Spec))
		: FMath::Max(
			ChannelDamage
			* AbilityScalar
			* ExplicitSourceModifier
			* HitZoneMultiplier
			* DifficultyScalar,
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
