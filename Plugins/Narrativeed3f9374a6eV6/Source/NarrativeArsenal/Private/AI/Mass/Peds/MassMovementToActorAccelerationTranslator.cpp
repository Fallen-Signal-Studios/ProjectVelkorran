// Copyright Narrative Tools 2025.


#include "AI/Mass/Peds/MassMovementToActorAccelerationTranslator.h"

#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassNavigationFragments.h"
#include "Character/NarrativeCharacterMovement.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Translators/MassCharacterMovementTranslators.h"
#include "MassMovementFragments.h"

UMassMovementToActorAccelerationTranslator::UMassMovementToActorAccelerationTranslator()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Movement);
	RequiredTags.Add<FMassCharacterMovementCopyToActorTag>();
	bRequiresGameThreadExecution = true;
}

void UMassMovementToActorAccelerationTranslator::ConfigureQueries(const TSharedRef<FMassEntityManager>& Manager)
{
	AddRequiredTagsToQuery(EntityQuery);
	EntityQuery.AddRequirement<FCharacterMovementComponentWrapperFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
}

void UMassMovementToActorAccelerationTranslator::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [this](FMassExecutionContext& Context)
	{
		const auto ComponentList = Context.GetMutableFragmentView<FCharacterMovementComponentWrapperFragment>();
		const auto MoveTargetFragments = Context.GetFragmentView<FMassMoveTargetFragment>();
		auto VelocityFragments = Context.GetFragmentView<FMassVelocityFragment>();
		
		const int32 NumEntities = Context.GetNumEntities();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			if (UCharacterMovementComponent* AsMovementComponent = ComponentList[i].Component.Get())
			{
				auto& MoveTargetFragment = MoveTargetFragments[i];
				FVector Input = VelocityFragments[i].Value.GetSafeNormal();

				// Send move to character movement component
				AsMovementComponent->RequestPathMove(Input);

				// Use slow walk for narrative npcs, and max walk speed otherwise
				// @todo this may be better done within an initializer and should be explicit when modifying speed vars to avoid confusion
				if (auto NarrativeMovement = Cast<UNarrativeCharacterMovement>(AsMovementComponent))
				{
					NarrativeMovement->SlowWalkSpeed = MoveTargetFragment.DesiredSpeed.Get();
				}
				else
				{
					AsMovementComponent->MaxWalkSpeed = MoveTargetFragment.DesiredSpeed.Get();
				}
			}
		}
	});
}
