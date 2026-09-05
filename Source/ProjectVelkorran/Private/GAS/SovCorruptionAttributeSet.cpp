// Copyright Fallen Signal Studios. All Rights Reserved.

#include "GAS/SovCorruptionAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

USovCorruptionAttributeSet::USovCorruptionAttributeSet()
{
	InitMaxCorruption(100.0f);
	InitCorruption(0.0f);
}

void USovCorruptionAttributeSet::PreAttributeChange(
	const FGameplayAttribute& Attribute,
	float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxCorruptionAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetCorruptionAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxCorruption());
	}
}

void USovCorruptionAttributeSet::PostAttributeChange(
	const FGameplayAttribute& Attribute,
	const float OldValue,
	const float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxCorruptionAttribute())
	{
		// PreAttributeChange already clamps the new maximum. Only the dependent
		// value needs reconciliation here; never write the attribute whose
		// PostAttributeChange callback is currently executing.
		const float ClampedCorruption = FMath::Clamp(
			GetCorruption(),
			0.0f,
			FMath::Max(NewValue, 1.0f));
		if (ClampedCorruption != GetCorruption())
		{
			SetCorruption(ClampedCorruption);
		}
	}
}

void USovCorruptionAttributeSet::PostGameplayEffectExecute(
	const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetMaxCorruptionAttribute())
	{
		SetMaxCorruption(FMath::Max(GetMaxCorruption(), 1.0f));
		SetCorruption(FMath::Clamp(GetCorruption(), 0.0f, GetMaxCorruption()));
	}
	else if (Data.EvaluatedData.Attribute == GetCorruptionAttribute())
	{
		SetCorruption(FMath::Clamp(GetCorruption(), 0.0f, GetMaxCorruption()));
	}
}

void USovCorruptionAttributeSet::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(
		USovCorruptionAttributeSet,
		Corruption,
		COND_None,
		REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(
		USovCorruptionAttributeSet,
		MaxCorruption,
		COND_None,
		REPNOTIFY_Always);
}

void USovCorruptionAttributeSet::OnRep_Corruption(
	const FGameplayAttributeData& OldCorruption)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(
		USovCorruptionAttributeSet,
		Corruption,
		OldCorruption);
}

void USovCorruptionAttributeSet::OnRep_MaxCorruption(
	const FGameplayAttributeData& OldMaxCorruption)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(
		USovCorruptionAttributeSet,
		MaxCorruption,
		OldMaxCorruption);
}
