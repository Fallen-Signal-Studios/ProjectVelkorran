// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovTarrikCharacter.h"

#include "Components/SovGuardComponent.h"
#include "Components/SovTarrikEchoGenerationComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
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

void ASovTarrikCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Legacy protagonist Blueprints can serialize null compatibility slots even
	// though their concrete native class still constructs these subobjects.
	// Restore only null slots from exact native subobjects owned by this pawn.
	// Nonnull mismatches and extra authored components retain canonical rejection.
	if (!GuardComponent)
	{
		auto* NativeGuard = Cast<USovGuardComponent>(
			GetDefaultSubobjectByName(TEXT("SovGuardComponent")));
		if (IsValid(NativeGuard) && NativeGuard->GetOwner() == this
			&& NativeGuard->CreationMethod == EComponentCreationMethod::Native)
		{
			GuardComponent = NativeGuard;
		}
	}
	if (!TarrikEchoGenerationComponent)
	{
		auto* NativeGenerator = Cast<USovTarrikEchoGenerationComponent>(
			GetDefaultSubobjectByName(TEXT("SovTarrikEchoGenerationComponent")));
		if (IsValid(NativeGenerator) && NativeGenerator->GetOwner() == this
			&& NativeGenerator->CreationMethod == EComponentCreationMethod::Native)
		{
			TarrikEchoGenerationComponent = NativeGenerator;
		}
	}
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
