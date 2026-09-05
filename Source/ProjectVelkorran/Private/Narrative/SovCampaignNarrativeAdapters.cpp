// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Narrative/SovCampaignNarrativeAdapters.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovEvidenceSourceComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Companions/SovCompanionComponent.h"
#include "Progression/SovTechniqueComponent.h"
#include "Sovereign/SovGameplayTags.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Tales/TalesComponent.h"

USovCampaignNarrativeCondition::USovCampaignNarrativeCondition()
{
	ConditionFilter = EConditionFilter::CF_OnlyPlayers;
	PartyConditionPolicy = EPartyConditionPolicy::PartyLeaderPasses;
}
bool USovCampaignNarrativeCondition::CheckCondition_Implementation(APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!IsValid(Controller) || Controller->GetPawn() != Target) { return false; }
	const auto* State = Controller->FindComponentByClass<USovCampaignStateComponent>();
	if (!State || !State->IsStateValid()) { return false; }
	const FGameplayTag Hero = State->GetActiveProtagonist();
	const auto* PlayerPawn = Cast<ASovPlayerCharacterBase>(Target);
	if (!PlayerPawn || PlayerPawn->GetProtagonistIdentityTag() != Hero
		|| (Hero != FSovGameplayTags::Get().Character_Player_Tarrik && Hero != FSovGameplayTags::Get().Character_Player_Selene)) { return false; }
	const FName Observer = ObserverId.IsNone() ? FName(Hero == FSovGameplayTags::Get().Character_Player_Tarrik ? TEXT("Tarrik") : TEXT("Selene")) : ObserverId;
	const FName Mission = MissionId.IsNone() && State->GetActiveMission() ? State->GetActiveMission()->MissionId : MissionId;
	switch (Query)
	{
	case ESovCampaignQuery::BeatComplete: return State->IsBeatComplete(Mission, RecordId);
	case ESovCampaignQuery::MissionComplete: return State->IsMissionComplete(Mission);
	case ESovCampaignQuery::Knowledge: return !RequiredKnowledge.IsEmpty() && State->HasKnowledge(Hero, RequiredKnowledge);
	case ESovCampaignQuery::EvidenceStage: return MinimumEvidenceStage > ESovEvidenceStage::Unknown && State->GetEvidenceStage(RecordId, Hero) >= MinimumEvidenceStage;
	case ESovCampaignQuery::EvidenceKnownBy: return State->ObserverKnowsEvidence(RecordId, Observer);
	case ESovCampaignQuery::ConsequenceKnown:
	case ESovCampaignQuery::ConsequencePayload:
	{
		FSovConsequenceRecord Record;
		return State->FindConsequence(RecordId, Observer, Record)
			&& (Query == ESovCampaignQuery::ConsequenceKnown || (RequiredPayload.IsValid() && Record.Definition.Payload.Contains(RequiredPayload)));
	}
	case ESovCampaignQuery::RelationshipMemory: return State->HasRelationshipMemory(Observer, SubjectId, MemoryType, RecordId);
	case ESovCampaignQuery::CompanionAvailable:
	{
		auto* Player = Cast<ASovPlayerCharacterBase>(Target);
		if (!Player || RecordId.IsNone()) { return false; }
		for (TActorIterator<AActor> It(Controller->GetWorld()); It; ++It)
		{
			if (auto* Companion = It->FindComponentByClass<USovCompanionComponent>(); Companion && Companion->CompanionId == RecordId)
			{
				FString Reason;
				if (Companion->CanRequestCommand(Player, ESovCompanionCommand::Regroup, Player, Reason)) { return true; }
			}
		}
		return false;
	}
	case ESovCampaignQuery::NarrativeTaskCompleted:
	{
		auto* Tales = Controller->FindComponentByClass<UTalesComponent>();
		return Tales && RequiredNarrativeTask && !RecordId.IsNone() && Tales->HasCompletedTask(RequiredNarrativeTask, RecordId.ToString(), 1);
	}
	case ESovCampaignQuery::TechniqueOwned:
	{
		const auto* PlayerState = Controller->GetPlayerState<APlayerState>();
		const auto* Techniques = PlayerState ? PlayerState->FindComponentByClass<USovTechniqueComponent>() : nullptr;
		return Techniques && Techniques->IsTechniqueStateValid() && RequiredTechnique && Techniques->HasPerk(RequiredTechnique);
	}
	default: return false;
	}
}
FString USovCampaignNarrativeCondition::GetGraphDisplayText_Implementation()
{
	return FString::Printf(TEXT("Campaign query %d: %s"), static_cast<int32>(Query), *RecordId.ToString());
}
USovCampaignNarrativeEvent::USovCampaignNarrativeEvent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	EventRuntime = EEventRuntime::End;
	EventFilter = EEventFilter::EF_OnlyPlayers;
	bRefireOnLoad = false;
	PartyEventPolicy = EPartyEventPolicy::PartyLeader;
}
void USovCampaignNarrativeEvent::ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent)
{
	if (!IsValid(Controller) || !Controller->HasAuthority() || Controller->GetPawn() != Target) { return; }
	auto* State = Controller->FindComponentByClass<USovCampaignStateComponent>();
	if (!State || !State->IsStateValid()) { return; }
	FString ConfigurationError;
	if (!ValidateConfiguration(State->GetActiveMission(), ConfigurationError)) { return; }
	if (Action == ESovCampaignNarrativeAction::CompleteBeat) { State->CompleteBeat(BeatId, false); return; }
	if (Action != ESovCampaignNarrativeAction::AcquireEvidence || !EvidenceSourceId.IsValid()) { return; }
	USovEvidenceSourceComponent* Source = nullptr;
	for (TActorIterator<AActor> It(Controller->GetWorld()); It; ++It)
	{
		TInlineComponentArray<USovEvidenceSourceComponent*> Components; It->GetComponents(Components);
		for (auto* Candidate : Components)
		{
			if (Candidate->SourceId == EvidenceSourceId)
			{
				if (Source) { return; } // Ambiguous authored source identity cannot grant a record.
				Source = Candidate;
			}
		}
	}
	if (Source) { Source->TryAcquire(Controller); }
}
FString USovCampaignNarrativeEvent::GetGraphDisplayText_Implementation()
{
	return Action == ESovCampaignNarrativeAction::CompleteBeat ? FString::Printf(TEXT("Commit campaign beat %s"), *BeatId.ToString())
		: FString::Printf(TEXT("Acquire verified evidence source %s"), *EvidenceSourceId.ToString());
}


