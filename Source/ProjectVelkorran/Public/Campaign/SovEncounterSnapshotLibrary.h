// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Campaign/SovEncounterTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovEncounterSnapshotLibrary.generated.h"

class UAbilitySystemComponent;
class UActorComponent;

/** Uses Narrative's component record format and save interface, never a second save file. */
UCLASS()
class PROJECTVELKORRAN_API USovEncounterSnapshotLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static bool CaptureResources(UAbilitySystemComponent* ASC, FSovCombatResourceSnapshot& OutSnapshot);
	static bool RestoreResources(UAbilitySystemComponent* ASC, const FSovCombatResourceSnapshot& Snapshot);
	static bool CaptureComponent(UActorComponent* Component, FNarrativeSaveComponent& OutRecord);
	static bool RestoreComponent(UActorComponent* Component, const FNarrativeSaveComponent& Record);
};
