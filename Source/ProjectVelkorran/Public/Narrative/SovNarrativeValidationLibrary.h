// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovNarrativeValidationLibrary.generated.h"
class UDialogue;
class USovCampaignDefinition;

/** Preflight over Narrative's real graph, suitable for an editor button or an unattended commandlet. */
UCLASS()
class PROJECTVELKORRAN_API USovNarrativeValidationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Campaign|Validation")
    static bool ValidateDialogue(UDialogue* Dialogue, USovCampaignDefinition* Mission, bool bShippingValidation,
        TArray<FString>& OutErrors, TArray<FString>& OutWarnings);
};
