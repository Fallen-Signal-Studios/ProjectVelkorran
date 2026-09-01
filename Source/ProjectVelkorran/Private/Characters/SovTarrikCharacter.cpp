// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovTarrikCharacter.h"

#include "Components/SovGuardComponent.h"
#include "Components/SovTarrikEchoGenerationComponent.h"
#include "Sovereign/SovGameplayTags.h"

ASovTarrikCharacter::ASovTarrikCharacter(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TarrikEchoGenerationComponent =
		CreateDefaultSubobject<USovTarrikEchoGenerationComponent>(
			TEXT("SovTarrikEchoGenerationComponent"));
	GuardComponent = CreateDefaultSubobject<USovGuardComponent>(
		TEXT("SovGuardComponent"));
}

FGameplayTag ASovTarrikCharacter::GetProtagonistIdentityTag() const
{
	return FSovGameplayTags::Get().Character_Player_Tarrik;
}

void ASovTarrikCharacter::HandleAbilitySystemReady(
	UNarrativeAbilitySystemComponent* ReadyAbilitySystem)
{
	Super::HandleAbilitySystemReady(ReadyAbilitySystem);

	if (TarrikEchoGenerationComponent)
	{
		TarrikEchoGenerationComponent->InitializeWithAbilitySystem(
			ReadyAbilitySystem);
	}
	if (GuardComponent)
	{
		GuardComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
	}
}

bool ASovTarrikCharacter::AreAdditionalCharacterSystemsReady() const
{
	return Super::AreAdditionalCharacterSystemsReady()
		&& IsValid(TarrikEchoGenerationComponent)
		&& TarrikEchoGenerationComponent->IsInitialized()
		&& IsValid(GuardComponent)
		&& GuardComponent->IsInitialized();
}

void ASovTarrikCharacter::OnRep_WieldState(
	const FWeaponWieldState& OldWieldState)
{
	Super::OnRep_WieldState(OldWieldState);

	if (TarrikEchoGenerationComponent)
	{
		TarrikEchoGenerationComponent->HandleOwnerWieldStateChanged();
	}
}
