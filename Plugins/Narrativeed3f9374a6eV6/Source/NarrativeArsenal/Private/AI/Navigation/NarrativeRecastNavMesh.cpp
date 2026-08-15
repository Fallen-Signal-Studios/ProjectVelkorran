// Copyright Narrative Tools 2025.

#include "AI/Navigation/NarrativeRecastNavMesh.h"

#include "ArsenalSettings.h"
#include "AI/NavigationSystemBase.h"
#include "AI/Cover/CoverTileGeneratorWrapper.h"
#include "AI/Navigation/NarrativeNavigationSystem.h"
#include "Detour/DetourNavMesh.h"
#include "NavMesh/RecastNavMeshGenerator.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "VisualLogger/VisualLogger.h"

DEFINE_LOG_CATEGORY_STATIC(LogCoverGeneration, All, All)

FChainLinkIndexContainer FChainLinkIndexContainer::Invalid = {};
FTileCover FTileCover::Invalid = {};

bool ANarrativeRecastNavMesh::DoesLinkFitInChain(const FChainLink& LinkCandidate, const FChainLink& ChainLink, const float AngleTolerance)
{
	bool bResult = false;
	const FVector CandidateNormal = ANarrativeRecastNavMesh::GetLinkDirectionNormal(LinkCandidate);
	if (CandidateNormal != FVector::ZeroVector)
	{
		const FVector LinkNormal = ANarrativeRecastNavMesh::GetLinkDirectionNormal(ChainLink);
		const bool bStartPass = LinkCandidate.Start.Equals(ChainLink.End, 0.1);
		const bool bEndPass = ChainLink.Start.Equals(LinkCandidate.End, 0.1);

		/*
		 * links are defined by edges that share one vertex.
		 * if 2 vertices match then they are the same edge and is regarded as a conflicting link.
		 */
		if (bStartPass != bEndPass && (bStartPass || bEndPass))
		{
			const float DirDot = FVector::DotProduct(CandidateNormal, LinkNormal);
			const float AngleToleranceCos = FMath::Cos(FMath::DegreesToRadians(AngleTolerance));
			// remove a small amount to compensate for floating point errors
			bResult = DirDot >= (AngleToleranceCos - UE_KINDA_SMALL_NUMBER);
		}
	}
	
	return bResult;
}

void ANarrativeRecastNavMesh::QueueTilesForUpdate(const TArray<FNavTileRef>& ChangedTiles)
{
	for (const FNavTileRef& TileRef : ChangedTiles)
	{
		if (DirtyTiles.Contains(TILE_UINT64(TileRef)))
		{
			continue;
		}

		DirtyTiles.Add(TILE_UINT64(TileRef));
	}
}

