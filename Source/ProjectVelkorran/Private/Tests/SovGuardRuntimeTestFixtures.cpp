// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovGuardRuntimeTestFixtures.h"

#include "Components/SovGuardComponent.h"
#include "Components/SovEchoComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

ASovGuardRuntimeTestCharacter::ASovGuardRuntimeTestCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TestGuard = CreateDefaultSubobject<USovGuardComponent>(TEXT("TestGuard"));
}

void ASovGuardRuntimeTestCharacter::InitializeGuardCombat()
{
	InitializeTestCombat(1);
	GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Tarrik);
	TestGuard->InitializeWithAbilitySystem(GetNarrativeAbilitySystemComponent());
	TestEcho->RestoreEchoFromCheckpoint(0.f);
	TestGuard->OnGuardStarted.AddUniqueDynamic(this, &ThisClass::ObserveGuardStart);
	TestGuard->OnGuardEnded.AddUniqueDynamic(this, &ThisClass::ObserveGuardEnd);
}

void ASovGuardRuntimeTestCharacter::ObserveGuardStart()
{
	++GuardStartCount;
	if (bInterruptGuardOnStart)
	{
		GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Movement_Ragdoll);
	}
}

void ASovGuardRuntimeTestCharacter::ObserveGuardEnd()
{
	++GuardEndCount;
}

USovGuardRuntimeBusyTestAbility::USovGuardRuntimeBusyTestAbility()
{
	ActivationOwnedTags.AddTag(FNarrativeGameplayTags::Get().State_Busy);
}
