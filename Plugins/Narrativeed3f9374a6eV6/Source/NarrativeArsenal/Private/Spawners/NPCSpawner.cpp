// Copyright Narrative Tools 2024. 


#include "Spawners/NPCSpawner.h"
#include "AI/NarrativeNPCController.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "AI/NPCDefinition.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include <Kismet/GameplayStatics.h>

#if WITH_EDITOR
#include "SubobjectDataSubsystem.h"
#endif
#include "Misc/DataValidation.h"
#include "Spawners/NPCSpawnComponent.h"
#include "Spawners/NPCSpawnVisualizerComponent.h"

#define LOCTEXT_NAMESPACE "NPCSpawner"

DEFINE_LOG_CATEGORY_STATIC(LogNPCSpawner, Log, All)

ANPCSpawner::ANPCSpawner()
{
#if WITH_EDITOR
	//Users can create a runtime grid if they want, but just leave it default for now 
	//RuntimeGrid = FName("NPCs");
#endif
	
#if WITH_EDITORONLY_DATA
	NPCSpawnVisualizer = CreateEditorOnlyDefaultSubobject<UNPCSpawnVisualizerComponent>(TEXT("NPC Spawn Visualizer"));
#endif
}

void ANPCSpawner::GetSpawnedNPCs(TArray<ANarrativeNPCCharacter*>& OutNPCs)
{
	TArray<UActorComponent*> Spawns;
	GetComponents(UNPCSpawnComponent::StaticClass(), Spawns);

	for (auto& Spawn : Spawns)
	{
		if (UNPCSpawnComponent* SpawnComp  =Cast<UNPCSpawnComponent>(Spawn))
		{
			if (ANarrativeNPCCharacter* NPCChar = Cast<ANarrativeNPCCharacter>(SpawnComp->SpawnedActor))
			{
				OutNPCs.Add(NPCChar);
			}
		}
	}
}

#if WITH_EDITOR
UNPCSpawnComponent* ANPCSpawner::CreateNPCSpawner()
{
	auto SubobjectSubsystem = GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
	check(SubobjectSubsystem);

	TArray<FSubobjectDataHandle> SubobjectData;
	SubobjectSubsystem->GatherSubobjectData(this, SubobjectData);
	checkf(SubobjectData.Num() > 0, TEXT("Actor is missing subobject data! Does it have a valid root component?"))

	FAddNewSubobjectParams Params = FAddNewSubobjectParams();
	Params.NewClass = UNPCSpawnComponent::StaticClass();
	Params.ParentHandle = SubobjectData[0]; // The first index will be the root component

	const FScopedTransaction Transaction(FText::FromString(TEXT("Add NPC Spawner")));
	Modify();
	
	FText FailReason;
	auto NewSubobjectHandle = SubobjectSubsystem->AddNewSubobject(Params, FailReason);

	if (!NewSubobjectHandle.IsValid())
	{
		UE_LOG(LogNPCSpawner, Error, TEXT("Unable to create NPCSpawnComponent: %s"), *FailReason.ToString())
	}
	
	return const_cast<UNPCSpawnComponent*>(NewSubobjectHandle.GetData()->GetObject<UNPCSpawnComponent>());
}
#endif

#if WITH_EDITOR
EDataValidationResult ANPCSpawner::IsDataValid(FDataValidationContext& Context) const
{
	TArray<UActorComponent*> Spawns;
	GetComponents(UNPCSpawnComponent::StaticClass(), Spawns);

	TSet<FGuid> SpawnGUIDs;

	for (auto& Spawn : Spawns)
	{
		if (UNPCSpawnComponent* NPCSpawn = Cast<UNPCSpawnComponent>(Spawn))
		{
			if (SpawnGUIDs.Contains(NPCSpawn->NPCSaveGUID))
			{
				FText ErrorText = FText::Format(LOCTEXT("ErrorDuplicateSpawn", "NPC Spawner has multiple spawn components with identical NPCSaveGUID: {0}. Make sure GUID's are unique. "), FText::FromString(NPCSpawn->NPCSaveGUID.ToString()));
				Context.AddMessage(FAssetData(this), EMessageSeverity::Error, ErrorText);
			}
			else
			{
				SpawnGUIDs.Add(NPCSpawn->NPCSaveGUID);
			}
		}
	}

	if (Context.GetNumErrors() > 0)
	{
		return EDataValidationResult::Invalid;
	}

	return Super::IsDataValid(Context);
}
#endif

#if WITH_EDITORONLY_DATA
void ANPCSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// render spawn visualizer again as all spawn components should be valid now
	if (NPCSpawnVisualizer)
	{
		NPCSpawnVisualizer->MarkRenderStateDirty();
	}
}
#endif 

#undef LOCTEXT_NAMESPACE 