// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Combat/Pickups/SovEchoCombatSustainPickup.h"

#include "Characters/SovPlayerCharacterBase.h"
#include "Components/SovEchoComponent.h"
#include "Net/UnrealNetwork.h"
#include "Sovereign/SovGameplayTags.h"

void ASovEchoCombatSustainPickup::InitializeEcho(const float InEchoAmount)
{
	if (!HasAuthority())
	{
		return;
	}

	EchoAmount = FMath::Max(InEchoAmount, 0.01f);
}

bool ASovEchoCombatSustainPickup::TryGrantTo(
	ASovPlayerCharacterBase* CollectingPlayer)
{
	if (!IsValid(CollectingPlayer))
	{
		return false;
	}

	USovEchoComponent* EchoComponent = CollectingPlayer->GetEchoComponent();
	if (!IsValid(EchoComponent))
	{
		return false;
	}
	if (EchoComponent->GetEcho()
		>= EchoComponent->GetMaxEcho() - KINDA_SMALL_NUMBER)
	{
		// Do not call AddEcho while full: AddEcho also records combat activity,
		// which would let an uncollected mote postpone inactivity decay.
		return false;
	}

	const float GrantedEcho = EchoComponent->AddEcho(
		FMath::Max(EchoAmount, 0.01f),
		FSovGameplayTags::Get().Echo_Source_CombatSustainPickup);
	return GrantedEcho > KINDA_SMALL_NUMBER;
}

void ASovEchoCombatSustainPickup::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASovEchoCombatSustainPickup, EchoAmount);
}
