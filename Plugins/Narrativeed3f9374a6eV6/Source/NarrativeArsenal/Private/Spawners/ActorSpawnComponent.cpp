// Copyright Narrative Tools 2025.


#include "Spawners/ActorSpawnComponent.h"
#include "Engine/World.h"

UActorSpawnComponent::UActorSpawnComponent()
{
	SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;


}

class AActor* UActorSpawnComponent::SpawnActor_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.bNoFail = true;
		SpawnParams.SpawnCollisionHandlingOverride = SpawnCollisionHandlingOverride;
		SpawnParams.Owner = GetOwner();

		return World->SpawnActor<AActor>(ActorClass, GetComponentTransform(), SpawnParams);
	}

	return nullptr; 
}

FString UActorSpawnComponent::GetEditorLabel_Implementation() const
{
	if (ActorClass)
	{
		return GetNameSafe(ActorClass);
	}

	return FString();
}
