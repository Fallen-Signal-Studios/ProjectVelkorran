// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MassCrowdVisualizationTrait.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "Campaign/SovCampaignMassTypes.h"
#include "SovCampaignMassProxy.generated.h"

/** Non-savable, noncombat visual. Stable ownership belongs exclusively to the encounter record/entity. */
UCLASS(NotBlueprintable, Transient)
class PROJECTVELKORRAN_API ASovCampaignMassProxy : public AActor
{
	GENERATED_BODY()
public:
	ASovCampaignMassProxy();
	bool RestoreVisuals(const TArray<FSovCampaignMassMesh>& Meshes, bool bAnimated = false);
	static bool ValidateAnimationProfile(const TArray<FSovCampaignMassMesh>& Meshes, FString& Error);
	void SetRouteMotion(bool bMoving, float Speed);
	void CaptureVisualState(TArray<FSovCampaignMassMesh>& Meshes) const;
private:
	UPROPERTY(Transient) TArray<TObjectPtr<class UMeshComponent>> PoseMeshes;
	TArray<float> ReferenceSpeeds;
};

/** Uses the existing Narrative crowd representation subsystem/actor management, without ped randomization. */
UCLASS()
class PROJECTVELKORRAN_API USovCampaignMassVisualizationTrait : public UMassCrowdVisualizationTrait
{
	GENERATED_BODY()
public:
	USovCampaignMassVisualizationTrait();
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS()
class PROJECTVELKORRAN_API USovCampaignMassMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	USovCampaignMassMovementProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	FMassEntityQuery Query;
};
