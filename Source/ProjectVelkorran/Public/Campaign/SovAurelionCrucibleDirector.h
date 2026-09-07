// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Campaign/SovEncounterDirector.h"
#include "Components/SovAurelionThermalFractureComponent.h"
#include "SovAurelionCrucibleDirector.generated.h"

class ASovCampaignEncounterObjective;
class ASovPlayerController;
class ASovCampaignHandoffAnchor;
class USovAurelionThermalFractureComponent;
struct FSovAurelionThermalFractureReceipt;

USTRUCT(BlueprintType)
struct FSovAurelionCrucibleLink
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ParticipantId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ComponentName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName LinkId;
};
USTRUCT()
struct FSovAurelionCrucibleLinkReceipt
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FName LinkId;
    UPROPERTY(SaveGame) FGuid InstanceId;
    UPROPERTY(SaveGame) FGuid TransactionId;
};

/** E4A: exact native link severs create a frozen, completed encounter boundary.
 * The same living NPCs are handed to E4B only after the real protagonist handoff commits. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionLinkPhaseDirector : public ASovEncounterDirector
{
    GENERATED_BODY()
public:
    ASovAurelionLinkPhaseDirector();
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Crucible") FName MissionId;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Crucible") FName CompletionBeat = TEXT("SeverCrucibleLinks");
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Crucible") FName HandoffBeat = TEXT("HandoffToTarrikCrucible");
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Crucible") FName EliteParticipantId;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Crucible") TArray<FSovAurelionCrucibleLink> RequiredLinks;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Crucible") TObjectPtr<ASovCampaignHandoffAnchor> HandoffAnchor;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Crucible") TObjectPtr<ASovCampaignEncounterObjective> PhaseBObjective;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Crucible") bool bAutoRequestHandoff = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Crucible") bool bAutoStartPhaseB = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Crucible", meta=(ClampMin="1",ClampMax="60")) float BoundarySettleTimeout = 30.f;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Aurelion|Crucible") FString LastPhaseError;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Aurelion|Crucible") bool CompletePhaseHandoff(ASovPlayerCharacterBase* Tarrik, FString& Error);
    virtual ESovEncounterProofType GetCampaignProofType() const override { return ESovEncounterProofType::AurelionLinks; }
    virtual bool HasConfirmedVictory() const override;
    virtual bool CompleteEncounter() override;
    virtual bool IsCompletedPhaseBoundaryQuiescentForSave(const ASovPlayerCharacterBase* Player) const override;
    virtual void Load_Implementation() override;
protected:
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    friend struct FSovCrucibleRuntimeTestAccess;
    bool TransferFrozenParticipants(class ASovAurelionThermalPhaseDirector* Destination, FString& Error);
    USovCommandLinkComponent* ResolveLink(const FSovAurelionCrucibleLink& Binding) const;
    bool BindAttemptLinks(FString& Error);
    bool HasLinkProof() const;
    bool HasHandoffJournal(const ASovPlayerCharacterBase* Player) const;
    void UnbindLinks();
    UFUNCTION() void HandlePhaseState(ESovEncounterState Previous, ESovEncounterState Current);
    UFUNCTION() void HandleLinkSever(const FSovCommandLinkSeverResult& Result);
    UPROPERTY(SaveGame) FGuid ProofAttemptId;
    UPROPERTY(SaveGame) TArray<FSovAurelionCrucibleLinkReceipt> LinkReceipts;
    UPROPERTY(SaveGame) bool bBoundaryFrozen = false;
    UPROPERTY(SaveGame) bool bTransferred = false;
    UPROPERTY(Transient) TArray<TObjectPtr<USovCommandLinkComponent>> BoundLinks;
    TWeakObjectPtr<ASovPlayerCharacterBase> ProofPlayer;
    TWeakObjectPtr<ASovPlayerController> ProofController;
    TWeakObjectPtr<UNarrativeAbilitySystemComponent> ProofASC;
    int32 ProofReadyEpoch = 0;
    uint64 ProofActorInfoEpoch = 0, ProofTransitionEpoch = 0;
    float SettleStartedAt = 0.f;
    bool bPhaseMutation = false;
};

/** E4B: an actual local Thermal Fracture receipt followed by conventional combat victory. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionThermalPhaseDirector : public ASovEncounterDirector
{
    GENERATED_BODY()
public:
    ASovAurelionThermalPhaseDirector();
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Crucible") FName EliteParticipantId;
    virtual ESovEncounterProofType GetCampaignProofType() const override { return ESovEncounterProofType::AurelionThermalFracture; }
    virtual bool HasConfirmedVictory() const override;
    virtual bool CompleteEncounter() override;
protected:
    virtual void Tick(float DeltaSeconds) override;
private:
    UFUNCTION() void HandlePhaseState(ESovEncounterState Previous, ESovEncounterState Current);
    UFUNCTION() void HandleFracture(const FSovAurelionThermalFractureReceipt& Receipt);
    void BindFracture();
    UPROPERTY(SaveGame) FGuid FractureAttemptId;
    UPROPERTY(SaveGame) FGuid FractureFrostId;
    UPROPERTY(SaveGame) FGuid FractureHeatId;
    UPROPERTY(SaveGame) FGuid FracturePayoffId;
    UPROPERTY(Transient) TObjectPtr<USovAurelionThermalFractureComponent> FractureSource;
};
