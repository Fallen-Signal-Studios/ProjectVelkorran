// Copyright Narrative Tools 2024. 


#include "ArsenalStatics.h"
#include "ArsenalSettings.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include <LevelSequence.h>
#include <MovieScene.h>
#include "AI/NPCDefinition.h"
#include <Components/LODSyncComponent.h>
#include <UnrealEngine.h>
#include "UnrealFramework/NarrativePlayerController.h"
#include "Settings/NarrativeCombatDeveloperSettings.h"
#include "NarrativeUIDeveloperSettings.h"
#include "Settings/NarrativeTimeOfDaySettings.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativeGameMode.h"
#include "AI/Activities/NPCActivity.h"
#include <AbilitySystemBlueprintLibrary.h>
#include <AbilitySystemComponent.h>
#include <NavigationSystem.h>

#include "CommonInputTypeEnum.h"
#include "EnhancedInputSubsystems.h"
#include "PlayerMappableKeySettings.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "AI/NarrativeNPCController.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "Widgets/NarrativeGameplayHUD.h"
#include "AbilitySystemGlobals.h"
#include "MassActorSubsystem.h"
#include "MassAgentComponent.h"
#include "MassAgentSubsystem.h"
#include "MassEntitySpawnDataGeneratorBase.h"
#include "MassEntityView.h"
#include "MassNavigationFragments.h"
#include "MassRepresentationFragments.h"
#include "MassSpawnerSubsystem.h"
#include "MassSpawnLocationProcessor.h"
#include "NarrativeWorldSettings.h"
#include "ZoneGraphAStar.h"
#include "ZoneGraphSubsystem.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "AI/Mass/EntityLOD.h"
#include "Slate/SlateBrushAsset.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Misc/UObjectToken.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "ZoneGraphTypes.h"
#include "AI/Mass/Peds/EntityData.h"
#include "ChaosVehicles/Public/ChaosWheeledVehicleMovementComponent.h"
#include "Engine/OverlapResult.h"
#include "Translators/MassCharacterMovementTranslators.h"
#include "Vehicles/Mass/VehicleFragments.h"
#include "Components/SplineComponent.h"
#include "Vehicles/NarrativeVehicleBase.h"
#include "Vehicles/Mass/VehicleRepresentationSubsystem.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "GameMapsSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogArsenalStatics, Log, All)

bool UArsenalStatics::GetGameplayTagFriendlyDisplayName(FGameplayTag Tag, FText& OutText)
{
	if (UArsenalSettings* Settings = GetMutableDefault<UArsenalSettings>())
	{
		if(Settings->TagFriendlyDisplayNames.Contains(Tag))
		{
			OutText = Settings->TagFriendlyDisplayNames[Tag];
			return true; 
		}	
	}

	return false; 
}

bool UArsenalStatics::IsActorNetStartup(const AActor* TestActor)
{
	return TestActor ? TestActor->IsNetStartupActor() : false; 
}

bool UArsenalStatics::IsSameTeam(const AActor* A, const AActor* B)
{
	if (A && B)
	{
		const INarrativeTeamAgentInterface* ATeam = Cast<const INarrativeTeamAgentInterface>(A);
		const INarrativeTeamAgentInterface* BTeam = Cast<const INarrativeTeamAgentInterface>(B);

		if (ATeam && BTeam)
		{
			return ATeam->GetFactions().HasAny(BTeam->GetFactions());
		}
	}

	return false;
}

ETeamAttitude::Type UArsenalStatics::GetAttitude(const AActor* TestActor, const AActor* Target)
{
	if (TestActor && Target)
	{
		if (const INarrativeTeamAgentInterface* TestActorTeam = Cast<const INarrativeTeamAgentInterface>(TestActor))
		{
			if (TestActorTeam)
			{
				return TestActorTeam->GetTeamAttitudeTowards(*Target);
			}
		}
	}

	return ETeamAttitude::Neutral;
}

FGameplayTagContainer UArsenalStatics::GetActorFactions(AActor* Actor)
{
	if (Actor)
	{
		if (INarrativeTeamAgentInterface* ActorTeam = Cast<INarrativeTeamAgentInterface>(Actor))
		{
			return ActorTeam->GetFactions();
		}
	}

	return FGameplayTagContainer();
}

void UArsenalStatics::AddFactionsToActor(AActor* Actor, const FGameplayTagContainer& Factions)
{
	if (Actor)
	{
		if (INarrativeTeamAgentInterface* ActorTeam = Cast<INarrativeTeamAgentInterface>(Actor))
		{
			TArray<FGameplayTag> FactionsTags;
			Factions.GetGameplayTagArray(FactionsTags);

			for (auto& T : FactionsTags)
			{			
				ActorTeam->AddFaction(T);
			}
		}
	}
}

void UArsenalStatics::RemoveFactionsFromActor(AActor* Actor, const FGameplayTagContainer& Factions)
{
	if (Actor)
	{
		if (INarrativeTeamAgentInterface* ActorTeam = Cast<INarrativeTeamAgentInterface>(Actor))
		{
			TArray<FGameplayTag> FactionsTags;
			Factions.GetGameplayTagArray(FactionsTags);

			for (auto& T : FactionsTags)
			{			
				ActorTeam->RemoveFaction(T);
			}
		}
	}
}

class UArsenalSettings* UArsenalStatics::GetNarrativeProSettings()
{

	if (UArsenalSettings* Settings = GetMutableDefault<UArsenalSettings>())
	{
		return Settings;
	}

	return nullptr; 
}

class UNarrativeTimeOfDaySettings* UArsenalStatics::GetTimeOfDaySettings()
{
	if (UNarrativeTimeOfDaySettings* Settings = GetMutableDefault<UNarrativeTimeOfDaySettings>())
	{
		return Settings;
	}

	return nullptr; 
}

class UNarrativeUIDeveloperSettings* UArsenalStatics::GetNarrativeUISettings()
{
	if (UNarrativeUIDeveloperSettings* Settings = GetMutableDefault<UNarrativeUIDeveloperSettings>())
	{
		return Settings;
	}

	return nullptr; 
}

class UNarrativeCombatDeveloperSettings* UArsenalStatics::GetCombatSettings()
{
	if (UNarrativeCombatDeveloperSettings* Settings = GetMutableDefault<UNarrativeCombatDeveloperSettings>())
	{
		return Settings;
	}

	return nullptr; 
}

FName UArsenalStatics::GetGameDefaultMapName()
{
	return FName(UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Default));
}

FName UArsenalStatics::GetGameEntryMapName()
{
	if (UArsenalSettings* Settings = GetMutableDefault<UArsenalSettings>())
	{
		return Settings->GameEntryMap.GetLongPackageFName();
	}

	return FName();
}

FName UArsenalStatics::GetCharacterCreatorMapName()
{
	if (UArsenalSettings* Settings = GetMutableDefault<UArsenalSettings>())
	{
		return Settings->CharacterCreatorMap.GetLongPackageFName();
	}

	return FName();
}

class UNarrativeGameUserSettings* UArsenalStatics::GetNarrativeGameUserSettings()
{
	return Cast<UNarrativeGameUserSettings>(UGameUserSettings::GetGameUserSettings());
}

ENarrativeGameplayDifficulty UArsenalStatics::GetGameplayDifficultyLevel()
{
	if (UNarrativeGameUserSettings* GUS = GetNarrativeGameUserSettings())
	{
		return GUS->GetGameplayDifficulty();
	}
	return ENarrativeGameplayDifficulty::Easy;
}

FVector2D UArsenalStatics::GetGameResolution()
{
	FVector2D Result = FVector2D( 1, 1 );

	Result.X = GSystemResolution.ResX;
	Result.Y = GSystemResolution.ResY;

	return Result;
}

FText UArsenalStatics::ReplaceInputVariables(class ANarrativePlayerController* PC, FText TextToReplace)
{
	if (PC)
	{
		//Replace variables in dialogue line
		FString LineString = TextToReplace.ToString();

		int32 OpenBraceIdx = -1;
		int32 CloseBraceIdx = -1;
		bool bFoundOpenBrace = LineString.FindChar('{', OpenBraceIdx);
		bool bFoundCloseBrace = LineString.FindChar('}', CloseBraceIdx);
		uint32 Iters = 0; // More than 50 wildcard replaces and something has probably gone wrong, so safeguard against that

		while (bFoundOpenBrace && bFoundCloseBrace && OpenBraceIdx < CloseBraceIdx && Iters < 50)
		{
			const FString VariableName = LineString.Mid(OpenBraceIdx + 1, CloseBraceIdx - OpenBraceIdx - 1);
			FString VariableVal = VariableName;

			FString L, R;

			//In Narrative Pro you can do {Input.Attack} and this should be replaced with the platform specific rich text ie <img id="Input.Xbox.Attack"/>
			if (VariableName.Split(".", &L, &R))
			{
				if (L.Equals("Input", ESearchCase::IgnoreCase))
				{
					const FString InputDeviceName = PC->GetNarrativeInputDeviceName();

					if (!InputDeviceName.IsEmpty())
					{
						VariableVal = FString::Printf(TEXT("<img id=\"Input.%s.%s\"/>"), *InputDeviceName, *R);
					}

					if (!VariableVal.IsEmpty())
					{
						LineString.RemoveAt(OpenBraceIdx, CloseBraceIdx - OpenBraceIdx + 1);
						LineString.InsertAt(OpenBraceIdx, VariableVal);
					}
				}
			}

			bFoundOpenBrace = LineString.FindChar('{', OpenBraceIdx);
			bFoundCloseBrace = LineString.FindChar('}', CloseBraceIdx);

			Iters++;
		}

		if (Iters > 0)
		{
			return FText::FromString(LineString);
		}
	}


	return TextToReplace;
}

