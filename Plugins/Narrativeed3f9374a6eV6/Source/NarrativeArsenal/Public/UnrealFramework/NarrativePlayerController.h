// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemInterface.h"
#include <GameplayTagAssetInterface.h>
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Tales/TalesComponent.h"
#include "Navigation/NavigationSubsystem.h"
#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "NarrativeCharacter.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "NarrativePlayerController.generated.h"

/**When an NPC spawn gets streamed out, it can optionally keep the NPC around by "tethering" it to our player controller.
That way we can save those NPCs to disk, even though their spawns are no longer streamed in to the world. */
USTRUCT(BlueprintType)
struct FNPCTether
{
	GENERATED_BODY()

	FNPCTether(){};

	//The currently spawned in NPC
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Tethered NPC")
	TObjectPtr<class ANarrativeNPCCharacter> NPCCharacter;

	//The NPC Definition for the NPC
	UPROPERTY(SaveGame, BlueprintReadOnly, VisibleAnywhere, Category = "Tethered NPC")
	TObjectPtr<class UNPCDefinition> NPCDef;

	//We need to store the NPCs save GUID so we can restore it after spawning the NPC, and their save record is fetched correctly. 
	UPROPERTY(SaveGame, BlueprintReadOnly, VisibleAnywhere, Category = "Tethered NPC")
	FGuid NPCSaveGUID;
};

//General
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCinematicEvent, ANarrativeLevelSequenceActor*, SequenceActor, const FNarrativeSequencePlaybackSettings&, InSettings);

/**
 * Base class for Player Controllers in Narrative Pro. Typically possesses an ANarrativePlayerCharacter. 
 */
