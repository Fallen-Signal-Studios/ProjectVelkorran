// Copyright Narrative Tools 2025.


#include "Navigation/MapTileRenderingComponent.h"

#include "Navigation/MapTileBounds.h"

class FMapTileRenderingSceneProxy final : public FDebugRenderSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}
	
	explicit FMapTileRenderingSceneProxy(const UMapTileRenderingComponent& InComponent)
		: FDebugRenderSceneProxy(&InComponent)
	{
		const AMapTileBounds* Owner = InComponent.GetOwner<AMapTileBounds>();
		if (Owner == nullptr)
		{
			return;
		}
		
		bDrawEvenIfNotSelected = !Owner->HasAnyFlags(RF_WasLoaded);

		for (const auto& POIData : Owner->POIs)
		{
			for (const FGameplayTag& LinkedPOITag : POIData.LinkedPOIs)
			{
				auto LinkedPOI = Owner->POIs.FindByPredicate([&LinkedPOITag](const FPOIData& POIDataTest)
				{
					return POIDataTest.POITag == LinkedPOITag;
				});

				if (LinkedPOI)
				{
					ArrowLines.Emplace(POIData.POILocation, LinkedPOI->POILocation, FColor::Red);
				}
			}
			//Texts.Emplace(POIData.POITag.ToString(), POIData.POILocation + FVector::UpVector * 100.f, FColor::Red);
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View) && (IsSelected() || bDrawEvenIfNotSelected);
		Result.bDynamicRelevance = true;
		return Result;
	}

	bool bDrawEvenIfNotSelected = true;
};

UMapTileRenderingComponent::UMapTileRenderingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FDebugRenderSceneProxy* UMapTileRenderingComponent::CreateDebugSceneProxy()
{
	return new FMapTileRenderingSceneProxy(*this);
}

FBoxSphereBounds UMapTileRenderingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox NewBounds = FBox::BuildAABB(GetComponentLocation(), FVector(10.f));
	
	const AMapTileBounds* Owner = GetOwner<AMapTileBounds>();
	for (const FPOIData& POI : Owner->POIs)
	{
		NewBounds += FBox::BuildAABB(POI.POILocation, FVector(10.f));
	}
	
	return NewBounds;
}
