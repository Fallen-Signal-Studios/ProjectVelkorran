// Copyright Narrative Tools 2024. 


#include "NarrativeActorProvider.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "AI/NPCDefinition.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include <Engine/World.h>
#include "Navigation/MapTileBounds.h"
#include "Navigation/NavigationSubsystem.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "NarrativeActorProvider"


UNarrativeTransformProvider::UNarrativeTransformProvider()
{

}

FTransform UNarrativeTransformProvider::ProvideTransform_Implementation(const UObject* WorldContextObject) 
{
	return FTransform();
}

FText UNarrativeTransformProvider::GetDescription() const
{
	return FText::GetEmpty();
}

UNarrativeActorProvider::UNarrativeActorProvider()
{

}

UNarrativeActorProvider_NPC::UNarrativeActorProvider_NPC()
{

}

UNarrativeActorProvider_GUIDLookup::UNarrativeActorProvider_GUIDLookup()
{

}


class AActor* UNarrativeActorProvider::ProvideActor_Implementation(const UObject* WorldContextObject)
{
	return nullptr; 
}


FTransform UNarrativeActorProvider::ProvideTransform_Implementation(const UObject* WorldContextObject)
{
	if (AActor* Actor = ProvideActor(WorldContextObject))
	{
		return Actor->GetActorTransform();
	}

	return FTransform();
}

FText UNarrativeActorProvider::GetDescription() const
{
	return FText::GetEmpty();
}

class AActor* UNarrativeActorProvider_NPC::ProvideActor_Implementation(const UObject* WorldContextObject)
{

	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		if (UNarrativeCharacterSubsystem* NPCSub = World->GetSubsystem<UNarrativeCharacterSubsystem>())
		{
			return NPCSub->FindNPC(NPCDefinition);
		}
	}

	
	return nullptr; 
}


FText UNarrativeActorProvider_NPC::GetDescription() const
{
	if (NPCDefinition)
	{
		return FText::Format(LOCTEXT("NPCProviderDescription", "Find NPC {0}"), NPCDefinition->NPCName);
	}

	return FText::GetEmpty();
}

class AActor* UNarrativeActorProvider_GUIDLookup::ProvideActor_Implementation(const UObject* WorldContextObject)
{

	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		if (UNarrativeSaveSubsystem* SaveSub = World->GetSubsystem<UNarrativeSaveSubsystem>())
		{
			return SaveSub->LookupActorByGUID(GUIDToLookup);
		}
	}

	return nullptr; 
}

FText UNarrativeActorProvider_GUIDLookup::GetDescription() const
{
	return LOCTEXT("GUIDProviderDescription", "Find Actor By GUID");
}

UNarrativeActorProvider_LevelReference::UNarrativeActorProvider_LevelReference()
{

}

class AActor* UNarrativeActorProvider_LevelReference::ProvideActor_Implementation(const UObject* WorldContextObject)
{
	if (!SoftActorReference.IsNull())
	{
		if (AActor* SoftActor = SoftActorReference.LoadSynchronous())
		{
			return SoftActor;
		}
	}

	return nullptr; 
}

FText UNarrativeActorProvider_LevelReference::GetDescription() const
{
	if (SoftActorReference.ToString().Len())
	{
		return FText::Format(LOCTEXT("NPCProviderDescription", "Actor Ref {0}"), FText::FromString(SoftActorReference.ToString()));
	}

	return FText::GetEmpty();
}

void UNarrativeActorProvider_LevelReference::OnActorSpawned(class AActor* SpawnedActor)
{
	if (SoftActorReference)
	{
		OnProviderActorReady.Broadcast(SpawnedActor);
		ActorSpawnedHandle.Reset();
	}
}

UNarrativeTransformProvider_POI::UNarrativeTransformProvider_POI()
{
	POITag = FGameplayTag();
}

FTransform UNarrativeTransformProvider_POI::ProvideTransform_Implementation(const UObject* WorldContextObject) 
{
	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		if (UNavigationSubsystem* NavSub = World->GetSubsystem<UNavigationSubsystem>())
		{
			FPOIData POI;
			NavSub->GetPointOfInterest(POI, POITag);

			return FTransform(POI.POILocation);
		}
	}

	return FTransform();
}

