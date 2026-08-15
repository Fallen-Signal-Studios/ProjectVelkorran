// Copyright Narrative Tools 2025.

#pragma once
#include "NarrativeTileCoverGenerator.h"

DECLARE_CYCLE_STAT(TEXT("FNarrativeTileCoverGenerator"), STAT_FNarrativeTileCoverGenerator, STATGROUP_TaskGraphTasks);
DECLARE_CYCLE_STAT(TEXT("FNarrativeTileCoverGenerator::LoopWork"), STAT_FNarrativeTileCoverGeneratorLoopWork, STATGROUP_TaskGraphTasks);

DEFINE_LOG_CATEGORY_STATIC(CoverGenerateTask, Log, All);

class FNarrativeTileCoverGenerator;

class FCoverTileGeneratorWrapper : public FNonAbandonableTask
{
	friend class ANarrativeRecastNavMesh;

public:

	TSharedPtr<FNarrativeTileCoverGenerator> GeneratorWorker;
	
	FCoverTileGeneratorWrapper(TSharedRef<FNarrativeTileCoverGenerator> InGeneratorWorker)
		: GeneratorWorker(InGeneratorWorker)
	{}
	
	void DoWork()
	{
		GeneratorWorker->DoWork();
		UE_LOG(CoverGenerateTask, Log, TEXT("Completed Cover Task"));
	}

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FCoverTileGenerator, STATGROUP_ThreadPoolAsyncTasks); }

	
};
