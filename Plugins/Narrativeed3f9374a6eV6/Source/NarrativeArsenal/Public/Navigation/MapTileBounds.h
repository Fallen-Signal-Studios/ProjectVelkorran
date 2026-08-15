// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <GameplayTagContainer.h>
#include "MapTileBounds.generated.h"


class UMapTileRenderingComponent;

USTRUCT(BlueprintType)
struct FMapTile
{

	GENERATED_BODY()

	FMapTile()
	{
		TileLocation = FVector();
	}

	//Tile image - will be deprecated in favour of TileImages. 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Map Tiles")
	TObjectPtr<class UTexture2D> TileImage;

	//Tile images
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Map Tiles")
	TMap<FGameplayTag, TObjectPtr<class UTexture2D>> TileImages;

	//World location of the tile 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Map Tiles")
	FVector TileLocation;

};

USTRUCT(BlueprintType)
struct FMapTileSet
{

	GENERATED_BODY()

	FMapTileSet()
	{
		TileImageSize = 0.f;
		GridWidth = 0;
	}

	//A tag identifying the layer this tile set is in
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Map Tiles")
	FGameplayTag TileLayer; 

	//Probably not required any more, this is the dimensions of the tile set. 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Map Tiles")
	int32 GridWidth;

	//The resolution we should draw each tile in this set at 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Map Tiles")
	float TileImageSize; 

	//The tiles in the set. 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Map Tiles")
	TArray<FMapTile> Tiles;
};


USTRUCT(BlueprintType)
struct FPOIData
{

	GENERATED_BODY()

	FPOIData() 
	{
		POIDisplayName = NSLOCTEXT("PointOfInterestLocation", "POIDisplayName", "Point of Interest");
		POISubtitle = NSLOCTEXT("PointOfInterestSubtitle", "POISubtitle", "Location");
		POILocation = FVector();
		bNeedsMapMarker = true;
		bSupportsFastTravel = true; 
		bIsDiscoverable = false; 

	};

	//The POI Tag ID 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Point of Interest", meta = (Categories = "Navigator.PointOfInterest"))
	FGameplayTag POITag;

	//The POIs we're linked to - this is considered by the high level navigation graph 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Point of Interest", meta = (Categories = "Navigator.PointOfInterest"))
	TArray<FGameplayTag> LinkedPOIs;

	//Fixed location the POI can refer to - useful for POIs that don't move around. 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Point of Interest")
	FVector POILocation;

	//Transform fast travelling to the POI should drop us at.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Point of Interest")
	FTransform POIFastTravelSpot;

	//Whether the POI needs a map marker or if it just exists as data. 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Point of Interest")
	bool bNeedsMapMarker; 
	
	//Whether the POI supports being fast travelled to 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Point of Interest")
	bool bSupportsFastTravel;

	//Whether the POI is considered discoverable, like a city, camp, etc. Icon will be greyed out until POI is discovered. 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Point of Interest")
	bool bIsDiscoverable; 

	/** The POI marker to display on the map  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Point Of Interest")
	TObjectPtr<class UTexture2D> MapMarkerIcon;

	//The display name the POI should show, for example if we discover a POI. 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Point of Interest")
	FText POIDisplayName;

	//The subtitle for the POI on the map 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Point of Interest")
	FText POISubtitle; 
};


UCLASS()
class NARRATIVEARSENAL_API AMapTileBounds : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMapTileBounds();

	//The tiles the tile generator has created 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Map Tiles")
	FMapTileSet TileSet;

	//All POIs found in the world are cached in here so we can create map markers for them 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Map Tiles")
	TArray<FPOIData> POIs;

	/** The width our map should be, in unreal units (cm)*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Map Bounds")
	float MapWidth;
	
	/*The number of 1024x1024 tiles that should make up the map. For smaller maps you could use a single tile - for large open world maps you'll want more
	or your map will become blurry and low resolution. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Map Bounds")
	int32 NumTiles;

	UPROPERTY(BlueprintReadOnly, Category = "Map Bounds")
	class UBoxComponent* MapTileBounds;

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(NoClear)
	UMapTileRenderingComponent* RenderingComponent;
#endif

	
};
