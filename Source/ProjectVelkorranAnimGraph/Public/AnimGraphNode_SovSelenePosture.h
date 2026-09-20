#pragma once

#include "AnimGraphNode_Base.h"
#include "Animation/AnimNode_SovSelenePosture.h"
#include "AnimGraphNode_SovSelenePosture.generated.h"

UCLASS()
class PROJECTVELKORRANANIMGRAPH_API UAnimGraphNode_SovSelenePosture : public UAnimGraphNode_Base
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Settings")
    FAnimNode_SovSelenePosture Node;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override { return FText::FromString(TEXT("Selene Feminine Posture")); }
    virtual FString GetNodeCategory() const override { return TEXT("Sovereign|Animation"); }
};
