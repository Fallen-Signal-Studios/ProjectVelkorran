// Copyright Narrative Tools 2025.

#include "AI/Cover/NarrativeTileCoverGenerator.h"
#include "Detour/DetourNavMesh.h"
#include "NavMesh/RecastHelpers.h"

// takes a given point and checks if it falls on the edge of an FBox's X Y cords, and returns what tile neighbour it leads to.
static ETileEdge GetTargetNeighbour(const FVector& Point, const FBox& Box)
{
	if (Point.X == Box.Min.X && Point.Y >  Box.Min.Y && Point.Y < Box.Max.Y) { return ETileEdge::Bottom;      }
	if (Point.Y == Box.Min.Y && Point.X == Box.Min.X)                        { return ETileEdge::BottomLeft;  }
	if (Point.Y == Box.Min.Y && Point.X >  Box.Min.X && Point.X < Box.Max.X) { return ETileEdge::Left;        }
	if (Point.Y == Box.Min.Y && Point.X == Box.Max.X)                        { return ETileEdge::TopLeft;     }
	if (Point.X == Box.Max.X && Point.Y >  Box.Min.Y && Point.Y < Box.Max.Y) { return ETileEdge::Top;         }
	if (Point.Y == Box.Max.Y && Point.X == Box.Max.X)                        { return ETileEdge::TopRight;    }
	if (Point.Y == Box.Max.Y && Point.X >  Box.Min.X && Point.X < Box.Max.X) { return ETileEdge::Right;       }
	if (Point.Y == Box.Max.Y && Point.X == Box.Min.X)                        { return ETileEdge::BottomRight; }
	return ETileEdge::None;
};

FNarrativeTileCoverGenerator::FNarrativeTileCoverGenerator(ANarrativeRecastNavMesh* NarrativeRecastNavMesh, const TArray<uint64>& InTileRefs,
	const TArray<uint32>& InTileIndexes, double InChainLinkAngleTolerance, double InSmallestChainLinkLength,
	double InChainLinkCorrectionAngleTolerance, uint8 InCorrectionIterationCount, double InPendingTileCreationTime)
	: TileRefs(InTileRefs),
	  TileIndexes(InTileIndexes),
	  TileLayer(0),
	  ChainLinkAngleTolerance(InChainLinkAngleTolerance),
	  SmallestChainLinkLength(InSmallestChainLinkLength),
	  ChainLinkCorrectionAngleTolerance(InChainLinkCorrectionAngleTolerance),
	  CorrectionIterationCount(InCorrectionIterationCount),
	  PendingTileCreationTime(InPendingTileCreationTime)
{
	NeighbourTileIndexes.Reserve(8);
	RecastNavMesh = NarrativeRecastNavMesh;
}

void FNarrativeTileCoverGenerator::Setup()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FNarrativeTileCoverGenerator::Setup);
	
	// collect tile data
	const dtNavMesh* DetourNavMesh = RecastNavMesh->GetRecastMesh();
	for (int32 Index = 0; Index < TileRefs.Num(); ++Index)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CollectTileData);
		
		const uint32 TileIndex = TileIndexes[Index];
		const dtMeshTile* dtTile = DetourNavMesh->getTile(TileIndex);
		dtMeshHeader* dtMeshHeader = dtTile->header;
		TileLayer = dtMeshHeader->layer;
		TileBounds.Add(TileIndex, Recast2UnrealBox(dtMeshHeader->bmin, dtMeshHeader->bmax));
		TileCoords.Add({dtMeshHeader->x, dtMeshHeader->y});

		// remove current tile covers and boundary hash as it is about to change and may not have any cover anymore.
		RecastNavMesh->TileCoverMutex.Lock();
		TileCoverElems.Add(&RecastNavMesh->TilesCovers[TileIndex]);
		TileCoverElems[Index]->OwnedCovers.Reset(TileCoverElems[Index]->OwnedCovers.Num());
		TileCoverElems[Index]->BoundaryHash.Reset();
		RecastNavMesh->TileCoverMutex.Unlock();

		// get all neighbours
		for (int8 NeighbourIndex = 0; NeighbourIndex < 8; ++NeighbourIndex)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(GetAllNeighbours);
			
			int32 X, Y;
			DetourNavMesh->getNeighbourCoords(TileCoords[Index].X, TileCoords[Index].Y, NeighbourIndex, X, Y);
			
			// cords are not validated by default...
			if (const dtMeshTile* dtNeighbourTile = DetourNavMesh->getTileAt(X, Y, TileLayer))
			{
				const uint64 dtNeighbourTileIndex = DetourNavMesh->getTileIndex(dtNeighbourTile);
				NeighbourTileIndexes.FindOrAdd(TileIndex).Add(dtNeighbourTileIndex);
			}
			else
			{
				// fill null neighbour indexes to allow for fast indexing later
				NeighbourTileIndexes.FindOrAdd(TileIndex).Add(INDEX_NONE);
			}
		}

		const int32 WallEdgesIndex = WallEdges.Add({});
		// the tile poly ref handed to this worker is the last ref it was making it invalid for working with going forward.
		// however, the tile index does not change and so we can rebuild the tile ref from the index. 
		const dtPolyRef TileRef = DetourNavMesh->encodePolyId(dtTile->salt, TileIndex, TileLayer);
		RecastNavMesh.Get()->GetEdgesInTile(FNavTileRef(TileRef), WallEdges[WallEdgesIndex]);

		// if no wall edges exist, then no work needs to be done.
		bComplete = WallEdges[WallEdgesIndex].IsEmpty();
	}
}

