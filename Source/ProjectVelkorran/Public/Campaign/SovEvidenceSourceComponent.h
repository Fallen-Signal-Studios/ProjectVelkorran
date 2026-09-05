// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Campaign/SovEvidenceDefinition.h"
#include "SovEvidenceSourceComponent.generated.h"

/** Authored proof source. Content decides whether interaction, investigation or authentication reveals it. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovEvidenceSourceComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USovEvidenceSourceComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") FName EvidenceId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") TObjectPtr<USovEvidenceDefinition> Definition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") ESovEvidenceStage RequestedStage = ESovEvidenceStage::Observed;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") FName SourceLocationId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") FName CustodianId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") FName SupportingEvidenceId;
	/** Distributed acquisitions occur at the actual, configured recipient source; the destination must equal CustodianId. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") FName CopyDestination;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") ESovRecordPublicity Publicity = ESovRecordPublicity::Private;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") TArray<FName> WitnessIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence|Viewmaker") bool bRequiresViewmaker = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence|Viewmaker", meta=(ClampMin="1",ClampMax="80")) float ScanHalfAngleDegrees = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence|Viewmaker") FGameplayTagContainer RequiredQueryKnowledge;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence|Viewmaker") TArray<FName> AuthoredTraceIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence|Viewmaker") FText AuthoredRouteHint;
	/** Stable per placed source; assign a GUID when authoring a reusable runtime-spawned source. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Evidence") FGuid SourceId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") FName AcquisitionMission;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") FName RequiredCompletedBeat;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") FGameplayTagContainer AllowedProtagonists;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") FGameplayTagContainer GrantedKnowledge;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence", meta=(ClampMin="1")) float InteractionRange = 300.f;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Evidence")
	bool TryAcquire(class APlayerController* Player);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Evidence|Viewmaker") bool TryScan(class APlayerController* Player);
	bool ValidateAcquisitionMode(APlayerController* Player) const;
	bool ValidateConfiguration(FString& OutError) const;
private:
	TWeakObjectPtr<APlayerController> ScanningPlayer;
	bool bAcquiring = false;
protected:
	virtual void OnComponentCreated() override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#endif
};