float UArsenalStatics::GetTimeOfDay(const UObject* WorldContextObject)
{
	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		if (ANarrativeGameState* NarrativeGameState = World->GetGameState<ANarrativeGameState>())
		{
			return NarrativeGameState->GetTimeOfDay();
		}
	}
	return 0.f; 
}

bool UArsenalStatics::IsTimeInRange(const float Time, const float RangeStart, const float RangeEnd)
{
	//Time is on same day 
	if (RangeStart <= RangeEnd)
	{
		return Time >= RangeStart && Time <= RangeEnd;
	}
	else // time spans into next day 
	{ 
		return Time >= RangeStart || Time <= RangeEnd;
	}
}

bool UArsenalStatics::IsDayTime(const UObject* WorldContextObject)
{
	if (UNarrativeTimeOfDaySettings* TODSettings = GetTimeOfDaySettings())
	{
		return IsTimeInRange(GetTimeOfDay(WorldContextObject), TODSettings->SunriseTime, TODSettings->SunsetTime);
	}

	return true; 
}

FString UArsenalStatics::GetTimeOfDayAsString(const UObject* WorldContextObject)
{
	const float Time = UArsenalStatics::GetTimeOfDay(WorldContextObject);

	return TimeToString(Time);
}

FString UArsenalStatics::TimeToString(const float InTime)
{
	float Time = InTime;

	if (Time > 2400.f)
	{
		Time = FMath::Fmod(InTime, 2400.f);
	}

	const FString Hour = FString::FromInt(FMath::FloorToInt(Time / 100.f)); 
	const int32 MinuteInt = FMath::TruncToInt((FMath::Fmod(Time, 100.f) / 100.f) * 60.f);
	const FString Minute = MinuteInt <= 9 ? "0" + FString::FromInt(MinuteInt) : FString::FromInt(MinuteInt);

	return FString::Printf(TEXT("%s:%s"), *Hour, *Minute);
}

float UArsenalStatics::GetTotalAccumulatedTime(const UObject* WorldContextObject)
{
	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		if (ANarrativeGameState* NarrativeGameState = World->GetGameState<ANarrativeGameState>())
		{
			return NarrativeGameState->GetAccumulatedTime();
		}
	}
	return 0.f; 
}

class UNarrativeGameplayHUD* UArsenalStatics::GetGameplayHUD(const UObject* WorldContextObject)
{
	if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(UGameplayStatics::GetPlayerController(WorldContextObject, 0)))
	{
		return PC->GetNarrativeGameplayHUD();
	}

	return nullptr; 
}

void UArsenalStatics::PushHUDNotification(const UObject* WorldContextObject, FText Message, const float Duration /*= 5.f*/)
{
	if (UNarrativeGameplayHUD* HUD = UArsenalStatics::GetGameplayHUD(WorldContextObject))
	{
		HUD->ShowNotification(Message, Duration);
	}
}

void UArsenalStatics::PushMajorHUDNotification(const UObject* WorldContextObject, FText Message, FText Subtitle, const float Duration /*= 5.f*/, const bool bOverrideCurrentNotification /*= true*/)
{
	if (UNarrativeGameplayHUD* HUD = UArsenalStatics::GetGameplayHUD(WorldContextObject))
	{
		HUD->ShowMajorNotification(Message, Subtitle, Duration, bOverrideCurrentNotification);
	}
}

ANarrativeGameState* UArsenalStatics::GetNarrativeGameState(const UObject* WorldContextObject)
{
	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		return World->GetGameState<ANarrativeGameState>();
	}

	return nullptr; 
}

ANarrativeGameMode* UArsenalStatics::GetNarrativeGameMode(const UObject* WorldContextObject)
{
	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		return World->GetAuthGameMode<ANarrativeGameMode>();
	}

	return nullptr;
}

bool UArsenalStatics::AddLooseGameplayTagsCount(AActor* Actor, const FGameplayTagContainer& GameplayTags, int32 Count, const bool bDontAddIfAlreadyOwned)
{
	if (UAbilitySystemComponent* AbilitySysComp = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor))
	{
		if (!bDontAddIfAlreadyOwned || !AbilitySysComp->HasAnyMatchingGameplayTags(GameplayTags))
		{
			AbilitySysComp->AddLooseGameplayTags(GameplayTags, Count);

			return true;
		}
	}

	return false;
}

bool UArsenalStatics::RemoveLooseGameplayTagsCount(AActor* Actor, const FGameplayTagContainer& GameplayTags, int32 Count)
{
	if (UAbilitySystemComponent* AbilitySysComp = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor))
	{
		//Epics API now throws an annoying error unless you manually check the tag exists in the container first. Hopefully that will be removed at some point. 
		if (AbilitySysComp->HasAnyMatchingGameplayTags(GameplayTags))
		{
			AbilitySysComp->RemoveLooseGameplayTags(GameplayTags, Count);
		}

		return true;
	}

	return false;
}

TArray<UObject*> UArsenalStatics::SortObjectArray_Comparator(const TArray<UObject*>& ObjectArray, FObjectComparator Comparator, const bool bReverse)
{
	TArray<UObject*> Array = ObjectArray;
	if (!bReverse)
	{
		Array.Sort([Comparator](UObject& A, UObject& B)
		{
			bool Result = false;
			Comparator.Execute(&A, &B, Result);
			return Result;
		});
	}
	else
	{
		Array.Sort([Comparator](UObject& A, UObject& B)
		{
			bool Result = false;
			Comparator.Execute(&A, &B, Result);
			return !Result;
		});
	}
	return Array;
}

bool UArsenalStatics::CheckPathExists(const FVector& PathStart, const FVector& PathEnd, AActor* PathfindingContext, TSubclassOf<UNavigationQueryFilter> FilterClass, const bool bUseFastCheck/*=true*/)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(PathfindingContext, EGetWorldErrorMode::LogAndReturnNull);

	if (World)
	{
		const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

		if (NavSys != nullptr && NavSys->GetDefaultNavDataInstance() != nullptr)
		{
			bool bValidPathContext = false;
			const ANavigationData* NavigationData = nullptr;

			if (PathfindingContext != nullptr)
			{
				INavAgentInterface* NavAgent = Cast<INavAgentInterface>(PathfindingContext);
			
				if (NavAgent != nullptr)
				{
					const FNavAgentProperties& AgentProps = NavAgent->GetNavAgentPropertiesRef();
					NavigationData = NavSys->GetNavDataForProps(AgentProps, PathStart);
					bValidPathContext = true;
				}
				else if (Cast<ANavigationData>(PathfindingContext))
				{
					NavigationData = (ANavigationData*)PathfindingContext;
					bValidPathContext = true;
				}
			}
			if (bValidPathContext == false)
			{
				// just use default
				NavigationData = NavSys->GetDefaultNavDataInstance();
			}

			check(NavigationData);

			const FPathFindingQuery Query(PathfindingContext, *NavigationData, PathStart, PathEnd, UNavigationQueryFilter::GetQueryFilter(*NavigationData, PathfindingContext, FilterClass));

			return NavSys->TestPathSync(Query, bUseFastCheck ? EPathFindingMode::Hierarchical : EPathFindingMode::Regular);
		}
	}

	return false; 
}

bool UArsenalStatics::BPFindTeleportSpot(const AActor* TestActor, FVector& PlaceLocation, FRotator PlaceRotation)
{
	if (UWorld* World = TestActor->GetWorld())
	{
		return World->FindTeleportSpot(TestActor, PlaceLocation, PlaceRotation);
	}

	return false; 
}

bool UArsenalStatics::BPEncroachingBlockingGeometry(const AActor* TestActor, FVector PlaceLocation, FRotator PlaceRotation)
{
	if (UWorld* World = TestActor->GetWorld())
	{
		return World->EncroachingBlockingGeometry(TestActor, PlaceLocation, PlaceRotation);
	}

	return false;
}

bool UArsenalStatics::IsObjectInEditorViewportWorld(UObject* Object)
{
	if (!Object)
	{
		return false;
	}

	if (UWorld* World = Object->GetWorld())
	{
		return World->WorldType == EWorldType::Editor ||  World->WorldType == EWorldType::EditorPreview;
	}

	return false; 
}

bool UArsenalStatics::IsWithEditor()
{
#if WITH_EDITOR
	return true;
#else
	return false;
#endif
}

