// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableComponent.h"
#include "Engine/StreamableManager.h"
#include "NarrativeSavableActor.h"
#include "InteractableItemPickup.generated.h"

class UNarrativeItem;



USTRUCT(BlueprintType)
struct FPickupConfiguration
{
	GENERATED_BODY()

	FPickupConfiguration()
	{
		QuantityToGive = 1;
	};

	//Pickup item to grant
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Pickup")
	TSubclassOf<UNarrativeItem> PickupClass;

	//Amount of pickup to grant. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Pickup")
	int32 QuantityToGive;
};

UCLASS(Blueprintable)
class NARRATIVEARSENAL_API AItemPickup : public AActor, public INarrativeSavableActor
{
	GENERATED_BODY()


public:

	AItemPickup(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override; 
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; 

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#endif 

	void Load_Implementation() override;
	bool ShouldRespawn_Implementation() const override;
	void SetActorGUID_Implementation(const FGuid& SavedGUID) override;
	FGuid GetActorGUID_Implementation() const override;
	
	//Called by our interaction component when pickup is taken. Auth only.
	virtual bool TakePickup(class ANarrativeCharacter* Taker);

	//Set the item this pickup will grant on take. 
	UFUNCTION(BlueprintCallable, Category = "Interactable")
	virtual void SetPickup(const FPickupConfiguration& InPickupConfig);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* PickupMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class UPickupInteractable* PickupInteractable;

	FPickupConfiguration GetPickupConfig() const;

protected:

	//The item and quantity the pickup should grant - use SetPickup, dont set this directly. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interactable")

	FGuid PickupSaveGUID;

	//The item and quantity the pickup should grant - use SetPickup, dont set this directly. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_PickupConfig, SaveGame, Category = "Interactable")
	FPickupConfiguration PickupConfig;

	/**We store our async load requests in here*/
	TSharedPtr<FStreamableHandle> PickupMeshLoadHandle;

	//This is called when the pickup mesh data has been streamed in and ready to be set - BP can override this if they need something custom. 
	UFUNCTION(BlueprintNativeEvent, Category = "Pickup")
	void OnPickupDataReady(FPickupMeshData Data);
	virtual void OnPickupDataReady_Implementation(FPickupMeshData Data);

	UFUNCTION()
	void OnRep_PickupConfig();

	UFUNCTION(BlueprintNativeEvent, Category = "Pickup")
	void RefreshPickup(const FPickupConfiguration& InPickupConfig);
	virtual void RefreshPickup_Implementation(const FPickupConfiguration& InPickupConfig);
};


/**
 * Interactable component on the item pickup 
 */
UCLASS(within=ItemPickup)
class NARRATIVEARSENAL_API UPickupInteractable : public UNarrativeInteractableComponent
{
	GENERATED_BODY()
	
public:
	UPickupInteractable();

	virtual bool Interact(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp) override;
	virtual bool CanInteract_Implementation(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp, FText& OutErrorText);

};