// Copyright Narrative Tools 2025.

#include "AI/Navigation/NarrativeNavigationSystem.h"
#include "ArsenalSettings.h"
#include "AI/Navigation/NarrativeRecastNavMesh.h"
#include "Detour/DetourNavMesh.h"
#include "Kismet/KismetSystemLibrary.h"

void UNarrativeNavigationSystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ANarrativeRecastNavMesh* NavData = Cast<ANarrativeRecastNavMesh>(GetDefaultNavDataInstance(FNavigationSystem::DontCreate)))
	{
		NavData->TickAsyncCoverBuild(DeltaSeconds);
	}	
	
}

UNarrativeNavigationSystem::UNarrativeNavigationSystem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

static void AddToChainLinkArrOrdered(TArray<FChainLink>& TargetCover, const FCoverContainer& CoverToAdd)
{
	if (TargetCover.IsEmpty())
	{
		TargetCover = CoverToAdd.CoverChain;
		return;
	}

	// cover chains are handed as whole pre-ordered chains.
	// this allows for a simple order check
	if (TargetCover.Last() < CoverToAdd.Start())
	{
		TargetCover.Append(CoverToAdd.CoverChain);
	}
	else
	{
		TargetCover.Insert(CoverToAdd.CoverChain, 0);
	}
}

static void WalkCoverInDirection(const UNarrativeNavigationSystem* NavSys, TMap<int64, TArray<int32>>& VisitedTilesAndCovers, FCoverContainer InCover, TArray<FChainLink>& OutFoundCover, bool bWalkForward)
{
	while (true) // TODO: add limit to this
	{
		FCoverContainer OutCover;
		NavSys->FindNextSectionOfCover(InCover, !bWalkForward, OutCover);
		if (!OutCover.IsValid())
		{
			break;
		}

		if (VisitedTilesAndCovers.Contains(OutCover.ParentTileIndex))
		{
			if (VisitedTilesAndCovers[OutCover.ParentTileIndex].Contains(OutCover.CoverChainIndex))
			{
				break;
			}
		
			VisitedTilesAndCovers[OutCover.ParentTileIndex].AddUnique(OutCover.CoverChainIndex);
		}
		else
		{
			VisitedTilesAndCovers.FindOrAdd(OutCover.ParentTileIndex).AddUnique(OutCover.CoverChainIndex);
		}

		// takes a cover and inserts or appends it to the start or end of an array of chain links
		AddToChainLinkArrOrdered(OutFoundCover, OutCover);
	
		InCover = OutCover;
	}	
};

