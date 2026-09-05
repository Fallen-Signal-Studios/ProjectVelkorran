// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovCampaignMassProxy.h"
#include "AI/Mass/Peds/NarrativePedFragments.h"
#include "AI/Mass/Peds/MassPedRepresentationSubsystem.h"
#include "AI/Mass/Peds/MassNarrativePedRepresentationActorManagement.h"
#include "AI/Mass/Peds/NarrativeMassParticipantBridge.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "MassCommonFragments.h"
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

bool ASovCampaignMassProxy::ValidateAnimationProfile(const TArray<FSovCampaignMassMesh>& Meshes, FString& Error)
{
	for (const auto& Record : Meshes)
	{
		if (Record.Mesh.IsNull()) { continue; }
		const USkeletalMesh* Mesh = Record.Mesh.Get(); const UAnimSequence* Animation = Record.RouteAnimation.Get();
		if (!Mesh || !Animation || !Mesh->GetSkeleton() || Animation->GetSkeleton() != Mesh->GetSkeleton()
			|| Animation->HasRootMotion() || Animation->GetPlayLength() <= UE_SMALL_NUMBER || !FMath::IsFinite(Record.AnimationReferenceSpeed)
			|| Record.AnimationReferenceSpeed <= 0.f || Record.AnimationReferenceSpeed > 1200.f
			|| !FMath::IsFinite(Record.AnimationTime) || Record.AnimationTime < 0.f)
		{
			Error = TEXT("Tier C requires a resident, matching-skeleton, in-place locomotion sequence for every skeletal part; keep the actor or use static Tier D.");
			return false;
		}
	}
	return true;
}

bool ASovCampaignMassProxy::RestoreVisuals(const TArray<FSovCampaignMassMesh>& Meshes, bool bAnimated)
{
	if (Meshes.IsEmpty() || Meshes.Num() > 32) { return false; }
	FString Error;
	if (bAnimated && !ValidateAnimationProfile(Meshes, Error)) { return false; }
	// Validate the complete resident set before replacing any existing visual. No callback may block on disk.
	for (const auto& Record : Meshes)
	{
		if (Record.RelativeTransform.ContainsNaN() || (Record.StaticMesh.IsNull() && !Record.Mesh.IsValid())
			|| (!Record.StaticMesh.IsNull() && !Record.StaticMesh.IsValid())
			|| (Record.StaticMesh.IsNull() && (Record.Bones.IsEmpty() || Record.Bones.Num() != Record.ComponentSpacePose.Num()))) { return false; }
		for (const auto& Material : Record.Materials) { if (!Material.IsNull() && !Material.IsValid()) { return false; } }
	}
	for (UMeshComponent* Mesh : PoseMeshes) { if (IsValid(Mesh)) { Mesh->DestroyComponent(); } }
	PoseMeshes.Reset(); ReferenceSpeeds.Reset();
	for (const auto& Record : Meshes)
	{
		ReferenceSpeeds.Add(Record.AnimationReferenceSpeed);
		if (!Record.StaticMesh.IsNull())
		{
			auto* Asset = Record.StaticMesh.Get();
			auto* Mesh = NewObject<UStaticMeshComponent>(this); AddInstanceComponent(Mesh); Mesh->SetupAttachment(RootComponent);
			Mesh->SetStaticMesh(Asset); Mesh->SetRelativeTransform(Record.RelativeTransform);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetGenerateOverlapEvents(false);
			Mesh->RegisterComponent(); PoseMeshes.Add(Mesh);
			for (int32 I = 0; I < Record.Materials.Num(); ++I) { Mesh->SetMaterial(I, Record.Materials[I].Get()); }
			continue;
		}
		USkeletalMesh* Asset = Record.Mesh.Get();
		if (bAnimated)
		{
			auto* Mesh = NewObject<USkeletalMeshComponent>(this); AddInstanceComponent(Mesh); Mesh->SetupAttachment(RootComponent);
			Mesh->SetSkeletalMesh(Asset); Mesh->SetRelativeTransform(Record.RelativeTransform);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetGenerateOverlapEvents(false);
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
			Mesh->SetComponentTickInterval(1.f / 30.f); Mesh->RegisterComponent(); PoseMeshes.Add(Mesh);
			for (int32 I = 0; I < Record.Materials.Num(); ++I) { Mesh->SetMaterial(I, Record.Materials[I].Get()); }
			Mesh->PlayAnimation(Record.RouteAnimation.Get(), true); Mesh->SetPosition(Record.AnimationTime, false); Mesh->bPauseAnims = true;
			for (FName Bone : Record.HiddenBones) { Mesh->HideBoneByName(Bone, EPhysBodyOp::PBO_None); }
			continue;
		}
		auto* Mesh = NewObject<UPoseableMeshComponent>(this);
		AddInstanceComponent(Mesh); Mesh->SetupAttachment(RootComponent);
		Mesh->SetSkeletalMesh(Asset); Mesh->SetRelativeTransform(Record.RelativeTransform);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetGenerateOverlapEvents(false);
		Mesh->RegisterComponent(); PoseMeshes.Add(Mesh);
		for (int32 I = 0; I < Record.Materials.Num(); ++I) { Mesh->SetMaterial(I, Record.Materials[I].Get()); }
		for (int32 I = 0; I < Record.Bones.Num(); ++I)
		{ Mesh->SetBoneTransformByName(Record.Bones[I], Record.ComponentSpacePose[I], EBoneSpaces::ComponentSpace); }
		for (FName Bone : Record.HiddenBones) { Mesh->HideBoneByName(Bone, EPhysBodyOp::PBO_None); }
		Mesh->RefreshBoneTransforms();
		Mesh->SetComponentTickEnabled(false);
	}
	return true;
}

