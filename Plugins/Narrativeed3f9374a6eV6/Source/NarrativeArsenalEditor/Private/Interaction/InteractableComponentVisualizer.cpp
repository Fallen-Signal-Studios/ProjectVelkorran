// Copyright Narrative Tools 2024. 


#include "Interaction/InteractableComponentVisualizer.h"
#include "Interaction/InteractableComponent.h"

void FNarrativeInteractableComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UNarrativeInteractableComponent* InteractableComp = Cast<const UNarrativeInteractableComponent>(Component);
	if (InteractableComp == nullptr)
	{
		return;
	}

	const AActor* InteractableOwner = InteractableComp->GetOwner();

	if(InteractableOwner == nullptr)
	{
		return;
	}

	FColor Color = FColor::Green;

	const FTransform OwnerLocalToWorld = InteractableOwner->GetActorTransform();

	for (int32 i = 0; i < InteractableComp->InteractionSlots.Num(); ++i)
	{
		if (InteractableComp->InteractionSlots.IsValidIndex(i))
		{
			FInteractionSlotConfig SlotConfig = InteractableComp->InteractionSlots[i];
			constexpr float DebugCylinderRadius = 40.f;
			TOptional<FTransform> Transform = SlotConfig.SlotTransform * OwnerLocalToWorld;
			Color = SlotConfig.DebugColor;


			if (!Transform.IsSet())
			{
				continue;
			}

			//Draw yellow arrows showing which slots we are linked to 
			if (SlotConfig.LinkedSlots.Num())
			{
				for (auto& LinkedSlot : InteractableComp->GetSlotsAtIndex(i))
				{
					if (InteractableComp->InteractionSlots.IsValidIndex(LinkedSlot))
					{
						FInteractionSlotConfig LinkedSlotConfig = InteractableComp->InteractionSlots[LinkedSlot];
						FTransform LinkedSlotTransform = LinkedSlotConfig.SlotTransform * OwnerLocalToWorld;

						FTransform ArrowTransform = Transform.GetValue();
						ArrowTransform.SetRotation((LinkedSlotTransform.GetLocation() - Transform.GetValue().GetLocation()).Rotation().Quaternion());
						const float ArrowLen = FVector::Dist(Transform.GetValue().GetLocation(), LinkedSlotTransform.GetLocation());

						DrawDirectionalArrow(PDI, ArrowTransform.ToMatrixNoScale(), FColor::Yellow, ArrowLen, /*ArrowSize*/1.f, SDPG_World, /*Thickness*/2.0f);
					}
				}
			}


			//Draw the slot itself 
			const FVector Location = Transform.GetValue().GetLocation();

			DrawDirectionalArrow(PDI, Transform.GetValue().ToMatrixNoScale(), Color, 2.f*DebugCylinderRadius, /*ArrowSize*/1.f, SDPG_World, /*Thickness*/1.0f);
			DrawCircle(PDI, Location, FVector::XAxisVector, FVector::YAxisVector, Color, DebugCylinderRadius, /*NumSides*/64, SDPG_World, /*Thickness*/2.f);

			if (IsValid(SlotConfig.SlotInteractBehavior))
			{
				for (auto& DebugSlot : SlotConfig.SlotInteractBehavior->GetDebugSlots(SlotConfig.SlotTransform, OwnerLocalToWorld))
				{
					DrawDirectionalArrow(PDI, DebugSlot.SlotDebugTransform.ToMatrixNoScale(), DebugSlot.SlotDebugColor, 1.5f*DebugCylinderRadius, /*ArrowSize*/1.f, SDPG_World, /*Thickness*/1.0f);
					DrawCircle(PDI,DebugSlot.SlotDebugTransform.GetLocation(), FVector::XAxisVector, FVector::YAxisVector, DebugSlot.SlotDebugColor, DebugCylinderRadius * 0.8f, /*NumSides*/64, SDPG_World, /*Thickness*/2.f);
				}
			}
		}
	}
}