bool UNarrativeNavigationSystem::FindAllCoverInRadiusToPoint(const UObject* WorldContext, const FVector& Point, TArray<FCoverContainer>& FoundCover, float SearchRadius)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UNarrativeNavigationSystem::FindAllCoverInRadiusToPoint);
	
	const UWorld* World = WorldContext? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	
	const UNarrativeNavigationSystem* NavigationSystem = FNavigationSystem::GetCurrent<UNarrativeNavigationSystem>(World);
	const ANarrativeRecastNavMesh* NavMesh = NavigationSystem? Cast<ANarrativeRecastNavMesh>(NavigationSystem->GetNavDataForProps(FNavAgentProperties(), Point)) : nullptr;

	// collect current nav poly
	const NavNodeRef PolyNodeRef = NavMesh? NavMesh->FindNearestPoly(Point, FVector{100}) : INVALID_NAVNODEREF;
	if (PolyNodeRef == INVALID_NAVNODEREF)
	{
		// no poly found
		return false;
	}

	// collect poly nav tile
	// note: this is not validated...
	uint32 PolyIndex, InitialTileIndex;
	if (!NavMesh->GetPolyTileIndex(PolyNodeRef, PolyIndex, InitialTileIndex))
	{
		// no tile found from poly
		return false;
	}

	// validate tile
	const dtNavMesh* Recast = NavMesh->GetRecastMesh();
	const dtMeshTile* Tile = Recast? Recast->getTile(InitialTileIndex) : nullptr;
	if (!Tile || !Tile->header)
	{
		// tile is not valid
		return false;
	}
	
	// grab tile neighbours 
	TArray<int64> TileIndexes = { InitialTileIndex };
	for (int i = 0; i < 8; ++i)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GetNeighbours);
		
		int32 X, Y;
		Recast->getNeighbourCoords(Tile->header->x, Tile->header->y, i, X, Y);
		
		const dtMeshTile* NTile = Recast->getTileAt(X, Y, Tile->header->layer);
		if (!NTile /*|| NTile->header*/)
		{
			// invalid neighbour tile
			continue;
		}
		
		TileIndexes.Add(Recast->getTileIndex(NTile));
	}
	
	// check tiles for cover and add to FoundCover
	NavMesh->TileCoverMutex.Lock();
	const TArray<FTileCover>& TileCovers = NavMesh->TilesCovers;
	for (const int64 TileIndex : TileIndexes)
	{
		if (!TileCovers.IsValidIndex(TileIndex))
		{
			continue;
		}
		
		const FTileCover& TileCover = TileCovers[TileIndex];
		for (int8 CoverIndex = 0; CoverIndex < TileCover.OwnedCovers.Num(); ++CoverIndex)
		{
			if (!TileCover.AnyCovers())
			{
				// no covers, skip tile
				continue;
			}
			
			FoundCover.Add(FCoverContainer{&TileCover.OwnedCovers[CoverIndex].Chain, TileIndex, CoverIndex});
		}
	}
	NavMesh->TileCoverMutex.Unlock();
	return true;
}

bool UNarrativeNavigationSystem::FindNearestCoverToPoint(const UObject* WorldContext, const FVector& Point, TArray<FChainLink>& FoundCover, int32& ChainLinkIndex, FVector& NearestPoint, float SearchRadius, FVector DirectionBias, float DirectionBiasToleranceAngle)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UNarrativeNavigationSystem::FindNearestCoverToPoint);

	const UNarrativeNavigationSystem* NavigationSystem = FNavigationSystem::GetCurrent<UNarrativeNavigationSystem>(WorldContext->GetWorld());
	if (!NavigationSystem)
	{
		return false;
	}

	ChainLinkIndex = INDEX_NONE;
	FoundCover.Empty(4);
	
	/* TODO: have search radius actually do something */
	// collect cover in the search radius
	TArray<FCoverContainer> FoundCoverInRadius;
	if (!FindAllCoverInRadiusToPoint(WorldContext, Point, FoundCoverInRadius, SearchRadius))
	{
		// no cover in radius was found
		return false;
	}
	
	// search for nearest cover
	double LastCoverDist = TNumericLimits<double>::Max();
	int32 FirstFoundCoverIdx = INDEX_NONE;
	for (int32 CoverIndex = 0; CoverIndex < FoundCoverInRadius.Num(); ++CoverIndex)
	{
		const FCoverContainer& Cover = FoundCoverInRadius[CoverIndex];

		// find the nearest chain link and check that the distance to it is within the search radius and biases
		int32 NearestChainLinkIndex;
		FVector NearPointOnChain;
		if (FindNearestCoverLink(Point, Cover.CoverChain, NearestChainLinkIndex, NearPointOnChain))
		{
			const float Dist = FVector::Distance(NearPointOnChain, Point);
			const float Dot = FVector::DotProduct(DirectionBias.GetSafeNormal2D(), (NearPointOnChain - Point).GetSafeNormal2D());
			const bool bIsInDirectionBias = DirectionBias == FVector::UpVector? true : Dot > FMath::Cos(FMath::DegreesToRadians(DirectionBiasToleranceAngle));
			if (Dist <= SearchRadius && bIsInDirectionBias && Dist < LastCoverDist)
			{
				FirstFoundCoverIdx = CoverIndex;
				LastCoverDist = Dist;
				NearestPoint = NearPointOnChain;
			}
		}
	}

	if (FirstFoundCoverIdx == INDEX_NONE)
	{
		return false;
	}

	const FCoverContainer& Cover = FoundCoverInRadius[FirstFoundCoverIdx];
	FoundCover = Cover.CoverChain;
	
	// track visited tiles and covers and add our first found cover one 
	TMap<int64, TArray<int32>> VisitedTilesAndCovers;
	VisitedTilesAndCovers.Add(Cover.ParentTileIndex).Add(Cover.CoverChainIndex);
	// walk along connected cover to get all new cover chains as one
	WalkCoverInDirection(NavigationSystem, VisitedTilesAndCovers, FoundCoverInRadius[FirstFoundCoverIdx], FoundCover, true);
	WalkCoverInDirection(NavigationSystem, VisitedTilesAndCovers, FoundCoverInRadius[FirstFoundCoverIdx], FoundCover, false);
	
	// evaluate all links in the chain. this validates ChainLinkIndex as without this it would be wrong.
	UNarrativeNavigationSystem::FindNearestCoverLink(Point, FoundCover, ChainLinkIndex, NearestPoint);
	return !FoundCover.IsEmpty();
}

