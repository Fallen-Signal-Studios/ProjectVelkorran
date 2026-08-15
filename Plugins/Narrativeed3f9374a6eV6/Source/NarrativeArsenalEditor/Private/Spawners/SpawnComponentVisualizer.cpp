// Copyright Narrative Tools 2024. 


#include "Spawners/SpawnComponentVisualizer.h"
#include "Spawners/SpawnComponent.h"
#include <CanvasTypes.h>
#include "Spawners/NPCSpawnComponent.h"
#include "AI/NPCDefinition.h"
#include <Editor.h>
#include <Editor/EditorEngine.h>
#include <Selection.h>

IMPLEMENT_HIT_PROXY(HSpawnVisProxy, HComponentVisProxy);

void FSpawnComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const USpawnComponent* SpawnComp = Cast<USpawnComponent>(Component);
	if (SpawnComp == nullptr)
	{
		return;
	}

	const AActor* SpawnOwner = SpawnComp->GetOwner();

	if (SpawnOwner == nullptr)
	{
		return;
	}

	FColor Color = SpawnComp->bAutoActivate ? FColor::Green : FColor::Red;

	constexpr float DebugCylinderRadius = 40.f;
	const FTransform Transform = SpawnComp->GetComponentTransform();

	PDI->SetHitProxy(new HSpawnVisProxy(Component));

	DrawDirectionalArrow(PDI, Transform.ToMatrixNoScale(), Color, 2.f * DebugCylinderRadius, /*ArrowSize*/5.f, SDPG_World, /*Thickness*/1.0f);
	DrawWireDiamond(PDI, Transform.ToMatrixNoScale(), 50.f, Color, 0, 3.f);
}

bool FSpawnComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	if (GEditor && VisProxy && VisProxy->Component.IsValid())
	{
		GEditor->GetSelectedComponents()->Modify();
		GEditor->GetSelectedComponents()->DeselectAll();
		GEditor->SelectComponent(const_cast<UActorComponent*>(VisProxy->Component.Get()), true, true);
	}

	return false;
}

void FSpawnComponentVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	const USpawnComponent* SpawnComp = Cast<USpawnComponent>(Component);
	if (SpawnComp == nullptr)
	{
		return;
	}

	if (SpawnComp)
	{
		FString SpawnNPCName = SpawnComp->GetEditorLabel();

		FVector2D PixelLoc;
		if (View->WorldToPixel(SpawnComp->GetComponentLocation(), PixelLoc))
		{
			Canvas->DrawShadowedString(PixelLoc.X, PixelLoc.Y, *SpawnNPCName, GEngine->GetMediumFont(), FLinearColor::White);
		}
	}
}

void FNPCSpawnComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UNPCSpawnComponent* SpawnComp = Cast<UNPCSpawnComponent>(Component);
	if (SpawnComp == nullptr)
	{
		return;
	}

	const AActor* SpawnOwner = SpawnComp->GetOwner();

	if(SpawnOwner == nullptr)
	{
		return;
	}

	FColor Color = SpawnComp->bAutoActivate ? FColor::Green : FColor::Red;

	constexpr float DebugCylinderRadius = 40.f;
	const FTransform Transform = SpawnComp->GetComponentTransform();

	PDI->SetHitProxy(new HSpawnVisProxy(Component));

	DrawDirectionalArrow(PDI, Transform.ToMatrixNoScale(), Color, 2.f*DebugCylinderRadius, /*ArrowSize*/5.f, SDPG_World, /*Thickness*/1.0f);
	DrawWireCapsule(PDI, Transform.GetLocation(), Transform.GetRotation().GetAxisX(), Transform.GetRotation().GetAxisY(), Transform.GetRotation().GetAxisZ(), Color, 40.f, 88.f, 20, 0, 2.f, 1.f, false);

	PDI->SetHitProxy(NULL);
}

void FNPCSpawnComponentVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	FSpawnComponentVisualizer::DrawVisualizationHUD(Component, Viewport, View, Canvas);
}