bool UArsenalStatics::IsObjectOwnedByNPC(const UObject* TestObject, class UNPCDefinition* NPCDefinition)
{
	if (TestObject)
	{
		if (const ANarrativeNPCController* NPCController = Cast<ANarrativeNPCController>(TestObject))
		{
			return NPCController->GetNPCData() == NPCDefinition;
		}

		const UObject* Outer = TestObject->GetOuter();

		while (IsValid(Outer))
		{
			if (const ANarrativeNPCController* NPCController = Cast<ANarrativeNPCController>(Outer))
			{
				return NPCController->GetNPCData() == NPCDefinition;
			}

			Outer = Outer->GetOuter();
		}
	}

	return false; 
}

void UArsenalStatics::ResetMappingsForDevice(APlayerController* PlayerController, const ECommonInputType& InputType)
{
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
	check(InputSubsystem);
	
	auto UserSettings = InputSubsystem->GetUserSettings();
	auto KeyProfile = UserSettings ? UserSettings->GetActiveKeyProfile() : nullptr;
	if (!KeyProfile) { return; }

	// Fetch mapping names
	const TMap<FName, FKeyMappingRow>& KeyMap = KeyProfile->GetPlayerMappingRows();

	TArray<FName> Mappings;
	KeyMap.GenerateKeyArray(Mappings);

	for (const FName& Mapping : Mappings)
	{
		// We need a ref to the mapping so we retrieve it again here
		if (auto KeyMappingRow = KeyProfile->FindKeyMappingRowMutable(Mapping))
		{
			for (FPlayerKeyMapping& KeyMapping : KeyMappingRow->Mappings)
			{
				// Only reset associated bindings
				if (KeyMapping.GetDefaultKey().IsGamepadKey() == (InputType == ECommonInputType::Gamepad))
				{
					KeyMapping.ResetToDefault();
				}
			}
		}
	}
}

FAIRequestID UArsenalStatics::MoveToLocationExtended(AController* InController, const FVector& InDestination, bool bReachTestIncludesRadii, float AcceptanceRadius, EPathFollowingReachMode ReachMode)
{
	UNavigationSystemV1* NavSys = InController ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(InController->GetWorld()) : nullptr;
	if (NavSys == nullptr || InController == nullptr || InController->GetPawn() == nullptr)
	{
		UE_LOG(LogArsenalStatics, Warning, TEXT("MoveToLocationAndWait called for NavSys:%s Controller:%s controlling Pawn:%s (if any of these is None then there's your problem"),
			*GetNameSafe(NavSys), *GetNameSafe(InController), InController ? *GetNameSafe(InController->GetPawn()) : TEXT("NULL"));
		return FAIRequestID::InvalidRequest;
	}

	// We need to initialize the PFComp beforehand to catch any early exits within SimpleMoveToLocation
	auto PFComp = InController->FindComponentByClass<UPathFollowingComponent>();
	if (PFComp == nullptr)
	{
		PFComp = NewObject<UPathFollowingComponent>(InController);
		PFComp->RegisterComponentWithWorld(InController->GetWorld());
		PFComp->Initialize();
	}

	if (PFComp == nullptr)
	{
		FMessageLog("PIE").Warning(FText::Format(
			FText::FromString("MoveToLocationAndWait failed for {0}: missing components"),
			FText::FromName(InController->GetFName())
		));
		return FAIRequestID::InvalidRequest;
	}

	if (!PFComp->IsPathFollowingAllowed())
	{
		FMessageLog("PIE").Warning(FText::Format(
			FText::FromString("MoveToLocationAndWait failed for {0}: movement not allowed"),
			FText::FromName(InController->GetFName())
		));
		return FAIRequestID::InvalidRequest;
	}

	const bool bAlreadyAtGoal = PFComp->HasReached(InDestination, ReachMode);

	// script source, keep only one move request at time
	if (PFComp->GetStatus() != EPathFollowingStatus::Idle)
	{
		PFComp->AbortMove(*NavSys, FPathFollowingResultFlags::ForcedScript | FPathFollowingResultFlags::NewRequest
			, FAIRequestID::AnyRequest, bAlreadyAtGoal ? EPathFollowingVelocityMode::Reset : EPathFollowingVelocityMode::Keep);
	}

	// script source, keep only one move request at time
	if (PFComp->GetStatus() != EPathFollowingStatus::Idle)
	{
		PFComp->AbortMove(*NavSys, FPathFollowingResultFlags::ForcedScript | FPathFollowingResultFlags::NewRequest);
	}

	if (bAlreadyAtGoal)
	{
		return PFComp->RequestMoveWithImmediateFinish(EPathFollowingResult::Success);
	}
	else
	{
		const FVector AgentNavLocation = InController->GetNavAgentLocation();
		const ANavigationData* NavData = NavSys->GetNavDataForProps(InController->GetNavAgentPropertiesRef(), AgentNavLocation);
		if (NavData)
		{
			FPathFindingQuery Query(InController, *NavData, AgentNavLocation, InDestination);
			FPathFindingResult Result = NavSys->FindPathSync(Query);
			if (Result.IsSuccessful())
			{
				auto MoveRequest = FAIMoveRequest(InDestination);
				MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
				MoveRequest.SetReachTestIncludesAgentRadius(bReachTestIncludesRadii);
				MoveRequest.SetReachTestIncludesGoalRadius(bReachTestIncludesRadii);
				
				return PFComp->RequestMove(MoveRequest, Result.Path);
			}
			else if (PFComp->GetStatus() != EPathFollowingStatus::Idle)
			{
				return PFComp->RequestMoveWithImmediateFinish(EPathFollowingResult::Invalid);
			}
		}
	}
	return FAIRequestID::InvalidRequest;
}

