// Copyright Narrative Tools 2024. 


#include "StableActorStatics.h"
#include "NarrativeStableActor.h"
#include "StableActor.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include <Engine/World.h>
#include <GameFramework/Actor.h>

FStableActor UStableActorStatics::MakeStableActor(AActor* Actor)
{
	if (Actor && Actor->Implements<UNarrativeStableActor>())
	{
		const FGuid ActorGUID = INarrativeStableActor::Execute_GetActorGUID(Actor);

		if (ActorGUID.IsValid())
		{
			return FStableActor(ActorGUID, Actor);
		}
	}

	return FStableActor();
}

FStableActor UStableActorStatics::MakeStableActorFromGUID(const UObject* WorldContextObject, const FGuid& StableActorGuid)
{
	if (WorldContextObject && StableActorGuid.IsValid())
	{
		if (const UWorld* World = WorldContextObject->GetWorld())
		{
			if (UNarrativeSaveSubsystem* SaveSub = World->GetSubsystem<UNarrativeSaveSubsystem>())
			{
				AActor* Actor = SaveSub->LookupActorByGUID(StableActorGuid);

				if (StableActorGuid.IsValid())
				{
					return FStableActor(StableActorGuid, Actor);
				}
			}
		}
	}
	return FStableActor();
}

class AActor* UStableActorStatics::GetStableActor(const UObject* WorldContextObject, const FStableActor& StableActor, bool& bOutSucceeded, TSubclassOf<class AActor> Type)
{
	if (AActor* Actor = StableActor.GetActor())
	{
		return Actor; 
	}

	const FGuid StableGUID = StableActor.GetStableGUID();

	if (WorldContextObject && StableGUID.IsValid())
	{
		if (const UWorld* World = WorldContextObject->GetWorld())
		{
			if (UNarrativeSaveSubsystem* SaveSub = World->GetSubsystem<UNarrativeSaveSubsystem>())
			{
				if (AActor* Actor = SaveSub->LookupActorByGUID(StableGUID))
				{
					bOutSucceeded = true; 
					return Actor; 
				}
			}
		}
	}

	return nullptr; 
}
