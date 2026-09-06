// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "SovBlueprintAuthoringLibrary.h"
#include "SovReviewRouteAuthoringLibrary.generated.h"
class UStringTable;
/** Editor-only bridge for UE string-table mutation, which is not exposed to Python. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovReviewRouteAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Only the exact technical-review string table is writable. Never saves any package. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FSovBlueprintAuthoringResult SetReviewObjectiveStrings(UStringTable* Table, const TMap<FString, FString>& Entries);
};