bool UArsenalStatics::GetPathOnZoneGraph(const UObject* WorldContextObject, const FVector& Start,
                                         const FVector& End, FInterpCurveVector& OutCurve, float& OutPathDistance, const FVector SearchExtent, const FZoneGraphTagMask& AnyTags, const FZoneGraphTagMask& AllTags, const FZoneGraphTagMask& NotTags)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	auto ZoneGraph = World ? World->GetSubsystem<UZoneGraphSubsystem>() : nullptr;
	
	if (!ZoneGraph)
	{
		return false;
	}

	FZoneGraphLaneLocation StartLane;
	FZoneGraphLaneLocation EndLane;

	// ZoneGraphTagFilter is not supported in BP. Exposed AnyTags for basic functionality.
	FZoneGraphTagFilter Filter;
	Filter.AllTags = AllTags;
	Filter.AnyTags = AnyTags;
	Filter.NotTags = NotTags;

	float DistanceSqr = 0.f;
	ZoneGraph->FindNearestLane(FBox::BuildAABB(Start, SearchExtent), Filter, StartLane, DistanceSqr);
	ZoneGraph->FindNearestLane(FBox::BuildAABB(End, SearchExtent), Filter, EndLane, DistanceSqr);

	UE_VLOG_LOCATION(WorldContextObject, LogArsenalStatics, Verbose, StartLane.Position, 20.f, FColor::Green, TEXT("Start Location Lane"));
	UE_VLOG_LOCATION(WorldContextObject, LogArsenalStatics, Verbose, EndLane.Position, 20.f, FColor::Green, TEXT("End Location Lane"));

	if (!StartLane.IsValid() || !EndLane.IsValid())
	{
		UE_LOG(LogArsenalStatics, Error, TEXT("StartLane/EndLane not valid! Unable to calculate path on zone graph. Ensure search extents/points are near a zonegraph."))
		return false;
	}
	
	if (const AZoneGraphData* Data = ZoneGraph->GetZoneGraphData(StartLane.LaneHandle.DataHandle))
	{
		const FZoneGraphStorage& ZoneGraphStorage = Data->GetStorage();
		FZoneGraphAStarWrapper Graph(ZoneGraphStorage);
		FZoneGraphAStar Pathfinder(Graph);
		
		FZoneGraphAStarNode StartNode(StartLane.LaneHandle.Index, StartLane.Position);
		FZoneGraphAStarNode EndNode(EndLane.LaneHandle.Index, EndLane.Position);
		FZoneGraphPathFilter PathFilter(ZoneGraphStorage, StartLane, EndLane, Filter);
		
		TArray<FZoneGraphAStarWrapper::FNodeRef> ResultPath;
		EGraphAStarResult Result = Pathfinder.FindPath(StartNode, EndNode, PathFilter, ResultPath);
		
		if (Result == SearchSuccess)
		{
			// If start/end lane indexes are the same, AStar returns early without a path. We need to create a path ourselves in this case
			if (ResultPath.IsEmpty())
			{
				ResultPath.Emplace(StartLane.LaneHandle.Index);
			}
			
			FZoneGraphLanePath LanePath;
			
			//Store the resulting lanes
			LanePath.Reset(ResultPath.Num());

			LanePath.StartLaneLocation = StartLane;
			LanePath.EndLaneLocation = EndLane;
			for (FZoneGraphAStarWrapper::FNodeRef Node : ResultPath)
			{
				LanePath.Add(FZoneGraphLaneHandle(Node, StartLane.LaneHandle.DataHandle));
			}

			TArray<FVector> Path;
			float AccumulatedDistance = 0.f;

			// Add start point to the curve
			{
				const FVector Point = StartLane.Position;
				Path.Emplace(Point);

				if (Path.Num() > 1)
				{
					AccumulatedDistance += FVector::Dist(Path[Path.Num()-2], Point);
				}
						
				int Ind = OutCurve.AddPoint(AccumulatedDistance, Point);
				OutCurve.Points[Ind].InterpMode = CIM_CurveAuto;
			}

			// Iterate through lane path
			for (int CurrLaneIndex = 0; CurrLaneIndex < LanePath.Lanes.Num(); CurrLaneIndex++)
			{
				const FZoneGraphLaneHandle& LaneHandle = LanePath.Lanes[CurrLaneIndex];

				const FZoneLaneData& Lane = ZoneGraphStorage.Lanes[LaneHandle.Index];
				int StartIndex = INDEX_NONE;
				int EndIndex = INDEX_NONE;
				
				if (LanePath.Lanes.Num() == 1)
				{
					// Special case for path that is only 1 lane
					// Record from start lane to end lane
					StartIndex = StartLane.LaneSegment + 1;
					EndIndex = EndLane.LaneSegment;
				}
				else if (CurrLaneIndex == 0)
				{
					// Record from start to end of lane
					StartIndex = StartLane.LaneSegment + 1;
					EndIndex = Lane.PointsEnd;
				}
				else if (CurrLaneIndex == LanePath.Lanes.Num() - 1)
				{
					// Record from start of lane to end point
					StartIndex = Lane.PointsBegin;
					EndIndex = EndLane.LaneSegment;
				}
				else
				{
					// Record full lane
					StartIndex = Lane.PointsBegin;
					EndIndex = Lane.PointsEnd;
				}

				// Record points
				for (int32 i = StartIndex; i < EndIndex; i++)
				{
					const FVector Point = ZoneGraphStorage.LanePoints[i];
					Path.Emplace(Point);

					if (Path.Num() > 1)
					{
						AccumulatedDistance += FVector::Dist(Path[Path.Num()-2], Point);
					}
					
					int Ind = OutCurve.AddPoint(AccumulatedDistance, Point);
					OutCurve.Points[Ind].InterpMode = CIM_CurveAuto;
				}
			}

			// If we are the last lane point, also add the final location as a point on the curve
			{
				const FVector Point = EndLane.Position;
				Path.Emplace(Point);

				if (Path.Num() > 1)
				{
					AccumulatedDistance += FVector::Dist(Path[Path.Num()-2], Point);
				}
					
				int Ind = OutCurve.AddPoint(AccumulatedDistance, Point);
				OutCurve.Points[Ind].InterpMode = CIM_CurveAuto;
			}
			
			OutPathDistance = AccumulatedDistance;
			OutCurve.AutoSetTangents();

#if ENABLE_VISUAL_LOG
			for (int i=1; i<Path.Num();i++)
			{
				const FVector& Point = Path[i];
				if (i == 1 || i == Path.Num() - 1)
				{
					UE_VLOG_SEGMENT_THICK(WorldContextObject, LogArsenalStatics, Verbose, Path[i-1], Point, FColor::Green, 2.f, TEXT("Zonegraph Path: Point %d"), i);
				}
				else
				{
					UE_VLOG_SEGMENT_THICK(WorldContextObject, LogArsenalStatics, Verbose, Path[i-1], Point, FColor::Green, 2.f, TEXT(""));
				}
			}
#endif
			
			return true;
		}
	}
	
	return false;
}

bool UArsenalStatics::GetPointOnZoneGraph(const UObject* WorldContextObject, const FVector& Location,
                                          FTransform& OutTransform, const FVector SearchExtent, const FZoneGraphTagMask& AnyTags,
                                          const FZoneGraphTagMask& AllTags, const FZoneGraphTagMask& NotTags)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	auto ZoneGraph = World ? World->GetSubsystem<UZoneGraphSubsystem>() : nullptr;
	checkf(ZoneGraph, TEXT("ZoneGraph subsystem not found!"));
	

	FZoneGraphTagFilter Filter;
	Filter.AllTags = AllTags;
	Filter.AnyTags = AnyTags;
	Filter.NotTags = NotTags;

	float DistanceSqr = 0.f;
	FZoneGraphLaneLocation LaneLocation;

	FBox Extents = FBox(Location - SearchExtent, Location + SearchExtent);
	bool bSuccess = ZoneGraph->FindNearestLane(Extents, Filter, LaneLocation, DistanceSqr);
	
	UE_VLOG_BOX(WorldContextObject, LogArsenalStatics, Verbose, Extents, bSuccess ? FColor::Green : FColor::Red, TEXT("ZoneGraph point query"));

	if (bSuccess)
	{
		OutTransform.SetLocation(LaneLocation.Position);
		OutTransform.SetRotation(LaneLocation.Direction.ToOrientationQuat());
		UE_VLOG_ARROW(WorldContextObject, LogArsenalStatics, Verbose, LaneLocation.Position, LaneLocation.Position + (LaneLocation.Direction * 500.f), FColor::Green, TEXT("GetPointOnZoneGraph"));
	}
	
	return bSuccess;
}

float UArsenalStatics::FindNearestKeyAlongCurve(const FInterpCurveVector& Curve, const FVector& Position)
{
	float DistanceSq = 0.f;
	return Curve.FindNearest(Position, DistanceSq);
}

void UArsenalStatics::GetPositionAlongCurve(const FInterpCurveVector& Curve, float InKey, FVector& OutPosition,
                                            FVector& OutTangent)
{
	OutPosition = Curve.Eval(InKey);
	OutTangent = Curve.EvalDerivative(InKey).GetSafeNormal();
}


void UArsenalStatics::GetCurveDataFromSplineComponent(class USplineComponent* SplineComp, FInterpCurveVector& OutCurve, float& OutDistance, bool bWorldSpace)
{
	if(SplineComp)
	{
		OutCurve.Reset();
		
		// Sample spline
		for (int i=0;i<SplineComp->GetSplineLength();i+=100)
		{
			FVector Location = SplineComp->GetLocationAtDistanceAlongSpline(i, bWorldSpace ? ESplineCoordinateSpace::World : ESplineCoordinateSpace::Local);
			OutCurve.AddPoint(i, Location);
		}

		OutDistance = SplineComp->GetSplineLength();
		OutCurve.AutoSetTangents();

#if ENABLE_VISUAL_LOG
		for (int i=100; i<OutDistance;i+=100)
		{
			const FVector& Point = OutCurve.Eval(i);
			UE_VLOG_SEGMENT_THICK(SplineComp, LogArsenalStatics, Verbose, OutCurve.Eval(i-100), Point, FColor::Green, 4.f, TEXT(""));
		}
#endif
	}
}

