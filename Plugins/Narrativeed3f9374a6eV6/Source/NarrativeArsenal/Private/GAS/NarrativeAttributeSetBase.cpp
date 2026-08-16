// Copyright Narrative Tools 2024.

#include "GAS/NarrativeAttributeSetBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemGlobals.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "GameplayTagContainer.h"
#include "NarrativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
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
}

void UNarrativeAttributeSetBase::PostAttributeChange(
	const FGameplayAttribute& Attribute,
	float OldValue,
	float NewValue)
{
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

	if (Data.EvaluatedData.Attribute == GetDamageAttribute()
		&& Data.EvaluatedData.Magnitude > 0.f
		&& Data.Target.HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable))
	{
		Data.EvaluatedData.Magnitude = 0.f;
		return false;
	}

	return true;
}

void UNarrativeAttributeSetBase::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
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

	AController* SourceController = nullptr;
	if (ANarrativeCharacter* SourceCharacter = Cast<ANarrativeCharacter>(SourceActor))
	{
		SourceController = SourceCharacter->GetController();
	}
	else if (SourceASC && SourceASC->AbilityActorInfo.IsValid())
	{
		SourceController = SourceASC->AbilityActorInfo->PlayerController.Get();
	}

	if (Context.GetEffectCauser())
	{
		SourceActor = Context.GetEffectCauser();
	}

	const auto NotifyAppliedDamage = [
		SourceASC,
		TargetASC,
		SourceActor,
		TargetActor,
		SourceController,
		&Data](const float AppliedDamage)
	{
		if (AppliedDamage <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		if (ANarrativePlayerController* PlayerController = Cast<ANarrativePlayerController>(SourceController))
		{
			if (TargetActor && TargetActor != SourceActor)
			{
				PlayerController->NotifyDealtDamage(TargetActor, AppliedDamage);
			}
		}

		if (SourceASC && TargetASC && SourceASC != TargetASC)
		{
			TargetASC->DamagedBy(SourceASC, AppliedDamage, Data.EffectSpec);
			SourceASC->DealtDamage(TargetASC, AppliedDamage, Data.EffectSpec);
		}
	};

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float IncomingDamage = FMath::Max(GetDamage(), 0.f);
		SetDamage(0.f);

		if (IncomingDamage <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const float OldShield = FMath::Max(GetShield(), 0.f);
		const float OldHealth = FMath::Max(GetHealth(), 0.f);

		FGameplayTagContainer EffectAssetTags;
		Data.EffectSpec.GetAllAssetTags(EffectAssetTags);

		static const FGameplayTag BypassShieldTag = FGameplayTag::RequestGameplayTag(
			FName(TEXT("Sov.Damage.BypassShield")),
			false);
		const bool bBypassesShield = BypassShieldTag.IsValid()
			&& EffectAssetTags.HasTagExact(BypassShieldTag);

		const float ShieldDamage = bBypassesShield
			? 0.f
			: FMath::Min(OldShield, IncomingDamage);
		const float RemainingDamage = bBypassesShield
			? IncomingDamage
			: FMath::Max(IncomingDamage - ShieldDamage, 0.f);
		const float HealthDamage = FMath::Min(OldHealth, RemainingDamage);

		if (ShieldDamage > 0.f)
		{
			SetShield(FMath::Clamp(OldShield - ShieldDamage, 0.f, GetMaxShield()));
		}

		if (RemainingDamage > 0.f)
		{
			SetHealth(FMath::Clamp(OldHealth - RemainingDamage, 0.f, GetMaxHealth()));
		}

		const float AppliedDamage = ShieldDamage + HealthDamage;
		NotifyAppliedDamage(AppliedDamage);

		AActor* Instigator = Context.GetOriginalInstigator();
		AActor* Causer = Context.GetEffectCauser();

		if (OldShield > 0.f && GetShield() <= 0.f)
		{
			OnShieldBroken.Broadcast(Instigator, Causer, Data.EffectSpec, ShieldDamage);
		}

		if (OldHealth > 0.f && GetHealth() <= 0.f)
		{
			OnOutOfHealth.Broadcast(Instigator, Causer, Data.EffectSpec, AppliedDamage);

			if (Instigator && Instigator != GetOwningActor())
			{
				FGameplayEventData EventData;
				EventData.EventTag = FNarrativeGameplayTags::Get().GameplayEvent_KilledEnemy;
				EventData.Instigator = Instigator;
				EventData.Target = GetOwningActor();

				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
					Instigator,
					EventData.EventTag,
					EventData);
			}
		}

		return;
	}

	if (Data.EvaluatedData.Attribute == GetPoiseDamageAttribute())
	{
		const float IncomingPoiseDamage = FMath::Max(GetPoiseDamage(), 0.f);
		SetPoiseDamage(0.f);

		if (IncomingPoiseDamage <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const float OldPoise = FMath::Max(GetPoise(), 0.f);
		SetPoise(FMath::Clamp(OldPoise - IncomingPoiseDamage, 0.f, GetMaxPoise()));
		const float AppliedPoiseDamage = FMath::Max(OldPoise - GetPoise(), 0.f);

		if (OldPoise > 0.f && GetPoise() <= 0.f)
		{
			OnPoiseBroken.Broadcast(
				Context.GetOriginalInstigator(),
				Context.GetEffectCauser(),
				Data.EffectSpec,
				AppliedPoiseDamage);
		}

		return;
	}

	if (Data.EvaluatedData.Attribute == GetHealAttribute())
	{
		const float RequestedHeal = FMath::Max(GetHeal(), 0.f);
		SetHeal(0.f);

		const float OldHealth = GetHealth();
		SetHealth(FMath::Clamp(OldHealth + RequestedHeal, 0.f, GetMaxHealth()));
		const float AppliedHeal = GetHealth() - OldHealth;

		if (AppliedHeal > 0.f && SourceASC && TargetASC)
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
