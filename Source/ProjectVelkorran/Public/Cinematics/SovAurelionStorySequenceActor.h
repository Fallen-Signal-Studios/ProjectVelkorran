// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "Campaign/SovAurelionPrioritySupport.h"
#include "SovAurelionStorySequenceActor.generated.h"

class USovCampaignCinematicComponent;
class USovAccessibilityPresentation;
class ASovPlayerController;

USTRUCT(BlueprintType)
struct FSovAurelionDialogueCue
{
    GENERATED_BODY()
    /** Seconds relative to the authored sequence playback range's lower bound. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float StartSeconds = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float DurationSeconds = 4.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Speaker;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(MultiLine=true)) FText Text;
    /** Unset means either route; conditional dialogue only reads the persisted M12 priority. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ESovAurelionRescuePriority RequiredPriority = ESovAurelionRescuePriority::Unset;
};

/** An authorable Narrative scene with its real campaign proof component and presentation-only dialogue.
 * Dialogue follows the native sequence clock. It never writes the campaign or supplies a receipt. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionStorySequenceActor : public ANarrativeLevelSequenceActor
{
    GENERATED_BODY()
public:
    ASovAurelionStorySequenceActor(const FObjectInitializer& Initializer);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion|Story") TObjectPtr<USovCampaignCinematicComponent> CampaignCinematic;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Story") TArray<FSovAurelionDialogueCue> DialogueCues;
    UFUNCTION(BlueprintPure, Category="Aurelion|Story") bool ValidateStoryContent(FString& Error) const;
    UFUNCTION(BlueprintPure, Category="Aurelion|Story") int32 GetCurrentDialogueCueIndex() const { return CurrentCue; }
    static bool ValidateDialogueCues(const TArray<FSovAurelionDialogueCue>& Cues, double SequenceSeconds, FString& Error);
    static int32 FindDialogueCue(const TArray<FSovAurelionDialogueCue>& Cues, double Seconds, ESovAurelionRescuePriority Priority = ESovAurelionRescuePriority::Unset);
    virtual void Tick(float DeltaSeconds) override;
private:
    uint64 DialogueGeneration = MAX_uint64;
    int32 CurrentCue = INDEX_NONE;
    TWeakObjectPtr<ASovPlayerController> DialogueController;
    TWeakObjectPtr<USovAccessibilityPresentation> DialoguePresentation;
};