void ANarrativeRecastNavMesh::RebuildCoverV2()
{
	// skip building if the wrong nav system is in use
	if (!FNavigationSystem::GetCurrent<UNarrativeNavigationSystem>(GetWorld()))
	{
		return;
	}
	
	// don't generate cover until nav mesh is done
	if (FRecastNavMeshGenerator* CurrentGenerator = static_cast<FRecastNavMeshGenerator*>(GetGenerator()))
	{
		if (CurrentGenerator->GetNumRemaningBuildTasks() > 0)
		{
			return;
		}
	}
	
	const dtNavMesh* DetourNavMesh = GetRecastMesh();
	if (!DetourNavMesh)
	{
		return;
	}
	
	// map directly to tiles
	if (TilesCovers.Num() != DetourNavMesh->m_maxTiles)
	{
		TilesCovers.Reset(DetourNavMesh->m_maxTiles);
		TilesCovers.Init({}, DetourNavMesh->m_maxTiles);
	}

	// loop over any existing tasks and try cancel them
	for (int32 RunningTaskIndex = 0; RunningTaskIndex < RunningCoverTasks.Num(); ++RunningTaskIndex)
	{
		const FRunningCoverTask& RunningCoverTask = RunningCoverTasks[RunningTaskIndex];
		if (!RunningCoverTask.AsyncTask->Cancel())
		{
			UE_LOG(LogCoverGeneration, Warning, TEXT("Running cover generation task canceled but still running..."))
		}
		else
		{
			// add any dirty tiles that where being worked on and remove the running task.
			if (!RunningCoverTask.TileRefs.IsEmpty())
			{
				DirtyTiles.Append(RunningCoverTask.TileRefs);
			}
			RunningCoverTasks.RemoveAtSwap(RunningTaskIndex, EAllowShrinking::No);
		}
	}

	// group tiles so that each worker task does not bloat the task graph
	TArray<TArray<uint64>> GroupedRefs = {{}};
	TArray<TArray<uint32>> GroupedIndexes = {{}};
	int32 GroupStage = 0;
	for (int32 DirtyTileIndex = 0; DirtyTileIndex < DirtyTiles.Num(); ++DirtyTileIndex)
	{
		const uint64 TileRef = DirtyTiles[DirtyTileIndex];
		//const dtMeshTile* MeshTile = DetourNavMesh->getTileByRef(TileRef);

		if (!(dtPolyRef)TileRef)
		{
			continue;
		}

		// the tile ref is outdated and only the tile index is valid to do work from.
		uint64 tileIndex = DetourNavMesh->decodePolyIdTile(TileRef);
		if (tileIndex >= DetourNavMesh->m_maxTiles)
		{
			continue;
		}
		
		const dtMeshTile* MeshTile = DetourNavMesh->getTile(tileIndex);
		if (!MeshTile || !MeshTile->header)
		{
			DirtyTiles.RemoveAtSwap(DirtyTileIndex, EAllowShrinking::No);
			continue;
		}
		const uint32 TileIndex = DetourNavMesh->getTileIndex(MeshTile);
		
		TArray<uint64>& TileRefWorkGroup = GroupedRefs[GroupStage];
		TArray<uint32>& TileIndexWorkGroup = GroupedIndexes[GroupStage];
		if (TileRefWorkGroup.Num() == 8)
		{
			GroupStage = GroupedRefs.Add({TileRef});
			GroupedIndexes.Add({TileIndex});
		}
		else
		{
			TileRefWorkGroup.Add(TileRef);
			TileIndexWorkGroup.Add(TileIndex);
		}
	}

	// create worker tasks for groups
	const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>();
	for (int32 WorkGroup = 0; WorkGroup < GroupedRefs.Num(); ++WorkGroup)
	{
		const TArray<uint64>& TileRefWorkGroup = GroupedRefs[WorkGroup];
		const TArray<uint32>& TileIndexWorkGroup = GroupedIndexes[WorkGroup];
		FRunningCoverTask PendingRunCoverTask;
		PendingRunCoverTask.TileRefs.Append(TileRefWorkGroup);

		// do not start duplicate tasks or tasks that share tiles from another worker group
		if (RunningCoverTasks.Contains(PendingRunCoverTask))
		{
			continue;
		}

		TSharedRef<FNarrativeTileCoverGenerator> TileCoverGenerator = MakeShareable(new FNarrativeTileCoverGenerator(
			this,
			TileRefWorkGroup,
			TileIndexWorkGroup,
			ArsenalSettings->ChainLinkAngleTolerance,
			ArsenalSettings->SmallestChainLinkLength,
			ArsenalSettings->ChainLinkCorrectionAngleTolerance,
			ArsenalSettings->CorrectionIterationCount,
			FPlatformTime::Seconds()));
		TileCoverGenerator->Setup();
		TUniquePtr<FAsyncTask<FCoverTileGeneratorWrapper>> TileTask = MakeUnique<FAsyncTask<FCoverTileGeneratorWrapper>>(TileCoverGenerator);
		TileTask->StartBackgroundTask(GThreadPool, EQueuedWorkPriority::Normal, EQueuedWorkFlags::None, -1, TEXT("Narrative_GenerateCoverForTileGroup"));			
		PendingRunCoverTask.AsyncTask = TileTask.Release();
		PendingRunCoverTask.TileRefs = TileRefWorkGroup;
		RunningCoverTasks.Add(PendingRunCoverTask);
		NumRunningTasks = RunningCoverTasks.Num();
	}
}

void ANarrativeRecastNavMesh::OnNavMeshTilesUpdated(const TArray<FNavTileRef>& ChangedTiles)
{	
	QueueTilesForUpdate(ChangedTiles);
}

