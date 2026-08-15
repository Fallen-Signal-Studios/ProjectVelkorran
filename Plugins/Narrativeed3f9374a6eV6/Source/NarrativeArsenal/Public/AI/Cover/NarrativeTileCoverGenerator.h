// Copyright Narrative Tools 2025.

#pragma once

#include "AI/Navigation/NarrativeRecastNavMesh.h"
#include "UObject/GCObject.h"

/**
 * this object takes any number of tiles and processes them for cover.
 * note: when any cover generator object exists, the recast nav mesh actor in the level world will not be garbage collected.
 */
class FNarrativeTileCoverGenerator : public FNoncopyable, public FGCObject
{
public:

	TObjectPtr<ANarrativeRecastNavMesh> RecastNavMesh;

	// tile refs this task is to process
	TArray<uint64> TileRefs;
	// tile indexes this task is to process
	TArray<uint32> TileIndexes;
	// tile cords to process
	TArray<FIntPoint> TileCoords;
	// tile bounds to process
	TMap<uint64, FBox> TileBounds;
	// tiles to process, neighbours
	TMap<uint64, TArray<uint64>> NeighbourTileIndexes;

	// layer of the tiles
	int32 TileLayer;

	TArray<FTileCover*> TileCoverElems;
	TArray<TArray<FNavigationWallEdge>> WallEdges;

	double ChainLinkAngleTolerance;
	double SmallestChainLinkLength;
	double ChainLinkCorrectionAngleTolerance;
	uint8 CorrectionIterationCount;
	double PendingTileCreationTime;

	// true when work is done
	bool bComplete = false;

public:
	
	FNarrativeTileCoverGenerator(
		ANarrativeRecastNavMesh* NarrativeRecastNavMesh,
		const TArray<uint64>& InTileRefs,
		const TArray<uint32>& InTileIndexes,
		double InChainLinkAngleTolerance,
		double InSmallestChainLinkLength,
		double InChainLinkCorrectionAngleTolerance,
		uint8 InCorrectionIterationCount,
		double InPendingTileCreationTime);

	void Setup();
	void DoWork();
	// remove any data allocated and refs
	void DumpAsyncData();

	bool AddLinkToExistingChain(const FChainLink& InChainLink, TArray<FCoverChainContainer>& InCover);
	
	// loops over provided covers and will merge any covers that should be one single cover
	static void MergeCovers(TArray<FCoverChainContainer>& Covers, double ChainLinkAngleTolerance);

	// loops over provided covers and will remove any links that are too small, and combine links that are to be straight
	static void FilterLinks(TArray<FCoverChainContainer>& Covers, double MinLinkLength, double MaxAngleForMerge, uint8 IterationCount);

	/* FGCObject */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	/* FGCObject */
};
