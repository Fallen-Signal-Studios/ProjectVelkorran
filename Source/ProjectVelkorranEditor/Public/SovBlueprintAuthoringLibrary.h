// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovBlueprintAuthoringLibrary.generated.h"

/** Python keeps this result intact on failure, including the compilation report. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRANEDITOR_API FSovBlueprintAuthoringResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Velkorran|Editor")
    bool bSucceeded = false;
    UPROPERTY(BlueprintReadOnly, Category="Velkorran|Editor")
    FString Report;
};

/** Scoped asset authoring operations. This module is never included in a game build. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovBlueprintAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Insert the Selene-only posture before PreLookAt in the existing Narrative base class. No save. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FSovBlueprintAuthoringResult ConfigureSeleneFemininePosture(UObject* Asset, UObject* Blend);
    /** PIE-only A/B preview and live node diagnostics. Never changes an asset. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FSovBlueprintAuthoringResult PreviewSeleneFemininePosture(UObject* Instance, bool bEnabled);
    /** Bake a short braced discharge from an existing weapon idle, preserving the grip. No save. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FSovBlueprintAuthoringResult AuthorBracedCastClip(UObject* Asset, UObject* Source,
        float Duration, float ReleaseTime, bool bSecondVariant);

    /** Cosmetic-only cast montage from a project clip. Clears notifies and root motion. No save. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FSovBlueprintAuthoringResult ConfigureProtagonistCastMontage(UObject* Asset, UObject* Sequence,
        float StartTime, float EndTime, float PlayRate);
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FSovBlueprintAuthoringResult ConfigureEnemyCastMontage(UObject* Asset, UObject* Sequence,
        float StartTime, float EndTime, float PlayRate, FName SlotName);

    /** Replace the copied melee overlay's third-person idle with a speed-driven stance blend. No save. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FSovBlueprintAuthoringResult ConfigureVerityTwinLocomotion(UObject* Asset, UObject* Blend,
        const TArray<UObject*>& Clips);

    /** Configure only a project-owned Verity montage; preserves native notify classes. Does not save. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FSovBlueprintAuthoringResult ConfigureVerityTwinMontage(UObject* Asset, UObject* Sequence,
        float AttackStart, float AttackEnd);

    /** Read-only digests of persistent UObject properties, including graph objects and CDOs. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FString FingerprintBlueprint(UObject* Asset);

    /** Add the current companion-command target only to the legacy attack-goal failure return. No save. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FSovBlueprintAuthoringResult AddCompanionAttackTargetFallback(UObject* Asset);

    /** Remap hard Blueprint/class/default-object references only in explicitly supplied /Game copies.
     * Does not save assets; the caller must inspect the compilation report before saving.
     */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor")
    static FSovBlueprintAuthoringResult RemapProjectBlueprintReferences(const TArray<UObject*>& ProjectAssets,
        const TArray<UObject*>& SourceAssets, const TArray<UObject*>& ReplacementAssets);
private:
    static bool RemapProjectBlueprintReferencesInternal(const TArray<UObject*>& ProjectAssets,
        const TArray<UObject*>& SourceAssets, const TArray<UObject*>& ReplacementAssets, FString& Report);
};
