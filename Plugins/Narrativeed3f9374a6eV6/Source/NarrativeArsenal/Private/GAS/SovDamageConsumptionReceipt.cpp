// Copyright Fallen Signal Studios. All Rights Reserved.
#include "GAS/SovCombatTypes.h"

struct FSovDamageConsumptionReceipt
{
	FGuid Transaction;
	TMap<TWeakObjectPtr<const UObject>, uint8> Consumers;
};

void FSovDamageResult::InitializeNativeReceipt()
{
	NativeConsumptionReceipt = MakeShared<FSovDamageConsumptionReceipt>();
	NativeConsumptionReceipt->Transaction = TransactionId;
}

bool FSovDamageResult::HasNativeReceipt() const
{
	return NativeConsumptionReceipt.IsValid() && TransactionId.IsValid()
		&& NativeConsumptionReceipt->Transaction == TransactionId;
}

bool FSovDamageResult::ConsumeNativeReceipt(const UObject* Consumer, uint8 Channel) const
{
	if (!IsInGameThread() || !IsValid(Consumer) || Channel >= 8 || !HasNativeReceipt()) { return false; }
	const TWeakObjectPtr<const UObject> Key(Consumer);
	uint8* Mask = NativeConsumptionReceipt->Consumers.Find(Key);
	// A programming error cannot turn one damage receipt into an unbounded observer registry.
	if (!Mask && NativeConsumptionReceipt->Consumers.Num() >= 32) { return false; }
	if (!Mask) { Mask = &NativeConsumptionReceipt->Consumers.Add(Key, 0); }
	const uint8 Bit = static_cast<uint8>(1u << Channel);
	if ((*Mask & Bit) != 0) { return false; }
	*Mask |= Bit;
	return true;
}
