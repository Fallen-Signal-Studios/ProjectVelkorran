// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassRepresentationSubsystem.h"
#include "MassPedRepresentationSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVEARSENAL_API UMassPedRepresentationSubsystem : public UMassRepresentationSubsystem
{
	GENERATED_BODY()

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
};
