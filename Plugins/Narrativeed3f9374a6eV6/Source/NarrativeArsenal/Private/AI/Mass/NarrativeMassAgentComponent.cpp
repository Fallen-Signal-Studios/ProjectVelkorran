// Copyright Narrative Tools 2025.


#include "AI/Mass/NarrativeMassAgentComponent.h"

#include "MassActorSubsystem.h"
#include "MassAgentSubsystem.h"
#include "MassEntityView.h"
#include "Engine/World.h"
#include "VisualLogger/VisualLogger.h"
#include "Vehicles/Mass/MassVehicle.h"


// Sets default values for this component's properties
UNarrativeMassAgentComponent::UNarrativeMassAgentComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

void UNarrativeMassAgentComponent::OnRegister()
{
	bool bAssignActorFragment = true;
	if (IsRunningCommandlet() || IsRunningCookCommandlet() || GIsCookerLoadingPackage)
	{
		// ignore, we're not doing any registration while cooking or running a commandlet
		bAssignActorFragment = false;
	}

	if (GetOuter() == nullptr || GetOuter()->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || HasAnyFlags(RF_ArchetypeObject))
	{
		bAssignActorFragment = false;
	}

	UWorld* World = GetWorld();
	if (World == nullptr || World->WorldType == EWorldType::None || World->WorldType == EWorldType::Inactive
#if WITH_EDITOR
		|| World->IsPreviewWorld() || (bAutoRegisterInEditorMode == false && World->IsGameWorld() == false)
#endif // WITH_EDITOR
		)
	{
		// we don't care about preview worlds. Those are transient, temporary worlds like the one created when opening a BP editor.
		bAssignActorFragment = false;
	}

	if (bAssignActorFragment)
	{
		UMassAgentSubsystem* MassAgentSubsystem = UWorld::GetSubsystem<UMassAgentSubsystem>(GetWorld());
		MassAgentSubsystem->GetOnMassAgentComponentEntityAssociated().AddUObject(this, &UNarrativeMassAgentComponent::AssignActorFragment);
	}
	
	Super::OnRegister();
}

void UNarrativeMassAgentComponent::AssignActorFragment(const UMassAgentComponent& MassAgentComponent)
{
	if (&MassAgentComponent != this) { return; }

	auto SetActorFragment = [&]()
	{
		// Net simulated actors will get handled automatically
		if (IsNetSimulating()) { return; }
		
		FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*GetWorld());
		FMassEntityView EntityView = FMassEntityView(EntityManager, GetEntityHandle());

		if (EntityView.IsValid())
		{
			// Assign actor fragment to the owner if its not already set
			if (FMassActorFragment* ActorInfo = EntityView.GetFragmentDataPtr<FMassActorFragment>())
			{
				if (!ActorInfo->IsValid())
				{
					ActorInfo->SetAndUpdateHandleMap(AgentHandle, GetOwner(), false);
#if ENABLE_VISUAL_LOG
					UE_VLOG(GetOwner(), LogMassVehicle, Log, TEXT("FMassActorFragment assigned to actor: %s"), *GetOwner()->GetName());
#endif
				}
			}
			else
			{
				UE_LOG(LogMassVehicle, Error, TEXT("FMassActorFragment not found for MassAgent: %s"), *GetName());
			}
		}
	};

	//SetActorFragment();
	//GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, SetActorFragment));
}

void UNarrativeMassAgentComponent::IncomingCollisionDetected(const FMassEntityHandle& EntityHandle)
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*GetWorld());

	if (!EntityManager.IsEntityActive(EntityHandle))
	{
		OnIncomingCollision.Broadcast(nullptr);
		return;
	}
	
	FMassEntityView EntityView = FMassEntityView(EntityManager, EntityHandle);

	if (EntityView.IsValid())
	{
		const FMassActorFragment* ActorFragment = EntityView.GetFragmentDataPtr<FMassActorFragment>();
		const AActor* Actor = (ActorFragment && ActorFragment->IsValid()) ? ActorFragment->Get() : nullptr;

		if (Actor)
		{
			OnIncomingCollision.Broadcast(Actor);
		}
	}
}

