// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Interaction/InteractableComponent.h"
#include "NarrativeSavableComponent.h"
#include "Corruption/SovCorruptionProfile.h"
#include "SovCorruptionInteractableComponent.generated.h"

UENUM(BlueprintType)
enum class ESovCorruptionInteraction : uint8 { CompromisedMachinery, Countermeasure, ProtectSignal };

/** Uses Narrative's real completed interaction path. ProtectSignal starts a verified, interruptible protection interval. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovCorruptionInteractableComponent : public UNarrativeInteractableComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()
public:
	USovCorruptionInteractableComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption") ESovCorruptionInteraction Action = ESovCorruptionInteraction::CompromisedMachinery;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption") TObjectPtr<USovCorruptionProfile> Profile;
	/** For ProtectSignal: the actual living friendly contaminated actor. For a countermeasure: optional source to disable. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Corruption") TObjectPtr<AActor> SourceActor;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption", meta=(ClampMin="1",ClampMax="60")) float ProtectionSeconds = 5.0f;
	UFUNCTION(BlueprintPure, Category="Corruption") float GetProtectionProgress() const { return ProtectionElapsed; }
	virtual bool CanInteract_Implementation(APawn* Interactor, UNarrativeInteractionComponent* InteractionComp, FText& ErrorMessage) override;
	virtual void OnInteract_Implementation(APawn* Interactor, UNarrativeInteractionComponent* InteractionComp) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void Load_Implementation() override;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	friend struct FSovCorruptionTestAccess;
	bool ValidateInteractor(APawn* Player, FName& OutMission) const;
	bool ValidateSignal(APawn* Player) const;
	bool CompleteRemedy(APawn* Player, ESovCorruptionEscape Remedy);
	UPROPERTY(SaveGame) TSet<FName> ConsumedMissions;
	TWeakObjectPtr<APawn> ProtectingPlayer;
	FName ProtectingMission;
	uint64 ProtectionDamageSequence = 0;
	float ProtectionElapsed = 0.0f;
	bool bApplying = false;
};
