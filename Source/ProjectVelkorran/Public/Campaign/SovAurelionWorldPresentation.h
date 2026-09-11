// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "SovAurelionWorldPresentation.generated.h"
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class ASovEncounterDirector;
class USovEncounterCoordinationComponent;

/** A physical route/set-piece view of an existing journal fact. It never grants progression. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionJournalGate : public AActor
{
    GENERATED_BODY()
public:
    ASovAurelionJournalGate();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UTextRenderComponent> Sign;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName MissionId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName BeatId;
    /** False: open after this fact. True: close after it (quarantine). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bBlockAfterCompletion = false;
    /** Optional existing graybox pieces shown with this gate (cage bars, staging actors). */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly) TArray<TObjectPtr<AActor>> BoundVisualActors;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bUseGateBody = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText ClosedText;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText OpenText;
    UFUNCTION(BlueprintPure) bool IsBlockingRoute() const { return bBlocking; }
protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    void Refresh();
    bool bBlocking = true;
    bool bApplied = false;
};

/** Draws an actual local combat cue before acknowledging a director's warning receipt. */
UCLASS()
class PROJECTVELKORRAN_API USovAurelionThreatWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void QueueWarning(USovEncounterCoordinationComponent* Source, FGuid Id, AActor* Attacker, float LeadSeconds);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
    struct FCue
    {
        TWeakObjectPtr<USovEncounterCoordinationComponent> Coordinator;
        TWeakObjectPtr<AActor> Attacker;
        FGuid Id;
        double ExpiresAt = 0;
        mutable bool bPainted = false;
        bool bAcknowledged = false;
    };
    TArray<FCue> Cues;
};

/** Only binds authored encounter warning producers to the local presentation surface. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionPresentationDirector : public AActor
{
    GENERATED_BODY()
public:
    ASovAurelionPresentationDirector();
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly) TArray<TObjectPtr<ASovEncounterDirector>> Encounters;
protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void HandleWarning(FGuid Id, AActor* Source, float LeadSeconds);
    UPROPERTY(Transient) TObjectPtr<USovAurelionThreatWidget> Widget;
};
