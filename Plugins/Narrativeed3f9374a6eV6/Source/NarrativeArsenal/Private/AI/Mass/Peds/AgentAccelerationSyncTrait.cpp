// Copyright Narrative Tools 2025.


#include "AI/Mass/Peds/AgentAccelerationSyncTrait.h"

#include "MassEntityTemplateRegistry.h"
#include "MassEntityView.h"
#include "MassMovementFragments.h"
#include "AI/Mass/MassAgentTraitsHelper.h"
#include "AI/Mass/Peds/MassMovementToActorAccelerationTranslator.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Translators/MassCharacterMovementTranslators.h"

void UAgentAccelerationSyncTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
                                                const UWorld& World) const
{
	// We mainly do the same logic compared to the MovementSyncTrait, the main difference is in the MassToActor translator
	BuildContext.AddFragment<FCharacterMovementComponentWrapperFragment>();
	BuildContext.AddFragment<FMassVelocityFragment>();
	
	BuildContext.GetMutableObjectFragmentInitializers().Add([=](UObject& Owner, FMassEntityView& EntityView, const EMassTranslationDirection CurrentDirection)
	{
		if (UCharacterMovementComponent* MovementComp = FMassAgentTraitsHelper::AsComponent<UCharacterMovementComponent>(Owner))
		{
			FCharacterMovementComponentWrapperFragment& ComponentFragment = EntityView.GetFragmentData<FCharacterMovementComponentWrapperFragment>();
			
			ComponentFragment.Component = MovementComp;

			FMassVelocityFragment& VelocityFragment = EntityView.GetFragmentData<FMassVelocityFragment>();

			// the entity is the authority
			if (CurrentDirection ==  EMassTranslationDirection::MassToActor)
			{
				MovementComp->bRunPhysicsWithNoController = true;
				MovementComp->SetMovementMode(EMovementMode::MOVE_Walking);
				MovementComp->Velocity = VelocityFragment.Value;
			}
			// actor is the authority
			else
			{
				VelocityFragment.Value = MovementComp->GetLastUpdateVelocity();
			}
		}
	});

	if (EnumHasAnyFlags(SyncDirection, EMassTranslationDirection::ActorToMass) || BuildContext.IsInspectingData())
	{
		BuildContext.AddTranslator<UMassCharacterMovementToMassTranslator>();
	}

	if (EnumHasAnyFlags(SyncDirection, EMassTranslationDirection::MassToActor) || BuildContext.IsInspectingData())
	{
		BuildContext.AddTranslator<UMassMovementToActorAccelerationTranslator>();
	}
}