void UArsenalStatics::ReleaseMassHandleOnActor(APawn* Pawn)
{
	UWorld* World = Pawn->GetWorld();
	auto AgentComponent = Pawn ? Pawn->GetComponentByClass<UMassAgentComponent>() : nullptr;
	auto& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	if(!IsValid(AgentComponent))
	{
		return; 
	}

	// Ensure that agent is valid and a puppet (managed by mass)
	auto AgentEntity = AgentComponent->GetEntityHandle();
	if (IsValid(Pawn) && IsValid(AgentComponent) && EntityManager.IsEntityActive(AgentEntity) && AgentComponent->IsPuppet())
	{
		TWeakObjectPtr<UMassAgentComponent> WeakAgentComponent = AgentComponent;
		TWeakObjectPtr<APawn> WeakPawn = Pawn;
		
		EntityManager.Defer().PushCommand<FMassDeferredChangeCompositionCommand>([WeakAgentComponent, AgentEntity, WeakPawn](FMassEntityManager& InOutEntityManager)
		{
			if (!WeakAgentComponent.IsValid()){ return; }

			if (!WeakPawn.IsValid()){ return; }
			
			if (InOutEntityManager.IsEntityActive(AgentEntity) == false) { return; }

			// Pause and disable the agent component. This is mainly for cleanup and ensuring systems know that this agent is no longer controlled by mass
			if (!WeakAgentComponent->IsPuppet())
			{
				return;
			}

			// First release the actor from mass - this ensures that the vehicle wont despawn
			if (UMassActorSubsystem* ActorSubsystem = UWorld::GetSubsystem<UMassActorSubsystem>(InOutEntityManager.GetWorld()))
			{
				ActorSubsystem->DisconnectActor(WeakPawn.Get(), AgentEntity);
			}

			// Clear representation fragment values so actor does not get automatically cleaned up
			FMassEntityView EntityView = FMassEntityView(InOutEntityManager, AgentEntity);
			if (auto RepresentationFragment = EntityView.GetFragmentDataPtr<FMassRepresentationFragment>())
			{
				RepresentationFragment->ActorSpawnRequestHandle.Invalidate();
			}
			
			WeakAgentComponent->PausePuppet(true);
			WeakAgentComponent->Disable();
			
			auto& EntityTemplate = WeakAgentComponent->GetEntityConfig().GetOrCreateEntityTemplate(*InOutEntityManager.GetWorld());
			const FMassArchetypeHandle& NewArchetype = EntityTemplate.GetArchetype();

			// Replace fragment data to the template on the BP. This ensures processors dont accidentally run on this entity
			const FMassArchetypeSharedFragmentValues& SharedFragments = EntityTemplate.GetSharedFragmentValues();
			InOutEntityManager.MoveEntityToAnotherArchetype(AgentEntity, NewArchetype, &SharedFragments);

			// Run object initializers again
			if (EntityTemplate.GetObjectFragmentInitializers().Num())
			{
				const TConstArrayView<FMassEntityTemplateData::FObjectFragmentInitializerFunction> ObjectFragmentInitializers = EntityTemplate.GetObjectFragmentInitializers();
				
				EntityView = FMassEntityView(EntityTemplate.GetArchetype(), AgentEntity);
				AActor* Owner = WeakAgentComponent->GetOwner();
				check(Owner);
				
				for (const FMassEntityTemplateData::FObjectFragmentInitializerFunction& Initializer : ObjectFragmentInitializers)
				{
					Initializer(*Owner, EntityView, EMassTranslationDirection::ActorToMass);
				}
			}
			
			//remove translation tags for mass -> actor
			InOutEntityManager.RemoveTagFromEntity(AgentEntity, FMassCharacterMovementCopyToActorTag::StaticStruct());
			InOutEntityManager.RemoveTagFromEntity(AgentEntity, FMassCharacterOrientationCopyToActorTag::StaticStruct());

			if (ANarrativeVehicleBase* NarrativeVehicle = Cast<ANarrativeVehicleBase>(WeakPawn))
			{
				NarrativeVehicle->SetManagedByMass(false);
			}

			// Reapply actor fragment data
			if (FMassActorFragment* ActorFragment = InOutEntityManager.GetFragmentDataPtr<FMassActorFragment>(AgentEntity))
			{
				ActorFragment->SetAndUpdateHandleMap(AgentEntity, WeakPawn.Get(), false);
			}
		});
		
		Pawn->SpawnDefaultController();
	}
}

void UArsenalStatics::MakeActorMassPuppet(AActor* Actor, UMassEntityConfigAsset* EntityConfig, bool bAutoDestroy)
{
	if (!IsValid(Actor)) { return; }
	if (!Actor->IsA<ANarrativeVehicleBase>())
	{
		UE_LOG(LogArsenalStatics, Error, TEXT("%hs: Currently only supports narrative vehicle base!"), __FUNCTION__);
	}
	
	UWorld* World = Actor->GetWorld();
	auto& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	TWeakObjectPtr<AActor> WeakActor = Actor;
	EntityManager.Defer().PushCommand<FMassDeferredCreateCommand>([WeakActor, EntityConfig, bAutoDestroy](FMassEntityManager& InOutEntityManager)
	{
		if (!WeakActor.IsValid()) { return; }
		
		UWorld* World = WeakActor->GetWorld();
		auto AgentComponent = WeakActor->GetComponentByClass<UMassAgentComponent>();
		const FMassEntityTemplate& Template = EntityConfig->GetOrCreateEntityTemplate(*World);

		if (AgentComponent->IsPuppet()) { return; }
		
		// 1. destroy agent entity
		// 2. create a new entity based on entity config
		// 3. apply appropriate settings so that agent comp is a puppet
		// 4. apply representation settings so that actor is the visualization
		// this basically ensures that the vehicle seamlessly transitions to being a puppet
		// this is basically what happens behind the scenes when creating actor visualization
		
		AgentComponent->UnregisterWithAgentSubsystem();
		AgentComponent->RegisterWithAgentSubsystem();
		
		TArray<FMassEntityHandle> Entities;
		UMassSpawnerSubsystem* SpawnerSubsystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(World);
		TSharedPtr<FMassEntityManager::FEntityCreationContext> SpawnContext = SpawnerSubsystem->SpawnEntities(Template, 1, Entities);

		// Since we only spawn one, we will use index 0
		AgentComponent->SetPuppetHandle(Entities[0]);

		FMassEntityView EntityView = FMassEntityView(InOutEntityManager, AgentComponent->GetEntityHandle());
		if (EntityView.IsValid())
		{
			const FMassEntityHandle& EntityHandle = EntityView.GetEntity();
			FMassActorFragment& ActorFragment = EntityView.GetFragmentData<FMassActorFragment>();
			FMassRepresentationFragment& RepFragment = EntityView.GetFragmentData<FMassRepresentationFragment>();
			RepFragment.CurrentRepresentation = EMassRepresentationType::HighResSpawnedActor;

			if (!ActorFragment.IsValid())
			{
				ActorFragment.SetAndUpdateHandleMap(EntityHandle, WeakActor.Get(), true);
			}
			
			if (FTransformFragment* TransformFragment = EntityView.GetFragmentDataPtr<FTransformFragment>())
			{
				TransformFragment->SetTransform(WeakActor->GetTransform());
			}

			// @todo this only will work for vehicles as we need to manually register the actor in the representation subsystem
			if (auto NarrativeVehicle = Cast<ANarrativeVehicleBase>(WeakActor.Get()))
			{
				UVehicleRepresentationSubsystem* VehicleSubsystem = UWorld::GetSubsystem<UVehicleRepresentationSubsystem>(World);
				VehicleSubsystem->AddManagedEntity(EntityHandle);
				NarrativeVehicle->SetManagedByMass(true);
			}

			// Apply auto destroy tag to entity
			if (bAutoDestroy)
			{
				InOutEntityManager.AddTagToEntity(EntityHandle, FAutoDestroyTag::StaticStruct());
			}
		}
	});
}

float UArsenalStatics::TimeUntilCollision(
	const FVector& AgentLocation, const FVector& AgentVelocity, float AgentRadius,
	const FVector& ObstacleLocation, const FVector& ObstacleVelocity, float ObstacleRadius)
{
	const float RadiusSum = AgentRadius + ObstacleRadius; // Gets min distance when objects are touching
	const FVector VecToObstacle = ObstacleLocation - AgentLocation; // Gets distance to objects
	const float C = FVector::DotProduct(VecToObstacle, VecToObstacle) - RadiusSum * RadiusSum; // Calculates squared dist between objects and squared min dist

	// If C < 0 this means we are already colliding
	if (C < 0.f)
	{
		return 0.f;
	}
	const FVector VelocityDelta = AgentVelocity - ObstacleVelocity; // Relative velocity
	const float A = FVector::DotProduct(VelocityDelta, VelocityDelta); // Squared velocity
	const float B = FVector::DotProduct(VecToObstacle, VelocityDelta); // Multiplies relative dist and velocity 
	const float Discriminator = B * B - A * C;

	// (Delta = b^2-4ac) -> derived from quadratic formula
	// Discriminant basically tells us the # of times collisions happen.
	// < 0 means no collision
	// 0 is a single point in time that we will collide - a 'graze' that we treat as no collision
	// > 0 means collision will happen
	
	if (Discriminator <= 0)
	{
		return TNumericLimits<float>::Max();
	}
	const float Tau = (B - FMath::Sqrt(Discriminator)) / A;
	return (Tau < 0) ? TNumericLimits<float>::Max() : Tau;
}

void UArsenalStatics::GetEntityLOD(const UObject* WorldContextObject, const UMassAgentComponent* Agent, EEntityLOD& OutLOD)
{
	OutLOD = EEntityLOD::Off; // Default is off
	
	auto World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) { return; }
	
	if (auto EntityManager = UE::Mass::Utils::GetEntityManager(World))
	{
		if (Agent->GetEntityHandle().IsValid())
		{
			auto& LODFragment = EntityManager->GetFragmentDataChecked<FMassRepresentationLODFragment>(Agent->GetEntityHandle());
			OutLOD = (EEntityLOD)LODFragment.LOD.GetIntValue();
		}
	}
}

FSpawnedEntities UArsenalStatics::SpawnMassEntity(const UObject* WorldContextObject,
                                                  UMassEntityConfigAsset* ConfigAsset, FVector Location)
{
	return SpawnMassEntities(WorldContextObject, ConfigAsset, {Location});
}

