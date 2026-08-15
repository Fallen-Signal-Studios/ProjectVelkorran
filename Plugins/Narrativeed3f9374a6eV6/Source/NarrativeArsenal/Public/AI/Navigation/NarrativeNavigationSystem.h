// Copyright Narrative Tools 2025.

#pragma once

#include "NavigationSystem.h"
#include "AI/Cover/CoverTypes.h"
#include "NarrativeNavigationSystem.generated.h"

/**
 * extenstion on UNavigationSystemV1 that interfaces with cover generation.
 */
UCLASS(Within=World, config=Engine, defaultconfig)
class NARRATIVEARSENAL_API UNarrativeNavigationSystem : public UNavigationSystemV1
{
	GENERATED_BODY()

protected:
	
	virtual void Tick(float DeltaSeconds) override;

public:
	
	UNarrativeNavigationSystem(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/*
	 * TODO: add a function that grabs the cover chain and its nearest connected chain neighbours.
	 */
	
	/**
	 * gets all cover within a radius of an actor.
	 * @param WorldContext world context.
	 * @param Point the given actor.
	 * @param FoundCover any cover that was found.
	 * @param SearchRadius how far can the cover be.
	 * @return true if any cover is found. 
	 */
	/* TODO: right now this only looks at the current tile and neighbours, it needs to actually use the radius lmao */
	UFUNCTION(BlueprintCallable, Category="Cover", meta=(WorldContext="WorldContext"))
	static bool FindAllCoverInRadiusToPoint(const UObject* WorldContext, const FVector& Point, TArray<FCoverContainer>& FoundCover, float SearchRadius = 100.0f);

	/**
	 * finds the nearest cover to the given actor.
	 * @param WorldContext world context.
	 * @param Point the given point.
	 * @param FoundCover any cover that was found.
	 * @param ChainLinkIndex found cover chain link index.
	 * @param SearchRadius how far can the cover be.
	 * @param DirectionBias direction of the cover to find. if FVector::UpVector is passed in, no direction bias is applied.
	 * @return true if any cover was found.
	 */
	UFUNCTION(BlueprintCallable, Category="Cover", meta=(WorldContext="WorldContext"))
	static bool FindNearestCoverToPoint(const UObject* WorldContext, const FVector& Point, TArray<FChainLink>& FoundCover, int32& ChainLinkIndex, FVector& NearestPoint, float SearchRadius = 100.0f, FVector DirectionBias = FVector::UpVector, float DirectionBiasToleranceAngle = 45.0f);

	/**
	 * returns if the given point is closer to the start of a given cover chain link.
	 * @param Point the given point.
	 * @param ChainLink the given chain link in cover.
	 * @return true, when the point is closer to the start. false when it is closer to the end.
	 */
	UFUNCTION(BlueprintPure, Category="Cover")
	static bool IsPointCloserToStartOfChainLink(const FVector& Point, const FChainLink& ChainLink)
	{
		return FVector::Distance(ChainLink.Start, Point) < FVector::Distance(ChainLink.End, Point);
	}

	static bool FindNearestCoverLink(const FVector& Point, const TArray<FChainLink>& ChainLinks, int32& ChainLinkIndex, FVector& NearestPoint);
	
	// finds the next cover connected to this cover
	bool FindNextSectionOfCover(const FCoverContainer& Cover, bool bSearchAtStartOfCover, FCoverContainer& OutCover) const;

	/**
	 * tests a point in world space applying the cover trace pattern and outputs what kind of cover it is.
	 * @param WorldContext world context.
	 * @param Point the given location.
	 * @param Direction direction of the cover.
	 * @param bIsLowCover true, if cover is low cover. false if high cover.
	 * @param bCanPeekLeft true if high cover and can peek left.
	 * @param bCanPeekRight true if high cover and can peek right.
	 * @return true if cover point is valid cover at all.
	 */
	UFUNCTION(BlueprintCallable, Category="Cover", meta=(WorldContext="WorldContext"))
	static UPARAM(DisplayName="Valid Cover") bool TestPointForCoverType(const UObject* WorldContext, const FVector& Point, const FVector& Direction, bool& bIsLowCover, bool& bCanPeekLeft, bool& bCanPeekRight);

	static FCoverTraceConfig GetCoverTraceConfig();

	static void RebuildAllCover();
	
};