UCLASS()
class NARRATIVEARSENAL_API ANarrativePlayerController : public APlayerController, 
	public INarrativeSavableActor, public IAbilitySystemInterface, public IGameplayTagAssetInterface, public INarrativeTeamAgentInterface, public INarrativeCharacterOwner
{
	GENERATED_BODY()
	
public:

	ANarrativePlayerController(const class FObjectInitializer& ObjectInitializer);

	//Interfaces 
	class UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual FGameplayTagContainer GetFactions() const override;
	virtual void AddFaction(const FGameplayTag& Faction) override;
	virtual void RemoveFaction(const FGameplayTag& Faction) override;
	ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	virtual void PrepareForSave_Implementation();
	virtual void Load_Implementation();

	virtual class ANarrativeCharacter* GetNarrativeCharacter() const override;

	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
	virtual bool IsLookInputIgnored() const override;
	virtual void PawnLeavingGame() override;

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;

	//POI marker calls this to perform fast travelling, which we implement in BP 
	UFUNCTION(BlueprintImplementableEvent, Category = "Narrative|Fast Travel")
	void FastTravelToPOI(const FPOIData& POI);

	//Basically just return the input device name in a way the narrative input icon data table understands. Keyboard, Xbox, PS5, etc. 
	UFUNCTION(BlueprintPure, Category = "Narrative|Input")
	FString GetNarrativeInputDeviceName() const;

	UFUNCTION(BlueprintPure, Category = "Narrative|Input")
	bool IsUsingGamepad() const;

	UFUNCTION(Client, Unreliable)
	virtual void NotifyDealtDamage(AActor* DamagedActor, const float DamageAmount);

	//Do whatever we like when we damage an actor, by default we put damage text up. 
	UFUNCTION(BlueprintImplementableEvent, Category = "Narrative|FX")
	void HandleDamageActor(AActor* DamagedActor, const float DamageAmount);

	//Associate the player controller with the given character. 
	virtual void SetOwnedCharacter(ANarrativePlayerCharacter* InCharacter);

	//Return the owned narrative char - will not be GetPawn() if we're in a car, on a mount, etc. - TODO see whether we want to always keep NChar possessed to avoid this. 
	UFUNCTION(BlueprintPure, Category = "Narrative")
	class ANarrativePlayerCharacter* GetOwnedCharacter() const;
	
	//Return the controlled narrative char
	UFUNCTION(BlueprintPure, Category = "Narrative")
	class ANarrativePlayerCharacter* GetControlledCharacter() const;

	//Ask the game mode to respawn our player. 
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter")
	virtual void TryRespawn();

	//Handle what respawning in supposed to do. This is quite game specific so you are expected to override this if you need to provide a more specific implementation. 
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	void HandleRespawn();
	virtual void HandleRespawn_Implementation();
	
	//Ask the game mode to respawn our player. 
	UFUNCTION(Server, Reliable, Category = "Narrative|NarrativeCharacter")
	virtual void ServerTryRespawn();
	
	/**When an NPC spawn gets streamed out, it can optionally keep the NPC around by "tethering" it to our player controller.
	That way we can save those NPCs to disk, even though their spawns are no longer streamed in to the world. This is important 
	for NPCs that need to wander away from their spawn, because they attacked us, or are following us for a quest, etc.*/
	virtual bool TetherNPC(ANarrativeNPCCharacter* NPCToTether);
	virtual bool UntetherNPC(ANarrativeNPCCharacter* NPCToUntether);
	virtual bool GetTether(const FGuid& NPCToCheckGUID, FNPCTether& OutTether) const;
	virtual void RespawnTethers();

	UFUNCTION()
	virtual void OnTetheredNPCDestroyed(AActor* DestroyedActor);

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Tethered NPC")
	TArray<FNPCTether> NPCTethers;

	UPROPERTY(SaveGame)
	FRotator SavedControlRotation;

	void AbilityInputPressed(FGameplayTag InputTag);
	void AbilityInputReleased(FGameplayTag InputTag);

protected:

	//Gives our playercontroller a chance to react to death.
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	void HandleDeath(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead);
	virtual void HandleDeath_Implementation(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead);

	virtual void BeginPlay() override; 
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void AutoManageActiveCameraTarget(AActor* SuggestedTarget) override; 
	virtual void OnRep_PlayerState() override;
	virtual void OnRep_Pawn() override;
	virtual void SetupInputComponent() override;
	virtual void SetCinematicMode(bool bInCinematicMode, bool bHidePlayer, bool bAffectsHUD, bool bAffectsMovement, bool bAffectsTurning) override;

	UFUNCTION()
	void HandleOwnedCharacterReady(ANarrativePlayerCharacter* ReadyCharacter);

	void RefreshGameplayReadiness();
	void EnsureGameplayHUDCreated();
	void RefreshGameplayMappingContext();
	
	/** Default MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputMappingContext> DefaultMappingContext;
	
	//The default abilities we grant the player! We also store their input mappings so player subclass can bind these 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilitySystem", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UNarrativeAbilityInputMapping> AbilityInputMappings;

	//This is the gameplay HUD class we'll create the HUD with. TODO consider moving to player definition to allow per def customization of this? 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative|UI")
	TSubclassOf<class UNarrativeGameplayHUD> GameplayHUDClass;

	UPROPERTY(BlueprintReadOnly, Category = "Narrative|UI")
	TObjectPtr<class UNarrativeGameplayHUD> GameplayHUD;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative|Interaction")
	TObjectPtr<class UPlayerInteractionComponent> InteractionComponent;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Transient, Category = "Narrative|Interaction")
	TObjectPtr<class UTalesComponent> TalesComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative|Navigation")
	TObjectPtr<class UNarrativeNavigationComponent> NavigationComponent;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> LookAction;

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:

	UFUNCTION(Client, Reliable, BlueprintCallable, Category = "Narrative|UI")
	virtual void ClientShowHUDNotification(const FText& Message, const float Duration);
	
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	FORCEINLINE class UNarrativeGameplayHUD* GetNarrativeGameplayHUD() const {return GameplayHUD;};
	 
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	FORCEINLINE class UTalesComponent* GetTalesComponent() const {return TalesComponent;};

	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	FORCEINLINE class UPlayerInteractionComponent* GetInteractionComponent() const {return InteractionComponent;};

public:

	virtual void LevelSequencePlayed(ANarrativeLevelSequenceActor* SequenceActor, const FNarrativeSequencePlaybackSettings& InSettings);
	virtual void LevelSequenceStopped(ANarrativeLevelSequenceActor* SequenceActor, const FNarrativeSequencePlaybackSettings& InSettings);

	UPROPERTY(BlueprintAssignable, Category = "Cinematics")
	FOnCinematicEvent OnLevelSequencePlay;

	UPROPERTY(BlueprintAssignable, Category = "Cinematics")
	FOnCinematicEvent OnLevelSequenceStop;

	TArray<TWeakObjectPtr<ANarrativeLevelSequenceActor>> CurrentSequences;

protected: 

	//We cache this because GetPawn() won't return our character if we started possessing a car, horse, etc. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative")
	TObjectPtr<class ANarrativePlayerCharacter> OwnedCharacter;

	bool bInputBindingsInstalled = false;
	bool bGameplayMappingContextApplied = false;

};
