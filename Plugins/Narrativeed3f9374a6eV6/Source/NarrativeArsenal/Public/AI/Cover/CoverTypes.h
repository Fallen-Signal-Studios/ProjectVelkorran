// Copyright Narrative Tools 2025.

#pragma once

#include "NavMesh/RecastNavMesh.h"
#include "Async/AsyncWork.h"
#include "CoverTypes.generated.h"

#define TILE_UINT64(TileRef) static_cast<uint64>(TileRef)

enum class ETileEdge : int32
{
	None        = INDEX_NONE,
	Bottom      = 0,
	BottomLeft  = 1,
	Left        = 2,
	TopLeft     = 3,
	Top         = 4,
	TopRight    = 5,
	Right       = 6,
	BottomRight = 7,
};

/**
 * A chain link consists of 3 bits of information:
 * a start, an end, and a normal facing towards the cover.
 *
 * this defines one link in a chain of cover (see FCoverContainer).
 * 
 */
USTRUCT(BlueprintType)
struct FChainLink
{
	GENERATED_BODY()

	// tile index that generates this edge
	UPROPERTY()
	uint64 ParentTileIndex;
	
	// the beginning of a chain link
	UPROPERTY(BlueprintReadWrite, Category="ChainLink")
	FVector Start;

	// the end of a chain link
	UPROPERTY(BlueprintReadWrite, Category="ChainLink")
	FVector End;

	// facing direction of where cover should be
	UPROPERTY()
	FVector CoverNormal;
	
	FChainLink()
	: ParentTileIndex(0),
	  Start(FVector::ZeroVector),
	  End(FVector::ZeroVector),
	  CoverNormal(FVector::ZeroVector)
	{}

	explicit FChainLink(const FNavigationWallEdge& InNavigationWallEdge, const FVector& InNormal = FVector::ZeroVector)
	: ParentTileIndex(0),
	  Start(InNavigationWallEdge.Start),
	  End(InNavigationWallEdge.End),
	  CoverNormal(InNormal)
	{
		const FVector Forward = (End - Start).GetSafeNormal();
		CoverNormal = FVector::CrossProduct(FVector::UpVector, Forward);
	}

	// explicitly for Algo::Sort()
	bool operator<(const FChainLink& Other) const
	{
		return End.Equals(Other.Start);
	}

	bool operator==(const FChainLink& Other) const
	{
		// NOTE: used to be an && but this saves some time making the assumption
		return End.Equals(Other.End, 0.1) || Start.Equals(Other.Start, 0.1);
	}

	// end to start: left <-
	FVector GetStartNormal() const { return (End - Start).GetSafeNormal(); }
	// start to end: right ->
	FVector GetEndNormal() const { return (Start - End).GetSafeNormal(); }

	double Length() const { return (End - Start).Length(); }
	
};

/**
 * 
 */
USTRUCT()
struct FCoverChainContainer
{
	GENERATED_BODY()
	
	// wall edges for the chain
	UPROPERTY()
	TArray<FChainLink> Chain;
	
	FCoverChainContainer()
	{
		Chain.Reserve(2);
	}

	FChainLink& Start() { return Chain[0]; }
	const FChainLink& Start() const { return Chain[0]; }
	FChainLink& End() { return Chain.Last(); }
	const FChainLink& End() const { return Chain.Last(); }
};

/**
 * 
 */
USTRUCT()
struct FChainLinkIndexContainer
{
	GENERATED_BODY()

	// tiles can have multiple parts of a cover chain. this points to each separate cover chain
	UPROPERTY()
	TArray<int8> ChainIndexes;

	bool AnyChains() const { return !ChainIndexes.IsEmpty(); }
	
	FChainLinkIndexContainer() {}
	FChainLinkIndexContainer(const int8 InChainIndex) :ChainIndexes({InChainIndex}) {}

	// an invalid, empty chain link index container
	static FChainLinkIndexContainer Invalid;
	
};

/**
 * 
 */
USTRUCT(BlueprintType)
struct FBoundaryHashInfo
{
	GENERATED_BODY()

	// this is the tile that connects this chain link.
	UPROPERTY()
	int64 NextTileIndex;
	
	// this is the cover chain index for the current tile, of the hash.
	UPROPERTY()
	int8 CoverChainIndex;

	FBoundaryHashInfo()
	: NextTileIndex(INDEX_NONE),
	  CoverChainIndex(INDEX_NONE)
	{}

	FBoundaryHashInfo(const int32 InCoverChainIndex, const int64 InNextTileIndex)
	: NextTileIndex(InNextTileIndex),
	  CoverChainIndex(InCoverChainIndex)	  
	{}

	bool IsValid() const { return NextTileIndex != INDEX_NONE && CoverChainIndex != INDEX_NONE; } 
};


