// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovBlueprintAuthoringLibrary.h"
#include "SovAurelionNPCIdentityLibrary.generated.h"

class UBlueprint;
class ASovNPCCharacterBase;

/** Repairs only the seven owned role copies, without loading definitions or saving assets. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovAurelionNPCIdentityLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Aurelion|Editor")
    static FSovBlueprintAuthoringResult RepairOwnedEarlyNPCIdentity(UBlueprint* Blueprint);

    /** Assign only a reviewed placed identity, outside PIE; no definition initialization or asset save. */
    UFUNCTION(BlueprintCallable, Category="Aurelion|Editor")
    static FSovBlueprintAuthoringResult AssignOwnedPlacedNPCIdentity(ASovNPCCharacterBase* NPC, const FGuid& ExpectedGUID);

private:
    // Shares the exact actor/identity admission and field assignment with the isolated native fixture.
    static FSovBlueprintAuthoringResult AssignIdentityToOwnedActor(ASovNPCCharacterBase* NPC, const FGuid& ExpectedGUID);
#if WITH_DEV_AUTOMATION_TESTS
    friend class FSovPlacedNPCIdentityAssignmentTest;
#endif
};
