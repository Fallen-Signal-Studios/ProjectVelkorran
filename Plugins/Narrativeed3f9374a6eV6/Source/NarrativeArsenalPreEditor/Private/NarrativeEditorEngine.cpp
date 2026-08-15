// Copyright Narrative Tools 2025.


#include "NarrativeEditorEngine.h"

#include "NarrativeStableActor.h"
#include "Misc/UObjectToken.h"

bool UNarrativeEditorEngine::Game_Map_Check(UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar,
                                            bool bCheckDeprecatedOnly)
{
	ActorGuids.Empty();
	return Super::Game_Map_Check(InWorld, Str, Ar, bCheckDeprecatedOnly);
}

bool UNarrativeEditorEngine::Game_Map_Check_Actor(const TCHAR* Str, FOutputDevice& Ar, bool bCheckDeprecatedOnly,
                                                  AActor* InActor)
{
	if (InActor->Implements<UNarrativeStableActor>())
	{
		auto ActorGuid = INarrativeStableActor::Execute_GetActorGUID(InActor);
		if (ActorGuid.IsValid())
		{
			if (ActorGuids.Contains(ActorGuid))
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Actor0"), FText::FromString(InActor->GetName()));
				Arguments.Add(TEXT("Actor1"), FText::FromString(ActorGuids[ActorGuid]->GetName()));
				FMessageLog("MapCheck").Warning()
					->AddToken(FUObjectToken::Create(InActor))
					->AddToken(FTextToken::Create(FText::Format(FText::FromString(" has same GUID as "), Arguments)))
					->AddToken(FUObjectToken::Create(ActorGuids[ActorGuid].Get()));
			}
			else
			{
				ActorGuids.Add(ActorGuid, InActor);
			}
		}
	}
	return Super::Game_Map_Check_Actor(Str, Ar, bCheckDeprecatedOnly, InActor);
}
