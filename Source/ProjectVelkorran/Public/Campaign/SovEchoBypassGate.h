// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Perception/AIPerceptionTypes.h"
#include "SovEchoBypassGate.generated.h"
class ASovEncounterDirector;
class ASovNPCCharacterBase;
class UBoxComponent;
class UPrimitiveComponent;
class UAIPerceptionComponent;
class USovSeleneEchoGenerationComponent;

/** Authored route geometry; rewards require real overlaps and continuous authority perception. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovEchoBypassGate : public AActor
{
	GENERATED_BODY()
public:
	ASovEchoBypassGate();
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bypass") TObjectPtr<ASovEncounterDirector> Encounter;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bypass") TArray<FName> ThreatParticipantIds;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bypass") TObjectPtr<UBoxComponent> EntryVolume;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bypass") TObjectPtr<UBoxComponent> ExitVolume;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bypass", meta = (ClampMin = "0.1")) float MaximumTraversalSeconds = 30.f;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
private:
	friend class USovSeleneEchoGenerationComponent;
	bool ConsumeReceipt(AActor* Player, const FGuid& ReceiptId, FGuid& OutAttemptId);
	bool ValidateCandidate(bool& bOutNewlyDisabled) const;
	void ClearCandidate();
	void IssueReceipt();
	UFUNCTION() void HandleEntry(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION() void HandleExit(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION() void HandlePerception(AActor* Actor, FAIStimulus Stimulus);
	TWeakObjectPtr<AActor> Candidate;
	TArray<TWeakObjectPtr<ASovNPCCharacterBase>> CandidateThreats;
	TArray<TWeakObjectPtr<UAIPerceptionComponent>> Perceptions;
	FGuid CandidateAttempt;
	FGuid PendingReceipt;
	float EnteredAt = 0.f;
	bool bConsumed = false;
};
