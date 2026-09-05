// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include <Runtime/AIModule/Classes/GenericTeamAgentInterface.h>
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include <GameplayTagContainer.h>
#include "Vehicles/NarrativeArsenalVehicleTypes.h"
#include "ZoneGraphTypes.h"
#include "Blueprint/UserWidget.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "Input/UIActionBindingHandle.h"
#include "MovieSceneSequence.h"
#include "Kismet/GameplayStatics.h"
#include "ZoneGraphTypes.h"
#include "../../../../Plugins/Runtime/CommonUI/Source/CommonInput/Public/CommonInputSubsystem.h"

#include "ArsenalStatics.generated.h"

struct FSpawnedEntities;
class UMassEntityConfigAsset;
class UChaosWheeledVehicleMovementComponent;
class ANarrativeWorldSettings;
enum class EPathFollowingReachMode : uint8;
class ULevelSequence;
enum class EEntityLOD : uint8;
class UMassAgentComponent;
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FObjectComparator, UObject*, A, UObject*, B, bool&, Result);

// define ot avoid needing to include header containing EPathFollowingReachMode, represents "EPathFollowingReachMode::OverlapAgent"
#define PATH_FOLLOW_REACH_MODE_OVERLAP_AGENT (EPathFollowingReachMode)1

//Used to expose VRAM stats to video settings menu or anything else that requires it 
USTRUCT(BlueprintType)
struct FGPUInfo
{
	GENERATED_BODY()

	/** Process-local memory budget in MiB; retained legacy name, not installed physical VRAM. */
	UPROPERTY(BlueprintReadOnly, Category = "GPU Info")
	int32 TotalVRAM = 0;

	/** Current process-local memory usage in MiB on the queried adapter. */
	UPROPERTY(BlueprintReadOnly, Category = "GPU Info")
	int32 CurrentVRAM = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GPU Info")
	FString GPUBrand = "";
};

/**
 * Useful BP exposed functions for Narrative Pro. 
 */