void FNarrativeTileCoverGenerator::DoWork()
{
	// async
	TRACE_CPUPROFILER_EVENT_SCOPE(FNarrativeTileCoverGenerator::DoWork);

	if (bComplete)
	{
		DumpAsyncData();
		return;
	}

	for (int32 Index = 0; Index < TileRefs.Num(); ++Index)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(generateCover);
		
		const uint32 WorkingTileIndex = TileIndexes[Index];
		const TArray<uint64>& WorkingTileNeighbourIndexes = NeighbourTileIndexes[WorkingTileIndex];
		
		TArray<FCoverChainContainer> NewCoverChains;
		NewCoverChains.Reserve(18);

		// sort chains in order of connection, chain links are always left to right (end to start)
		Algo::Sort(WallEdges[Index], [](const FNavigationWallEdge& A, const FNavigationWallEdge& B)
		{
			return A.End.Equals(B.Start);
		});

		// each wall section in the tile is iterated over and either put into an existing cover chain.
		// if the wall does not fit in to an existing cover chain, then a new cover chain is created making it separate cover.
		for (const FNavigationWallEdge& TileWall : WallEdges[Index])
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(CollectWallEdges);

			// convert the wall edge to a friendly chain link type for processing
			FChainLink TileWallChainLink = FChainLink(TileWall);
			TileWallChainLink.ParentTileIndex = WorkingTileIndex;
			
			if (NewCoverChains.IsEmpty())
			{
				// no cover chains exist for the tile, create a new one.
				FCoverChainContainer CoverContainer;
				CoverContainer.Chain.Add(TileWallChainLink);
				NewCoverChains.Add(CoverContainer);
				continue;
			}
			
			// search existing cover for the tile and check if the wall should be a link in an existing cover chain.
			if (!AddLinkToExistingChain(TileWallChainLink, NewCoverChains))
			{
				// wall is a new separate cover for tile, as it did not fit in any other existing cover chain for the tile.
				FCoverChainContainer CoverContainer;
				CoverContainer.Chain.Add(TileWallChainLink);
				NewCoverChains.Add(CoverContainer);
			}
		}

		// chains need to be ordered before any other steps, the order of these links will not change from this point onwards.
		for (FCoverChainContainer& Chain : NewCoverChains)
		{
			Algo::Sort(Chain.Chain);
		}

		// sometimes cover links are added to new covers when they should be one cover chain.
		// so a merging step that checks if any covers start and ends should be connected and will merge the chain as a result.
		MergeCovers(NewCoverChains, ChainLinkAngleTolerance);

		// look over all links in each chain checking if they are too small or the angle between them are small enough.
		// this is done separate to the merge covers as 2 links at a time are evaluated.
		FilterLinks(NewCoverChains, SmallestChainLinkLength, ChainLinkCorrectionAngleTolerance, CorrectionIterationCount);
		
		/*
		 * boundary hash
		 * --------------
		 * the boundary hash is a map of locations to cover chain indexes of tile covers that extend beyond the parent tile and into its neighbour tiles.
		 * a linked list is not used here to allow each tile to be updated independently of any other neighbouring tiles.
		 * in doing so it prevents possible race conditions and complex updating and invalidation methods.
		 *
		 * when adding to the boundary hash, only the start and end of a cover chain is checked because the middle
		   of a cover chain can not extend beyond a tile before the last link in the chain.
		 * 
		 * example of boundary hash:
		 *   tile 1  tile 2
		 * |       |       |
		 * |   <---+<---   |
		 * |       |       |
		 * the '+' is the location for the boundary hash.
		   both tile 1 and tile 2 would have the same location in the boundary hash "linking" them.
		 */
		// generate boundary hash
		TMap<FVector, FBoundaryHashInfo> BoundaryHash;
		for (int32 ChainIndex = 0; ChainIndex < NewCoverChains.Num(); ++ChainIndex)
		{
			FCoverChainContainer& CoverChain = NewCoverChains[ChainIndex];
			
			{// start chain link
				const FChainLink& ChainLink = CoverChain.Start();
				const ETileEdge Edge = GetTargetNeighbour(ChainLink.Start, TileBounds[WorkingTileIndex]);
				if (Edge != ETileEdge::None)
				{
					const int64 NeighbourIndex = WorkingTileNeighbourIndexes[static_cast<int32>(Edge)];
					BoundaryHash.Add(ChainLink.Start, {ChainIndex, NeighbourIndex});
				}
			}
			
			{// end chain link
				const FChainLink& ChainLink = CoverChain.End();
				const ETileEdge Edge = GetTargetNeighbour(ChainLink.End, TileBounds[WorkingTileIndex]);
				if (Edge != ETileEdge::None)
				{
					const int64 NeighbourIndex = WorkingTileNeighbourIndexes[static_cast<int32>(Edge)];
					BoundaryHash.Add(ChainLink.End, {ChainIndex, NeighbourIndex});
				}
			}			
		}

		// set tile covers and boundary hash
		RecastNavMesh->TileCoverMutex.Lock();
		FTileCover* TileCover = TileCoverElems[Index];
		TileCover->OwnedCovers.Reset(TileCover->OwnedCovers.Num());
		TileCover->OwnedCovers = NewCoverChains;
		TileCover->BoundaryHash = BoundaryHash;
		RecastNavMesh->TileCoverMutex.Unlock();
	}
	
	bComplete = true;
	DumpAsyncData();
}

