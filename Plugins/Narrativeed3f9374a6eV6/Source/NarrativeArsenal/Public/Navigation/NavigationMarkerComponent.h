// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "MapMarker.h"
#include "Components/ActorComponent.h"
#include "NavigationMarkerComponent.generated.h"


/**Add this component to any actor you want to show up on Narrative Navigator UI like the compass, world map, etc. */
UCLASS( ClassGroup=(Narrative), Blueprintable, meta=(BlueprintSpawnableComponent) )
class NARRATIVEARSENAL_API UNavigationMarkerComponent : public UActorComponent
{
	GENERATED_BODY()

protected:

	// Sets default values for this component's properties
	UNavigationMarkerComponent(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void RegisterMarker();
	virtual void RemoveMarker();

	virtual void Activate(bool bReset) override;
	virtual void Deactivate() override;
	virtual void DestroyComponent(bool bPromoteChildren) override;

	UPROPERTY(EditAnywhere, Instanced, Category = "Marker Setup")
	TObjectPtr<class UMapMarker> MarkerObject; 

};
