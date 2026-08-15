// Copyright Narrative Tools 2024. 


#include "Tales/NarrativeTrigger.h"
#include "Tales/NarrativeEvent.h"
#include "UnrealFramework/NarrativeCharacter.h"

UNarrativeTrigger::UNarrativeTrigger()
{

}

void UNarrativeTrigger::Initialize_Implementation()
{

}

void UNarrativeTrigger::Deactivate()
{
	bIsActive = false;

	OnDeactivate();

	if (OwnerCharacter)
	{
		for (auto& Event : TriggerEvents)
		{
			if (Event)
			{
				OwnerCharacter->SetEventActive(Event, false);
			}
		}
	}
}

void UNarrativeTrigger::Activate()
{
	bIsActive = true;

	OnActivate();

	if (OwnerCharacter)
	{
		for (auto& Event : TriggerEvents)
		{
			if (Event)
			{
				OwnerCharacter->SetEventActive(Event, true);
			}
		}
	}
}

bool UNarrativeTrigger::IsActive_Implementation()
{
	return false;
}

bool UNarrativeTrigger::GetActive() const
{
	return bIsActive;
}

FString UNarrativeTrigger::GetDescription_Implementation() const
{
	return GetName();
}
