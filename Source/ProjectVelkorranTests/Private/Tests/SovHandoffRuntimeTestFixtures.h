// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Framework/SovPlayerController.h"
#include "CharacterCreator/NarrativeSaveWithCreatorData.h"
#include "SovHandoffRuntimeTestFixtures.generated.h"
class ASovPlayerState;
class UPlayerDefinition;

/** Seeds only asset/visual readiness prerequisites; all gate, component, save and finish methods are production. */
UCLASS(Transient, NotBlueprintable)
class ASovHandoffRuntimeTestPawn : public ASovPlayerCharacterBase
{
	GENERATED_BODY()
public:
	ASovHandoffRuntimeTestPawn(const FObjectInitializer& ObjectInitializer);
	virtual FGameplayTag GetProtagonistIdentityTag() const override;
	virtual void PossessedBy(AController* NewController) override { APawn::PossessedBy(NewController); }
	bool StageTestReadiness(ASovPlayerState* State, bool bVisualReady);
	void SetCinematicVisualForTest(ANarrativeCharacterVisual* Visual) { CharVisual = Visual; }
	void ApplyCinematicStartupEffectsForTest() { AddStartupEffects(); }
	void SetTestVisualReady(bool bReady)
	{
		bVisualReadyForGameplay = bReady;
		if (bReady) { TryFinalizeCharacterReadiness(); }
		else { InvalidateCharacterReadiness(); }
	}
};

UCLASS(Transient, NotBlueprintable)
class ASovHandoffRuntimeTestController : public ASovPlayerController
{
	GENERATED_BODY()
public:
	ASovHandoffRuntimeTestController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
	void SetTestPlayerState(ASovPlayerState* State);
	virtual void PrepareForSave_Implementation() override;
	TFunction<void()> OnPrepareTravelSave;
	UPROPERTY() TArray<TObjectPtr<UObject>> KeepAlive;
};

/** Native serialization callback seam; all platform IO still uses the production save path. */
UCLASS(Transient, NotBlueprintable)
class USovTravelOwnerFenceTestSave : public UNarrativeSaveWithCreatorData
{
	GENERATED_BODY()
public:
	static TFunction<void(bool)> OnTravelSerialization;
	virtual void Serialize(FArchive& Ar) override;
};
