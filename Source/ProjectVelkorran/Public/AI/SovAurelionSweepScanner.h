// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SovAurelionSweepScanner.generated.h"

class ASovEncounterDirector;
class ASovPlayerCharacterBase;
class ASovPlayerController;
class ASovDroneNPCBase;
class ANarrativeNPCController;
class UNarrativeAbilitySystemComponent;
class USovCampaignDefinition;
class USpotLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** A physical environmental sensor, not an NPC decision owner. It sends only finite
 * NetworkSensor observations to the two existing authored relay drones. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionSweepScanner : public AActor
{
    GENERATED_BODY()
public:
    ASovAurelionSweepScanner();
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Scanner") FName ScannerId;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Scanner") FName MissionId;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Scanner") TObjectPtr<ASovEncounterDirector> RelayDirector;
    /** Exactly two unique participant IDs; never a class, spawn request or nearby-actor search. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Scanner") TArray<FName> RelayDroneIds;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Scanner") bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Scanner", meta=(ClampMin="100", ClampMax="3000")) float ScanRange = 2400.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Scanner", meta=(ClampMin="5", ClampMax="45")) float ConeHalfAngle = 18.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Scanner", meta=(ClampMin="0", ClampMax="80")) float SweepHalfArc = 55.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Scanner", meta=(ClampMin="1", ClampMax="20")) float SweepPeriod = 6.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Scanner", meta=(ClampMin="-60", ClampMax="30")) float Pitch = -12.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Scanner", meta=(ClampMin="0", ClampMax="1")) float PhaseOffset = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Scanner", meta=(ClampMin="1", ClampMax="30")) float ObservationLifetime = 30.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion|Scanner") TObjectPtr<USpotLightComponent> Cone;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion|Scanner") TObjectPtr<UStaticMeshComponent> Head;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion|Scanner") TObjectPtr<UTextRenderComponent> Label;
    UFUNCTION(BlueprintPure, Category="Aurelion|Scanner") bool HasPendingObservation() const { return bPending; }
    UFUNCTION(BlueprintPure, Category="Aurelion|Scanner") int32 GetAlertedRecipientCount() const;
    UFUNCTION(BlueprintPure, Category="Aurelion|Scanner") bool ValidateConfiguration(FString& Error) const;
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    struct FReceiver
    {
        FName Id;
        TWeakObjectPtr<ASovDroneNPCBase> Pawn;
        TWeakObjectPtr<ANarrativeNPCController> Controller;
        TWeakObjectPtr<UNarrativeAbilitySystemComponent> ASC;
        uint64 ActorInfoEpoch = 0;
        bool bDelivered = false;
    };
    struct FObservation
    {
        FName ScannerId;
        FTransform SensorTransform;
        TWeakObjectPtr<ASovEncounterDirector> Director;
        TWeakObjectPtr<ASovPlayerCharacterBase> Player;
        TWeakObjectPtr<ASovPlayerController> Controller;
        TWeakObjectPtr<UNarrativeAbilitySystemComponent> ASC;
        TWeakObjectPtr<USovCampaignDefinition> Mission;
        uint64 DirectorGeneration = 0;
        uint64 ActorInfoEpoch = 0;
        uint64 TransitionEpoch = 0;
        int32 ReadyEpoch = 0;
        FGuid Attempt;
        bool bEncounterWasActive = false;
        FVector Position = FVector::ZeroVector;
        double ExpiresAt = 0.;
        float Lifetime = 0.f;
        TArray<FReceiver> Receivers;
    };
    bool GetCurrentPlayer(ASovPlayerCharacterBase*& Player, ASovPlayerController*& PC) const;
    bool CanSee(const ASovPlayerCharacterBase* Player, FVector& SeenPosition) const;
    bool CaptureObservation();
    bool OwnsObservation() const;
    void DeliverObservation();
    void ClearObservation();
    void UpdatePresentation();
    void PresentCue(ASovPlayerController* PC, const FText& Text);
    FObservation Observation;
    bool bPending = false;
    bool bEnding = false;
    bool bUpdating = false;
};
