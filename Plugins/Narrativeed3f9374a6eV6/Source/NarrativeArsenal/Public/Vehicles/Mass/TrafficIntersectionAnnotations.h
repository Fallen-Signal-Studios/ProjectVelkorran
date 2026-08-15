// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "TrafficLightSubsystem.h"
#include "ZoneGraphAnnotationComponent.h"
#include "TrafficIntersectionAnnotations.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class NARRATIVEARSENAL_API UTrafficIntersectionAnnotations : public UZoneGraphAnnotationComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTrafficIntersectionAnnotations();

	virtual void HandleEvents(const FInstancedStructContainer& Events) override;
	virtual void TickAnnotation(const float DeltaTime, FZoneGraphAnnotationTagContainer& AnnotationTagContainer) override;
	virtual void PostSubsystemsInitialized() override;

#if UE_ENABLE_DEBUG_DRAWING
	virtual void DebugDraw(FZoneGraphAnnotationSceneProxy* DebugProxy) override;
#endif

	UPROPERTY(EditAnywhere, Category = "TrafficIntersection")
	FZoneGraphTag CloseLaneTag;
	
	TArray<FTrafficPeriodEvent> PeriodEvents;

	UPROPERTY(Transient)
	TObjectPtr<UTrafficLightSubsystem> TrafficLightSubsystem;
};
