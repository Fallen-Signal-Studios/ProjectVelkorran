// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovPlayerCharacterBase.h"

#include "Character/CharacterDefinition.h"
#include "Character/PlayerDefinition.h"
#include "Components/SovEchoComponent.h"
#include "Components/SovCorruptionComponent.h"
#include "Components/SovHealthRechargeComponent.h"
#include "Components/SovPoiseComponent.h"
#include "Components/SovShieldComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Sovereign/SovGameplayTags.h"

ASovPlayerCharacterBase::ASovPlayerCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EchoComponent = CreateDefaultSubobject<USovEchoComponent>(TEXT("SovEchoComponent"));
	CorruptionComponent = CreateDefaultSubobject<USovCorruptionComponent>(TEXT("SovCorruptionComponent"));
	ShieldComponent = CreateDefaultSubobject<USovShieldComponent>(TEXT("SovShieldComponent"));
	HealthRechargeComponent = CreateDefaultSubobject<USovHealthRechargeComponent>(
		TEXT("SovHealthRechargeComponent"));
	PoiseComponent = CreateDefaultSubobject<USovPoiseComponent>(TEXT("SovPoiseComponent"));
}

bool ASovPlayerCharacterBase::PrepareCampaignInitialization(UPlayerDefinition* Definition)
{
	if (!HasAuthority() || GetController() || bCharacterReady || !IsValid(Definition)) { return false; }
	bCampaignManagedInitialization = true;
	SetPlayerDefinition(Definition);
	return true;
}

bool ASovPlayerCharacterBase::IsCampaignDataReadyToApply() const
{
	return bCampaignManagedInitialization && HasAuthority() && !bInitialPlayerDataApplied
		&& bAuthoritativeGameplayInitialized && bProjectSystemsInitialized
		&& bAbilitySystemReadyPublished && bVisualReadyForGameplay
		&& IsValid(InitializedAbilitySystem) && InitializedAbilitySystem->GetAvatarActor() == this
		&& InitializedPlayerDefinition == PlayerDefinition && AreAdditionalCharacterSystemsReady();
}

bool ASovPlayerCharacterBase::CompleteCampaignDataInitialization(bool bGrantDefaultInventory)
{
	if (!IsCampaignDataReadyToApply()) { return false; }
	if (bGrantDefaultInventory) { InitNewCharacter(GetCharacterDefinition()); }
	else { bInitializedNewCharacter = true; }
	NotifyInitialPlayerDataApplied();
	return IsCharacterReady();
}

void ASovPlayerCharacterBase::FailCampaignInitialization()
{
	bCampaignInitializationFailed = true;
	InvalidateCharacterReadiness();
}

void ASovPlayerCharacterBase::OnCharacterVisualInitialized()
{
	if (!bCampaignManagedInitialization) { Super::OnCharacterVisualInitialized(); return; }
	if (!bVisualReadyForGameplay)
	{
		// Skip NarrativePlayerCharacter's generic LoadPlayerData: its PawnData can
		// belong to the other protagonist. Preserve Narrative's visual notification.
		ANarrativeCharacter::OnCharacterVisualInitialized();
		bVisualReadyForGameplay = true;
	}
	TryFinalizeCharacterReadiness();
}

UNarrativeSaveWithCreatorData* ASovPlayerCharacterBase::GetCharacterCreatorData() const
{
	return bCampaignManagedInitialization ? nullptr : Super::GetCharacterCreatorData();
}

FGuid ASovPlayerCharacterBase::GetActorGUID_Implementation() const
{
	// Runtime identity must not be generated on the CDO and copied to every pawn.
	if (!CampaignSaveGuid.IsValid() && !IsTemplate())
	{
		const_cast<ASovPlayerCharacterBase*>(this)->CampaignSaveGuid = FGuid::NewGuid();
	}
	return CampaignSaveGuid;
}

void ASovPlayerCharacterBase::SetActorGUID_Implementation(const FGuid& SavedGUID)
{
	if (SavedGUID.IsValid()) { CampaignSaveGuid = SavedGUID; }
}

void ASovPlayerCharacterBase::HandleAbilitySystemReady(
	UNarrativeAbilitySystemComponent* ReadyAbilitySystem)
{
	Super::HandleAbilitySystemReady(ReadyAbilitySystem);

	if (EchoComponent)
	{
		EchoComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
	}
	if (ShieldComponent)
	{
		ShieldComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
	}
	if (HealthRechargeComponent)
	{
		HealthRechargeComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
	}
	if (PoiseComponent)
	{
		PoiseComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
	}
}

FGameplayTag ASovPlayerCharacterBase::GetProtagonistIdentityTag() const
{
	return FGameplayTag();
}

bool ASovPlayerCharacterBase::AreAdditionalCharacterSystemsReady() const
{
	return !bCampaignInitializationFailed && Super::AreAdditionalCharacterSystemsReady()
		&& IsValid(EchoComponent) && EchoComponent->IsInitialized()
		&& IsValid(ShieldComponent) && ShieldComponent->IsInitialized()
		&& IsValid(HealthRechargeComponent) && HealthRechargeComponent->IsInitialized()
		&& IsValid(PoiseComponent) && PoiseComponent->IsInitialized();
}

void ASovPlayerCharacterBase::OnDefinitionSet_Implementation(
	UCharacterDefinition* NewDefinition)
{
	Super::OnDefinitionSet_Implementation(NewDefinition);

	const FGameplayTag ProtagonistTag = GetProtagonistIdentityTag();
	if (!IsValid(NewDefinition) || !ProtagonistTag.IsValid())
	{
		return;
	}

	UNarrativeAbilitySystemComponent* NarrativeAbilitySystem =
		Cast<UNarrativeAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!IsValid(NarrativeAbilitySystem))
	{
		return;
	}

	FGameplayTagContainer DefinitionTags = NewDefinition->DefaultOwnedTags;
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	DefinitionTags.RemoveTag(SovTags.Character_Player_Tarrik);
	DefinitionTags.RemoveTag(SovTags.Character_Player_Selene);
	DefinitionTags.AddTag(ProtagonistTag);
	NarrativeAbilitySystem->SetDefinitionOwnedTags(DefinitionTags);
}
