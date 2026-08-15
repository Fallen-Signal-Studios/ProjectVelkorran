// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassAgentComponent.h"
#include "NarrativeMassAgentComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FIncomingCollision, const AActor*, Actor);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class NARRATIVEARSENAL_API UNarrativeMassAgentComponent : public UMassAgentComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UNarrativeMassAgentComponent();

protected:
	virtual void OnRegister() override;

	void AssignActorFragment(const UMassAgentComponent& MassAgentComponent);

	UPROPERTY(BlueprintAssignable, Category = "Incoming Collision")
	FIncomingCollision OnIncomingCollision;

public:
	void IncomingCollisionDetected(const FMassEntityHandle& EntityHandle);
};
