// Copyright Narrative Tools 2024. 


#include "GAS/NarrativeAttributeSetBase.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "NarrativeGameplayTags.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemBlueprintLibrary.h"


UNarrativeAttributeSetBase::UNarrativeAttributeSetBase()
{

}

void UNarrativeAttributeSetBase::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		//AdjustAttributeForMaxChange(Health, MaxHealth, NewValue, GetHealthAttribute());
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		//AdjustAttributeForMaxChange(Stamina, MaxStamina, NewValue, GetStaminaAttribute());
	}
	else if (Attribute == GetMaxShieldAttribute())
	{
		//AdjustAttributeForMaxChange(Shield, MaxShield, NewValue, GetShieldAttribute());
	}
	else if (Attribute == GetMaxEchoAttribute())
	{
		//AdjustAttributeForMaxChange(Echo, MaxEcho, NewValue, GetEchoAttribute());
	}
}

bool UNarrativeAttributeSetBase::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		if (Data.EvaluatedData.Magnitude > 0.f)
		{
			//Return false to throw out the execution if we're invulnerable 
			if (Data.Target.HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable))
			{
				Data.EvaluatedData.Magnitude = 0.f;
				return false;
			}
		}
	}
	return true;
}

void UNarrativeAttributeSetBase::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	UNarrativeAbilitySystemComponent* SourceASC = Cast<UNarrativeAbilitySystemComponent>(Context.GetOriginalInstigatorAbilitySystemComponent());
	const FGameplayTagContainer& SourceTags = *Data.EffectSpec.CapturedSourceTags.GetAggregatedTags();
	FGameplayTagContainer SpecAssetTags;
	Data.EffectSpec.GetAllAssetTags(SpecAssetTags);

	// Get the Target actor, which should be our owner
	AActor* TargetActor = nullptr;
	AController* TargetController = nullptr;
	ANarrativeCharacter* TargetCharacter = nullptr;
	UNarrativeAbilitySystemComponent* TargetASC = nullptr;

	if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
	{
		TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
		TargetCharacter = Cast<ANarrativeCharacter>(TargetActor);

		if (TargetActor)
		{
			TargetASC = Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor));
		}


		//Since the project has AIControllers actor ability info won't have a valid ref, try get from character
		if (TargetCharacter)
		{
			TargetController = TargetCharacter->GetController();

		}
		else
		{
			TargetController = Data.Target.AbilityActorInfo->PlayerController.Get();
		}
	}

	// Get the Source actor
	AActor* SourceActor = nullptr;
	AController* SourceController = nullptr;
	ANarrativeCharacter* SourceCharacter = nullptr;

	if (SourceASC && SourceASC->AbilityActorInfo.IsValid() && SourceASC->AbilityActorInfo->AvatarActor.IsValid())
	{
		SourceActor = SourceASC->AbilityActorInfo->AvatarActor.Get();
		SourceCharacter = Cast<ANarrativeCharacter>(SourceActor);

		if (SourceCharacter)
		{
			SourceController = SourceCharacter->GetController();
		}
		else
		{
			SourceController = SourceASC->AbilityActorInfo->PlayerController.Get();
		}

		// Set the causer actor d on context if it's set
		if (Context.GetEffectCauser())
		{
			SourceActor = Context.GetEffectCauser();
		}
	}

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		// Try to extract a hit result
		FHitResult HitResult;
		if (Context.GetHitResult())
		{
			HitResult = *Context.GetHitResult();
		}

		// Store a local copy of the amount of damage done and clear the damage attribute.
		// DamageDealt is the post-shield overflow (health damage only). TotalIncomingDamage is the
		// full damage before shield absorption, read directly off the spec so we can notify even on
		// pure shield hits where DamageDealt == 0.
		const float DamageDealt = GetDamage();
		SetDamage(0.f);

		const float TotalIncomingDamage = Data.EffectSpec.GetSetByCallerMagnitude(
			FNarrativeGameplayTags::Get().SetByCaller_Damage, false, 0.f);

		if (DamageDealt > 0.0f)
		{
			// Energy shield absorption happens up-front in UNarrativeDamageExecCalc, which routes only the
			// overflow (damage the shield couldn't absorb) into this Damage meta attribute. So here we simply
			// apply the overflow to Health and clamp it.
			const float NewHealth = GetHealth() - DamageDealt;
			SetHealth(FMath::Clamp(NewHealth, 0.0f, GetMaxHealth()));
		}

		// Notify on every hit that had non-zero incoming damage — including pure shield hits where
		// DamageDealt == 0. This ensures health bars and aggro systems fire even when shields absorb
		// the full hit. We pass TotalIncomingDamage so listeners know how hard the hit was.
		const float NotifyDamage = TotalIncomingDamage > 0.f ? TotalIncomingDamage : DamageDealt;
		if (NotifyDamage > 0.0f)
		{
			if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(SourceController))
			{
				if (TargetActor != SourceActor)
				{
					if (!TargetCharacter || TargetCharacter->IsAlive())
					{
						if (TargetActor)
						{
							PC->NotifyDealtDamage(TargetActor, NotifyDamage);
						}
					}
				}
			}

			if (SourceASC && TargetASC)
			{
				TargetASC->DamagedBy(SourceASC, NotifyDamage, Data.EffectSpec);
				SourceASC->DealtDamage(TargetASC, NotifyDamage, Data.EffectSpec);
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealAttribute()) // Heal metaattribute 
	{
		const float HealAmount = GetHeal();

		//Binding to attribute changed doesn't give us valid instigator data as GEModData is null, so we do this as a workaround 
		if (SourceASC && TargetASC)
		{
			TargetASC->HealedBy(SourceASC, HealAmount, Data.EffectSpec);
		}

		SetHealth(FMath::Clamp(GetHealth() + HealAmount, 0.0f, GetMaxHealth()));
		SetHeal(0.f);
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// Handle other health changes.
		// Health loss should go through Damage.
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		// Handle stamina changes.
		SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
	}
	else if (Data.EvaluatedData.Attribute == GetShieldAttribute())
	{
		SetShield(FMath::Clamp(GetShield(), 0.0f, GetMaxShield()));

		// When the exec calc fully absorbs a hit with shields, DamageDealt == 0 so the Damage
		// meta-attribute block never runs and PostGameplayEffectExecute is only called here.
		// Fire DealtDamage/DamagedBy so health bars, aggro, and other listeners still trigger.
		// Data.EvaluatedData.Magnitude is the raw modifier from the exec calc (negative = absorbed).
		const float ShieldLost = -Data.EvaluatedData.Magnitude;
		if (ShieldLost > 0.f && SourceASC && TargetASC && SourceASC != TargetASC)
		{
			if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(SourceController))
			{
				if (!TargetCharacter || TargetCharacter->IsAlive())
				{
					if (TargetActor)
					{
						PC->NotifyDealtDamage(TargetActor, ShieldLost);
					}
				}
			}

			TargetASC->DamagedBy(SourceASC, ShieldLost, Data.EffectSpec);
			SourceASC->DealtDamage(TargetASC, ShieldLost, Data.EffectSpec);
		}
	}
	else if (Data.EvaluatedData.Attribute == GetEchoAttribute())
	{
		// Handle echo changes.
		SetEcho(FMath::Clamp(GetEcho(), 0.0f, GetMaxEcho()));
	}


	if (GetHealth() <= 0.f)
	{
		const FGameplayEffectContextHandle& EffectContext = Data.EffectSpec.GetEffectContext();
		AActor* Instigator = EffectContext.GetOriginalInstigator();
		AActor* Causer = EffectContext.GetEffectCauser();

		OnOutOfHealth.Broadcast(Instigator, Causer, Data.EffectSpec, Data.EvaluatedData.Magnitude);

		// Notify the killer's ASC so kill-driven abilities (e.g. the Emperor's Wrath super,
		// which listens for GameplayEvent.KilledEnemy to extend its duration) can react.
		if (Instigator && Instigator != GetOwningActor())
		{
			FGameplayEventData EventData;
			EventData.EventTag = FNarrativeGameplayTags::Get().GameplayEvent_KilledEnemy;
			EventData.Instigator = Instigator;
			EventData.Target = GetOwningActor();

			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Instigator, EventData.EventTag, EventData);
		}
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
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, Echo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, MaxEcho, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, StaminaRegenRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, Armor, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, AttackRating, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, StealthRating, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNarrativeAttributeSetBase, AttackDamage, COND_None, REPNOTIFY_Always);
}

void UNarrativeAttributeSetBase::AdjustAttributeForMaxChange(FGameplayAttributeData& AffectedAttribute, const FGameplayAttributeData& MaxAttribute, float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	const float CurrentMaxValue = MaxAttribute.GetCurrentValue();
	if (!FMath::IsNearlyEqual(CurrentMaxValue, NewMaxValue) && AbilityComp)
	{
		// Change current value to maintain the current Val / Max percent
		const float CurrentValue = AffectedAttribute.GetCurrentValue();
		float NewDelta = (CurrentMaxValue > 0.f) ? (CurrentValue * NewMaxValue / CurrentMaxValue) - CurrentValue : NewMaxValue;

		AbilityComp->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Additive, NewDelta);
	}
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

void UNarrativeAttributeSetBase::OnRep_AttackDamage(const FGameplayAttributeData& OldAttackDamage)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, AttackDamage, OldAttackDamage);
}

void UNarrativeAttributeSetBase::OnRep_StealthRating(const FGameplayAttributeData& OldStealthRating)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNarrativeAttributeSetBase, StealthRating, OldStealthRating);
}

UNarrativeCharacterAttributeSet::UNarrativeCharacterAttributeSet()
{

}
