// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovBlueprintAuthoringLibrary.h"
#include "SovWeaponHUDAuthoringLibrary.generated.h"

/** Repairs only the owned weapon-info copy; never saves or changes the vendor widget. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovWeaponHUDAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FSovBlueprintAuthoringResult RepairOwnedWeaponHUD(UObject* Asset);
};
