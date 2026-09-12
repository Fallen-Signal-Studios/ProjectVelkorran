// Copyright Narrative Tools 2024. 


#include "UnrealFramework/NarrativePlayerState.h"
#include "AI/NarrativeAIStartupDiagnostics.h"
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
	FNarrativeAIStartupDiagnostics::Record(this, TEXT("player_factions_assigned"), Factions.ToString());
	if (ANarrativePlayerCharacter* PlayerCharacter = Cast<ANarrativePlayerCharacter>(GetPawn()))
	{
		if (UCharacterMapMarker* Marker = PlayerCharacter->GetMarkerComponent())
		{
			Marker->RefreshMarker();
		}

		FNarrativeAIStartupDiagnostics::Record(PlayerCharacter, TEXT("player_faction_publication"), Factions.ToString());
		PlayerCharacter->OnFactionUpdated.Broadcast();

		// OnFactionUpdated is a bare member on the character, so nothing outside the character
		// can bind it. Republish the same fact through the game state, which is the broker AI
		// already reaches for faction questions.
		//
		// This is the edge that was missing: an NPC that perceived this character before its
		// factions existed resolved Neutral, rejected it, and then received no further signal -
		// UE suppresses the same-state Sight notification, and a membership change raises no
		// faction event of its own.
		if (const UWorld* World = GetWorld())
		{
			if (ANarrativeGameState* NarrativeGameState = World->GetGameState<ANarrativeGameState>())
			{
				NarrativeGameState->NotifyFactionMembershipChanged(PlayerCharacter, Factions);
			}
		}
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
