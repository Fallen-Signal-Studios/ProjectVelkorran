// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include <GameplayTagContainer.h>
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "NarrativeActorProvider.generated.h"

//Allows blueprints to create instanced providers!  
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FInstancedActorProvider
{
	GENERATED_BODY()


	FInstancedActorProvider(){};

	//The instanced goal
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadOnly, Category = "Provider")
	TObjectPtr<UNarrativeActorProvider> Provider; 

};

//Allows blueprints to create instanced providers!  
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FInstancedTransformProvider
{
	GENERATED_BODY()

	FInstancedTransformProvider(){};

	//The instanced goal
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadOnly, Category = "Provider")
	TObjectPtr<UNarrativeTransformProvider> Provider; 

};

/**
 * Provides an actor/transform based on some passed in information. Similar to EQSContexts, but more general gameplay oriented. 
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, AutoExpandCategories = ("Provider"))
class NARRATIVEARSENAL_API UNarrativeProviderBase : public UObject
{
	GENERATED_BODY()

public:

	UNarrativeProviderBase();

	//Return some text describing what the actor provider is providing
	UFUNCTION(BlueprintPure, Category = "Provider")
	virtual FText GetDescription() const;
};

/**
 * Provides an actor/transform based on some passed in information. Similar to EQSContexts, but more general gameplay oriented. 
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, AutoExpandCategories = ("Provider"))
class NARRATIVEARSENAL_API UNarrativeTransformProvider : public UNarrativeProviderBase
{
	GENERATED_BODY()
	
public: 

	UNarrativeTransformProvider();

	// Allows the Object to get a valid UWorld from it's outer.
	virtual UWorld* GetWorld() const override
	{
		if (HasAllFlags(RF_ClassDefaultObject))
		{
			// If we are a CDO, we must return nullptr instead of calling Outer->GetWorld() to fool UObject::ImplementsGetWorld.
			return nullptr;
		}

		UObject* Outer = GetOuter();

		while (Outer)
		{
			UWorld* World = Outer->GetWorld();
			if (World)
			{
				return World;
			}

			Outer = Outer->GetOuter();
		}

		return nullptr;
	}

	//Provide a transform instead of an actor. By default uses ProvideActors() transform if you dont override this. 
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Provider", meta = (WorldContext = "WorldContextObject"))
	FTransform ProvideTransform(const UObject* WorldContextObject);
	virtual FTransform ProvideTransform_Implementation(const UObject* WorldContextObject);

	virtual FText GetDescription() const override;

};

//Called when a stable actor is spawned in
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProviderActorReady, class AActor*, Actor);


/**
 * Provides an actor/transform on top of the transform. Similar to EQSContexts, but more general gameplay oriented. 
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, AutoExpandCategories = ("Provider"))
class NARRATIVEARSENAL_API UNarrativeActorProvider : public UNarrativeTransformProvider
{
	GENERATED_BODY()
	
public: 

	UNarrativeActorProvider();

	// Allows the Object to get a valid UWorld from it's outer.
	virtual UWorld* GetWorld() const override
	{
		if (HasAllFlags(RF_ClassDefaultObject))
		{
			// If we are a CDO, we must return nullptr instead of calling Outer->GetWorld() to fool UObject::ImplementsGetWorld.
			return nullptr;
		}

		UObject* Outer = GetOuter();

		while (Outer)
		{
			UWorld* World = Outer->GetWorld();
			if (World)
			{
				return World;
			}

			Outer = Outer->GetOuter();
		}

		return nullptr;
	}

	//This delegate can be used in cases where the actor to provide is not immediately ready, perhaps because the actor isnt loaded in yet. 
	UPROPERTY(BlueprintAssignable, Category = "Provider")
	FOnProviderActorReady OnProviderActorReady; 

	//Provide the actor. 
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Provider", meta = (WorldContext = "WorldContextObject"))
	class AActor* ProvideActor(const UObject* WorldContextObject);
	virtual class AActor* ProvideActor_Implementation(const UObject* WorldContextObject);

	virtual FTransform ProvideTransform_Implementation(const UObject* WorldContextObject);

	virtual FText GetDescription() const override;

};

// Ask an actor provider to provide its actor - handles polling if the actor can't immediately be provided. 
// For event-based instead of polling provide a poll interval of -1. Then, make sure your provider calls the OnProvided delegate.
// Generally this is not really recommended as polling is pretty inexpensive especially with a reasonable poll time and is much simpler. 
UCLASS(MinimalAPI, BlueprintType, meta=(ExposedAsyncProxy="AsyncTask"))
class UAsyncAction_ProvideActor : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	UPROPERTY()
	UNarrativeActorProvider* CachedProvider;

	UPROPERTY()
	TWeakObjectPtr<UWorld> World;

	float PollInterval;

	FTimerHandle PollHandle; 

	// called once the actor is successfully provided. 
	UPROPERTY(BlueprintAssignable)
	FOnProviderActorReady OnProvided;
	
	// called if the provider fails 
	UPROPERTY(BlueprintAssignable)
	FOnProviderActorReady OnFailed;

public:

	/* UBlueprintAsyncActionBase */
	virtual void Activate() override; 
	virtual void SetReadyToDestroy() override;
	/* UBlueprintAsyncActionBase */

	/**
	 * Provide an actor using the given provider. This is async because sometimes the actor to provide isn't available yet, because it isn't spawned etc.  In that case we'll broadcast when it is. 
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly="true", Category="Providers", WorldContext = "WorldContextObject"))
	static UAsyncAction_ProvideActor* ProvideActor(const UObject* WorldContextObject, UNarrativeActorProvider* Provider, float PollInterval=1.f);

	// when called, sets the task to be ready to destroy, removing all bound events
	UFUNCTION(BlueprintCallable, Category="Provider")
	void EndTask() { SetReadyToDestroy(); }

protected:
	
	UFUNCTION()
	void PollActor();

	UFUNCTION()
	void OnProvideActor(class AActor* ProvidedActor);
	
};

/**
 * Provides an NPC
 */
