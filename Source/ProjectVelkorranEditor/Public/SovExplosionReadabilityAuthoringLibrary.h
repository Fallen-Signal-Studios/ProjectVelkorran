// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovExplosionReadabilityAuthoringLibrary.generated.h"
class UNiagaraSystem;
class UBlueprint;

/** Finite editor-only explosion presentation operations. Never saves or changes gameplay. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovExplosionReadabilityAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Stable handle IDs/modes/ownership, exposed inputs and complete active renderer properties, including stateless. */
    UFUNCTION(BlueprintCallable, Category="Aurelion|Combat Art")
    static TMap<FString, FString> InspectExplosionSystem(UNiagaraSystem* System);
    /** Only two exact owned destinations; switches Debris/GroundDust off, preserving every other handle and renderer. */
    UFUNCTION(BlueprintCallable, Category="Aurelion|Combat Art")
    static bool PrepareOwnedExplosionSystem(UNiagaraSystem* System, FString& Error);
    /** Every persistent property of the exact rocket or gunshot CDO; no transient process state. */
    UFUNCTION(BlueprintCallable, Category="Aurelion|Combat Art")
    static TMap<FString, FString> InspectExplosionBindingDefaults(UBlueprint* Blueprint);
    /** Compiles only the same two exact Blueprints, reports native errors and never saves. */
    UFUNCTION(BlueprintCallable, Category="Aurelion|Combat Art")
    static bool CompileExplosionBindingBlueprint(UBlueprint* Blueprint, FString& Error);
};