void ASovCampaignMassProxy::SetRouteMotion(bool bMoving, float Speed)
{
	for (int32 I = 0; I < PoseMeshes.Num(); ++I)
	{
		if (auto* Mesh = Cast<USkeletalMeshComponent>(PoseMeshes[I]))
		{
			const float Reference = ReferenceSpeeds.IsValidIndex(I) ? ReferenceSpeeds[I] : 0.f;
			Mesh->bPauseAnims = !bMoving || !FMath::IsFinite(Speed) || Speed <= 0.f || Reference <= 0.f;
			if (!Mesh->bPauseAnims) { Mesh->SetPlayRate(FMath::Clamp(Speed / Reference, .1f, 4.f)); }
		}
	}
}

void ASovCampaignMassProxy::CaptureVisualState(TArray<FSovCampaignMassMesh>& Meshes) const
{
	if (Meshes.Num() != PoseMeshes.Num()) { return; }
	for (int32 Index = 0; Index < Meshes.Num(); ++Index)
	{
		auto* Mesh = Cast<USkeletalMeshComponent>(PoseMeshes[Index]); auto& Record = Meshes[Index];
		if (!Mesh || Mesh->GetSkeletalMeshAsset() != Record.Mesh.Get()) { continue; }
		if (auto* Animation = Mesh->GetSingleNodeInstance()) { Record.AnimationTime = Animation->GetCurrentTime(); }
		Record.Bones.Reset(); Record.ComponentSpacePose.Reset(); Record.LocalPose.Reset();
		for (int32 BoneIndex = 0; BoneIndex < Mesh->GetNumBones(); ++BoneIndex)
		{
			const FName Bone = Mesh->GetBoneName(BoneIndex); Record.Bones.Add(Bone);
			Record.ComponentSpacePose.Add(Mesh->GetSocketTransform(Bone, RTS_Component));
			const int32 Parent = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetParentIndex(BoneIndex);
			Record.LocalPose.Add(Parent == INDEX_NONE ? Record.ComponentSpacePose.Last()
				: Record.ComponentSpacePose.Last().GetRelativeTransform(Mesh->GetSocketTransform(Mesh->GetBoneName(Parent), RTS_Component)));
		}
	}
}

USovCampaignMassVisualizationTrait::USovCampaignMassVisualizationTrait()
{
	Params.RepresentationActorManagementClass = UMassNarrativePedRepresentationActorManagement::StaticClass();
	RepresentationSubsystemClass = UMassPedRepresentationSubsystem::StaticClass();
	HighResTemplateActor = ASovCampaignMassProxy::StaticClass();
	LowResTemplateActor = ASovCampaignMassProxy::StaticClass();
	bAllowServerSideVisualization = true;
	bRegisterStaticMeshDesc = false;
	bRequireValidStaticMeshInstanceDesc = false;
	Params.LODRepresentation[0] = EMassRepresentationType::HighResSpawnedActor;
	Params.LODRepresentation[1] = EMassRepresentationType::LowResSpawnedActor;
	Params.LODRepresentation[2] = EMassRepresentationType::LowResSpawnedActor;
	Params.LODRepresentation[3] = EMassRepresentationType::None;
}

void USovCampaignMassVisualizationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FTransformFragment>();
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
			if (auto* Proxy = Cast<ASovCampaignMassProxy>(const_cast<AActor*>(Actors[I].Get())))
			{
				auto* Receipt = Proxy->FindComponentByClass<UNarrativeMassParticipantReceiptComponent>();
				if (Receipt && Receipt->GetEntity() == Chunk.GetEntity(I) && Receipt->IsCurrent(EntityManager))
				{ Proxy->SetRouteMotion(Owners[I].Owner.IsValid() && !Route.bPresentationOnly && Route.Points.IsValidIndex(Route.NextPoint), Route.Speed); }
			}
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
