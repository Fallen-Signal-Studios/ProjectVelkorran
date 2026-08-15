// Copyright Narrative Tools 2025.

#pragma once

#include "NavMesh/RecastNavMesh.h"
#include "AI/Cover/CoverTypes.h"
#include "NarrativeRecastNavMesh.generated.h"

struct FDirtyTile
{
	uint64 X;
	uint64 Y;
	uint32 Layer;

	bool operator==(const FDirtyTile& Other) const
	{
		return Other.X == X && Other.Y == Y && Other.Layer == Layer;
	}
};

/**
 * extension to ARecastNavMesh that implements cover generation.
 */
UCLASS()
class NARRATIVEARSENAL_API ANarrativeRecastNavMesh : public ARecastNavMesh
{
	GENERATED_BODY()

	friend class FNarrativeTileCoverGenerator;
	friend class UNarrativeNavigationSystem;
	
protected:

	// list of tiles that need / are being updated
	TArray<uint64> DirtyTiles;

	/// Mutex lock to ensure no race conditions
	/// any and all access to 'TilesCovers' is to be guarded by this
	mutable FCriticalSection TileCoverMutex;
	
	// 1 to 1 length array of cover for each tile in the nav mesh
	UPROPERTY()
	TArray<FTileCover> TilesCovers;

	/// running async tasks that are processing and generating cover.
	/// as long as one of these exist, this actor will not be garbage collected.
	TArray<FRunningCoverTask> RunningCoverTasks;

	// total running cover generation tasks
	int32 NumRunningTasks;

public:

	// returns the save normal from the link start to end
	static FVector GetLinkDirectionNormal(const FChainLink& ChainLink) { return ChainLink.GetStartNormal(); }

	/// returns true if the link candidate is connected to the chain link, and is within the angle tolerance comparing
	/// the link candidates direction to the chain link direction.
	/// NOTE: assumes both FChainLink params are valid FChainLink's
	static bool DoesLinkFitInChain(const FChainLink& LinkCandidate, const FChainLink& ChainLink, const float AngleTolerance);

protected:
	
	// appends any tiles to the dirty tile list to have work done on them.
	void QueueTilesForUpdate(const TArray<FNavTileRef>& ChangedTiles);

public:
	
	// rebuilds cover for dirty tiles. may trigger nav mesh regeneration.
	void RebuildCoverV2();

	/* ARecastNavMesh */
	virtual void OnNavMeshTilesUpdated(const TArray<FNavTileRef>& ChangedTiles) override;
	virtual void OnNavMeshGenerationFinished() override;
	virtual void RebuildAll() override;
	/* ARecastNavMesh */

	// called each frame 
	void TickAsyncCoverBuild(float DeltaSeconds);
	// called when cover generation is complete
	void OnCoverGenerationFinished();

	const FTileCover& GetTileCover(const uint64 TileIndex) const { return TilesCovers.IsValidIndex(TileIndex)? TilesCovers[TileIndex] : FTileCover::Invalid; }

protected:
	
	UFUNCTION(CallInEditor)
	void DebugDrawTemp();
	
	UFUNCTION(CallInEditor)
	void DebugRebuild();
};
