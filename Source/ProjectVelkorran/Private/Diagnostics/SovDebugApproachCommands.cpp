// Copyright Fallen Signal Studios. All Rights Reserved.
// Development-only console commands that drive the local player into an authored
// encounter, so the intermittent AI startup stall can be exercised without a human.
//
// Priority 4 established that unattended packaged runs never reproduce the stall: the
// hostiles never perceive the player, so the suspected race cannot fire. Its precondition
// is deterministic though - every goal generator initializes 55-59 ms before the player's
// factions publish, and the player reads Neutral during that interval. Reproducing the
// failure needs player Sight inside that window, which needs the player somewhere the
// Hounds can see.
//
// Diagnostic only. Compiled out of Shipping entirely, registered only when explicitly
// invoked, and it changes no shipping gameplay path: it repositions a pawn and nothing
// else. It does not alter perception, factions, goals, attitudes or behavior trees.
#include "CoreMinimal.h"
#if !UE_BUILD_SHIPPING
#include "Diagnostics/SovLogChannels.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GenericTeamAgentInterface.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

namespace SovDebugApproach
{
	/** Nearest NPC the player is actually hostile toward, so the probe cannot pick an ally. */
	APawn* FindNearestHostile(UWorld* World, const APawn* PlayerPawn)
	{
		const IGenericTeamAgentInterface* PlayerTeam = Cast<const IGenericTeamAgentInterface>(PlayerPawn);
		APawn* Best = nullptr;
		double BestDistanceSquared = TNumericLimits<double>::Max();
		for (TActorIterator<ANarrativeNPCCharacter> It(World); It; ++It)
		{
			ANarrativeNPCCharacter* Candidate = *It;
			if (!IsValid(Candidate) || Candidate == PlayerPawn) { continue; }
			// Prefer a real hostility answer; fall back to nearest NPC when the player
			// pawn does not implement the team interface, rather than silently doing nothing.
			if (PlayerTeam && PlayerTeam->GetTeamAttitudeTowards(*Candidate) != ETeamAttitude::Hostile)
			{
				const IGenericTeamAgentInterface* CandidateTeam = Cast<const IGenericTeamAgentInterface>(Candidate);
				if (!CandidateTeam || CandidateTeam->GetTeamAttitudeTowards(*PlayerPawn) != ETeamAttitude::Hostile)
				{
					continue;
				}
			}
			const double DistanceSquared = FVector::DistSquared(Candidate->GetActorLocation(), PlayerPawn->GetActorLocation());
			if (DistanceSquared < BestDistanceSquared) { BestDistanceSquared = DistanceSquared; Best = Candidate; }
		}
		return Best;
	}

	void PlaceNearHostile(UWorld* World, const float Distance)
	{
		if (!IsValid(World)) { return; }
		APlayerController* Controller = World->GetFirstPlayerController();
		APawn* PlayerPawn = Controller ? Controller->GetPawn() : nullptr;
		if (!IsValid(PlayerPawn))
		{
			UE_LOG(LogSovAI, Warning, TEXT("SOV_DEBUG_APPROACH no player pawn"));
			return;
		}
		APawn* Hostile = FindNearestHostile(World, PlayerPawn);
		if (!IsValid(Hostile))
		{
			UE_LOG(LogSovAI, Warning, TEXT("SOV_DEBUG_APPROACH no hostile found"));
			return;
		}
		const FVector HostileLocation = Hostile->GetActorLocation();
		const FVector Offset = Hostile->GetActorForwardVector() * FMath::Max(Distance, 100.f);
		const FVector Target(HostileLocation.X + Offset.X, HostileLocation.Y + Offset.Y, HostileLocation.Z);
		const FRotator Facing = (HostileLocation - Target).Rotation();
		const bool bMoved = PlayerPawn->TeleportTo(Target, Facing);
		// Logged in the same stream as the trace so placement can be ordered against
		// faction publication and the first Sight callback without clock correlation.
		UE_LOG(LogSovAI, Warning,
			TEXT("SOV_DEBUG_APPROACH moved=%d player=%s hostile=%s distance=%.0f world_time=%.4f"),
			bMoved ? 1 : 0, *GetNameSafe(PlayerPawn), *GetNameSafe(Hostile), Distance, World->GetTimeSeconds());
	}

	void Schedule(UWorld* World, const float Distance, const float Delay)
	{
		if (!IsValid(World)) { return; }
		if (Delay <= 0.f) { PlaceNearHostile(World, Distance); return; }
		// FTSTicker, not the world timer manager: a timer set during world initialization is
		// discarded when the world transitions to play, so the placement never runs.
		TWeakObjectPtr<UWorld> WeakWorld(World);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld, Distance](float)
		{
			if (UWorld* Alive = WeakWorld.Get()) { PlaceNearHostile(Alive, Distance); }
			else { UE_LOG(LogSovAI, Warning, TEXT("SOV_DEBUG_APPROACH world gone before placement")); }
			return false;
		}), Delay);
		UE_LOG(LogSovAI, Warning, TEXT("SOV_DEBUG_APPROACH scheduled delay=%.3f distance=%.0f world=%s"),
			Delay, Distance, *GetPathNameSafe(World));
	}

	void Approach(const TArray<FString>& Args, UWorld* World)
	{
		const float Distance = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 500.f;
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;
		Schedule(World, Distance, Delay);
	}

	// -ExecCmds runs against the transient /Temp world, not the gameplay world, so a timer
	// set from there is discarded when that world is torn down. These variables can be set
	// before world initialization with -dpcvars, and the delegate below arms the real Game
	// world. The delay is then measured from world initialization, which is what makes
	// sweeping across the ~0.47s faction-publication boundary meaningful.
	TAutoConsoleVariable<float> Distance(TEXT("sov.DebugApproachHostileDistance"), 0.f,
		TEXT("Development only. Above 0, place the player this far in front of the nearest hostile "
			 "after the Game world initializes. 0 disables."), ECVF_Default);
	TAutoConsoleVariable<float> Delay(TEXT("sov.DebugApproachHostileDelay"), 0.5f,
		TEXT("Development only. Seconds after Game world initialization before placement."), ECVF_Default);

	void OnPostWorldInitialization(UWorld* World, const UWorld::InitializationValues)
	{
		if (!IsValid(World) || World->WorldType != EWorldType::Game) { return; }
		// The engine creates a transient /Temp world that also reports EWorldType::Game.
		// Scheduling against it wastes the probe on a world that is torn down immediately.
		if (GetPathNameSafe(World).StartsWith(TEXT("/Temp/"))) { return; }
		const float ArmedDistance = Distance.GetValueOnGameThread();
		if (ArmedDistance <= 0.f) { return; }
		Schedule(World, ArmedDistance, Delay.GetValueOnGameThread());
	}

	/** Registered at static initialization; FWorldDelegates is available before any world. */
	struct FRegistrar
	{
		FRegistrar()
		{
			FWorldDelegates::OnPostWorldInitialization.AddStatic(&OnPostWorldInitialization);
		}
	};
	static FRegistrar GRegistrar;
}

static FAutoConsoleCommandWithWorldAndArgs GSovDebugApproachHostile(
	TEXT("sov.DebugApproachHostile"),
	TEXT("Development only. sov.DebugApproachHostile <Distance=500> <DelaySeconds=0>. "
		 "Teleports the local player pawn in front of the nearest hostile NPC so AI perception "
		 "can acquire it. Diagnostic placement only; changes no gameplay system."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SovDebugApproach::Approach));
#endif