void ANarrativeRecastNavMesh::OnNavMeshGenerationFinished()
{
	Super::OnNavMeshGenerationFinished();
	
	// even though the nav mesh is finished generating, not all information is in the right places. so cover generation can only begin next frame. 
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ANarrativeRecastNavMesh::RebuildCoverV2));
}

void ANarrativeRecastNavMesh::RebuildAll()
{	
	Super::RebuildAll();
}

void ANarrativeRecastNavMesh::TickAsyncCoverBuild(float DeltaSeconds)
{
	int64 RunningTasks = RunningCoverTasks.Num();
	for (int32 RunningTaskIndex = RunningCoverTasks.Num() - 1; RunningTaskIndex >=0; --RunningTaskIndex)
	{
		if (RunningCoverTasks[RunningTaskIndex].AsyncTask->IsDone())
		{
			for (const uint64& TileRef : RunningCoverTasks[RunningTaskIndex].TileRefs)
			{
				DirtyTiles.Remove(TileRef);
			}
			RunningCoverTasks.RemoveAtSwap(RunningTaskIndex, EAllowShrinking::No);
			RunningTasks--;
		}
	}
	
	if (NumRunningTasks > RunningTasks)
	{
		NumRunningTasks = RunningTasks;
		if (RunningTasks == 0)
		{
			OnCoverGenerationFinished();
		}
	}
}

void ANarrativeRecastNavMesh::OnCoverGenerationFinished()
{
	DebugDrawTemp();
}

void ANarrativeRecastNavMesh::DebugDrawTemp()
{
	TileCoverMutex.Lock();
	
	for (const FTileCover& TileCover : TilesCovers)
	{
		int32 TileIndex = INDEX_NONE;
		int64 CoverNum = 0;
		for (const FCoverChainContainer& Covers : TileCover.OwnedCovers)
		{
			const FColor Color = FColor::MakeRandomColor();
			int32 LinkNum = 0;
			for (const FChainLink& Link : Covers.Chain)
			{
				TileIndex = Link.ParentTileIndex;

#if ENABLE_VISUAL_LOG
				UE_VLOG_ARROW(this, LogCoverGeneration, Log, Link.Start, Link.End, Color, TEXT("Tile: %s | Cover: %s | Link: %s"), *FString::FromInt(TileIndex), *FString::FromInt(CoverNum), *FString::FromInt(LinkNum));
				
				UE_VLOG_ARROW(this, LogCoverGeneration, Log, Link.Start, Link.Start + (Link.CoverNormal * 50.f), Color, TEXT("norm"));
#endif 
				
				LinkNum++;
			}
			
			CoverNum++;
		}

		int32 HashDisplayOffsetMultiplier = 1;
		for (const auto&[HashLocation, HashInfo] : TileCover.BoundaryHash)
		{
#if ENABLE_VISUAL_LOG
			UE_VLOG_LOCATION(this, LogCoverGeneration, Log, HashLocation, 5, FColor::MakeRandomColor(), TEXT("Tile: %s"), *FString::FromInt(TileIndex));
#endif 

			const FColor Color = FColor::MakeRandomColor();
			if (!TileCover.OwnedCovers.IsValidIndex(HashInfo.CoverChainIndex))
			{
				continue;
			}
			
			const FCoverChainContainer& Cover = TileCover.OwnedCovers[HashInfo.CoverChainIndex];
			for (const FChainLink& Link : Cover.Chain)
			{
#if ENABLE_VISUAL_LOG
				UE_VLOG_ARROW(this, LogActor, Log, Link.Start, Link.End, Color, TEXT(""));
				UE_VLOG_LOCATION(this, LogActor, Log, HashLocation + FVector(0, 0, 10 * HashDisplayOffsetMultiplier), 5, Color, TEXT("Tile: %s | Chain: %s"), *FString::FromInt(TileIndex), *FString::FromInt(HashInfo.CoverChainIndex));
#endif 
			}
			HashDisplayOffsetMultiplier++;
		}
	}
	
	TileCoverMutex.Unlock();
}

void ANarrativeRecastNavMesh::DebugRebuild()
{
	//RebuildAll();
	RebuildCoverV2();
}
