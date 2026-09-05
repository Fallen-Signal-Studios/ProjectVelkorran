// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NarrativeSavableActor.h"
#include "SovRecoveryExclusionVolume.generated.h"
class UBoxComponent;

/** Authored lethal hazard, duel or canon-failure region. It never applies damage by itself. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovRecoveryExclusionVolume : public AActor, public INarrativeSavableActor
{
	GENERATED_BODY()
public:
	ASovRecoveryExclusionVolume();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Recovery") TObjectPtr<UBoxComponent> ExclusionBounds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category="Recovery") bool bActive = true;
	/** Stable authoring identifier, suitable for mission diagnostics. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Recovery") FName ExclusionId;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Recovery") void SetExclusionActive(bool bEnabled);
	/** Conservative bounds intersection includes capsule edges, independent of physics overlap updates. */
	static bool ExcludesCapsule(const UWorld* World, const FVector& Center, float Radius, float HalfHeight);
	virtual ENarrativeRestorePhase GetSaveRestorePhase() const override { return ENarrativeRestorePhase::World; }
	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& Guid) override { SaveGuid = Guid; }
	virtual bool ShouldRespawn_Implementation() const override { return false; }
private:
	UPROPERTY(SaveGame) FGuid SaveGuid;
};
