// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "SovBlueprintAuthoringLibrary.h"
#include "SovAurelionAuthoringLibrary.generated.h"
class UStringTable;
class ULevelSequence;
USTRUCT(BlueprintType)
struct FSovAurelionBindingResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bSucceeded = false;
    UPROPERTY(BlueprintReadOnly) FString Report;
    UPROPERTY(BlueprintReadOnly) FGuid Binding;
};
UCLASS()
class PROJECTVELKORRANEDITOR_API USovAurelionAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Exact project-owned Aurelion table only. Validates the complete batch before mutation; never saves. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FSovBlueprintAuthoringResult SetAurelionStrings(UStringTable* Table, const TMap<FString, FString>& Entries);
    /** An existing root binding in an Aurelion sequence, with one unambiguous participant tag. Never saves. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FSovBlueprintAuthoringResult TagAurelionSequenceBinding(ULevelSequence* Sequence, FGuid Binding, FName Tag);
    /** Creates an unbound Narrative-character possessable, resolved by the native scene owner at runtime. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FSovAurelionBindingResult AddAurelionCharacterBinding(ULevelSequence* Sequence, FName Tag);
};
