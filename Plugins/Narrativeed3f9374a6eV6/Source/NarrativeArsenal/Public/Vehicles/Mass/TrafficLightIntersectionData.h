// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "ZoneGraphAnnotationTypes.h"
#include "ZoneGraphTypes.h"
#include "Containers/StaticArray.h"
#include "TrafficLightIntersectionData.generated.h"

class UTrafficLightSubsystem;

UENUM()
enum class ELaneState : uint8
{
	Open,
	Closed
};

// Defines what lanes are available to be used within an intersection side
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EIntersectionSideRule : uint8
{
	AllClosed = 0,
	RightOpen = 1 << 1,
	StraightOpen = 1 << 2,
	LeftOpen = 1 << 3,
	AllDirectionsOpen = RightOpen | StraightOpen | LeftOpen
};
ENUM_CLASS_FLAGS(EIntersectionSideRule)

USTRUCT()
struct FTrafficIntersectionSide
{
	GENERATED_BODY()

	FTrafficIntersectionSide() = default;

	// These lanes represent the lanes within the intersection that were coming from one side (incoming).
	TArray<FZoneGraphLaneHandle> Lanes;

	// Bitmask (EIntersectionSideRule) to set custom values for this intersection side. For example, during a cutscene
	uint8 SideOverride = (uint8)EIntersectionSideRule::AllClosed;

	FVector DirectionIntoIntersection = FVector::ZeroVector;
	FVector SideLocation = FVector::ZeroVector;
};

USTRUCT()
struct FTrafficIntersectionSideHandle
{
	GENERATED_BODY()

	FTrafficIntersectionSideHandle() = default;

	FTrafficIntersectionSideHandle(const FZoneGraphDataHandle& InZoneGraphDataHandle, int InIntersectionIndex, const int InSideIndex) :
	ZoneGraphDataHandle(InZoneGraphDataHandle), IntersectionIndex(InIntersectionIndex), SideIndex(InSideIndex) {}

	FZoneGraphDataHandle ZoneGraphDataHandle = FZoneGraphDataHandle();
	int IntersectionIndex = INDEX_NONE;
	int SideIndex = INDEX_NONE;

	TConstArrayView<FTrafficIntersectionSide> GetIntersectionSides(
		UTrafficLightSubsystem* TrafficLightSubsystem) const;
	TArrayView<FTrafficIntersectionSide> GetMutableIntersectionSides(
		UTrafficLightSubsystem* TrafficLightSubsystem);

	const FTrafficIntersectionSide& GetIntersectionSide(UTrafficLightSubsystem* TrafficLightSubsystem) const;
	FTrafficIntersectionSide& GetMutableIntersectionSide(UTrafficLightSubsystem* TrafficLightSubsystem);

	struct FTrafficLightIntersection& GetIntersection(UTrafficLightSubsystem* TrafficLightSubsystem) const;

	bool IsValid() const;
};

USTRUCT()
struct FTrafficPeriod
{
	GENERATED_BODY()

	FTrafficPeriod() = default;
	
	FTrafficPeriod(const TArray<FZoneGraphLaneHandle>& InLanes, float InDuration, EIntersectionSideRule InLanesCovered) :
		Lanes(InLanes), LanesCoveredMask(InLanesCovered), Duration(InDuration) {}

	// Lanes that are managed during this period
	TArray<FZoneGraphLaneHandle> Lanes;

	// Specifies the type of lane that is affected by this period.
	EIntersectionSideRule LanesCoveredMask;

	float Duration = -1;
};

USTRUCT()
struct FTrafficPeriodEvent : public FZoneGraphAnnotationEventBase
{
	GENERATED_BODY()

	FTrafficPeriodEvent() = default;

	FTrafficPeriod Period = FTrafficPeriod();
	ELaneState State = ELaneState::Open; 
};

USTRUCT()
struct FTrafficLightIntersection
{
	GENERATED_BODY()

	FTrafficLightIntersection() = default;

	FTrafficLightIntersection(int32 IntersectionZoneIndex)
	{
		ZoneIndex = IntersectionZoneIndex;
	}

	// Used as an identifier so we dont add multiple intersections
	int32 ZoneIndex = INDEX_NONE;

	// Max 4 sides
	TArray<FTrafficIntersectionSide, TInlineAllocator<4>> IntersectionSides;

	TArray<FTrafficPeriod> TrafficPeriods;
	int CurrentPeriodIndex = INDEX_NONE;
	float RemainingPeriodDuration = -1;
	bool bOverrideIntersection = false;

