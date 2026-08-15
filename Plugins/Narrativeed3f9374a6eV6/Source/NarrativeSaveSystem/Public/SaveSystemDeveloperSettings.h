// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SaveSystemDeveloperSettings.generated.h"

/**
 * Settings for configuring the Narrative Save System. 
 */
UCLASS(BlueprintType, config = Engine, defaultconfig, meta = (DisplayName="Narrative - Save System"))
class NARRATIVESAVESYSTEM_API USaveSystemDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:

	USaveSystemDeveloperSettings();

	/** If true, Narrative will automatically load the save in slot 1, provided you have a game saved in that slot.
	This is great for QA testing - it saves you having to open the pause menu and load your game every time you load in. */
	//UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Save System")
	//bool bAutoLoadFirstSaveInEditor;

#if WITH_EDITORONLY_DATA
	/** folder where shared saves are located. relative to project saved directory */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Save System")
	FDirectoryPath SharedSavesDirectory;
#endif

	/** The class to use for our save game  */
	UPROPERTY(config, noclear, EditAnywhere, Category = "Narrative Pro|Save System", meta=(MetaClass="/Script/NarrativeSaveSystem.NarrativeSave"))
	FSoftClassPath SaveGameClass;

protected:

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
};
