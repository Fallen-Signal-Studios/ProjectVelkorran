// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Editor/UnrealEdEngine.h"
#include "NarrativeEditorEngine.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVEARSENALPREEDITOR_API UNarrativeEditorEngine : public UUnrealEdEngine
{
	GENERATED_BODY()

	virtual bool Game_Map_Check(UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar, bool bCheckDeprecatedOnly) override;
	virtual bool Game_Map_Check_Actor(const TCHAR* Str, FOutputDevice& Ar, bool bCheckDeprecatedOnly, AActor* InActor) override;
	
	TMap<FGuid, TWeakObjectPtr<AActor>> ActorGuids;
};
