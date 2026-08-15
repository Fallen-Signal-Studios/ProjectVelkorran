// Copyright Narrative Tools 2024. 


#include "SaveSystemDeveloperSettings.h"
#include "NarrativeSave.h"

USaveSystemDeveloperSettings::USaveSystemDeveloperSettings()
{
	//bAutoLoadFirstSaveInEditor = true;
	SaveGameClass = UNarrativeSave::StaticClass();

#if WITH_EDITORONLY_DATA
	SharedSavesDirectory.Path = FPaths::ProjectSavedDir() / "SharedSaves";
	FPaths::MakePathRelativeTo(SharedSavesDirectory.Path, *FPaths::ProjectSavedDir());
#endif
	
}

#if WITH_EDITORONLY_DATA
void USaveSystemDeveloperSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	// ensure that the path is relative so that things won't break between multiple users
	if (PropertyName == GET_MEMBER_NAME_CHECKED(FDirectoryPath, Path))
	{
		FPaths::MakePathRelativeTo(SharedSavesDirectory.Path, *FPaths::ProjectSavedDir());
	}
}
#endif 