void FNarrativeTileCoverGenerator::DumpAsyncData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FNarrativeTileCoverGenerator::DumpAsyncData);
	
	TileRefs.Empty();
	TileIndexes.Empty();
	TileCoords.Empty();
	TileBounds.Empty();
	NeighbourTileIndexes.Empty();
	TileCoverElems.Empty();
	WallEdges.Empty();
	RecastNavMesh = nullptr;
}

bool FNarrativeTileCoverGenerator::AddLinkToExistingChain(const FChainLink& InChainLink, TArray<FCoverChainContainer>& InCover)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AddLinkToExistingChain);

	// check the in chain link with the in cover to see if it needs to be added to an existing cover
	for (FCoverChainContainer& Cover : InCover)
	{
		for (const FChainLink& ChainLink : Cover.Chain)
		{
			if (ANarrativeRecastNavMesh::DoesLinkFitInChain(InChainLink, ChainLink, ChainLinkAngleTolerance))
			{
				Cover.Chain.Add(InChainLink);
				return true;
			}
		}
	}
	
	return false;
}

void FNarrativeTileCoverGenerator::MergeCovers(TArray<FCoverChainContainer>& Covers, double ChainLinkAngleTolerance)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MergeCovers);
	
	// at least 2 elems must exist to check for merging
	if (Covers.Num() < 2)
	{
		return;
	}

	// loop over all elems and look for start and end chain links that should be connected to other covers
	for (int32 ToBeMergedIndex = 0; ToBeMergedIndex < Covers.Num(); ++ToBeMergedIndex)
	{
		FCoverChainContainer& ChainToBeMerged = Covers[ToBeMergedIndex];
			
		for (int32 MergeWithIndex = 0; MergeWithIndex < Covers.Num(); ++MergeWithIndex)
		{
			// only check chains that are not the same as the chain looking to be merged.
			// or if the cover to merge with has no covers which could be caused from past chains being merged.
			if (MergeWithIndex == ToBeMergedIndex || Covers[MergeWithIndex].Chain.IsEmpty())
			{					
				continue;
			}
				
			FCoverChainContainer& ChainToMergeWith = Covers[MergeWithIndex];
						
			// when the chain is connected from the start of the chain to merge with the chain to merge with, a simple append works.
			if (ANarrativeRecastNavMesh::DoesLinkFitInChain(ChainToBeMerged.Start(), ChainToMergeWith.End(), ChainLinkAngleTolerance))
			{
				TArray<FChainLink> NewChain;
				NewChain = ChainToMergeWith.Chain;
				NewChain.Append(ChainToBeMerged.Chain);
				ChainToMergeWith.Chain = NewChain;
					
				ChainToBeMerged.Chain.Empty();
				break;
			}

			// when the chain is connected from the end of the chain to merge with to the merge with chain, it needs to be inserted before all other elems.
			if (ANarrativeRecastNavMesh::DoesLinkFitInChain(ChainToBeMerged.End(), ChainToMergeWith.Start(), ChainLinkAngleTolerance))
			{
				TArray<FChainLink> NewChain;
				NewChain = ChainToBeMerged.Chain;
				NewChain.Append(ChainToMergeWith.Chain);
				ChainToMergeWith.Chain = NewChain;
					
				ChainToBeMerged.Chain.Empty();
				break;
			}
		}
	}

	// remove all chains that have been emptied and merged into another chain
	Covers.RemoveAll([](const FCoverChainContainer& CoverChain)
	{
		return CoverChain.Chain.IsEmpty();
	});
}

