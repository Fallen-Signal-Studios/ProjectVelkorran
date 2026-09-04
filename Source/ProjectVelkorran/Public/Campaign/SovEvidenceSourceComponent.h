// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SovEvidenceSourceComponent.generated.h"

/** Authored proof source. Content decides whether interaction, investigation or authentication reveals it. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovEvidenceSourceComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USovEvidenceSourceComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") FName EvidenceId;
	/** Stable per placed source; assign a GUID when authoring a reusable runtime-spawned source. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Evidence") FGuid SourceId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") FName AcquisitionMission;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") FName RequiredCompletedBeat;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") FGameplayTagContainer AllowedProtagonists;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence") FGameplayTagContainer GrantedKnowledge;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Evidence", meta=(ClampMin="1")) float InteractionRange = 300.f;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Evidence")
	bool TryAcquire(class APlayerController* Player);
protected:
	virtual void OnComponentCreated() override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#endif
};
