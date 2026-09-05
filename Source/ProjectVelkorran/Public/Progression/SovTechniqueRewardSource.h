// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SovTechniqueRewardSource.generated.h"
class ASovEncounterDirector;
class ASovPlayerState;
UENUM(BlueprintType)
enum class ESovTechniqueRewardProof : uint8 { MissionComplete, BeatComplete, EncounterComplete };
/** Authored one-time reward backed by completed campaign/encounter state. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovTechniqueRewardSource : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique") FName RewardId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique") FGameplayTag Protagonist;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique", meta=(ClampMin="1", ClampMax="5")) int32 Points = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique") ESovTechniqueRewardProof Proof = ESovTechniqueRewardProof::BeatComplete;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique") FName MissionId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique") FName BeatId;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Technique") TObjectPtr<ASovEncounterDirector> Encounter;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique", meta=(ClampMin="1.0", Units="cm")) float MaximumClaimDistance = 400.f;
	bool HasNativeProof(const ASovPlayerState* Player) const;
};
