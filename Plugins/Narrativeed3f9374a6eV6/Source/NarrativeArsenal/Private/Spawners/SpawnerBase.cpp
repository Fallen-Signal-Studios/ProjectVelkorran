// Copyright Narrative Tools 2024. 


#include "Spawners/SpawnerBase.h"
#include "Components/BillboardComponent.h"
#include "SaveSystemStatics.h"
#include "Spawners/SpawnComponent.h"
#include "Subsystems/NarrativeSaveSubsystem.h"

// Sets default values
ASpawnerBase::ASpawnerBase()
{
	SpawnerRoot = CreateDefaultSubobject<USceneComponent>("SpawnerRoot");
	SetRootComponent(SpawnerRoot);

#if WITH_EDITOR
	SetIsSpatiallyLoaded(true);

	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));

	if (SpriteComponent)
	{
		auto SettlementIconFinder = ConstructorHelpers::FObjectFinder<UTexture2D>(TEXT("/Script/Engine.Texture2D'/NarrativePro/Pro/Editor/Texture/Icons/T_Spawner.T_Spawner'"));
		if (SettlementIconFinder.Succeeded())
		{
			SpriteComponent->SetSprite(SettlementIconFinder.Object);
		}

		SpriteComponent->SetRelativeScale3D_Direct(FVector(2.f, 2.f, 2.f));
		SpriteComponent->bIsScreenSizeScaled = true;
		SpriteComponent->SetupAttachment(GetRootComponent());
	}
#endif

	 USaveSystemStatics::CreateSaveGuid(SpawnerSaveGUID);
	 bActivateOnBeginPlay = true;

}

// Called when the game starts or when spawned
void ASpawnerBase::BeginPlay()
{
	Super::BeginPlay();
	
	//Load before we spawn as our spawner comps need to be ready 
	USaveSystemStatics::LoadSingleActor(this);

	if (bActivateOnBeginPlay)
	{
		SpawnActors();
	}
}

void ASpawnerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	//Remove any spawned actors when we end play
	RemoveActors();

	//If we're being streamed out, save so we can load back in when player comes back.
	if (EndPlayReason == EEndPlayReason::RemovedFromWorld)
	{
		USaveSystemStatics::SaveSingleActor(this);
	}
	else if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		//If we destroy a spawner, remove it from save 
		USaveSystemStatics::RemoveSingleActor(this);
	}


}

void ASpawnerBase::SpawnActors()
{	
	TArray<UActorComponent*> Spawns;
	GetComponents(USpawnComponent::StaticClass(), Spawns);

	for (auto& Spawn : Spawns)
	{
		if (USpawnComponent* SpawnComp  =Cast<USpawnComponent>(Spawn))
		{
			if (SpawnComp->IsActive())
			{
				if (SpawnComp->ShouldSpawnActor())
				{
					SpawnComp->TrySpawnActor();
				}
				else
				{
					UE_LOG(LogTemp, Display, TEXT("Spawn actor returned false "));
				}
			}
		}
	}
}

void ASpawnerBase::RemoveActors()
{
	TArray<UActorComponent*> Spawns;
	GetComponents(USpawnComponent::StaticClass(), Spawns);

	for (auto& Spawn : Spawns)
	{
		if (USpawnComponent* SpawnComp  =Cast<USpawnComponent>(Spawn))
		{
			if (SpawnComp->ShouldDespawnActor())
			{
				SpawnComp->TryDespawnActor();
			}
		}
	}
}

void ASpawnerBase::SetActorGUID_Implementation(const FGuid& SavedGUID)
{
	SpawnerSaveGUID = SavedGUID;
}

FGuid ASpawnerBase::GetActorGUID_Implementation() const
{
	return SpawnerSaveGUID;
}

