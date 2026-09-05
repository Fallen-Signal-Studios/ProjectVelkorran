// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovFieldRecoveryRuntimeTestFixtures.h"
#include "FieldRecovery/SovFieldRecoveryComponent.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
void ASovFieldRecoveryTestCharacter::InitializeSharedState(ASovPlayerState* State, bool bSelene)
{
	SetPlayerState(State);
	AbilitySystemComponent = CastChecked<UNarrativeAbilitySystemComponent>(State->GetAbilitySystemComponent());
	AttributeSetBase = State->GetAttributeSetBase();
	InitializeExertion(bSelene);
	AbilitySystemComponent->InitAbilityActorInfo(State, this);
	GetFieldRecoveryComponent()->InitializeWithAbilitySystem(AbilitySystemComponent);
}
