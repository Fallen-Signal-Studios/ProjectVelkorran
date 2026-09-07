// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SovAurelionCheckpoint.generated.h"

class ASovPlayerController;
class USovCampaignStateComponent;

/** Combat entry and priority checkpoints have their own native transaction owners. */
UENUM(BlueprintType)
enum class ESovAurelionCheckpoint : uint8 { ContextCP0, SeleneEntryCP2, MeetingCP3, QuarantineCP6, CoreCP7, ConversationCP8, DepartureCP9 };

/** Place on a stable route threshold. Captures existing state only; never completes a mission beat. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionCheckpoint : public AActor
{
    GENERATED_BODY()
public:
    ASovAurelionCheckpoint();
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion") ESovAurelionCheckpoint Checkpoint = ESovAurelionCheckpoint::ContextCP0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion") TObjectPtr<class UBoxComponent> Threshold;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion") bool bCaptureOnOverlap = true;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Aurelion") FString LastError;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Aurelion") bool RequestCheckpoint(ASovPlayerController* Controller, FString& Error);
    UFUNCTION(BlueprintPure, Category="Aurelion") FName GetBoundaryId() const;
    static bool MatchesProgress(ESovAurelionCheckpoint Boundary, const USovCampaignStateComponent* State);
protected:
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    bool ContainsPlayer(const AActor* Player) const;
    UFUNCTION() void HandleStateRestored(bool bValid);
    UFUNCTION() void HandleOverlap(class UPrimitiveComponent* Component, AActor* Actor, class UPrimitiveComponent* Other,
        int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
    bool bWriting = false;
    bool bAutoAttempted = false;
    uint64 StateEpoch = 0;
    TWeakObjectPtr<USovCampaignStateComponent> ObservedState;
};
