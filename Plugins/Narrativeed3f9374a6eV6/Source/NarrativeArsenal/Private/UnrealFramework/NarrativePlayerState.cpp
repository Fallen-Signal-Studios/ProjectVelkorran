// Copyright Narrative Tools 2024. 


#include "UnrealFramework/NarrativePlayerState.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Items/InventoryComponent.h"
#include "SkillTrees/SkillTreeComponent.h"
#include "Navigation/NavigationMarkerComponent.h"
#include <Net/UnrealNetwork.h>
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "Engine/World.h"
#include "Character/CharacterMapMarker.h"

ANarrativePlayerState::ANarrativePlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Create ability system component, and set it to be explicitly replicated
	AbilitySystemComponent = CreateDefaultSubobject<UNarrativeAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSetBase = CreateDefaultSubobject<UNarrativeAttributeSetBase>(TEXT("AttributeSetBase"));

	SkillTreeComponent = CreateDefaultSubobject<USkillTreeComponent>(TEXT("SkillTreeComponent"));

	SetNetUpdateFrequency(100.0f);
}

class ANarrativeCharacter* ANarrativePlayerState::GetNarrativeCharacter() const
{
	return Cast<ANarrativeCharacter>(GetPawn());
}

class UAbilitySystemComponent* ANarrativePlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

class UNarrativeAttributeSetBase* ANarrativePlayerState::GetAttributeSetBase() const
{
	return AttributeSetBase;
}

FGameplayTagContainer ANarrativePlayerState::GetFactions() const
{
	return Factions;
}

void ANarrativePlayerState::AddFaction(const FGameplayTag& Faction)
{
	Factions.AddTag(Faction);
	OnRep_Faction();
}

void ANarrativePlayerState::RemoveFaction(const FGameplayTag& Faction)
{
	Factions.RemoveTag(Faction);
	OnRep_Faction();
}

bool ANarrativePlayerState::IsAlive() const
{
	return GetHealth() > 0.f;
}

float ANarrativePlayerState::GetHealth() const
{
	return AttributeSetBase->GetHealth();
}

void ANarrativePlayerState::BeginPlay()
{
	Super::BeginPlay();
}

void ANarrativePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANarrativePlayerState, Factions);
}

void ANarrativePlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();

	if (ANarrativePlayerCharacter* PlayerCharacter = Cast<ANarrativePlayerCharacter>(GetPawn()))
	{
		if (UCharacterMapMarker* Marker = PlayerCharacter->GetMarkerComponent())
		{
			Marker->DefaultMarkerSettings.MarkerTitleText = FText::FromString(GetPlayerName());
			Marker->RefreshMarker();
		}
	}
}

void ANarrativePlayerState::SetFactions(const FGameplayTagContainer& NewFactions)
{
	if (NewFactions.IsValid())
	{
		Factions = NewFactions;
		OnRep_Faction();
	}
}

void ANarrativePlayerState::OnRep_Faction()
{
	if (ANarrativePlayerCharacter* PlayerCharacter = Cast<ANarrativePlayerCharacter>(GetPawn()))
	{
		if (UCharacterMapMarker* Marker = PlayerCharacter->GetMarkerComponent())
		{
			Marker->RefreshMarker();
		}

		PlayerCharacter->OnFactionUpdated.Broadcast();
	}
}
//
//ETeamAttitude::Type ANarrativePlayerState::GetTeamAttitudeTowards(const AActor& Other) const
//{
//	//TODO use GS 
//	if(const INarrativeTeamAgentInterface* OtherTeamAgent = Cast<const INarrativeTeamAgentInterface>(&Other))
//	{
//		if (Faction)
//		{
//			ENarrativeFactionID OurFaction = Faction->FactionID;
//			ENarrativeFactionID TheirFaction = static_cast<ENarrativeFactionID>(OtherTeamAgent->GetGenericTeamId().GetId());
//
//			if (ANarrativeGameState* GS = Cast<ANarrativeGameState>(GetWorld()->GetGameState()))
//			{
//				return GS->GetAttitudeTowards(OurFaction, TheirFaction);
//			}
//
//		}
//	}
//
//	return ETeamAttitude::Neutral;
//}