UCLASS(Blueprintable, EditInlineNew, AutoExpandCategories = ("Provider"), meta = (DisplayName = "Find NPC"))
class NARRATIVEARSENAL_API UNarrativeActorProvider_NPC : public UNarrativeActorProvider
{
	GENERATED_BODY()
	
public:

	 UNarrativeActorProvider_NPC();

	 virtual class AActor* ProvideActor_Implementation(const UObject* WorldContextObject) override;
	 virtual FText GetDescription() const override;

	 //The NPC to provide 
	 UPROPERTY(EditAnywhere, Category = "Actor Provider")
	 TObjectPtr<class UNPCDefinition> NPCDefinition;
};

/**
 * Finds an actor in the world with the specified save GUID. 
 * 
 * Find by level reference is a lot simpler and more intuitive to use - you should use this instead if possible.
 */
UCLASS(Blueprintable, EditInlineNew, AutoExpandCategories = ("Provider"), meta = (DisplayName = "Find Actor By Save GUID"))
class NARRATIVEARSENAL_API UNarrativeActorProvider_GUIDLookup : public UNarrativeActorProvider
{
	GENERATED_BODY()
	
public:

	 UNarrativeActorProvider_GUIDLookup();

	 virtual class AActor* ProvideActor_Implementation(const UObject* WorldContextObject) override;
	 virtual FText GetDescription() const override;

	 //The GUID to lookup 
	 UPROPERTY(EditAnywhere, Category = "Actor Provider")
	 FGuid GUIDToLookup;
};

/**
 * Finds an actor in the world with the specified level reference.
 */
UCLASS(Blueprintable, EditInlineNew, AutoExpandCategories = ("Provider"), meta = (DisplayName = "Level Actor"))
class NARRATIVEARSENAL_API UNarrativeActorProvider_LevelReference : public UNarrativeActorProvider
{
	GENERATED_BODY()
	
public:

	 UNarrativeActorProvider_LevelReference();

	 virtual class AActor* ProvideActor_Implementation(const UObject* WorldContextObject) override;
	 virtual FText GetDescription() const override;

	UFUNCTION()
	void OnActorSpawned(class AActor* SpawnedActor);

	FDelegateHandle ActorSpawnedHandle;

	 //The actor reference 
	 UPROPERTY(EditAnywhere, Category = "Actor Provider")
	 TSoftObjectPtr<class AActor> SoftActorReference;
};

/**
 * Finds an actor in the world with the specified class
 */
UCLASS(Blueprintable, EditInlineNew, AutoExpandCategories = ("Provider"), meta = (DisplayName = "Actor of Class"))
class NARRATIVEARSENAL_API UNarrativeActorProvider_ActorOfClass : public UNarrativeActorProvider
{
	GENERATED_BODY()
	
public:

	 UNarrativeActorProvider_ActorOfClass();

	 virtual class AActor* ProvideActor_Implementation(const UObject* WorldContextObject) override;
	 virtual FText GetDescription() const override;

	 //The actor reference - TODO probably needs to be soft reffed
	 UPROPERTY(EditAnywhere, Category = "Actor Provider")
	 TSubclassOf<class AActor> ActorClassToFind;
};

/**
 * Finds an actor in the world with the specified level reference.
 */
UCLASS(Blueprintable, EditInlineNew, AutoExpandCategories = ("Provider"), meta = (DisplayName = "Point of Interest (Transform)"))
class NARRATIVEARSENAL_API UNarrativeTransformProvider_POI : public UNarrativeTransformProvider
{
	GENERATED_BODY()
	
public:

	 UNarrativeTransformProvider_POI();

	 virtual FTransform ProvideTransform_Implementation(const UObject* WorldContextObject) override;
	 virtual FText GetDescription() const override;

	/** The Point of interest we should find to return the transform of */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Point Of Interest", meta = (Categories = "Narrative.POIs"))
	FGameplayTag POITag;

};

/**
 * Provide a hardcoded transform to use. 
 */
UCLASS(Blueprintable, EditInlineNew, AutoExpandCategories = ("Provider"), meta = (DisplayName = "Specified Transform"))
class NARRATIVEARSENAL_API UNarrativeTransformProvider_SpecifiedTransform : public UNarrativeTransformProvider
{
	GENERATED_BODY()
	
public:

	 UNarrativeTransformProvider_SpecifiedTransform();

	 virtual FTransform ProvideTransform_Implementation(const UObject* WorldContextObject) override;

	/** The Point of interest we should find to return the transform of */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Point Of Interest")
	FTransform SpecifiedTransform;

};