/*
 * representation of cover for a given tile
 */
USTRUCT()
struct FTileCover
{
	GENERATED_BODY()

	static FTileCover Invalid;
	
	// covers decided in this tile
	UPROPERTY()
	TArray<FCoverChainContainer> OwnedCovers;

	// all covers that start or end in this tile on a tile boundary the start or end location are the keys
	UPROPERTY()
	TMap<FVector, FBoundaryHashInfo> BoundaryHash;

	FTileCover()
	{
		OwnedCovers.Reserve(4);
		BoundaryHash.Reserve(4);
	}

	bool AnyCovers() const
	{
		for (const FCoverChainContainer& Cover : OwnedCovers)
		{
			if (!Cover.Chain.IsEmpty())
			{
				return true;
			}
		}
		return false;
	}

	bool HashContains(const FVector& Point) const { return BoundaryHash.IsEmpty()? false : BoundaryHash.Contains(Point); }
	const FBoundaryHashInfo& GetCoverFromHash(const FVector& Point) const { return BoundaryHash[Point]; }
	int32 EndLinkIndex(int32 CoverChainIndex) const { return OwnedCovers[CoverChainIndex].Chain.Num()-1; }
};

class FCoverTileGeneratorWrapper;

struct FRunningCoverTask
{
	TArray<uint64> TileRefs;
	FAsyncTask<FCoverTileGeneratorWrapper>* AsyncTask;

	FRunningCoverTask()
	: AsyncTask(nullptr)
	{}

	bool operator==(const FRunningCoverTask& Other) const
	{
		for (const auto& OtherTileRef :Other.TileRefs)
		{
			if (TileRefs.Contains(OtherTileRef))
			{
				return true;
			}
		}
		return false;
	}
};

/**
 * container for a cover chain
 */
USTRUCT(BlueprintType)
struct FCoverContainer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="CoverContainer")
	int64 ParentTileIndex = INDEX_NONE;

	// cover chain index of the parent tile
	UPROPERTY(BlueprintReadOnly, Category="CoverContainer")
	int32 CoverChainIndex = INDEX_NONE;
	
	UPROPERTY(BlueprintReadOnly, Category="CoverContainer")
	TArray<FChainLink> CoverChain;
	
	FCoverContainer() = default;
	FCoverContainer(const TArray<FChainLink>* InChain, int64 InTileIndex, int8 InCoverIndex)
	: ParentTileIndex(InTileIndex), CoverChainIndex(InCoverIndex), CoverChain(*InChain)
	{}

	bool IsValid() const { return ParentTileIndex != INDEX_NONE && CoverChainIndex != INDEX_NONE && !CoverChain.IsEmpty(); }

	const FChainLink& Start() const { return CoverChain[0]; }
	const FChainLink& End()   const { return CoverChain.Last(); }
	
	void Reset()
	{
		ParentTileIndex = INDEX_NONE;
		CoverChainIndex = INDEX_NONE;
		CoverChain.Empty();
	}
	
};

/**
 * collection of data for tracing for valid cover positions 
 */
USTRUCT(BlueprintType)
struct FCoverTraceConfig
{
	GENERATED_BODY()

	// trace order: low -> high -> left -> right
	FVector TraceStart[4];
	
	// the distance between points along a cover chain.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CoverTraceConfig")
	float CoverSpacing;

	// how high up a cover must be to be considered low cover.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CoverTraceConfig")
	float HalfHeight;

	// TODO: come up with a good way to explain this. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CoverTraceConfig")
	float PeekOverHeight;

	// how high to check for side peeking.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CoverTraceConfig")
	float PeekSideHeight;

	// how far out from the point to check for side peeking.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CoverTraceConfig")
	float PeekSideWidth;

	/* TODO: maybe have this linked to the edge distance or something? */
	// how deep towards the cover to trace.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CoverTraceConfig")
	float TraceDepth;
	
	FCoverTraceConfig()
	: CoverSpacing   (150.0f),
	  HalfHeight     (85.0f ),
	  PeekOverHeight (150.0f),
	  PeekSideHeight (115.0f),
	  PeekSideWidth  (65.0f ),
	  TraceDepth     (100.0f)
	{
		TraceStart[0] = {0.0f, 0.0f, HalfHeight}; // low cover pass
		TraceStart[1] = {0.0f, 0.0f, PeekOverHeight}; // high cover pass
		TraceStart[2] = {0.0f, -PeekSideWidth, PeekSideHeight}; // lean pass
		TraceStart[3] = {0.0f, PeekSideWidth, PeekSideHeight}; // lean pass
	}

	FVector Low()       const { return TraceStart[0]; }
	FVector High()      const { return TraceStart[1]; }
	FVector LeanLeft()  const { return TraceStart[2]; }
	FVector LeanRight() const { return TraceStart[3]; }
};

