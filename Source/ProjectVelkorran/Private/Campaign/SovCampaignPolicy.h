#pragma once

// Pure policy shared by runtime campaign transactions and portable regression tests.
namespace SovCampaignPolicy
{
enum class Result { Allowed, Duplicate, Invalid, Prerequisite, Knowledge, Protected, NotViewed };

inline Result CompleteBeat(bool ActiveMission, bool KnownBeat, bool AlreadyComplete,
	bool PrerequisitesMet, bool KnowledgeMet, bool ProtectedWritesValid,
	bool SkipPresentation, bool HasCinematic, bool PreviouslyViewed, bool InteractiveChoice)
{
	if (!ActiveMission || !KnownBeat) { return Result::Invalid; }
	if (AlreadyComplete) { return Result::Duplicate; }
	if (!PrerequisitesMet) { return Result::Prerequisite; }
	if (!KnowledgeMet) { return Result::Knowledge; }
	if (!ProtectedWritesValid) { return Result::Protected; }
	if (SkipPresentation && (!HasCinematic || !PreviouslyViewed || InteractiveChoice)) { return Result::NotViewed; }
	return Result::Allowed;
}

inline bool CanHandoff(bool Authority, bool AlreadyTransitioning, bool SourceReady,
	bool SourceAlive, bool MandatoryBeatsComplete, bool DeclaredSuccessor, bool DifferentIdentity)
{
	return Authority && !AlreadyTransitioning && SourceReady && SourceAlive
		&& MandatoryBeatsComplete && DeclaredSuccessor && DifferentIdentity;
}

inline bool MayCommitAsync(unsigned long long CurrentEpoch, unsigned long long CallbackEpoch,
	bool Pending, bool SourceStillOwned)
{
	return CurrentEpoch != 0 && CurrentEpoch == CallbackEpoch && Pending && SourceStillOwned;
}
}
