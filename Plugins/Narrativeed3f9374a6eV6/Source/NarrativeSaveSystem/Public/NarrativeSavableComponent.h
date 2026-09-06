// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include <UObject/Interface.h>
#include "NarrativeSavePhases.h"
#include "NarrativeSavableComponent.generated.h"

/**
 * Any components implementing this will be captured by Narrative Save subsystem, provided their owner implements INarrativeSavableActor. 
 */
UINTERFACE(BlueprintType)
class NARRATIVESAVESYSTEM_API UNarrativeSavableComponent : public UInterface
{
	GENERATED_BODY()
	
};

/**
 * Any Components implementing this interface will be capture by the Narrative Save subsystem
 */
class NARRATIVESAVESYSTEM_API INarrativeSavableComponent
{
	GENERATED_BODY()

public:
    /** Lower phases deserialize first within their existing actor owner. */
    virtual ENarrativeRestorePhase GetSaveRestorePhase() const { return ENarrativeRestorePhase::Interactables; }
	/**
	 * Native, side-effect-free preflight of a serialized component record. Implementations
	 * may decode into detached temporary state, but must not deserialize into this live
	 * component or change its owner/effects. Runs before any actor restore mutation.
	 * Existing native and Blueprint-only savables retain their previous behavior.
	 */
	virtual bool ValidateSaveRecord(const TArray<uint8>& RecordBytes) const { return true; }

	/**
	 * Native completion result queried after the existing Load event. True also permits
	 * an accepted, owner-bound restore waiting for initialization; it does not certify
	 * that asynchronous restoration has completed. False rejects the actor load.
	 */
	virtual bool WasSaveRecordLoadAccepted() const { return true; }

	/**
	 * Native opt-in compatibility hook for an existing component absent from an older
	 * actor record. Runs in this component's restore phase. The default does nothing;
	 * it must not dispatch Load against unrelated, stale serialized member state.
	 */
	virtual bool LoadMissingSaveRecord() { return true; }
	/** Required by default. Use only for cosmetic/DLC state whose absence cannot change canon or progression. */
	UFUNCTION(BlueprintNativeEvent)
	bool IsOptionalSaveRecord() const;
	virtual bool IsOptionalSaveRecord_Implementation() const;
	
	//Tell the Component it is about to be saved, and needs to populate all its save data 
	UFUNCTION(BlueprintNativeEvent)
	void PrepareForSave();
	virtual void PrepareForSave_Implementation();

	//Tell the Component it has been loaded in from a save. 
	UFUNCTION(BlueprintNativeEvent)
	void Load();
	virtual void Load_Implementation();

};
