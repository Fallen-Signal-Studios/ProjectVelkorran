#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "GameFramework/Actor.h"
#include "Logging/LogVerbosity.h"
#include "VisualLogger/VisualLogger.h"

namespace FMassAgentTraitsHelper 
{
	template<typename T>
	T* AsComponent(UObject& Owner)
	{
		T* Component = nullptr;
		if (AActor* AsActor = Cast<AActor>(&Owner))
		{
			Component = AsActor->FindComponentByClass<T>();
		}
		else
		{
			Component = Cast<T>(&Owner);
		}

#if ENABLE_VISUAL_LOG
		UE_CVLOG_UELOG(Component == nullptr, &Owner, LogMass, Error, TEXT("Trying to extract %s from %s failed")
			, *T::StaticClass()->GetName(), *Owner.GetName());
#endif

		return Component;
	}
}