UCLASS()
class NARRATIVEARSENAL_API UArsenalStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	//Grab monitor names by iterating display metrics 
	UFUNCTION(BlueprintPure, Category = "GPU Info")
	static TArray<FString> GetMonitorNames();

	/** Windows primary-display adapter stats (not necessarily the render adapter). Clears OutInfo and returns false on failure. */
	UFUNCTION(BlueprintPure, Category = "GPU Info")
	static bool GetGPUInfo(FGPUInfo& OutInfo);

	//Grab a friendly display name for a given tag if one is defined in the project settings. 
	UFUNCTION(BlueprintPure, Category = "Utility")
	static bool GetGameplayTagFriendlyDisplayName(FGameplayTag Tag, FText& OutText);

	//Check if an actor is net startup or not, which isnt typically BP exposed. 
	UFUNCTION(BlueprintPure, Category = "Utility")
	static bool IsActorNetStartup(const AActor* TestActor);

	//Checks we're the same team, not just that we're friendly with each other.
	UFUNCTION(BlueprintPure, Category = "Teams")
	static bool IsSameTeam(const AActor* TestActor, const AActor* Target);

	UFUNCTION(BlueprintPure, Category = "Teams")
	static ETeamAttitude::Type GetAttitude(const AActor* TestActor, const AActor* Target);

	//Get the factions from the given actor
	UFUNCTION(BlueprintCallable, Category = "Teams")
	static FGameplayTagContainer GetActorFactions(AActor* Actor);

	//Put the actor in the given factions, provided it implements the team interface. 
	UFUNCTION(BlueprintCallable, Category = "Teams")
	static void AddFactionsToActor(AActor* Actor, UPARAM(meta = (Categories="Narrative.Factions"))const FGameplayTagContainer& Factions);

	//remove the actor from the given factions, provided it implements the team interface. 
	UFUNCTION(BlueprintCallable, Category = "Teams")
	static void RemoveFactionsFromActor(AActor* Actor, UPARAM(meta = (Categories="Narrative.Factions"))const FGameplayTagContainer& Factions);

	//Return the narrative pro settings with the values configured in your DefaultEngine.ini file.  
	UFUNCTION(BlueprintPure, Category = "Settings")
	static class UArsenalSettings* GetNarrativeProSettings();

	//Return the time of day settings with the values configured in your DefaultGame.ini file.  
	UFUNCTION(BlueprintPure, Category = "Settings")
	static class UNarrativeTimeOfDaySettings* GetTimeOfDaySettings();

	//BP getter for UI Settings.  
	UFUNCTION(BlueprintPure, Category = "Settings")
	static class UNarrativeUIDeveloperSettings* GetNarrativeUISettings();

	//BP getter for combat Settings. 
	UFUNCTION(BlueprintPure, Category = "Settings")
	static class UNarrativeCombatDeveloperSettings* GetCombatSettings();

	//Return the game default map name, this is typically the main menu. 
	UFUNCTION(BlueprintPure, Category = "Settings")
	static FName GetGameDefaultMapName();

	//Return the game entry map name defined in the Narrative Pro settings. 
	UFUNCTION(BlueprintPure, Category = "Settings")
	static FName GetGameEntryMapName();

	//Return the charactor creator map name defined in the Narrative Pro settings. 
	UFUNCTION(BlueprintPure, Category = "Settings")
	static FName GetCharacterCreatorMapName();

	//Return the narrative game user settings
	UFUNCTION(BlueprintPure, Category = "Settings")
	static class UNarrativeGameUserSettings* GetNarrativeGameUserSettings(); 

	//Return the current gameplay difficulty level
	UFUNCTION(BlueprintPure, Category = "Settings")
	static ENarrativeGameplayDifficulty GetGameplayDifficultyLevel();

	//Return the current screen resolution
	UFUNCTION(BlueprintPure, Category = "Resolution")
	static FVector2D GetGameResolution();

	//Given some text, replace any {Input.Interact} style inputs with their rich text platform specific icon equivalents, ie {Input.Attack} becomes <img id=Input.Xbox.Attack/>
	UFUNCTION(BlueprintPure, Category = "Wildcards")
	static FText ReplaceInputVariables(class ANarrativePlayerController* PC, FText TextToReplace);

	//Get the ingame time of day 
	UFUNCTION(BlueprintPure, Category = "Narrative", meta = (WorldContext = "WorldContextObject"))
	static float GetTimeOfDay(const UObject* WorldContextObject);

	//check if the time of day provided falls in the given range - handles looping over to the next day 
	UFUNCTION(BlueprintPure, Category = "Narrative")
	static bool IsTimeInRange(const float Time, const float RangeStart, const float RangeEnd);

	//Return true if it is currently day time.  
	UFUNCTION(BlueprintPure, Category = "Narrative", meta = (WorldContext = "WorldContextObject"))
	static bool IsDayTime(const UObject* WorldContextObject);

	//Get the ingame time of day, formatted as a string ie 16:35
	UFUNCTION(BlueprintPure, Category = "Narrative", meta = (WorldContext = "WorldContextObject"))
	static FString GetTimeOfDayAsString(const UObject* WorldContextObject);

	//Convert the float time into a 24 hour string
	UFUNCTION(BlueprintPure, Category = "Narrative", meta = (WorldContext = "WorldContextObject"))
	static FString TimeToString(const float Time);

	//Get the total accumulated time since the player started playing the game, where 2400 is one full ingame day. 
	UFUNCTION(BlueprintPure, Category = "Narrative", meta = (WorldContext = "WorldContextObject"))
	static float GetTotalAccumulatedTime(const UObject* WorldContextObject);

	//Quickly get the gameplay HUD from the local player. 
	UFUNCTION(BlueprintPure, Category = "Narrative", meta = (WorldContext = "WorldContextObject"))
	static class UNarrativeGameplayHUD* GetGameplayHUD(const UObject* WorldContextObject);
	//Add a notification to the Narrative HUD, provided one has been created 

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Narrative Common UI", meta = (DisplayName = "Show Narrative HUD Notification", WorldContext = "WorldContextObject"))
	static void PushHUDNotification(const UObject* WorldContextObject, FText Message, const float Duration = 5.f);

	//Add a major notification to the Narrative HUD, provided one has been created 
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Narrative Common UI", meta = (DisplayName = "Show Major Narrative HUD Notification", WorldContext = "WorldContextObject"))
	static void PushMajorHUDNotification(const UObject* WorldContextObject, FText Message, FText Subtitle, const float Duration = 5.f, const bool bOverrideCurrentNotification = true);

	//Get the narrative game state
	UFUNCTION(BlueprintPure, Category = "Narrative", meta = (WorldContext = "WorldContextObject"))
	static ANarrativeGameState* GetNarrativeGameState(const UObject* WorldContextObject);

	//Get the narrative game mode
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Narrative", meta = (WorldContext = "WorldContextObject"))
	static ANarrativeGameMode* GetNarrativeGameMode(const UObject* WorldContextObject);

	//Add a number of gameplay tags to our ASC. Can skip adding if we already have the tag. 
	UFUNCTION(BlueprintCallable, Category = "GAS")
	static bool AddLooseGameplayTagsCount(AActor* Actor, const FGameplayTagContainer& GameplayTags, int32 Count, const bool bDontAddIfAlreadyOwned);

	//Add a number of gameplay tags to our ASC. Can skip adding if we already have the tag. 
	UFUNCTION(BlueprintCallable, Category = "GAS")
	static bool RemoveLooseGameplayTagsCount(AActor* Actor, const FGameplayTagContainer& GameplayTags, int32 Count);

	//Sorts an array using a function so BP can easily sort object arrays!
	UFUNCTION(BlueprintPure, Category = "Utility")
	static TArray<UObject*> SortObjectArray_Comparator(const TArray<UObject*>& ObjectArray, FObjectComparator Comparator, const bool bReverse);

	UFUNCTION(BlueprintCallable, Category = "Navigation")
	static bool CheckPathExists(const FVector& PathStart, const FVector& PathEnd, AActor* PathfindingContext, TSubclassOf<UNavigationQueryFilter> FilterClass, const bool bUseFastCheck=true);

	//BP exposed version of this UWorld function that finds an adjust location given an actor 
	UFUNCTION(BlueprintCallable, Category = "Utility")
	static bool BPFindTeleportSpot(const AActor* TestActor, UPARAM(Ref) FVector& PlaceLocation, FRotator PlaceRotation );
	
	//BP exposed version of this UWorld function that tests if an actor would be blocked in a given spot. 
	UFUNCTION(BlueprintCallable, Category = "Utility")
	static bool BPEncroachingBlockingGeometry(const AActor* TestActor, FVector PlaceLocation, FRotator PlaceRotation );

	//Lets us check whether a given objects world type is editor or PIE etc 
	UFUNCTION(BlueprintPure, Category = "Navigation")
	static bool IsObjectInEditorViewportWorld(UObject* Object);

	//Lets us check whether we're currently with the editor, or false if we're in a packaged game
	UFUNCTION(BlueprintPure, Category = "Navigation")
	static bool IsWithEditor();

	//Test is the object is owned by an NPC with the specified definition
	UFUNCTION(BlueprintPure, Category = "Development", meta = (DevelopmentOnly))
	static bool IsObjectOwnedByNPC(const UObject* TestObject, class UNPCDefinition* NPCDefinition);

	//Resets remapped keybindings for the selected input device
	UFUNCTION(BlueprintCallable, Category = "Input")
	static void ResetMappingsForDevice(APlayerController* PlayerController, const ECommonInputType& InputType);

	// Simple move to location which also exposes reach test and acceptance radius for more precise movements. Similar functionality can be found in BP using async nodes.
	static FAIRequestID MoveToLocationExtended(AController* InController, const FVector& InDestination, bool bReachTestIncludesRadii = false, float AcceptanceRadius = -1, EPathFollowingReachMode ReachMode = PATH_FOLLOW_REACH_MODE_OVERLAP_AGENT);

	/** Tries to find a narrative ASC on the given actor.  */
	UFUNCTION(BlueprintPure, Category = Ability, Meta=(DefaultToSelf = "Actor"))
	static class UNarrativeAbilitySystemComponent* GetNarrativeAbilitySystemComponent(AActor* Actor);
	
	/**
	 * Finds a path using the zonegraph
	 * @param WorldContextObject Object used to find UWorld
	 * @param Start Start Posittion
	 * @param End Ending Position
	 * @param OutCurve Resulting path curve
	 * @param OutPathDistance Total distance of the path - can be used for querying the OutCurve
	 * @param SearchExtent The extent to search for nearby lanes from the start/end points
	 * @param AnyTags Filter passes if any of the 'AnyTags', and all of the 'AllTags', and none of the 'NotTags' are present.
	 * @param AllTags Filter passes if any of the 'AnyTags', and all of the 'AllTags', and none of the 'NotTags' are present.
	 * @param NotTags Filter passes if any of the 'AnyTags', and all of the 'AllTags', and none of the 'NotTags' are present.
	 * @return Whether a path was found
	 */
	UFUNCTION(BlueprintCallable, Category = "Navigation", meta = (AdvancedDisplay=4, AutoCreateRefTerm="AnyTags,AllTags,NotTags", WorldContext = "WorldContextObject"))
	static bool GetPathOnZoneGraph(const UObject* WorldContextObject, const FVector& Start,
	                               const FVector& End, FInterpCurveVector& OutCurve, float& OutPathDistance, const FVector SearchExtent = FVector(1000.f), const FZoneGraphTagMask& AnyTags = FZoneGraphTagMask(), const FZoneGraphTagMask& AllTags = FZoneGraphTagMask(), const FZoneGraphTagMask&
		                               NotTags = FZoneGraphTagMask());

	/**
	 * Attempts to find the nearest zonegraph lane location within the search extents
	 * @param WorldContextObject Object used to find UWorld
	 * @param Location The location to query for a zonegraph
	 * @param OutTransform If successful, returns the location and direction of the zongraph point found
	 * @param SearchExtent The search extent to find a zonegraph lane
	 * @param AnyTags Filter passes if any of the 'AnyTags', and all of the 'AllTags', and none of the 'NotTags' are present.
	 * @param AllTags Filter passes if any of the 'AnyTags', and all of the 'AllTags', and none of the 'NotTags' are present.
	 * @param NotTags Filter passes if any of the 'AnyTags', and all of the 'AllTags', and none of the 'NotTags' are present.
	 * @return Whether a zonegraph point could be found
	 */
	UFUNCTION(BlueprintCallable, Category = "Navigation", meta = (AdvancedDisplay=4, AutoCreateRefTerm="AnyTags,AllTags,NotTags", WorldContext = "WorldContextObject"))
	static bool GetPointOnZoneGraph(const UObject* WorldContextObject, const FVector& Location, FTransform&
	                                OutTransform, const FVector SearchExtent = FVector(1000.f), const FZoneGraphTagMask& AnyTags = FZoneGraphTagMask(), const FZoneGraphTagMask& AllTags = FZoneGraphTagMask(), const FZoneGraphTagMask&
		                                NotTags = FZoneGraphTagMask());
	
	/**
	 * Calculates the nearest key from the input Position
	 * @param Curve The vector curve to use
	 * @param Position Location to find the nearest key
	 * @return Nearest key value from Position
	 */
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	static float FindNearestKeyAlongCurve(const FInterpCurveVector& Curve, const FVector& Position);

	/**
	 * Evaluates the curve using the selected Key
	 * @param Curve The vector curve to use
	 * @param InKey The key used to evaluate the curve
	 * @param OutPosition Output vector from the associated key
	 * @param OutTangent Output derivative from the associated key
	 */
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	static void GetPositionAlongCurve(const FInterpCurveVector& Curve, float InKey, FVector& OutPosition,
	                                  FVector& OutTangent);

	/**Given a spline component, pull the distance and interp curve data from it - something BP can't natively do.
	Curve points are transformed into world space. */
	UFUNCTION(BlueprintCallable, Category = "Spline")
	static void GetCurveDataFromSplineComponent(class USplineComponent* SplineComp, FInterpCurveVector& OutCurve, float& OutDistance, bool bWorldSpace = true);

	// Releases a mass actor and applies the default controller to the actor
	UFUNCTION(BlueprintCallable, Category = "AI")
	static void ReleaseMassHandleOnActor(APawn* Pawn);
	
	/**
	 * Registers the actor with the mass system and turns it into a 'puppet'
	 * @param Actor The actor we want mass to manage
	 * @param EntityConfig The config we will apply to the created entity
	 * @param bAutoDestroy Whether we automatically destroy the entity when it reaches Off LOD
	 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	static void MakeActorMassPuppet(AActor* Actor, UMassEntityConfigAsset* EntityConfig, bool bAutoDestroy);
	
	/**
	 * Calculates the time to collision based on position, velocity, and size of agent/obstacle
	 * @param AgentLocation The agents world location
	 * @param AgentVelocity The current velocity of the agent
	 * @param AgentRadius The radius size of the agent
	 * @param ObstacleLocation The obstacle location we want to test for
	 * @param ObstacleVelocity the velocity of the obstacle
	 * @param ObstacleRadius The radius size of the obstacle
	 * @return The time till collision or TNumericLimits<float>::Max() if no collision is found
	 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	static float TimeUntilCollision(
		const FVector& AgentLocation, const FVector& AgentVelocity, float AgentRadius,
		const FVector& ObstacleLocation, const FVector& ObstacleVelocity, float ObstacleRadius);

	UFUNCTION(BlueprintCallable, Category = "Mass", meta = (WorldContext = "WorldContextObject"))
	static void GetEntityLOD(const UObject* WorldContextObject, const UMassAgentComponent* Agent, EEntityLOD& OutLOD);

	/**
	 * Spawns a single mass entity at the specified location
	 * @param ConfigAsset The config asset to use to spawn the entity
	 * @param Location The location to place the entity at
	 */
	UFUNCTION(BlueprintCallable, Category = "Mass", meta = (WorldContext = "WorldContextObject"))
	static FSpawnedEntities SpawnMassEntity(const UObject* WorldContextObject, UMassEntityConfigAsset* ConfigAsset,
	                                          FVector Location);

	/**
	 * Spawns mass entities at the specified locations. The # of entities spawned corresponds with the length of the locations array
	 * @param ConfigAsset The config asset to use to spawn the entities
	 * @param Locations The locations to place the entities at. The size of the array corresponds with the # of entities spawned
	 */
	UFUNCTION(BlueprintCallable, Category = "Mass", meta = (WorldContext = "WorldContextObject"))
	static FSpawnedEntities SpawnMassEntities(const UObject* WorldContextObject, UMassEntityConfigAsset* ConfigAsset,
	                                          TArray<FVector> Locations);

	/**
	 * Destroys mass entities contained within SpawnedEntities
	 * @param WorldContextObject The world context
	 * @param SpawnedEntities The entities that we want to destroy
	 */
	UFUNCTION(BlueprintCallable, Category = "Mass", meta = (WorldContext = "WorldContextObject"))
	static void DestroyEntities(const UObject* WorldContextObject, FSpawnedEntities SpawnedEntities);

	/**
	 * Determines whether mass controls the actor specified. This usually means the actor is used for visualization and its lifetime is controlled by a mass system.
	 * @param Actor The actor to test
	 * @return Whether the actor is controlled by mass
	 */
	UFUNCTION(BlueprintCallable, Category = "Mass")
	static bool IsControlledByMass(const AActor* Actor);

	/**
	 * Removes obstacle fragments for the specified actor. This means that other mass controlled entities will no longer consider this actor for avoidance.
	 * @param Actor The actor that we want to remove obstacle fragments from
	 */
	UFUNCTION(BlueprintCallable, Category = "Mass")
	static void RemoveObstacleFragments(AActor* Actor);

	// Gets the asset name for a given soft object path
	UFUNCTION(BlueprintPure, DisplayName = "Soft Object Path To Asset Name", Category = "Utilities", meta=(CompactNodeTitle = "->", BlueprintThreadSafe, BlueprintAutocast))
	static FString Conv_SoftObjPathToSoftObjRef(const FSoftObjectPath& SoftObjectPath);

	// Gets the asset name for a given soft object reference
	UFUNCTION(BlueprintPure, DisplayName="Soft Object Reference To Asset Name", Category = "Utilities", meta=(CompactNodeTitle = "->", BlueprintThreadSafe, BlueprintAutocast))
	static FString Conv_SoftObjRefToAssetName(TSoftObjectPtr<UObject> SoftObjectReference);

	// Gets the asset name for a given soft Class reference
	UFUNCTION(BlueprintPure, DisplayName="Soft Class Reference To Asset Name", Category = "Utilities", meta=(CompactNodeTitle = "->", BlueprintThreadSafe, BlueprintAutocast))
	static FString Conv_SoftClassRefToAssetName(TSoftClassPtr<UObject> SoftClassReference);

	UFUNCTION(BlueprintCallable, Category = "Painting", meta = (AutoCreateRefTerm = "Tint"))
	static void DrawDashedLine(UPARAM(ref) FPaintContext& Context, const TArray<FVector2f>& Points, const FLinearColor& Tint, float Thickness, float DashLengthPx);

	/**
	 * Determines the closest point in the Points array to Location
	 * @param Points The array of points to query
	 * @param Location The location to find the closest point to
	 * @return Index or INDEX_NONE of the point array of the closest point to Location
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative")
	static int GetClosestPoint(const TArray<FVector>& Points, const FVector& Location);

	/**Set the input mode to a given type - this will use CommonUIs action router. */
	UFUNCTION(BlueprintCallable, Category = "Narrative")
	static void SetNarrativeUIInputMode(const APlayerController* PlayerController, ECommonInputMode CommonInputMode);

	/**Set the input mode to a given type, will use an explicitly provided input config rather than constructing one.  */
	UFUNCTION(BlueprintCallable, Category = "Narrative")
	static void SetNarrativeUIInputModeExplicit(const APlayerController* PlayerController, const FUIInputConfig& InputConfig);

	/**Set the input mode to a given type, will use an explicitly provided input config rather than constructing one.  */
	UFUNCTION(BlueprintPure, Category = "Narrative")
	static bool IsWidgetInActiveRoot(const class UCommonActivatableWidget* Widget);

	/** Given an FKey, fetch the CommonUI brush icon to display in UI. */ 
	UFUNCTION(BlueprintPure, Category = "Narrative")
	static bool GetInputKeyBrush(const UCommonInputSubsystem* CommonInputSubsystem, FSlateBrush& OutBrush, FKey Key, ECommonInputType InputType, const FName GamepadName);

	/**
	 * get a transform from a tagged track from a given level sequence. 
	 * @param LevelSequence the sequence to look in.
	 * @param Tag the tagged name to look for.
	 * @param Transform the resulting transform, if any.
	 * @return true if found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Narrative")
	static bool GetStartTransformByTag(UMovieSceneSequence* LevelSequence, FName Tag, FTransform& Transform);

	// returns the narrative world settings
	UFUNCTION(BlueprintPure, Category="Narrative")
	static ANarrativeWorldSettings* GetNarrativeWorldSettings(const UObject* WorldContext);

	// returns the engine idle rotation speed (RPM)
	UFUNCTION(BlueprintPure, Category="ChaosWheeledVehicleMovement")
	static float GetEngineIdleRotationSpeed(UChaosWheeledVehicleMovementComponent* Target);

	// packs vehicle light info into a single float to be used with a vehicle body material
	UFUNCTION(BlueprintPure, Category="Narrative|Vehicles")
	static FPackedVehicleInstanceCustomData PackVehicleInstanceCustomData(FVehicleInstanceCustomData CustomData);
	
	/**
	* Modified engine function to use character capsule and check where we're going. 
	*/
	UFUNCTION(BlueprintCallable, Category = "Game", DisplayName="Predict Character Path", meta = (WorldContext = "WorldContextObject"))
	static bool PredictCharacterPath(const ACharacter* Character, const FPredictProjectilePathParams& PredictParams, FPredictProjectilePathResult& PredictResult);
	
	/**
	* Hash a unique GUID using a string. 
	*/
	UFUNCTION(BlueprintPure, Category = "Game", meta = (WorldContext = "WorldContextObject"))
	static FGuid CreateUniqueGUIDFromStringHash(const FString& String);

	
	/**
	* Check if a given point is inside a water volume. 
	*/
	UFUNCTION(BlueprintPure, Category = "Game", meta = (WorldContext = "WorldContextObject"))
	static bool IsPointInWaterVolume(const UObject* WorldContextObject, const FVector& Point);

	/**Narrative version of ApplyRadialDamageWithFalloff that instead of dealing damage to the found actors, will instead find actors with ASCs. TODO needs to use TargetData. */
	UFUNCTION(BlueprintCallable, Category = "Narrative")
	static bool ApplyRadialExplosionWithFalloff(const UObject* WorldContextObject, const FVector& Origin, float Radius, UCurveFloat* DamageCurve,
		TSubclassOf<class UGameplayEffect> ExplosionDamageEffect, const TArray<AActor*>& IgnoreActors, UNarrativeAbilitySystemComponent* DamageCauserASC, ECollisionChannel DamagePreventionChannel);

	/**
	 * Returns the narrative player character for the player controller at the specified player index, will return null if the pawn is not a character.
	 * This will not include characters of remote clients with no available player controller, you can iterate the PlayerStates list for that.
	 *
	 * @param PlayerIndex	Index in the player controller list, starting first with local players and then available remote ones
	 */
	UFUNCTION(BlueprintPure, Category="Game", meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
	static class ANarrativePlayerCharacter* GetNarrativePlayerCharacter(const UObject* WorldContextObject, int32 PlayerIndex);



};
