// Copyright Narrative Tools 2022. 


#include "Navigation/MapTileBounds.h"
#include <Components/BoxComponent.h>

#include "Navigation/MapTileRenderingComponent.h"

// Sets default values
AMapTileBounds::AMapTileBounds()
{

	MapWidth = 10000.f;
	NumTiles = 3;

	MapTileBounds = CreateDefaultSubobject<UBoxComponent>("MapTileBounds");
	MapTileBounds->SetLineThickness(10.f);
	MapTileBounds->SetBoxExtent(FVector(MapWidth / 2, MapWidth / 2, 1.f));
	MapTileBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(MapTileBounds);

#if WITH_EDITOR

	SetIsSpatiallyLoaded(false);

#endif

#if WITH_EDITORONLY_DATA
	RenderingComponent = CreateEditorOnlyDefaultSubobject<UMapTileRenderingComponent>(TEXT("RenderComp"));
#endif

}
#if WITH_EDITOR
void AMapTileBounds::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (MapTileBounds)
	{
		MapTileBounds->SetBoxExtent(FVector(MapWidth / 2, MapWidth / 2, 1.f));
	}
}
#endif 
