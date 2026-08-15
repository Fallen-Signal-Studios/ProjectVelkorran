// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "NarrativeCharacter.h"
#include "NarrativePlayerCharacter.generated.h"

/**
 * Base class for a player controlled Narrative Character. 
 */
UCLASS()
class NARRATIVEARSENAL_API ANarrativePlayerCharacter : public ANarrativeCharacter
{
	GENERATED_BODY()
	
public:

	//So GM can set player definition
	friend class ANarrativeGameMode;
	friend class ANarrativePlayerController; 

	ANarrativePlayerCharacter(const class FObjectInitializer& ObjectInitializer);
	
	//Returns the player controller, checking previouscontroller in case we're controlling a vehicle or something. 
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	class ANarrativePlayerController* GetPlayerController() const;

	//Returns the player state, checking previouscontroller in case we're controlling a vehicle or something. 
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	class ANarrativePlayerState* GetNarrativePlayerState() const;

	virtual AController* GetOwningController() const override;
	
protected:

	bool ASCInputBound = false;

	virtual void Tick(float DeltaTime) override; 
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override; 
	virtual void OnRep_PlayerState() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FGameplayTagContainer GetFactions() const override;
	virtual void AddFaction(const FGameplayTag& Faction) override;
	virtual void RemoveFaction(const FGameplayTag& Faction) override;
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	//Server uses this to set when client has informed its ready. For client this is if we've sent the notify RPC off. 
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	bool bClientNotifiedVisualReady=false;

	virtual bool IsPlayerControlled() const override;
	virtual bool IsBotControlled() const override;

	virtual FText GetCharacterName() const override;
	virtual void ApplyAppearance_Implementation(class UCharacterAppearance* DefaultAppearance) override;
	virtual TSubclassOf<class ANarrativeCharacterVisual> GetCharacterVisualClass_Implementation(class UCharacterAppearance* DefaultAppearance) const override;
	virtual void OnCharacterVisualInitialized() override;
	virtual class UNarrativeSaveWithCreatorData* GetCharacterCreatorData() const;
	
	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> MoveAction;
	
	/** The player definition for this character */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_PlayerDefinition, Category = NarrativeCharacter, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UPlayerDefinition> PlayerDefinition;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);
	void CompletedMove();

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual class UCharacterDefinition* GetCharacterDefinition() const override;
	virtual void SetPlayerDefinition(class UPlayerDefinition* PDef);

	UFUNCTION()
	virtual void OnRep_PlayerDefinition();

public:

	//Get the Player definition from the character
	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter")
	FORCEINLINE class UPlayerDefinition* GetPlayerDefinition() const {return PlayerDefinition;};

	//Return whether camera is inside the head - essentially, whether we're in First Person. We can be in first person mode
	virtual bool IsCameraInsideHead() const override;

	//Return whether the camera should follow the head bone location 
	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter", meta = (BlueprintThreadSafe))
	virtual bool ShouldCameraFollow3PHeadBoneLocation() const;

	//The local input vector
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Narrative")
	FVector2D MovementVector; 

protected:

	/** Required to grab the NPCController even if its not current possessing us, say because we're in a vehicle*/
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "NPC")
	ANarrativePlayerController* CachedController;
};
