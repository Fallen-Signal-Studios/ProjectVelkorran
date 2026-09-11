// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Character/NarrativeCharacterVisual.h"
#include "SovNPCVisualLifecycleTestFixtures.generated.h"

/** Explicitly completes an empty loaded mesh set through the real producer. */
UCLASS()
class ASovNPCVisualLifecycleTestVisual : public ANarrativeCharacterVisual
{
	GENERATED_BODY()
public:
	void SetCharacterForTest(ANarrativeCharacter* Character) { OwnerCharacter = Character; }
	void CompleteMeshesForTest() { OnBaseMeshesReady(); }
	int32 FinalAppearanceEvents = 0;
	virtual void BaseAppearanceApplied_Implementation() override
	{
		++FinalAppearanceEvents;
		Super::BaseAppearanceApplied_Implementation();
	}
};

/** No asynchronous content request; actual NPC init/save/visual methods remain. */
UCLASS()
class ASovNPCVisualLifecycleTestCharacter : public ASovNPCCharacterBase
{
	GENERATED_BODY()
public:
	int32 NewCharacterCalls = 0;
	int32 VisualNotifications = 0;
	bool bDestroyDuringNewCharacter = false;
	bool bDestroyDuringVisualNotification = false;
	UPROPERTY() TObjectPtr<ASovNPCVisualLifecycleTestVisual> ReplacementVisual;
	void SetVisualForTest(ASovNPCVisualLifecycleTestVisual* Visual) { CharVisual = Visual; }
	bool HasMarkerForTest() const { return MapMarker != nullptr; }
	virtual void OnDefinitionSet_Implementation(UCharacterDefinition* NewDefinition) override {}
	virtual void InitNewCharacter_Implementation(UCharacterDefinition* NewDefinition) override
	{
		++NewCharacterCalls;
		Super::InitNewCharacter_Implementation(NewDefinition);
		if (bDestroyDuringNewCharacter) { Destroy(); }
	}
	UFUNCTION() void ObserveVisual(ANarrativeCharacter* Character)
	{
		if (Character != this) { return; }
		++VisualNotifications;
		if (bDestroyDuringVisualNotification) { Destroy(); }
		else if (ReplacementVisual)
		{
			CharVisual = ReplacementVisual;
			ReplacementVisual = nullptr;
		}
	}
};
