// Copyright Narrative Tools 2025.


#include "UnrealFramework/NarrativeParty.h"

#include "Net/UnrealNetwork.h"
#include "UnrealFramework/NarrativePlayerController.h"

#include "UnrealFramework/NarrativePlayerState.h"

ANarrativeParty::ANarrativeParty(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PartyTalesComponent = CreateDefaultSubobject<UNarrativePartyComponent>("PartyTalesComponent");

	bReplicates = true;
}

void ANarrativeParty::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANarrativeParty, PartyMembers);
}

bool ANarrativeParty::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	//Parties only replicate to their members. This is nice because anything that reps off this actor will only go to members of this party. 
	return PartyMemberControllers.Contains(RealViewer);
}

void ANarrativeParty::AddPartyMember(ANarrativePlayerState* PS)
{
	if (PS)
	{
		if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(PS->GetOwningController()))
		{
			PartyMembers.AddUnique(PS);
			PartyMemberControllers.Add(PC);

			if (PartyTalesComponent)
			{
				PartyTalesComponent->AddPartyMember(PC->GetTalesComponent());
			}
		}
	}
}

void ANarrativeParty::RemovePartyMember(ANarrativePlayerState* PS)
{
	if (PS)
	{
		if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(PS->GetOwningController()))
		{
			PartyMembers.Remove(PS);
			PartyMemberControllers.Remove(PC);

			if (PartyTalesComponent)
			{
				PartyTalesComponent->RemovePartyMember(PC->GetTalesComponent());
			}
		}
	}
}
