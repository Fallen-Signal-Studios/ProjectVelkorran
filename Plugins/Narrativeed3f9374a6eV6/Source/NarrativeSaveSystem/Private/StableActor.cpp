// Copyright Narrative Tools 2024. 


#include "StableActor.h"

AActor* FStableActor::GetActor() const
{
	return StableActorRef.Get();
}
