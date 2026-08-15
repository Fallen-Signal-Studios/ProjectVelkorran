// Copyright Narrative Tools 2022. 


#include "Tales/NarrativeNodeBase.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeEvent.h"
#include "Tales/TalesComponent.h"
#include "Tales/NarrativePartyComponent.h"
#include "AI/NarrativeCharacterSubsystem.h"

UNarrativeNodeBase::UNarrativeNodeBase()
{
	//autofill the ID
	ID = GetFName();
}

#if WITH_EDITOR

void UNarrativeNodeBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty)
	{
		//If we changed the ID, make sure it doesn't conflict with any other IDs in the quest
		if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UNarrativeNodeBase, ID))
		{
			EnsureUniqueID();
		}
	}
}

#endif 

void UNarrativeNodeBase::ProcessEvents(APawn* Pawn, APlayerController* Controller, class UTalesComponent* NarrativeComponent, const EEventRuntime Runtime)
{
	if (!NarrativeComponent)
	{
		UE_LOG(LogNarrative, Warning, TEXT("Tried running events on node %s but Narrative Comp was null."), *GetNameSafe(this));
		return;
	}

	const bool bIsLoading = NarrativeComponent->bIsLoading;

	for (auto& Event : Events)
	{
		if (Event)
		{
			const bool bShouldFire = (!bIsLoading || Event->bRefireOnLoad ) && (Event->EventRuntime == Runtime || Event->EventRuntime == EEventRuntime::Both);

			if (bShouldFire)
			{
				
				TArray<UCharacterDefinition*> CharTargets = Event->GetCharacterTargets();
				
				//If we have any NPC targets, run the event on them instead of the owning player! 
				if (CharTargets.Num())
				{
					//NPCOnly events have special functionality to grab all avatars
					if (UWorld* World = NarrativeComponent->GetWorld())
					{
						if (UNarrativeCharacterSubsystem* NPCS = World->GetSubsystem<UNarrativeCharacterSubsystem>())
						{
							for (auto& NPCTarget : CharTargets)
							{
								if (ANarrativeCharacter* Character = NPCS->FindCharacter(NPCTarget))
								{
									Event->ExecuteEvent(Character, Controller, NarrativeComponent);
								}
							}
						}
					}
				}
				else
				{
					//Run on the party leader or party members.
					//Get party, or ask solo component for its owning party if we're not one ourselves.               
					UNarrativePartyComponent* Party = Cast<UNarrativePartyComponent>(NarrativeComponent);             
					
					if (Event->PartyEventPolicy > EPartyEventPolicy::Party && Party)
					{
						for (auto& Member : Party->GetPartyMembers())
						{
							if (Member)
							{
								Event->ExecuteEvent(Member->GetOwningPawn(), Member->GetOwningController(), Member);

								//Party leader is always first member so we can break now, we've processed them 
								if (Event->PartyEventPolicy == EPartyEventPolicy::PartyLeader)
								{
									break;
								}
							}
						}
					}
					else //If event policy is party, thats the default. NarrativeComponent will either be our party at this point, or owning solo player if we don't have a party. 
					{
						Event->ExecuteEvent(NarrativeComponent->GetOwningPawn(), NarrativeComponent->GetOwningController(), NarrativeComponent);
					}
				}

			}
		}
	}
}

bool UNarrativeNodeBase::AreConditionsMet(APawn* Pawn, APlayerController* Controller, class UTalesComponent* NarrativeComponent)
{

	if (!NarrativeComponent)
	{
		UE_LOG(LogNarrative, Warning, TEXT("Tried running conditions on node %s but Narrative Comp was null."), *GetNameSafe(this));
		return false;
	}
	  
	//Ensure all conditions are met
	for (auto& Cond : Conditions)
	{	
		if (Cond)
		{
			//Get party, or ask solo component for its owning party if we're not one. 
			UNarrativePartyComponent* Party = Cast<UNarrativePartyComponent>(NarrativeComponent);

			if (!Party)
			{
				Party = NarrativeComponent->GetParty();
			}
			
			//Run conditions on party if we're not standalone. 
			if (Cond->PartyConditionPolicy > EPartyConditionPolicy::None && Party && NarrativeComponent->GetNetMode() != NM_Standalone)
			{
				if (Cond->PartyConditionPolicy == EPartyConditionPolicy::PartyPasses)
				{
					if (Cond->CheckCondition(nullptr, nullptr, Party) == Cond->bNot)
					{
						return false; 
					}
				}
				else if (Cond->PartyConditionPolicy == EPartyConditionPolicy::PartyLeaderPasses)
				{
					UTalesComponent* Leader = Party->GetPartyLeader();
					
					if (Cond->CheckCondition( Leader->GetOwningPawn(), Leader->GetOwningController(), Leader) == Cond->bNot)
					{
						return false; 
					}
				}
				else if (Cond->PartyConditionPolicy == EPartyConditionPolicy::AnyPlayerPasses || Cond->PartyConditionPolicy == EPartyConditionPolicy::AllPlayersPass)
				{
					for (auto& Member : Party->GetPartyMembers())
					{
						if (Member)
						{
							if (Cond->CheckCondition( Member->GetOwningPawn(), Member->GetOwningController(), Member) == Cond->bNot)
							{
								//If any fail and we need all to pass then return false 
								if (Cond->PartyConditionPolicy == EPartyConditionPolicy::AllPlayersPass)
								{
									return false; 
								}
							} //IF any succeeded, break, we've succeeded 
							else if (Cond->PartyConditionPolicy == EPartyConditionPolicy::AnyPlayerPasses)
							{
								break;
							}
						}
					}
				}
				
				return true; 
				
			}
			else //Standard non party condition checking 
			{
				TArray<UCharacterDefinition*> CharTargets = Cond->GetCharacterTargets();
			
				if (CharTargets.Num())
				{
					if (UWorld* World = NarrativeComponent->GetWorld())
					{
						if (UNarrativeCharacterSubsystem* NPCS = World->GetSubsystem<UNarrativeCharacterSubsystem>())
						{
							for (auto& NPCTarget : CharTargets)
							{
								if (ANarrativeCharacter* Character = NPCS->FindCharacter(NPCTarget))
								{
									if (Cond->CheckCondition(Character, Controller, NarrativeComponent) == Cond->bNot)
									{
										return false; 
									}
								}
							}
						
						}
					}
				}
				else
				{
					if (Cond->CheckCondition(Pawn, Controller, NarrativeComponent) == Cond->bNot)
					{
						return false;
					}
				}
			}
		}
	}

	return true;
}