FText UNarrativeTransformProvider_POI::GetDescription() const
{
	if (POITag.IsValid())
	{
		return FText::Format(LOCTEXT("POIProviderDescription", "POI {0}"), FText::FromString(POITag.ToString()));
	}


	return FText::GetEmpty();
}

UNarrativeActorProvider_ActorOfClass::UNarrativeActorProvider_ActorOfClass()
{
	
}

AActor* UNarrativeActorProvider_ActorOfClass::ProvideActor_Implementation(const UObject* WorldContextObject)
{
	return UGameplayStatics::GetActorOfClass(WorldContextObject, ActorClassToFind);
}

FText UNarrativeActorProvider_ActorOfClass::GetDescription() const
{
	if (IsValid(ActorClassToFind))
	{
		return FText::Format(LOCTEXT("POIProviderDescription", "Actor of Class {0}"), FText::FromString(*GetNameSafe(ActorClassToFind)));
	}


	return FText::GetEmpty();
}



UNarrativeProviderBase::UNarrativeProviderBase()
{

}

FText UNarrativeProviderBase::GetDescription() const
{
	return FText::FromString(GetName());
}

UNarrativeTransformProvider_SpecifiedTransform::UNarrativeTransformProvider_SpecifiedTransform()
{

}

FTransform UNarrativeTransformProvider_SpecifiedTransform::ProvideTransform_Implementation(const UObject* WorldContextObject)
{
	return SpecifiedTransform;
}



void UAsyncAction_ProvideActor::Activate()
{
	if (UWorld* OurWorld = World.Get())
	{
		if (CachedProvider)
		{
			//Immediately provide the actor, otherwise wait for it to be provided. 
			if (AActor* Actor = CachedProvider->ProvideActor(OurWorld))
			{
				OnProvided.Broadcast(Actor);
			}
			else if(PollInterval > 0.f)
			{
				OurWorld->GetTimerManager().SetTimer(PollHandle, this, &UAsyncAction_ProvideActor::PollActor, PollInterval, true);


				CachedProvider->OnProviderActorReady.AddDynamic(this, &UAsyncAction_ProvideActor::OnProvideActor);
			}
		}
		else
		{
			OnFailed.Broadcast(nullptr);
		}
	}
	else
	{
		OnFailed.Broadcast(nullptr);
	}

	Super::Activate();
}

void UAsyncAction_ProvideActor::SetReadyToDestroy()
{
	if (CachedProvider)
	{
		CachedProvider->OnProviderActorReady.RemoveAll(this);
		CachedProvider = nullptr;
	}

	if (UWorld* OurWorld = World.Get())
	{
		OurWorld->GetTimerManager().ClearTimer(PollHandle);
	}

	Super::SetReadyToDestroy();
}

UAsyncAction_ProvideActor* UAsyncAction_ProvideActor::ProvideActor(const UObject* WorldContextObject, UNarrativeActorProvider* Provider, float InPollInterval)
{
	UAsyncAction_ProvideActor* Action = NewObject<UAsyncAction_ProvideActor>();

	if (WorldContextObject && Provider && Action)
	{
		Action->CachedProvider = Provider;
		Action->PollInterval = InPollInterval;
		Action->World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

		Action->RegisterWithGameInstance(WorldContextObject);
	}

	return Action;
}

void UAsyncAction_ProvideActor::PollActor()
{
	if (UWorld* OurWorld = World.Get())
	{
		if (CachedProvider)
		{
			//Immediately provide the actor, otherwise wait for it to be provided. 
			if (AActor* Actor = CachedProvider->ProvideActor(OurWorld))
			{
				OnProvided.Broadcast(Actor);
				EndTask();
			}
		}
	}
}

void UAsyncAction_ProvideActor::OnProvideActor(AActor* ProvidedActor)
{
	if (ProvidedActor)
	{
		OnProvided.Broadcast(ProvidedActor);
	}
}

#undef LOCTEXT_NAMESPACE 