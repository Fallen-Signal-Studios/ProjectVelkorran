// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovCombatFeedbackAuthoringLibrary.generated.h"
class UNiagaraSystem;

USTRUCT(BlueprintType)
struct FSovFeedbackEmitterReadback
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) bool bEnabled = false;
    UPROPERTY(BlueprintReadOnly) bool bOwnedBySystem = false;
    UPROPERTY(BlueprintReadOnly) bool bGPU = false;
    UPROPERTY(BlueprintReadOnly) TArray<FString> RendererClasses;
    UPROPERTY(BlueprintReadOnly) TArray<FString> RendererObjects;
    UPROPERTY(BlueprintReadOnly) int32 LightRendererCount = 0;
};
USTRUCT(BlueprintType)
struct FSovFeedbackSystemReadback
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString AssetPath;
    UPROPERTY(BlueprintReadOnly) bool bValid = false;
    UPROPERTY(BlueprintReadOnly) bool bReady = false;
    UPROPERTY(BlueprintReadOnly) bool bCompiling = false;
    UPROPERTY(BlueprintReadOnly) TArray<FSovFeedbackEmitterReadback> Emitters;
    UPROPERTY(BlueprintReadOnly) TArray<FString> UserParameterNames;
    UPROPERTY(BlueprintReadOnly) TArray<FString> UserParameterTypes;
    UPROPERTY(BlueprintReadOnly) TMap<FString, float> FloatParameters;
    UPROPERTY(BlueprintReadOnly) TMap<FString, FLinearColor> ColorParameters;
};

/** Narrow editor-only inspection and sanitation of explicitly duplicated combat systems. Never saves. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovCombatFeedbackAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Aurelion|Combat Art")
    static FSovFeedbackSystemReadback InspectFeedbackSystem(UNiagaraSystem* System);
    /** Exact handle names come from InspectFeedbackSystem. Removing secondary emitters never edits particle inputs. */
    UFUNCTION(BlueprintCallable, Category="Aurelion|Combat Art")
    static bool PrepareOwnedFeedbackSystem(UNiagaraSystem* System, const TArray<FString>& DisabledEmitterNames,
        const TArray<FString>& RequiredSignalEmitterNames, const TMap<FString, float>& FloatParameters,
        const TMap<FString, FLinearColor>& ColorParameters, FString& Error);
    UFUNCTION(BlueprintCallable, Category="Aurelion|Combat Art")
    static bool FinishFeedbackCompilation(UNiagaraSystem* System);
};
