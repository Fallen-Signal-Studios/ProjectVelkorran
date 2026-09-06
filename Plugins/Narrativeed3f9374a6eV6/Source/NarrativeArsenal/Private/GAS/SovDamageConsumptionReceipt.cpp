// Copyright Fallen Signal Studios. All Rights Reserved.
#include "GAS/SovCombatTypes.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Actor.h"

struct FSovDamageConsumptionReceipt
{
	FGuid Transaction;
	TWeakObjectPtr<const UNarrativeAttributeSetBase> TargetAttributes;
	TWeakObjectPtr<UNarrativeAbilitySystemComponent> TargetASC;
	TWeakObjectPtr<AActor> TargetActor;
	uint64 TargetLifeEpoch = 0;
	uint64 TargetActorInfoEpoch = 0;
	TMap<TWeakObjectPtr<const UObject>, uint8> Consumers;
};

namespace SovDamageReceipt
{
	bool IsCurrentTarget(const TSharedPtr<FSovDamageConsumptionReceipt>& Receipt,
		const FGuid& Transaction, const AActor* Target)
	{
		if (!IsInGameThread() || !Receipt.IsValid() || !Transaction.IsValid()
			|| Receipt->Transaction != Transaction) { return false; }
		const auto* Attributes = Receipt->TargetAttributes.Get();
		UNarrativeAbilitySystemComponent* ASC = Receipt->TargetASC.Get();
		AActor* Avatar = Receipt->TargetActor.Get();
		return IsValid(Attributes) && IsValid(ASC) && IsValid(Avatar)
			&& !Avatar->IsActorBeingDestroyed() && Avatar->HasAuthority()
			&& Target == Avatar && ASC->GetAvatarActor() == Avatar
			&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Avatar) == ASC
			&& Attributes->GetOwningAbilitySystemComponent() == ASC
			&& ASC->GetSet<UNarrativeAttributeSetBase>() == Attributes
			&& Attributes->GetCombatLifeEpoch() == Receipt->TargetLifeEpoch
			&& ASC->GetCombatActorInfoEpoch() == Receipt->TargetActorInfoEpoch;
	}
}

void FSovDamageResult::InitializeNativeReceipt(const UNarrativeAttributeSetBase* TargetAttributes)
{
	NativeConsumptionReceipt.Reset();
	if (!IsInGameThread() || !IsValid(TargetAttributes) || !TransactionId.IsValid()) { return; }
	UNarrativeAbilitySystemComponent* ASC = Cast<UNarrativeAbilitySystemComponent>(TargetAttributes->GetOwningAbilitySystemComponent());
	AActor* Avatar = IsValid(ASC) ? ASC->GetAvatarActor() : nullptr;
	if (!IsValid(Avatar) || !Avatar->HasAuthority() || Avatar->IsActorBeingDestroyed()
		|| ASC->GetSet<UNarrativeAttributeSetBase>() != TargetAttributes
		|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Avatar) != ASC) { return; }
	NativeConsumptionReceipt = MakeShared<FSovDamageConsumptionReceipt>();
	NativeConsumptionReceipt->Transaction = TransactionId;
	NativeConsumptionReceipt->TargetAttributes = TargetAttributes;
	NativeConsumptionReceipt->TargetASC = ASC;
	NativeConsumptionReceipt->TargetActor = Avatar;
	NativeConsumptionReceipt->TargetLifeEpoch = TargetAttributes->GetCombatLifeEpoch();
	NativeConsumptionReceipt->TargetActorInfoEpoch = ASC->GetCombatActorInfoEpoch();
}

bool FSovDamageResult::HasNativeReceipt() const
{
	return NativeConsumptionReceipt.IsValid() && TransactionId.IsValid()
		&& NativeConsumptionReceipt->Transaction == TransactionId;
}

bool FSovDamageResult::IsCurrentTargetLife(const UAbilitySystemComponent* ExpectedTargetASC) const
{
	return SovDamageReceipt::IsCurrentTarget(NativeConsumptionReceipt, TransactionId, TargetActor.Get())
		&& (!ExpectedTargetASC || NativeConsumptionReceipt->TargetASC.Get() == ExpectedTargetASC);
}

bool FSovStatusApplicationRequest::IsCurrentDamageOrigin() const
{
	return !OriginatingDamageReceipt.IsValid()
		|| SovDamageReceipt::IsCurrentTarget(OriginatingDamageReceipt, RequestId, TargetActor.Get());
}

bool FSovDamageResult::ConsumeNativeReceipt(const UObject* Consumer, const uint8 Channel) const
{
	if (!IsInGameThread() || !IsValid(Consumer) || Channel >= 8 || !IsCurrentTargetLife()) { return false; }
	const TWeakObjectPtr<const UObject> Key(Consumer);
	uint8* Mask = NativeConsumptionReceipt->Consumers.Find(Key);
	// Keep memory bounded by one live packet, never by an actor's lifetime hit count.
	if (!Mask && NativeConsumptionReceipt->Consumers.Num() >= 32) { return false; }
	if (!Mask) { Mask = &NativeConsumptionReceipt->Consumers.Add(Key, 0); }
	const uint8 Bit = static_cast<uint8>(1u << Channel);
	if ((*Mask & Bit) != 0) { return false; }
	*Mask |= Bit;
	return true;
}
