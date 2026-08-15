// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Public/PoseSearch/PoseSearchFeatureChannel_Position.h"
#include "NarrativePoseSearchFeatureChannel_Climb.generated.h"

/**
 * Builds the query from a ledge transform that's already in the correct space for climbing
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativePoseSearchFeatureChannel_Climb : public UPoseSearchFeatureChannel_Position
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, BlueprintImplementableEvent, meta=(BlueprintThreadSafe, DisplayName = "Get Local Position"), Category = "Settings")
	FVector BP_GetLocalPosition(const UAnimInstance* AnimInstance) const;

	virtual void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext) const override;
};
