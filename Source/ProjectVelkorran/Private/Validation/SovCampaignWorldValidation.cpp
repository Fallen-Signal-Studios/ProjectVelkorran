// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Validation/SovCampaignWorldValidation.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignHandoffAnchor.h"
#include "Campaign/SovEncounterDirector.h"
#include "Campaign/SovEncounterCoordinationComponent.h"
#include "Campaign/SovEvidenceSourceComponent.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Cinematics/SovCampaignCinematicComponent.h"
#include "Components/SovCommandLinkComponent.h"
#include "Components/SovWeakPointComponent.h"
#include "Components/SceneComponent.h"
#include "Diagnostics/SovLogChannels.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Items/VendorInventoryComponent.h"
#include "LevelSequence.h"
#include "NarrativeStableActor.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "World/SovWorldTransitActor.h"

int32 SovCampaignWorldValidation::Validate(UWorld* World, const USovCampaignDefinition* Mission, bool bShippingValidation,
	TMap<FName, FString>* GlobalEncounterOwners)
{
	int32 Errors = 0;
	const auto Fail = [&](const TCHAR* Code, const UObject* Object, const FString& Message)
	{
		UE_LOG(LogSovMission, Error, TEXT("[%s] %s: %s"), Code, *GetPathNameSafe(Object), *Message);
		++Errors;
	};
	if (!IsValid(World) || !IsValid(Mission)) { Fail(TEXT("WORLD.LOAD"), Mission, TEXT("Mission world could not be loaded.")); return Errors; }
	// A persistent-level-only success is not proof of a complete partitioned map.
	// Until a licensed editor worker enumerates/loads its actor descriptors, reject
	// this category explicitly; do not initialize gameplay to force actors to spawn.
	if (World->IsPartitionedWorld())
	{ Fail(TEXT("WORLD.PARTITION_COVERAGE"), World, TEXT("World Partition actor-descriptor loading is required before this map can qualify. Persistent actors alone are incomplete evidence.")); }
	for (const ULevelStreaming* Streaming : World->GetStreamingLevels())
	{
		if (Streaming && !Streaming->GetLoadedLevel())
		{ Fail(TEXT("WORLD.STREAMING_COVERAGE"), Streaming, TEXT("Unloaded streaming level prevents complete placed-actor validation.")); }
	}
	TArray<AActor*> Actors;
	TArray<ULevel*> Levels;
	if (World->PersistentLevel) { Levels.Add(World->PersistentLevel); }
	for (ULevel* Level : World->GetLevels()) { if (Level) { Levels.AddUnique(Level); } }
	for (ULevel* Level : Levels)
	{
		if (!Level) { continue; }
		for (AActor* Actor : Level->Actors) { if (IsValid(Actor) && !Actor->IsActorBeingDestroyed()) { Actors.AddUnique(Actor); } }
	}
	TMap<FGuid, AActor*> StableActors;
	TMap<FName, AActor*> EncounterIds, AnchorIds, TransitIds;
	TMap<const AActor*, const ASovEncounterDirector*> ParticipantOwners;
	TMap<FName, int32> CinematicBeats;
	int32 Starts = 0, Encounters = 0, Cinematics = 0, EvidenceSources = 0, WeakPoints = 0, CommandLinks = 0;
	for (AActor* Actor : Actors)
	{
		if (Actor->Implements<UNarrativeStableActor>())
		{
			const FGuid Id = INarrativeStableActor::Execute_GetActorGUID(Actor);
			if (!Id.IsValid() || StableActors.Contains(Id))
			{ Fail(TEXT("WORLD.STABLE_GUID"), Actor, TEXT("Stable actor GUID is invalid or duplicated in this map.")); }
			else { StableActors.Add(Id, Actor); }
		}
		if (const auto* Start = Cast<APlayerStart>(Actor)) { Starts += Start->PlayerStartTag == Mission->EntryPlayerStartTag ? 1 : 0; }
		if (const auto* Director = Cast<ASovEncounterDirector>(Actor))
		{
			++Encounters;
			if (Director->EncounterId.IsNone() || EncounterIds.Contains(Director->EncounterId))
			{ Fail(TEXT("WORLD.ENCOUNTER_ID"), Actor, TEXT("Encounter ID is absent or duplicated.")); }
			else { EncounterIds.Add(Director->EncounterId, Actor); }
			if (GlobalEncounterOwners && !Director->EncounterId.IsNone())
			{
				const FString* Previous = GlobalEncounterOwners->Find(Director->EncounterId);
				if (Previous && *Previous != Director->GetPathName())
				{ Fail(TEXT("WORLD.GLOBAL_ENCOUNTER_ID"), Actor, TEXT("Encounter ID is already owned by another actor in the campaign manifest.")); }
				else { GlobalEncounterOwners->Add(Director->EncounterId, Director->GetPathName()); }
			}
			if (!FMath::IsFinite(Director->CompletionEchoReserve) || Director->CompletionEchoReserve < 0.f
				|| !FMath::IsFinite(Director->RestoreTimeoutSeconds) || Director->RestoreTimeoutSeconds <= 0.f)
			{ Fail(TEXT("WORLD.ENCOUNTER_LIMITS"), Actor, TEXT("Encounter reward reserve and restore timeout must be finite and valid.")); }
			FString Error;
			const auto* Coordination = Director->GetCoordinationComponent();
			if (!Coordination || !Coordination->ValidateComposition(Error))
			{ Fail(TEXT("WORLD.COMPOSITION"), Actor, Coordination ? Error : TEXT("Encounter has no coordination component.")); }
			if (Director->Participants.IsEmpty()) { Fail(TEXT("WORLD.PARTICIPANTS"), Actor, TEXT("Authored encounter has no checkpoint participants.")); }
			for (const auto& Participant : Director->Participants)
			{
				const AActor* NPC = Participant.Character;
				if (!IsValid(NPC) || !Actors.Contains(NPC))
				{ Fail(TEXT("WORLD.PARTICIPANT_RESIDENCY"), Actor, TEXT("Checkpoint participant is absent from the loaded mission actor set.")); continue; }
				if (ParticipantOwners.Contains(NPC) && ParticipantOwners.FindChecked(NPC) != Director)
				{ Fail(TEXT("WORLD.PARTICIPANT_OWNER"), NPC, TEXT("Checkpoint participant belongs to multiple encounter directors.")); }
				else { ParticipantOwners.Add(NPC, Director); }
			}
		}
		if (const auto* Anchor = Cast<ASovCampaignHandoffAnchor>(Actor))
		{
			const auto* Beat = Mission->FindBeat(Anchor->HandoffBeat);
			if (Anchor->AnchorId.IsNone() || AnchorIds.Contains(Anchor->AnchorId) || Anchor->MissionId != Mission->MissionId
				|| !Beat || !Beat->HandoffToProtagonist.IsValid() || Beat->RequiredHandoffAnchorId != Anchor->AnchorId
				|| !Anchor->Destination || Anchor->Destination->GetComponentTransform().ContainsNaN()
				|| !FMath::IsFinite(Anchor->RequestRange) || Anchor->RequestRange <= 0.f)
			{ Fail(TEXT("WORLD.HANDOFF"), Actor, TEXT("Handoff anchor identity, mission beat, destination or request range is invalid.")); }
			else { AnchorIds.Add(Anchor->AnchorId, Actor); }
		}
		if (const auto* Transit = Cast<ASovWorldTransitActor>(Actor))
		{
			if (Transit->TransitId.IsNone() || TransitIds.Contains(Transit->TransitId))
			{ Fail(TEXT("WORLD.TRANSIT_ID"), Actor, TEXT("Transit ID is absent or duplicated.")); }
			else { TransitIds.Add(Transit->TransitId, Actor); }
		}
		TInlineComponentArray<UActorComponent*> Components; Actor->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			FString Error;
			if (bShippingValidation && Component->IsA<UVendorInventoryComponent>())
			{ Fail(TEXT("WORLD.PROHIBITED_VENDOR"), Component, TEXT("Campaign contains a vendor inventory runtime class regardless of asset directory name.")); }
			if (const auto* Evidence = Cast<USovEvidenceSourceComponent>(Component))
			{ ++EvidenceSources; if (!Evidence->ValidateConfiguration(Error)) { Fail(TEXT("WORLD.EVIDENCE"), Component, Error); } }
			if (const auto* WeakPoint = Cast<USovWeakPointComponent>(Component))
			{ ++WeakPoints; if (!WeakPoint->HasValidWeakPointConfiguration()) { Fail(TEXT("WORLD.WEAK_POINT"), Component, TEXT("Weak-point configuration is invalid.")); } }
			if (const auto* Link = Cast<USovCommandLinkComponent>(Component))
			{ ++CommandLinks; if (!Link->HasValidCommandLinkConfiguration()) { Fail(TEXT("WORLD.COMMAND_LINK"), Component, TEXT("Command-link configuration is invalid.")); } }
			if (const auto* Cinematic = Cast<USovCampaignCinematicComponent>(Component))
			{
				++Cinematics;
				const auto* Beat = Mission->FindBeat(Cinematic->BeatId);
				if (!Cinematic->ValidateConfiguration(Error)) { Fail(TEXT("WORLD.CINEMATIC_CONFIG"), Component, Error); }
				if (Cinematic->MissionId != Mission->MissionId || !Beat || !Beat->bRequiresCinematicProof)
				{ Fail(TEXT("WORLD.CINEMATIC_BEAT"), Component, TEXT("Cinematic does not identify this mission's cinematic-proof beat.")); }
				else { ++CinematicBeats.FindOrAdd(Cinematic->BeatId); }
				ULevelSequence* Sequence = Cinematic->Sequence.LoadSynchronous();
				if (!Sequence || !Cinematic->ValidateAuthoredSequence(Sequence, Error))
				{ Fail(TEXT("WORLD.CINEMATIC_SEQUENCE"), Component, Sequence ? Error : TEXT("Cinematic sequence cannot be loaded.")); }
				for (const auto& Participant : Cinematic->Participants)
				{
					if (Participant.bControlledProtagonist) { continue; }
					int32 Matches = 0;
					for (const AActor* Candidate : Actors)
					{
						if (!Candidate->ActorHasTag(Participant.ActorTag)) { continue; }
						++Matches;
						if (!Candidate->IsA<ANarrativeCharacter>())
						{ Fail(TEXT("WORLD.CINEMATIC_CHARACTER"), Candidate, TEXT("Cinematic participant tag resolves to an incompatible character class.")); }
					}
					if (Matches != 1) { Fail(TEXT("WORLD.CINEMATIC_PARTICIPANT"), Component, TEXT("Cinematic participant tag must resolve to exactly one loaded actor.")); }
				}
				for (const auto& Contract : Cinematic->TransitPostconditions)
				{
					const auto* Transit = Contract.Transit.Get();
					if (!IsValid(Transit) || !Actors.Contains(Transit) || Transit->TransitId != Contract.ExpectedTransitId)
					{ Fail(TEXT("WORLD.CINEMATIC_TRANSIT"), Component, TEXT("Cinematic transit postcondition must resolve to the exact loaded mechanism identity.")); }
				}
			}
		}
	}
	if (Starts != 1) { Fail(TEXT("WORLD.ENTRY_START"), World, TEXT("Mission entry tag must match exactly one loaded PlayerStart.")); }
	for (const auto& Beat : Mission->Beats)
	{
		if (Beat.HandoffToProtagonist.IsValid() && !AnchorIds.Contains(Beat.RequiredHandoffAnchorId))
		{ Fail(TEXT("WORLD.MISSING_HANDOFF"), World, TEXT("Mission handoff beat has no valid placed anchor.")); }
		if (Beat.bRequiresCinematicProof && CinematicBeats.FindRef(Beat.BeatId) != 1)
		{ Fail(TEXT("WORLD.MISSING_CINEMATIC"), World, TEXT("Cinematic-proof beat must have exactly one placed cinematic contract.")); }
	}
	UE_LOG(LogSovMission, Display, TEXT("[WORLD.COVERAGE] %s: actors=%d stable=%d encounters=%d cinematics=%d evidence=%d weakpoints=%d commandlinks=%d errors=%d. Runtime-spawned actors and gameplay cleanup require automation/packaged-route gates."),
		*World->GetPathName(), Actors.Num(), StableActors.Num(), Encounters, Cinematics, EvidenceSources, WeakPoints, CommandLinks, Errors);
	return Errors;
}
