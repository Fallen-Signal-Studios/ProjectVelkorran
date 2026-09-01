// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovSeleneCharacter.h"

#include "Components/SovDeflectionComponent.h"
#include "Components/SovSeleneEchoGenerationComponent.h"
#include "Sovereign/SovGameplayTags.h"

ASovSeleneCharacter::ASovSeleneCharacter(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DeflectionComponent = CreateDefaultSubobject<USovDeflectionComponent>(
		TEXT("SovDeflectionComponent"));
	SeleneEchoGenerationComponent =
		CreateDefaultSubobject<USovSeleneEchoGenerationComponent>(
			TEXT("SovSeleneEchoGenerationComponent"));
}

FGameplayTag ASovSeleneCharacter::GetProtagonistIdentityTag() const
{
	return FSovGameplayTags::Get().Character_Player_Selene;
}

void ASovSeleneCharacter::HandleAbilitySystemReady(
	UNarrativeAbilitySystemComponent* ReadyAbilitySystem)
{
	Super::HandleAbilitySystemReady(ReadyAbilitySystem);

	if (DeflectionComponent)
	{
		DeflectionComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
	}
	if (SeleneEchoGenerationComponent)
	{
		SeleneEchoGenerationComponent->InitializeWithAbilitySystem(
			ReadyAbilitySystem);
	}
}

bool ASovSeleneCharacter::AreAdditionalCharacterSystemsReady() const
{
	return Super::AreAdditionalCharacterSystemsReady()
		&& IsValid(DeflectionComponent)
		&& DeflectionComponent->IsInitialized()
		&& IsValid(SeleneEchoGenerationComponent)
		&& SeleneEchoGenerationComponent->IsInitialized();
}
