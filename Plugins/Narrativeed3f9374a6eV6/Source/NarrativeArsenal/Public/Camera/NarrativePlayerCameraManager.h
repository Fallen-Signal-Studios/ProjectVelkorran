// Copyright Narrative Tools 2025.

#pragma once

#include "Camera/PlayerCameraManager.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "NarrativePlayerCameraManager.generated.h"

UCLASS(BlueprintType)
class NARRATIVEARSENAL_API ANarrativePlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()


protected:

	ANarrativePlayerCameraManager(const FObjectInitializer& ObjectInit);

	virtual void ProcessViewRotation(float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot) override;
	virtual void DoUpdateCamera(float DeltaTime) override;
	virtual void InitializeFor(class APlayerController* PC) override;

	virtual bool WantsFirstPersonRender() const;

	UPROPERTY(EditDefaultsOnly, Category = "Narrative Player Camera Manager")
	float FirstPersonRenderScale;

	UPROPERTY()
	TObjectPtr<ANarrativePlayerController> OwningNarrativeController; 

	UPROPERTY()
	TObjectPtr<ANarrativeCharacter> OwningNarrativeCharacter; 
};