FSpawnedEntities UArsenalStatics::SpawnMassEntities(const UObject* WorldContextObject,
                                                    UMassEntityConfigAsset* ConfigAsset, TArray<FVector> Locations)
{
	// Invalid config asset
	if (!ConfigAsset) { return FSpawnedEntities(); }

	// No entities to spawn
	if (Locations.IsEmpty()) { return FSpawnedEntities(); }
	
	auto World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
	auto SpawnerSubsystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(World);
	
	check(SpawnerSubsystem);

	FMassEntitySpawnDataGeneratorResult Result = FMassEntitySpawnDataGeneratorResult();
	Result.NumEntities = Locations.Num(); // Spawn entities based on size of locations array
	Result.EntityConfigIndex = 0; // Only spawning one type of entity
	
	Result.SpawnDataProcessor = UMassSpawnLocationProcessor::StaticClass();
	Result.SpawnData.InitializeAs<FMassTransformsSpawnData>();
	FMassTransformsSpawnData& Transforms = Result.SpawnData.GetMutable<FMassTransformsSpawnData>();

	Transforms.Transforms.Reserve(Result.NumEntities);
	for (int i = 0; i < Result.NumEntities; i++)
	{
		FTransform& Transform = Transforms.Transforms.AddDefaulted_GetRef();
		Transform.SetLocation(Locations[i]);
	}

	auto& EntityTemplate = ConfigAsset->GetOrCreateEntityTemplate(*World);
	TArray<FMassEntityHandle> OutEntities;
	SpawnerSubsystem->SpawnEntities(EntityTemplate.GetTemplateID(), Locations.Num(), Result.SpawnData, Result.SpawnDataProcessor, OutEntities);

	return FSpawnedEntities(OutEntities);
}

void UArsenalStatics::DestroyEntities(const UObject* WorldContextObject, FSpawnedEntities SpawnedEntities)
{
	auto World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
	auto SpawnerSubsystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(World);

	check(SpawnerSubsystem);

	SpawnerSubsystem->DestroyEntities(SpawnedEntities.SpawnedEntities);
}

bool UArsenalStatics::IsControlledByMass(const AActor* Actor)
{
	if (!Actor) { return false; }
	
	if (auto AgentComponent = Actor->GetComponentByClass<UMassAgentComponent>())
	{
		return AgentComponent->IsPuppet();
	}
	
	return false;
}

void UArsenalStatics::RemoveObstacleFragments(AActor* Actor)
{
	if (!Actor) { return; }
	
	if (auto AgentComponent = Actor->GetComponentByClass<UMassAgentComponent>())
	{
		auto& World = *Actor->GetWorld();
		auto& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
		const auto& EntityHandle = AgentComponent->GetEntityHandle();

		// Entity not ready yet
		if (!EntityManager.IsEntityActive(EntityHandle)) { return; }

		// Remove obstacle fragments
		EntityManager.Defer().RemoveFragment<FVehicleObstacleFragment>(EntityHandle);
		EntityManager.Defer().RemoveFragment<FMassNavigationObstacleGridCellLocationFragment>(EntityHandle);
	}
}

FString UArsenalStatics::Conv_SoftObjPathToSoftObjRef(const FSoftObjectPath& SoftObjectPath)
{
	return SoftObjectPath.GetAssetName();
}

FString UArsenalStatics::Conv_SoftObjRefToAssetName(TSoftObjectPtr<UObject> SoftObjectReference)
{
	return SoftObjectReference.GetAssetName();
}

FString UArsenalStatics::Conv_SoftClassRefToAssetName(TSoftClassPtr<UObject> SoftClassReference)
{
	return SoftClassReference.GetAssetName();
}

void UArsenalStatics::DrawDashedLine(FPaintContext& Context, const TArray<FVector2f>& Points, const FLinearColor& Tint, float Thickness, float DashLengthPx)
{
	Context.MaxLayer++;
	
	FSlateDrawElement::MakeDashedLines(
		Context.OutDrawElements,
		Context.MaxLayer,
		Context.AllottedGeometry.ToPaintGeometry(),
		CopyTemp(Points),
		ESlateDrawEffect::None,
		Tint,
		Thickness,
		DashLengthPx);
}

int UArsenalStatics::GetClosestPoint(const TArray<FVector>& Points, const FVector& Location)
{
	float ClosestDist = FLT_MAX;
	int ClosestIndex = INDEX_NONE;
	
	for (int i=0; i<Points.Num(); i++)
	{
		const FVector& Point = Points[i];
		float DistSq = FVector::DistSquared(Location, Point);
		if (DistSq < ClosestDist)
		{
			ClosestDist = DistSq;
			ClosestIndex = i;
		}
	}
	
	return ClosestIndex;
}

void UArsenalStatics::SetNarrativeUIInputMode(const APlayerController* PlayerController, ECommonInputMode CommonInputMode)
{
	check(PlayerController);

	if (const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		if (UCommonUIActionRouterBase* ActionRouter = LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>())
		{
			//TODO could use same setup as activatable widget? 
			FUIInputConfig InputConfig;
			if (CommonInputMode == ECommonInputMode::Game)
			{
				InputConfig = FUIInputConfig(CommonInputMode, EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown, true);
			}
			else
			{
				InputConfig = FUIInputConfig(CommonInputMode, EMouseCaptureMode::CaptureDuringMouseDown, false);
			}

			ActionRouter->SetActiveUIInputConfig(InputConfig);
		}
	}

}

void UArsenalStatics::SetNarrativeUIInputModeExplicit(const APlayerController* PlayerController, const FUIInputConfig& InputConfig)
{
	check(PlayerController);

	if (const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		if (UCommonUIActionRouterBase* ActionRouter = LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>())
		{
			ActionRouter->SetActiveUIInputConfig(InputConfig);

		}
	}

}

bool UArsenalStatics::IsWidgetInActiveRoot(const class UCommonActivatableWidget* Widget) 
{
	check(Widget->GetOwningPlayer());

	if (const APlayerController* PC = Widget->GetOwningPlayer())
	{
		if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UCommonUIActionRouterBase* ActionRouter = LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>())
			{

				return ActionRouter->IsWidgetInActiveRoot(Widget);
			}
		}
	}

	return false; 
}

bool UArsenalStatics::GetInputKeyBrush(const UCommonInputSubsystem* CommonInputSubsystem, FSlateBrush& OutBrush, FKey Key, ECommonInputType InputType, const FName GamepadName)
{
	if (!CommonInputSubsystem)
	{
		return false; 
	}

	if (UCommonInputPlatformSettings::Get()->TryGetInputBrush(OutBrush, Key, CommonInputSubsystem->GetCurrentInputType(), CommonInputSubsystem->GetCurrentGamepadName()))
	{
		return true;
	}

	return false;
}