bool UNarrativeNavigationSystem::FindNearestCoverLink(const FVector& Point, const TArray<FChainLink>& ChainLinks, int32& ChainLinkIndex, FVector& NearestPoint)
{
	ChainLinkIndex = INDEX_NONE;
	NearestPoint = FVector::ZeroVector;
	if (!ChainLinks.IsEmpty())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FindNearestLink);
		float LastDistanceToLink = TNumericLimits<float>::Max();
		for (int32 LinkIndex = 0; LinkIndex < ChainLinks.Num(); ++LinkIndex)
		{
			const FChainLink& ChainLink = ChainLinks[LinkIndex];
			const FVector ClosePointOnLink = FMath::ClosestPointOnSegment(Point, ChainLink.End, ChainLink.Start);
			const float DistToCover = FVector::Distance(Point, ClosePointOnLink);
			if (DistToCover <= LastDistanceToLink)
			{
				LastDistanceToLink = DistToCover;
				ChainLinkIndex = LinkIndex;
				NearestPoint = ClosePointOnLink;
			}
		}
	}
	
	return ChainLinkIndex != INDEX_NONE; 
}

bool UNarrativeNavigationSystem::FindNextSectionOfCover(const FCoverContainer& Cover, bool bSearchAtStartOfCover, FCoverContainer& OutCover) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UNarrativeNavigationSystem::FindNextSectionOfCover);
	
	OutCover.Reset();
	if (!Cover.IsValid())
	{
		return false;
	}

	const FChainLink Link = bSearchAtStartOfCover? Cover.CoverChain[0] : Cover.CoverChain.Last();
	const FVector LinkHash = bSearchAtStartOfCover? Link.Start : Link.End;
	
	const ANarrativeRecastNavMesh* NavMesh = Cast<ANarrativeRecastNavMesh>(GetNavDataForProps(FNavAgentProperties(), LinkHash));
	if (!NavMesh)
	{
		return false;
	}
	
	NavMesh->TileCoverMutex.Lock();
	ON_SCOPE_EXIT{
		// not checking the Navmesh ptr validity here because if it is invalid then
		// something else is wrong and needs to be inspected!
		NavMesh->TileCoverMutex.Unlock();
	};

	// ensure that the current cover does indeed span outside of this current tile
	const FTileCover& TileCover = NavMesh->TilesCovers[Cover.ParentTileIndex];
	if (!TileCover.HashContains(LinkHash))
	{
		// the current cover ends within the current tile in the given direction
		return false;
	}

	// index to look at for where to go next
	const int64 NextTileIndex = TileCover.GetCoverFromHash(LinkHash).NextTileIndex;
	if (NextTileIndex == INDEX_NONE)
	{
		return false;
	}

	// check that there is any possible cover in the tile
	const FTileCover& NTileCover = NavMesh->TilesCovers[NextTileIndex];
	if (!NTileCover.HashContains(LinkHash) || !NTileCover.AnyCovers())
	{
		return false;
	}

	// check if the cover fits with the chain
	const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>();
	const FBoundaryHashInfo& CoverHashInfo = NTileCover.GetCoverFromHash(LinkHash);
	if (const FCoverChainContainer* NCoverChain = &NTileCover.OwnedCovers[CoverHashInfo.CoverChainIndex])
	{
		FChainLink LinkCandidate = bSearchAtStartOfCover? NCoverChain->End() : NCoverChain->Start();
		if (ANarrativeRecastNavMesh::DoesLinkFitInChain(LinkCandidate, Link, ArsenalSettings->ChainLinkAngleTolerance))
		{
			OutCover = FCoverContainer{&NCoverChain->Chain, NextTileIndex, CoverHashInfo.CoverChainIndex};
			return true;
		}
	}
	
	return false;
}