void FNarrativeTileCoverGenerator::FilterLinks(TArray<FCoverChainContainer>& Covers, const double MinLinkLength, const double MaxAngleForMerge, const uint8 IterationCount)
{
	for (int32 Iteration = 0; Iteration < IterationCount; ++Iteration)
	{
		for (FCoverChainContainer& Cover : Covers)
		{
			if (Cover.Chain.Num() < 2)
			{
				continue;
			}
		
			TArray<FChainLink> CoverChain;
			for (int32 ChainLinkIndex = 1; ChainLinkIndex < Cover.Chain.Num(); ++ChainLinkIndex)
			{
				const int32 FirstLinkIndex = ChainLinkIndex-1;
				const FChainLink& FirstChainLink = Cover.Chain[FirstLinkIndex];
				const FChainLink& SecondChainLink = Cover.Chain[ChainLinkIndex];

				// when any of the links are too short, we merge them as one.
				const bool bFirstLinkTooShort = FirstChainLink.Length() <= MinLinkLength;
				const bool bSecondLinkTooShort = SecondChainLink.Length() <= MinLinkLength;

				// if the angle is too small between the links, then merge them as one.
				const float DirDot = FVector::DotProduct(FirstChainLink.GetStartNormal(), SecondChainLink.GetStartNormal());
				const float AngleToleranceCos = FMath::Cos(FMath::DegreesToRadians(MaxAngleForMerge));
				const bool bAngleMerge = DirDot >= (AngleToleranceCos - UE_KINDA_SMALL_NUMBER);

				// BUG: seems to leave a lingering link...
				if (bFirstLinkTooShort || bSecondLinkTooShort || bAngleMerge)
				{
					FChainLink NewChain = FirstChainLink;
					NewChain.End = SecondChainLink.End;
					CoverChain.Add(NewChain);
					continue;
				}

				CoverChain.Add(FirstChainLink);
				if (ChainLinkIndex == Cover.Chain.Num()-1)
				{
					CoverChain.Add(SecondChainLink);
				}
			}
			Cover.Chain = CoverChain;
		}
	}
}

void FNarrativeTileCoverGenerator::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(RecastNavMesh);
}

FString FNarrativeTileCoverGenerator::GetReferencerName() const
{
	return TEXT("ANarrativeRecastNavMesh");
}