bool UArsenalStatics::GetStartTransformByTag(UMovieSceneSequence* LevelSequence, FName Tag, FTransform& Transform)
{
	const UMovieScene* Scene = LevelSequence? LevelSequence->GetMovieScene() : nullptr;
	if (!Scene)
	{
		return false;
	}

	// check binding exists
	const TMap<FName, FMovieSceneObjectBindingIDs>& BindingTags = Scene->AllTaggedBindings();
	if (!BindingTags.Contains(Tag))
	{
		UE_LOG(LogArsenalStatics, Warning, TEXT("Could not find binding ID '%s'"), *Tag.ToString());
		
		//FMessageLog("PIE")
		//	.Warning(FText::Format(NSLOCTEXT("ArsenalStatics", "NoBinding", "Could not find binding ID '{0}'"), FText::FromName(Tag)))
		//	->AddToken(FUObjectToken::Create(LevelSequence));
		return false;
	}
	
	const TArray<FMovieSceneObjectBindingID>& BindingIDs = BindingTags[Tag].IDs;

	// find binding
	const TArray<FMovieSceneBinding>& SceneBindings = Scene->GetBindings();
	const FMovieSceneBinding* FoundBinding = SceneBindings.FindByPredicate([&BindingIDs](const FMovieSceneBinding& SceneBinding)
	{
		for (const FMovieSceneObjectBindingID& ID : BindingIDs)
		{
			if (SceneBinding.GetObjectGuid() == ID.GetGuid())
			{
				return true;
			}
		}
		return false;
	});
	
	if (!FoundBinding)
	{
		UE_LOG(LogArsenalStatics, Warning, TEXT("Could not find binding for ID '%s'"), *Tag.ToString());
		
		//FMessageLog("PIE")
  //      	.Warning(FText::Format(NSLOCTEXT("ArsenalStatics", "NoBinding", "Could not find binding for ID '{0}'"), FText::FromName(Tag)))
  //      	->AddToken(FUObjectToken::Create(LevelSequence));
		return false;
	}

	// collect transform track
	UMovieScene3DTransformTrack* TransformTrack = nullptr;
	for (UMovieSceneTrack* Track : FoundBinding->GetTracks())
	{
		if (TransformTrack = Cast<UMovieScene3DTransformTrack>(Track); TransformTrack)
		{
			break;
		}
	}
	
	// no track 
	if (!TransformTrack)
	{
		//UE_LOG(LogArsenalStatics, Error, TEXT("Could not find track for binding ID '%s'"), *Tag.ToString());
		//FMessageLog("PIE")
		//	.Warning(FText::Format(NSLOCTEXT("ArsenalStatics", "NoTrack", "Could not find track for binding ID '{0} as no transform track was available.'"), FText::FromName(Tag)))
		//	->AddToken(FUObjectToken::Create(LevelSequence));
		return false;
	}

	// get sections and start evaluation of first section
	const TArray<UMovieSceneSection*>& TrackSections = TransformTrack->GetAllSections();
	if (const UMovieScene3DTransformSection* TransformSection = TrackSections.IsEmpty()? nullptr : Cast<UMovieScene3DTransformSection>(TrackSections[0]))
	{		
		// get start frame
		FFrameNumber StartFrame;
		Scene->GetPlaybackRange().SetLowerBoundValue(StartFrame);
			
		const TArrayView<FMovieSceneDoubleChannel*> Channels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
		if (Channels.Num() < 9)
		{
			UE_LOG(LogArsenalStatics, Error, TEXT("Unexpected number of double tracks (%d) in transform section '%s'"), Channels.Num(), *TransformSection->GetPathName());
			FMessageLog("PIE")
			.Warning(FText::Format(NSLOCTEXT("ArsenalStatics", "BadNumberOfTracks",
				"Unexpected number of double tracks {0} in transform section '{1}'"), FText::AsNumber(Channels.Num()), FText::FromName(Tag)))
			->AddToken(FUObjectToken::Create(LevelSequence));
			return false;
		}

		// get channels
		FMovieSceneDoubleChannel* LocationXChannel = Channels[0];
		FMovieSceneDoubleChannel* LocationYChannel = Channels[1];
		FMovieSceneDoubleChannel* LocationZChannel = Channels[2];

		FMovieSceneDoubleChannel* RotationXChannel = Channels[3];
		FMovieSceneDoubleChannel* RotationYChannel = Channels[4];
		FMovieSceneDoubleChannel* RotationZChannel = Channels[5];

		FMovieSceneDoubleChannel* ScaleXChannel = Channels[6];
		FMovieSceneDoubleChannel* ScaleYChannel = Channels[7];
		FMovieSceneDoubleChannel* ScaleZChannel = Channels[8];
		
		// evaluate channels
		FVector Location, Scale;
		FRotator Rotation;

		// wrapper for repeat code
		auto EvalChannel = [](const FMovieSceneDoubleChannel* Channel, const FFrameNumber& Frame, double& Result)
		{
			if (Channel)
			{
				Channel->Evaluate(Frame, Result);
			}
		};
			
		EvalChannel(LocationXChannel, StartFrame, Location.X);
		EvalChannel(LocationYChannel, StartFrame, Location.Y);
		EvalChannel(LocationZChannel, StartFrame, Location.Z);

		EvalChannel(RotationYChannel, StartFrame, Rotation.Pitch);
		EvalChannel(RotationZChannel, StartFrame, Rotation.Yaw);
		EvalChannel(RotationXChannel, StartFrame, Rotation.Roll);

		EvalChannel(ScaleXChannel, StartFrame, Scale.X);
		EvalChannel(ScaleYChannel, StartFrame, Scale.Y);
		EvalChannel(ScaleZChannel, StartFrame, Scale.Z);

		// update transform
		Transform.SetLocation(Location);
		Transform.SetRotation(Rotation.Quaternion());
		Transform.SetScale3D(Scale);
		return true;
	}
	
	//UE_LOG(LogArsenalStatics, Error, TEXT("Could not find track sections for binding ID '%s'"), *Tag.ToString());
	//FMessageLog("PIE")
	//.Warning(FText::Format(NSLOCTEXT("ArsenalStatics", "NoTrackSections", "Could not find track sections for binding ID '{1}'"), FText::FromName(Tag)))
	//->AddToken(FUObjectToken::Create(LevelSequence));
	
	return false;
}

ANarrativeWorldSettings* UArsenalStatics::GetNarrativeWorldSettings(const UObject* WorldContext)
{
	UWorld* World = WorldContext? WorldContext->GetWorld() : nullptr;
	return World? Cast<ANarrativeWorldSettings>(World->GetWorldSettings()) : nullptr;
}

float UArsenalStatics::GetEngineIdleRotationSpeed(UChaosWheeledVehicleMovementComponent* Target)
{
	return Target ? Target->EngineSetup.EngineIdleRPM : 0.0f;
}

FPackedVehicleInstanceCustomData UArsenalStatics::PackVehicleInstanceCustomData(FVehicleInstanceCustomData CustomData)
{
	return FPackedVehicleInstanceCustomData(CustomData);
}

bool UArsenalStatics::PredictCharacterPath(const ACharacter* Character, const FPredictProjectilePathParams& PredictParams, FPredictProjectilePathResult& PredictResult)
{
	PredictResult.Reset();
	bool bBlockingHit = false;

	UWorld const* const World = GEngine->GetWorldFromContextObject(Character, EGetWorldErrorMode::LogAndReturnNull);
	if (World && PredictParams.SimFrequency > UE_KINDA_SMALL_NUMBER)
	{
		const float SubstepDeltaTime = 1.f / PredictParams.SimFrequency;
		const float GravityZ = FMath::IsNearlyEqual(PredictParams.OverrideGravityZ, 0.0f) ? World->GetGravityZ() : PredictParams.OverrideGravityZ;
		const float ProjectileRadius = PredictParams.ProjectileRadius;

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PredictProjectilePath), PredictParams.bTraceComplex);
		FCollisionObjectQueryParams ObjQueryParams;
		const bool bTraceWithObjectType = (PredictParams.ObjectTypes.Num() > 0);
		const bool bTracePath = PredictParams.bTraceWithCollision && (PredictParams.bTraceWithChannel || bTraceWithObjectType);
		if (bTracePath)
		{
			QueryParams.AddIgnoredActor(Character);
			if (bTraceWithObjectType)
			{
				for (auto Iter = PredictParams.ObjectTypes.CreateConstIterator(); Iter; ++Iter)
				{
					const ECollisionChannel& Channel = ECC_Pawn;//Character->GetCapsuleComponent()->GetCollisionObjectType();
					ObjQueryParams.AddObjectTypesToQuery(Channel);
				}
			}
		}

		FVector CurrentVel = PredictParams.LaunchVelocity;
		FVector TraceStart = PredictParams.StartLocation;
		FVector TraceEnd = TraceStart;
		float CurrentTime = 0.f;
		PredictResult.PathData.Reserve(FMath::Min(128, FMath::CeilToInt(PredictParams.MaxSimTime * PredictParams.SimFrequency)));
		PredictResult.AddPoint(TraceStart, CurrentVel, CurrentTime);

		FHitResult ObjectTraceHit(NoInit);
		FHitResult ChannelTraceHit(NoInit);
		ObjectTraceHit.Time = 1.f;
		ChannelTraceHit.Time = 1.f;

		const float MaxSimTime = PredictParams.MaxSimTime;
		while (CurrentTime < MaxSimTime)
		{
			// Limit step to not go further than total time.
			const float PreviousTime = CurrentTime;
			const float ActualStepDeltaTime = FMath::Min(MaxSimTime - CurrentTime, SubstepDeltaTime);
			CurrentTime += ActualStepDeltaTime;

			// Integrate (Velocity Verlet method)
			TraceStart = TraceEnd;
			FVector OldVelocity = CurrentVel;
			CurrentVel = OldVelocity + FVector(0.f, 0.f, GravityZ * ActualStepDeltaTime);
			TraceEnd = TraceStart + (OldVelocity + CurrentVel) * (0.5f * ActualStepDeltaTime);
			PredictResult.LastTraceDestination.Set(TraceEnd, CurrentVel, CurrentTime);

			if (bTracePath)
			{
				bool bObjectHit = false;
				bool bChannelHit = false;
				const FCollisionShape CharacterShape = FCollisionShape::MakeCapsule(Character->GetCapsuleComponent()->GetScaledCapsuleRadius(), Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
				if (bTraceWithObjectType)
				{
					bObjectHit = World->SweepSingleByObjectType(ObjectTraceHit, TraceStart, TraceEnd, Character->GetActorRotation().Quaternion(), ObjQueryParams, CharacterShape, QueryParams);
				}
				if (PredictParams.bTraceWithChannel)
				{
					bChannelHit = World->SweepSingleByChannel(ChannelTraceHit, TraceStart, TraceEnd, FQuat::Identity, PredictParams.TraceChannel, CharacterShape, QueryParams);
				}

				// See if there were any hits.
				if (bObjectHit || bChannelHit)
				{
					// Hit! We are done. Choose trace with earliest hit time.
					PredictResult.HitResult = (ObjectTraceHit.Time < ChannelTraceHit.Time) ? ObjectTraceHit : ChannelTraceHit;
					const float HitTimeDelta = ActualStepDeltaTime * PredictResult.HitResult.Time;
					const float TotalTimeAtHit = PreviousTime + HitTimeDelta;
					const FVector VelocityAtHit = OldVelocity + FVector(0.f, 0.f, GravityZ * HitTimeDelta);
					PredictResult.AddPoint(PredictResult.HitResult.Location, VelocityAtHit, TotalTimeAtHit);
					bBlockingHit = true;
					break;
				}
			}

			PredictResult.AddPoint(TraceEnd, CurrentVel, CurrentTime);
		}

		// Draw debug path
#if ENABLE_DRAW_DEBUG
		if (PredictParams.DrawDebugType != EDrawDebugTrace::None)
		{
			const bool bPersistent = PredictParams.DrawDebugType == EDrawDebugTrace::Persistent;
			const float LifeTime = (PredictParams.DrawDebugType == EDrawDebugTrace::ForDuration) ? PredictParams.DrawDebugTime : 0.f;
			const float DrawRadius = (ProjectileRadius > 0.f) ? ProjectileRadius : 5.f;

			// draw the path
			for (const FPredictProjectilePathPointData& PathPt : PredictResult.PathData)
			{
				::DrawDebugCapsule(World, PathPt.Location, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), Character->GetCapsuleComponent()->GetScaledCapsuleRadius(), Character->GetActorQuat(), FColor::Green, bPersistent, LifeTime);
			}
			// draw the impact point
			if (bBlockingHit)
			{
				::DrawDebugCapsule(World, PredictResult.HitResult.Location, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), Character->GetCapsuleComponent()->GetScaledCapsuleRadius(), Character->GetActorQuat(), FColor::Green, bPersistent, LifeTime);
			}
		}