bool USovCampaignNarrativeCondition::ValidateConfiguration(const USovCampaignDefinition* Mission, FString& OutError) const
{
	bool bValid = false;
	switch (Query)
	{
	case ESovCampaignQuery::BeatComplete:
		bValid = !RecordId.IsNone() && (!Mission || (!MissionId.IsNone() && MissionId != Mission->MissionId) || Mission->FindBeat(RecordId)); break;
	case ESovCampaignQuery::MissionComplete: bValid = !MissionId.IsNone() || Mission; break;
	case ESovCampaignQuery::Knowledge: bValid = !RequiredKnowledge.IsEmpty(); break;
	case ESovCampaignQuery::EvidenceStage: bValid = !RecordId.IsNone() && MinimumEvidenceStage > ESovEvidenceStage::Unknown && MinimumEvidenceStage <= ESovEvidenceStage::Distributed; break;
	case ESovCampaignQuery::EvidenceKnownBy:
	case ESovCampaignQuery::ConsequenceKnown:
	case ESovCampaignQuery::CompanionAvailable: bValid = !RecordId.IsNone(); break;
	case ESovCampaignQuery::ConsequencePayload: bValid = !RecordId.IsNone() && RequiredPayload.IsValid(); break;
	case ESovCampaignQuery::RelationshipMemory: bValid = !SubjectId.IsNone() && MemoryType <= ESovRelationshipMemoryType::BoundaryCrossed; break;
	case ESovCampaignQuery::TechniqueOwned: bValid = RequiredTechnique != nullptr; break;
	case ESovCampaignQuery::NarrativeTaskCompleted: bValid = RequiredNarrativeTask && !RecordId.IsNone(); break;
	default: break;
	}
	OutError = bValid ? FString() : TEXT("Campaign condition has an unknown query, missing stable reference, invalid evidence/memory kind or unconfigured typed payload.");
	return bValid;
}

bool USovCampaignNarrativeEvent::ValidateConfiguration(const USovCampaignDefinition* Mission, FString& OutError) const
{
	bool bValid = false;
	if (Action == ESovCampaignNarrativeAction::CompleteBeat)
	{
		const auto* Beat = Mission ? Mission->FindBeat(BeatId) : nullptr;
		bValid = !BeatId.IsNone() && (!Mission || (Beat && !Beat->bRequiresCinematicProof && !Beat->bRequiresCoActionProof && !Beat->HandoffToProtagonist.IsValid()));
	}
	else if (Action == ESovCampaignNarrativeAction::AcquireEvidence) { bValid = EvidenceSourceId.IsValid(); }
	bValid &= !bRefireOnLoad && EventRuntime == EEventRuntime::End && EventFilter == EEventFilter::EF_OnlyPlayers;
	OutError = bValid ? FString() : TEXT("Campaign events require an end-only, non-refiring native beat or a physical source GUID. Handoff/co-action/cinematic proof cannot be authored as a dialogue write.");
	return bValid;
}
