// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovCampaignMassProxy.h"
#include "AI/Mass/Peds/NarrativePedFragments.h"
#include "AI/Mass/Peds/MassPedRepresentationSubsystem.h"
#include "AI/Mass/Peds/MassNarrativePedRepresentationActorManagement.h"
#include "AI/Mass/Peds/NarrativeMassParticipantBridge.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "MassCommonFragments.h"
#include "MassLODFragments.h"
#include "MassCrowdFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"
#include "MassRepresentationTypes.h"
#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "Materials/MaterialInterface.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"

ASovCampaignMassProxy::ASovCampaignMassProxy()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	SetCanBeDamaged(false);
}

bool ASovCampaignMassProxy::RestoreVisuals(const TArray<FSovCampaignMassMesh>& Meshes)
{
	for (UMeshComponent* Mesh : PoseMeshes) { if (IsValid(Mesh)) { Mesh->DestroyComponent(); } }
	PoseMeshes.Reset();
	if (Meshes.IsEmpty() || Meshes.Num() > 32) { return false; }
	for (const auto& Record : Meshes)
	{
		if (!Record.StaticMesh.IsNull())
		{
			auto* Asset = Record.StaticMesh.LoadSynchronous(); if (!Asset) { return false; }
			auto* Mesh = NewObject<UStaticMeshComponent>(this); AddInstanceComponent(Mesh); Mesh->SetupAttachment(RootComponent);
			Mesh->SetStaticMesh(Asset); Mesh->SetRelativeTransform(Record.RelativeTransform);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetGenerateOverlapEvents(false);
			Mesh->RegisterComponent(); PoseMeshes.Add(Mesh);
			for (int32 I = 0; I < Record.Materials.Num(); ++I) { Mesh->SetMaterial(I, Record.Materials[I].LoadSynchronous()); }
			continue;
		}
		USkeletalMesh* Asset = Record.Mesh.LoadSynchronous();
		if (!Asset || Record.Bones.IsEmpty() || Record.Bones.Num() != Record.ComponentSpacePose.Num()) { return false; }
		auto* Mesh = NewObject<UPoseableMeshComponent>(this);
		AddInstanceComponent(Mesh); Mesh->SetupAttachment(RootComponent);
		Mesh->SetSkeletalMesh(Asset); Mesh->SetRelativeTransform(Record.RelativeTransform);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetGenerateOverlapEvents(false);
		Mesh->RegisterComponent(); PoseMeshes.Add(Mesh);
		for (int32 I = 0; I < Record.Materials.Num(); ++I) { Mesh->SetMaterial(I, Record.Materials[I].LoadSynchronous()); }
		for (int32 I = 0; I < Record.Bones.Num(); ++I)
		{ Mesh->SetBoneTransformByName(Record.Bones[I], Record.ComponentSpacePose[I], EBoneSpaces::ComponentSpace); }
		for (FName Bone : Record.HiddenBones) { Mesh->HideBoneByName(Bone, EPhysBodyOp::PBO_None); }
		Mesh->RefreshBoneTransforms();
		Mesh->SetComponentTickEnabled(false);
	}
	return true;
}

USovCampaignMassVisualizationTrait::USovCampaignMassVisualizationTrait()
{
	Params.RepresentationActorManagementClass = UMassNarrativePedRepresentationActorManagement::StaticClass();
	RepresentationSubsystemClass = UMassPedRepresentationSubsystem::StaticClass();
	HighResTemplateActor = ASovCampaignMassProxy::StaticClass();
	LowResTemplateActor = ASovCampaignMassProxy::StaticClass();
	bAllowServerSideVisualization = true;
	bRegisterStaticMeshDesc = false;
#if WITH_EDITORONLY_DATA
	bRequireValidStaticMeshInstanceDesc = false;
#endif
	Params.LODRepresentation[0] = EMassRepresentationType::HighResSpawnedActor;
	Params.LODRepresentation[1] = EMassRepresentationType::LowResSpawnedActor;
	Params.LODRepresentation[2] = EMassRepresentationType::LowResSpawnedActor;
	Params.LODRepresentation[3] = EMassRepresentationType::None;
}

void USovCampaignMassVisualizationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FTransformFragment>();
	BuildContext.AddFragment<FMassViewerInfoFragment>();
	BuildContext.AddFragment<FMassActorFragment>();
	BuildContext.AddTag<FMassCrowdTag>();
	BuildContext.AddFragment<FNarrativeMassParticipantFragment>();
	BuildContext.AddFragment<FSovCampaignMassRouteFragment>();
	Super::BuildTemplate(BuildContext, World);
}

USovCampaignMassMovementProcessor::USovCampaignMassMovementProcessor()
	: Query(*this)
{
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
}

void USovCampaignMassMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	Query.AddRequirement<FSovCampaignMassRouteFragment>(EMassFragmentAccess::ReadWrite);
	Query.AddRequirement<FNarrativeMassParticipantFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
}

void USovCampaignMassMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	Query.ForEachEntityChunk(Context, [&EntityManager](FMassExecutionContext& Chunk)
	{
		auto Transforms = Chunk.GetMutableFragmentView<FTransformFragment>();
		auto Routes = Chunk.GetMutableFragmentView<FSovCampaignMassRouteFragment>();
		auto Owners = Chunk.GetFragmentView<FNarrativeMassParticipantFragment>();
		auto Actors = Chunk.GetFragmentView<FMassActorFragment>();
		const float Delta = FMath::Clamp(Chunk.GetDeltaTimeSeconds(), 0.f, 0.25f);
		for (int32 I = 0; I < Chunk.GetNumEntities(); ++I)
		{
			auto& Route = Routes[I];
			if (!Owners[I].Owner.IsValid() || Route.bPresentationOnly || !Route.Points.IsValidIndex(Route.NextPoint)
				|| !FMath::IsFinite(Route.Speed) || Route.Speed <= 0.f) { continue; }
			FTransform& Transform = Transforms[I].GetMutableTransform();
			const FVector Offset = Route.Points[Route.NextPoint] - Transform.GetLocation();
			const float Distance = Offset.Size();
			const float Step = Route.Speed * Delta;
			if (Distance <= Step) { Transform.SetLocation(Route.Points[Route.NextPoint++]); }
			else { Transform.AddToTranslation(Offset / Distance * Step); }
			if (Distance > UE_SMALL_NUMBER) { Transform.SetRotation(Offset.Rotation().Quaternion()); }
			// These are plain actor proxies, not UMassAgentComponent NPCs: synchronize their visible transform explicitly.
			if (AActor* Actor = const_cast<AActor*>(Actors[I].Get()))
			{
				auto* Receipt = Actor->FindComponentByClass<UNarrativeMassParticipantReceiptComponent>();
				if (Receipt && Receipt->GetEntity() == Chunk.GetEntity(I) && Receipt->IsCurrent(EntityManager))
				{ Actor->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics); }
			}
		}
	});
}
