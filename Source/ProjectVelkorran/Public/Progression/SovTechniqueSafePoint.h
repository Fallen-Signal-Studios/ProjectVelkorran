// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SovTechniqueSafePoint.generated.h"
class UBoxComponent;
class ASovPlayerState;
/** Physical authored safe point; caller-supplied booleans cannot authorize respec. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovTechniqueSafePoint : public AActor
{
	GENERATED_BODY()
public:
	ASovTechniqueSafePoint();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Technique") TObjectPtr<UBoxComponent> SafeBounds;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Technique") FName SafePointId;
	/** Mark only authored major checkpoints and supply stations; ordinary refinement points do not implicitly refill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Field Recovery") bool bRefillsFieldRecovery = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique", meta=(ClampMin="100.0", Units="cm")) float HostileExclusionRadius = 1500.f;
	bool AllowsModification(const ASovPlayerState* Player) const;
};
