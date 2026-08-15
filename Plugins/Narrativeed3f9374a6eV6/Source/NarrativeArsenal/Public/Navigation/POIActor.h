// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TargetPoint.h"
#include <GameplayTagContainer.h>

#include "POIRenderingComponent.h"
#include "Engine/Texture2D.h"
#include "POIActor.generated.h"

/**
 * These actors easily allow designers to create POIs that show up on the map. They are also world partitioned and by default will become discovered
 * when your overlap them - so add any overlap volumes you want to your POI to define its discovery bounds. 
 */
UCLASS()
class NARRATIVEARSENAL_API APOIActor : public ATargetPoint
{
	GENERATED_BODY()
	
public:

	APOIActor(const FObjectInitializer& ObjectInitializer);

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

	/** The tag for this location*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Point Of Interest", meta = (Categories = "Narrative.POIs"))
	FGameplayTag POITag;

	/** Whether this POI needs a map marker or not */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Point Of Interest")
	bool bCreateMapMarker;

	/** Whether this POI supports being fast travelled to or not  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Point Of Interest")
	bool bSupportsFastTravel;

	/** The display name for this poi when we display it in the UI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Point Of Interest")
	FText POIDisplayName;

	/** The POI marker to display on the map  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Point Of Interest")
	TObjectPtr<class UTexture2D> POIIcon;

	/** POIs this POIActor is linked to - used when NPCs do high-level navigation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Point Of Interest")
	TArray<TSoftObjectPtr<APOIActor>> LinkedPOIs;

	//Used to define where fast travelling places our character
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UCapsuleComponent> FastTravelCapsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Display, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UArrowComponent> FastTravelCapsuleArrow;

#if WITH_EDITORONLY_DATA
	UPROPERTY(NoClear)
	UPOIRenderingComponent* RenderingComponent;
#endif

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category="Editor Tools")
	void CreatePOIVolume();
#endif
};