	TArray<FZoneGraphLaneHandle> GetLanes() const
	{
		TArray<FZoneGraphLaneHandle> Lanes;
		for (const FTrafficIntersectionSide& IntersectionSide : IntersectionSides)
		{
			Lanes.Append(IntersectionSide.Lanes);
		}

		return Lanes;
	}

	void DrawDebug(const UWorld& World, const FZoneGraphStorage& Storage) const
	{
#if WITH_EDITOR
		for (int i=0;i<IntersectionSides.Num();i++)
		{
			FColor Color = FColor::Red;
			if (i == 1)
				Color = FColor::Green;
			if (i == 2)
				Color = FColor::Blue;
			if (i == 3)
				Color = FColor::Yellow;
			
			for (const FZoneGraphLaneHandle& Lane : IntersectionSides[i].Lanes)
			{
				auto LaneData = Storage.Lanes[Lane.Index];

				for (int j=LaneData.PointsBegin+1;j<LaneData.PointsEnd;j++)
				{
					FString Output = "";
					if (j == LaneData.PointsBegin+1)
					{
						Output = FString::Printf(TEXT("Index: %d"), i);
					}
					UE_VLOG_SEGMENT_THICK(&World, LogTemp, Verbose, Storage.LanePoints[j-1], Storage.LanePoints[j], Color, 5.f, TEXT("%s"), *Output);
				}
			}
		}
#endif
	}

	void SortSides()
	{
		// @todo probably better outside of struct
		struct FFloatAndID
		{
			FFloatAndID(float InNum, int32 InID)
			{
				Num = InNum;
				ID = InID;
			}
		
			float Num;
			int32 ID;

			bool operator<(const FFloatAndID& Other) const
			{
				return Num < Other.Num;
			}
		};
		
		TArray<FFloatAndID> ZAngleAndSideIndexArray;
		for (int32 S = 0; S < IntersectionSides.Num(); S++)
		{
			const FVector SideDirection = IntersectionSides[S].DirectionIntoIntersection;

			static const FVector ReferenceDirection(1.0f, 0.0f, 0.0f);
			const float Dot = FVector::DotProduct(ReferenceDirection, SideDirection);
			const FVector Cross = FVector::CrossProduct(ReferenceDirection, SideDirection);	

			const float SortSign = (Cross.Z > 0.0f ? 1.0f : -1.0f);
			const float ZAngle = FMath::Acos(Dot);
			const float SignedZAngle = SortSign * ZAngle;

			ZAngleAndSideIndexArray.Add(FFloatAndID(SignedZAngle, S));
		}
	
		ZAngleAndSideIndexArray.Sort();

		TArray<FTrafficIntersectionSide, TInlineAllocator<4>> OldSides = IntersectionSides;
		IntersectionSides.Empty();
		for (int32 S = 0; S < ZAngleAndSideIndexArray.Num(); S++)
		{
			const int32 SOld = ZAngleAndSideIndexArray[S].ID;
			IntersectionSides.Add(OldSides[SOld]);
		}
	}

	const FTrafficPeriod* GetCurrentPeriod() const
	{
		if (TrafficPeriods.IsValidIndex(CurrentPeriodIndex))
			return &TrafficPeriods[CurrentPeriodIndex];

		return nullptr;
	}

	FTrafficPeriod& IncrementCurrentPeriod();

	bool IsSquareShaped() const;

	int GetSidesConnectingLanes(int StartSideIndex, int EndSideIndex, const FZoneGraphStorage& ZoneGraphStorage, TArray<FZoneGraphLaneHandle>&
	                       OutTrafficLanes) const;

	void SetOverrideIntersection(bool bShouldOverrideIntersection, UWorld* World);

	bool operator==(const FTrafficLightIntersection& OtherIntersection) const
	{
		return ZoneIndex == OtherIntersection.ZoneIndex;
	}
};

USTRUCT()
struct FTrafficLightData
{
	GENERATED_BODY()

	FTrafficLightData() = default;

	FZoneGraphDataHandle DataHandle = FZoneGraphDataHandle();
	
	TArray<FTrafficLightIntersection> Intersections;

	FTrafficLightIntersection& FindOrAddIntersection(int32 IntersectionZoneIndex)
	{
		auto Intersection = Intersections.FindByKey(IntersectionZoneIndex);
		if (!Intersection)
		{
			Intersection = &(Intersections.Add_GetRef(FTrafficLightIntersection(IntersectionZoneIndex)));
		}
		
		return *Intersection;
	}
};