bool UNarrativeNavigationSystem::TestPointForCoverType(const UObject* WorldContext, const FVector& Point, const FVector& Direction, bool& bIsLowCover, bool& bCanPeekLeft, bool& bCanPeekRight)
{
	const FCoverTraceConfig CoverTraceConfig = GetCoverTraceConfig();
	const auto IsCoverTraceBlocked = [&WorldContext, &CoverTraceConfig](const FVector& InPoint, const FVector& InDirection, const FVector& InTracePosition) -> bool
	{
		const FVector DirectionNormalised = InDirection.GetSafeNormal2D();
		const FVector& TraceStartPoint = DirectionNormalised.Rotation().RotateVector(InTracePosition);
		const FVector TraceStart = InPoint + TraceStartPoint;
		const FVector TraceEnd = TraceStart + (DirectionNormalised * CoverTraceConfig.TraceDepth);
		TArray<AActor*> ActorsToIgnore{};
		FHitResult Hit;
		return UKismetSystemLibrary::LineTraceSingle(
			WorldContext,
			TraceStart,
			TraceEnd,
			UEngineTypes::ConvertToTraceType(ECC_Visibility),
			false,
			ActorsToIgnore,
			EDrawDebugTrace::ForDuration,
			Hit,
			true
		);
	};

	const bool bLowHit = IsCoverTraceBlocked(Point, Direction, CoverTraceConfig.Low());
	bIsLowCover = bLowHit && !IsCoverTraceBlocked(Point, Direction, CoverTraceConfig.High());
	
	bCanPeekLeft = !bIsLowCover && !IsCoverTraceBlocked(Point, Direction, CoverTraceConfig.LeanLeft());
	bCanPeekRight = !bIsLowCover && !IsCoverTraceBlocked(Point, Direction, CoverTraceConfig.LeanRight());

	return bIsLowCover || bCanPeekLeft || bCanPeekRight;
}

FCoverTraceConfig UNarrativeNavigationSystem::GetCoverTraceConfig()
{
	const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>();
	return ArsenalSettings->CoverTraceConfig;
}

void UNarrativeNavigationSystem::RebuildAllCover()
{
#if WITH_EDITOR
	UWorld* EditorWorld = GEngine? GEditor->GetEditorWorldContext().World() : nullptr;
	UNarrativeNavigationSystem* NarrativeNavigationSystem = FNavigationSystem::GetCurrent<UNarrativeNavigationSystem>(EditorWorld);
	ARecastNavMesh* NavMesh = NarrativeNavigationSystem? Cast<ARecastNavMesh>(NarrativeNavigationSystem->GetNavDataForProps(FNavAgentProperties())) : nullptr;
	if (NavMesh)
	{
		// rebuild is triggered for next tick as some params may have been set before RebuildAllCover() is called but are not fully propagated.
		// the primary case when this happens is with PostEditPropertyChange().
		EditorWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(NavMesh, &ARecastNavMesh::RebuildAll));
	}
#endif
}
