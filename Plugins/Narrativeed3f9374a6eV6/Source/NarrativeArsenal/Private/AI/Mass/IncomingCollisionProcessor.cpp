// Copyright Narrative Tools 2025.


#include "AI/Mass/IncomingCollisionProcessor.h"

#include "ArsenalStatics.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassDebuggerSubsystem.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "MassRepresentationFragments.h"
#include "AI/Mass/IncomingCollisionFragments.h"
#include "AI/Mass/NarrativeMassAgentComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Vehicles/NarrativeVehicleBase.h"
#include "Vehicles/Mass/MassVehicle.h"
#include "Vehicles/Mass/MassVehicleSubsystem.h"

UIncomingCollisionProcessor::UIncomingCollisionProcessor() : EntityQuery(*this)
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Avoidance;
}

void UIncomingCollisionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FCollisionDetectionSharedFragment>();
	EntityQuery.AddSubsystemRequirement<UMassVehicleSubsystem>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::All); // ignore vehicle passengers

	EntityQuery.AddRequirement<FIncomingCollisionFragment>(EMassFragmentAccess::ReadWrite);

#if WITH_MASSGAMEPLAY_DEBUG
	EntityQuery.AddSubsystemRequirement<UMassDebuggerSubsystem>(EMassFragmentAccess::ReadOnly);
#endif
}

void UIncomingCollisionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Key and value entities will both receive events of incoming collision
	TArray<TTuple<FMassEntityHandle, FMassEntityHandle>> IncomingCollisionEntities;
	
	EntityQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& Context)
	{
		TConstArrayView<FTransformFragment> TransformFragments = Context.GetFragmentView<FTransformFragment>();
		TConstArrayView<FAgentRadiusFragment> RadiusFragments = Context.GetFragmentView<FAgentRadiusFragment>();
		const UMassVehicleSubsystem& VehicleSubsystem = Context.GetSubsystemChecked<UMassVehicleSubsystem>();
		const FCollisionDetectionSharedFragment& CollisionDetectionSharedFragment = Context.GetConstSharedFragment<FCollisionDetectionSharedFragment>();
		TConstArrayView<FMassActorFragment> ActorFragments = Context.GetFragmentView<FMassActorFragment>();
		
#if WITH_MASSGAMEPLAY_DEBUG
		const UMassDebuggerSubsystem& MassDebuggerSubsystem = Context.GetSubsystemChecked<UMassDebuggerSubsystem>();
#endif

		TArrayView<FIncomingCollisionFragment> IncomingCollisionFragments = Context.GetMutableFragmentView<FIncomingCollisionFragment>();
		
		Context.ForEachEntityInChunk([&](FMassExecutionContext& MassContext, int32 EntityIndex)
		{
			const FTransformFragment& TransformFragment = TransformFragments[EntityIndex];
			const FAgentRadiusFragment& RadiusFragment = RadiusFragments[EntityIndex];
			FIncomingCollisionFragment& IncomingCollisionFragment = IncomingCollisionFragments[EntityIndex];
			const FMassActorFragment& ActorFragment = ActorFragments[EntityIndex];

			const FVector& EntityLocation = TransformFragment.GetTransform().GetLocation();
			
			IncomingCollisionFragment.Reset();

			// Ignore passengers in vehicle
			TArray<FMassEntityHandle> IgnoreHandles;
			if (ActorFragment.IsValid())
			{
				if (const UNarrativeInteractableComponent* InteractableComponent = ActorFragment.Get()->FindComponentByClass<UNarrativeInteractableComponent>())
				{
					for (const FActiveInteractionSlot& SlotStatus : InteractableComponent->SlotStatuses)
					{
						if (SlotStatus.SlotStatus != EInteractionSlotStatus::ISS_Occupied) { continue; }
						
						// Interaction components are attached to controllers
						if (AController* Controller = SlotStatus.SlotUser->GetOwner<AController>())
						{
							if (UMassAgentComponent* AgentComponent = Controller->GetPawn()->FindComponentByClass<UMassAgentComponent>())
							{
								FMassEntityHandle EntityHandle = AgentComponent->GetEntityHandle();
								if (EntityHandle.IsValid())
								{
									IgnoreHandles.Add(EntityHandle);
								}
							}
						}
					}
				}
			}

			TArray<FMassEntityHandle> NearbyObstacles;
			FBox ObstacleQueryExtent = FBox::BuildAABB(EntityLocation, FVector(CollisionDetectionSharedFragment.ObstacleQueryRange));
			VehicleSubsystem.VehicleObstacles.Query(ObstacleQueryExtent, NearbyObstacles);

			// Perform TimeToCollision checks on nearby obstacles
			for (const FMassEntityHandle& NearbyObstacle : NearbyObstacles)
			{
				if (NearbyObstacle == MassContext.GetEntity(EntityIndex)) { continue; }
				if (IgnoreHandles.Contains(NearbyObstacle)) { continue; }
				
				FMassEntityView ObstacleView = FMassEntityView(MassContext.GetEntityManagerChecked(), NearbyObstacle);
				if (!ObstacleView.IsValid()) { continue; }
				
				const FTransformFragment& ObstacleTransform = ObstacleView.GetFragmentData<FTransformFragment>();
				const FAgentRadiusFragment& ObstacleRadius = ObstacleView.GetFragmentData<FAgentRadiusFragment>();

				const FMassVelocityFragment* ObstacleVelocityFragment = ObstacleView.GetFragmentDataPtr<FMassVelocityFragment>();
				FVector ObstacleVelocity = ObstacleVelocityFragment ? ObstacleVelocityFragment->Value : FVector::ZeroVector;

				const FVector& ObstacleLocation = ObstacleTransform.GetTransform().GetLocation();

				float ObstacleCollisionTime = UArsenalStatics::TimeUntilCollision(EntityLocation, TransformFragment.GetTransform().GetRotation().GetForwardVector() * CollisionDetectionSharedFragment.EntityAverageSpeed,
					RadiusFragment.Radius * CollisionDetectionSharedFragment.ObstacleRadiusMultiplier, ObstacleLocation, ObstacleVelocity, ObstacleRadius.Radius * CollisionDetectionSharedFragment.ObstacleRadiusMultiplier);

				if (ObstacleCollisionTime < IncomingCollisionFragment.TimeUntilCollision)
				{
					IncomingCollisionFragment.TimeUntilCollision = ObstacleCollisionTime;
					IncomingCollisionFragment.IncomingEntity = NearbyObstacle;
					IncomingCollisionFragment.DistanceToObstacle = FVector::Dist(ObstacleLocation, EntityLocation);
				}
			}
			
			// Queue event if we have an incoming entity
			bool bIncomingEntityChanged = IncomingCollisionFragment.IncomingEntity != IncomingCollisionFragment.LastEventEntity;
			bool bIsValidEvent = IncomingCollisionFragment.TimeUntilCollision <= CollisionDetectionSharedFragment.TimeToCollisionEvent || !IncomingCollisionFragment.IncomingEntity.IsValid();
			if (bIncomingEntityChanged && bIsValidEvent)
			{
				IncomingCollisionFragment.LastEventEntity = IncomingCollisionFragment.IncomingEntity;
				IncomingCollisionEntities.Add({Context.GetEntity(EntityIndex), IncomingCollisionFragment.IncomingEntity});
			}

#if ENABLE_VISUAL_LOG
			FTransformFragment* ObstacleTransform = IncomingCollisionFragment.IncomingEntity.IsValid() ? EntityManager.GetFragmentDataPtr<FTransformFragment>(IncomingCollisionFragment.IncomingEntity) : nullptr;
			if (MassDebuggerSubsystem.GetSelectedEntity() == Context.GetEntity(EntityIndex) && ObstacleTransform)
			{
				UE_VLOG_ARROW(this, LogMassVehicle, Log, EntityLocation, ObstacleTransform->GetTransform().GetLocation(), FColor::Red, TEXT("Dist: %f"), IncomingCollisionFragment.DistanceToObstacle);
			}
#endif
		});
	});

	// Push event to agent components
	Context.Defer().PushCommand<FMassDeferredSetCommand>([&, IncomingCollisionEntities](const FMassEntityManager& Manager)
	{
		for (const TTuple<FMassEntityHandle, FMassEntityHandle>& IncomingCollisionEntity : IncomingCollisionEntities)
		{
			FMassActorFragment* ActorFragment = Manager.IsEntityActive(IncomingCollisionEntity.Key) ? Manager.GetFragmentDataPtr<FMassActorFragment>(IncomingCollisionEntity.Key) : nullptr;
			AActor* Actor = ActorFragment ? ActorFragment->GetMutable() : nullptr;
			UNarrativeMassAgentComponent* NarrativeAgentComponent = Actor ? Actor->FindComponentByClass<UNarrativeMassAgentComponent>() : nullptr;
			
			if (NarrativeAgentComponent)
			{
				NarrativeAgentComponent->IncomingCollisionDetected(IncomingCollisionEntity.Value);
#if ENABLE_VISUAL_LOG
				UE_VLOG(Actor, LogMassVehicle, Log, TEXT("Incoming Collision Detected: %s"), *IncomingCollisionEntity.Value.DebugGetDescription());
#endif
			}
			else
			{
#if ENABLE_VISUAL_LOG
				UE_VLOG(this, LogMassVehicle, Warning, TEXT("Unable to determine actor/agent component for incoming collision %s! ActorFragment: %s | Actor: %s | (Actor Fragment not set?)"), *IncomingCollisionEntity.Key.DebugGetDescription(), ActorFragment ? TEXT("Valid") : TEXT("Not Valid"), *GetNameSafe(Actor));
#endif
			}
		}
	});
}
