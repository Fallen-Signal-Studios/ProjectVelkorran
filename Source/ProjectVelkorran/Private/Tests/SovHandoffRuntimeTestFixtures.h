// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Framework/SovPlayerController.h"
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
	void SetTestVisualReady(bool bReady) { bVisualReadyForGameplay = bReady; }
};

UCLASS(Transient, NotBlueprintable)
class ASovHandoffRuntimeTestController : public ASovPlayerController
{
	GENERATED_BODY()
public:
	ASovHandoffRuntimeTestController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
	void SetTestPlayerState(ASovPlayerState* State);
	UPROPERTY() TArray<TObjectPtr<UObject>> KeepAlive;
};
