// Copyright Narrative Tools 2025.


#include "AI/Mass/Peds/NarrativeVisualizationTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "MassRepresentationSubsystem.h"
#include "AI/Mass/Peds/MassNarrativePedRepresentationActorManagement.h"
#include "AI/Mass/Peds/MassPedRepresentationSubsystem.h"
#include "Engine/StaticMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogNarrativeVisualizationTrait, Log, All);

UNarrativeVisualizationTrait::UNarrativeVisualizationTrait()
{
	Params.RepresentationActorManagementClass = UMassNarrativePedRepresentationActorManagement::StaticClass();
	RepresentationSubsystemClass = UMassPedRepresentationSubsystem::StaticClass();
	bRegisterStaticMeshDesc = false;
}

void UNarrativeVisualizationTrait::SanitizeParams(FMassRepresentationParameters& InOutParams,
	const bool bStaticMeshDeterminedInvalid) const
{
	// Do nothing as we will initialize the static mesh desc later
	//Super::SanitizeParams(InOutParams, bStaticMeshDeterminedInvalid);
}

void UNarrativeVisualizationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
	const UWorld& World) const
{
	Super::BuildTemplate(BuildContext, World);

	UMassRepresentationSubsystem* RepresentationSubsystem = Cast<UMassRepresentationSubsystem>(World.GetSubsystemBase(RepresentationSubsystemClass));
	if (RepresentationSubsystem == nullptr && !BuildContext.IsInspectingData())
	{
		RepresentationSubsystem = UWorld::GetSubsystem<UMassRepresentationSubsystem>(&World);
		check(RepresentationSubsystem);
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.AddFragment<FNarrativePedFragment>();
	
	auto RepresentationFragment = BuildContext.GetFragment<FMassRepresentationFragment>();

	auto PedPropertiesCopy = PedProperties;
	
	if (RepresentationFragment)
	{
		// Add all NPC definitions to the representation subsystem for future use within processors
		for (const TSoftObjectPtr<UPedNPCDefinition>& PedNPCDefinition : PedProperties.NarrativePeds)
		{
			if (PedNPCDefinition.IsNull()) { continue; }
			
			// Add MeshDesc from NPC definitions
			FMassStaticMeshInstanceVisualizationMeshDesc MeshDesc = FMassStaticMeshInstanceVisualizationMeshDesc();

			//@todo load async - we need the definition to be valid now, need to look into loading the definition at an earlier stage.
			MeshDesc.Mesh = PedNPCDefinition.LoadSynchronous()->BakedPedMesh.LoadSynchronous();

			FStaticMeshInstanceVisualizationDesc VisualizationDesc = StaticMeshInstanceDesc;
			VisualizationDesc.Meshes.Add(MeshDesc);
		
			PedPropertiesCopy.VisualizationHandles.Add(RepresentationSubsystem->FindOrAddStaticMeshDesc(VisualizationDesc));
		}
	}

	const FConstSharedStruct PedPropertiesFragment = EntityManager.GetOrCreateConstSharedFragment(PedPropertiesCopy);
	BuildContext.AddConstSharedFragment(PedPropertiesFragment);
}

#if WITH_EDITOR
bool UNarrativeVisualizationTrait::ValidateParams() const
{
	bool bIssuesFound = false;

	// if this test is called on any of the CDOs we don't care, we're never going to utilize those in practice.
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		if (PedProperties.NarrativePeds.IsEmpty())
		{
			bIssuesFound = true;
			UE_LOG(LogNarrativeVisualizationTrait, Error, TEXT("No Narrative Peds were defined. Unable to visualize entities without NPC Definitions in %s."), *GetPathName());
		}
	}

	return !bIssuesFound;
}
#endif