#endif //ENABLE_DRAW_DEBUG
	}

	return bBlockingHit;
}

FGuid UArsenalStatics::CreateUniqueGUIDFromStringHash(const FString& String)
{
	FSHAHash Hash;
	FSHA1::HashBuffer(TCHAR_TO_UTF8(*String), String.Len(), Hash.Hash);
    
	uint32* HashUints = reinterpret_cast<uint32*>(Hash.Hash);
	return FGuid(HashUints[0], HashUints[1], HashUints[2], HashUints[3]);
}

bool UArsenalStatics::IsPointInWaterVolume(const UObject* WorldContextObject, const FVector& Point)
{
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		for (auto VolumeIter = World->GetNonDefaultPhysicsVolumeIterator(); VolumeIter; ++VolumeIter)
		{
			const APhysicsVolume* Volume = VolumeIter->Get();
			if (Volume && Volume->bWaterVolume)
			{
				if(Volume->EncompassesPoint(Point))
				{
					return true; 
				}
			}
		}
	}

	return false; 
}


static TAutoConsoleVariable<int32> CVarDebugExplosionDamage(
	TEXT("n.gas.DebugExplosionDamage"),
	false,
	TEXT("Whether to print debug explosion info. 0=Off 1=On"),
	ECVF_Default);

static bool ComponentIsDamageableFrom(UPrimitiveComponent* VictimComp, FVector const& Origin, AActor const* IgnoredActor, const TArray<AActor*>& IgnoreActors, ECollisionChannel TraceChannel, FHitResult& OutHitResult)
{
	FCollisionQueryParams LineParams(SCENE_QUERY_STAT(ComponentIsVisibleFrom), true, IgnoredActor);
	LineParams.AddIgnoredActors( IgnoreActors );

	// Do a trace from origin to middle of box
	UWorld* const World = VictimComp->GetWorld();
	check(World);

	FVector const TraceEnd = VictimComp->Bounds.Origin;
	FVector TraceStart = Origin;
	if (Origin == TraceEnd)
	{
		// tiny nudge so LineTraceSingle doesn't early out with no hits
		TraceStart.Z += 0.01f;
	}

	// Only do a line trace if there is a valid channel, if it is invalid then result will have no fall off
	if (TraceChannel != ECollisionChannel::ECC_MAX)
	{
		bool const bHadBlockingHit = World->LineTraceSingleByChannel(OutHitResult, TraceStart, TraceEnd, TraceChannel, LineParams);
		//::DrawDebugLine(World, TraceStart, TraceEnd, FLinearColor::Red, true);

		// If there was a blocking hit, it will be the last one
		if (bHadBlockingHit)
		{
			if (OutHitResult.Component == VictimComp)
			{
				// if blocking hit was the victim component, it is visible
				return true;
			}
			else
			{
				if (CVarDebugExplosionDamage.GetValueOnGameThread() >= 1)
				{
					// if we hit something else blocking, it's not
					UE_LOG(LogDamage, Warning, TEXT("Radial Damage to %s blocked by %s (%s)"), *GetNameSafe(VictimComp), *OutHitResult.GetHitObjectHandle().GetName(), *GetNameSafe(OutHitResult.Component.Get()));
				}

				return false;
			}
		}
	}
	else
	{
		UE_LOG(LogDamage, Warning, TEXT("ECollisionChannel::ECC_MAX is not valid! No falloff is added to damage"));
	}

	// didn't hit anything, assume nothing blocking the damage and victim is consequently visible
	// but since we don't have a hit result to pass back, construct a simple one, modeling the damage as having hit a point at the component's center.
	FVector const FakeHitLoc = VictimComp->GetComponentLocation();
	FVector const FakeHitNorm = (Origin - FakeHitLoc).GetSafeNormal();		// normal points back toward the epicenter
	OutHitResult = FHitResult(VictimComp->GetOwner(), VictimComp, FakeHitLoc, FakeHitNorm);
	return true;
}

bool UArsenalStatics::ApplyRadialExplosionWithFalloff(const UObject* WorldContextObject, const FVector& Origin, float Radius, UCurveFloat* DamageCurve,
	TSubclassOf<class UGameplayEffect> ExplosionDamageEffect, const TArray<AActor*>& IgnoreActors, UNarrativeAbilitySystemComponent* DamageCauserASC, ECollisionChannel DamagePreventionChannel)
{
	if (!DamageCauserASC || !IsValid(ExplosionDamageEffect) || !IsValid(DamageCauserASC->GetAvatarActor()) || !IsValid(DamageCurve))
	{
		return false; 
	}

	AActor* DamageCauser = DamageCauserASC->GetOwner();

	FCollisionQueryParams SphereParams(SCENE_QUERY_STAT(ApplyRadialDamage),  false, DamageCauser);

	SphereParams.AddIgnoredActors(IgnoreActors);

	// query scene to see what we hit
	TArray<FOverlapResult> Overlaps;
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllDynamicObjects), FCollisionShape::MakeSphere(Radius), SphereParams);
	}

	// collate into per-actor list of hit components
	TMap<AActor*, TArray<FHitResult> > OverlapComponentMap;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* const OverlapActor = Overlap.OverlapObjectHandle.FetchActor();

		if (OverlapActor &&
			OverlapActor != DamageCauser &&
			Overlap.Component.IsValid())
		{
			FHitResult Hit;
			if (ComponentIsDamageableFrom(Overlap.Component.Get(), Origin, DamageCauser, IgnoreActors, DamagePreventionChannel, Hit))
			{
				TArray<FHitResult>& HitList = OverlapComponentMap.FindOrAdd(OverlapActor);
				HitList.Add(Hit);
			}
		}
	}

	bool bAppliedDamage = false;

	if (OverlapComponentMap.Num() > 0)
	{
		// call damage function on each affected actors
		for (TMap<AActor*, TArray<FHitResult> >::TIterator It(OverlapComponentMap); It; ++It)
		{
			AActor* const Victim = It.Key();
			TArray<FHitResult> const& ComponentHits = It.Value();

			if (UNarrativeAbilitySystemComponent* VictimNASC = GetNarrativeAbilitySystemComponent(Victim))
			{
				if (AActor* VictimAvatar = VictimNASC->GetAvatarActor())
				{
					if (ComponentHits.IsValidIndex(0))
					{
						const float DistFromExplosion = FVector::Dist(VictimAvatar->GetActorLocation(), Origin);

						const float Damage = DamageCurve->GetFloatValue(DistFromExplosion); //FMath::GetMappedRangeValueClamped(FVector2D(DamageInnerRadius, DamageOuterRadius), FVector2D(BaseDamage, MinimumDamage), DistFromExplosion);

						//Make an explosion spec handle. 
						FGameplayAbilityTargetDataHandle TargetHandle = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(ComponentHits[0]);

						//TODO use effect spec rather than dealdamage. 
						FGameplayEffectSpecHandle SpecHandle = DamageCauserASC->MakeOutgoingSpec(ExplosionDamageEffect, 1.0f, DamageCauserASC->MakeEffectContext());

						if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
						{
							Spec->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, Damage);
						}

						DamageCauserASC->ApplyGameplayEffectSpecToTargetData(SpecHandle, TargetHandle);
					}

				}
			}			

			//Let the explosion victim handle being hit by the explosion however they want 
			if (Victim->Implements<UNarrativeImpactInterface>())
			{
				INarrativeImpactInterface::Execute_HandleExplosionImpact(Victim, DamageCauserASC, Origin, 1.f);
			}

			bAppliedDamage = true;
		}
	}

	return bAppliedDamage;
}

class ANarrativePlayerCharacter* UArsenalStatics::GetNarrativePlayerCharacter(const UObject* WorldContextObject, int32 PlayerIndex)
{
	return Cast<ANarrativePlayerCharacter>(UGameplayStatics::GetPlayerCharacter(WorldContextObject, PlayerIndex));
}


class UNarrativeAbilitySystemComponent* UArsenalStatics::GetNarrativeAbilitySystemComponent(AActor* Actor)
{
	return Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor));
}
