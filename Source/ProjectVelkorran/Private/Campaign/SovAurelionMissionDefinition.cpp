// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovAurelionMissionDefinition.h"

#include "Campaign/SovEvidenceDefinition.h"
#include "Characters/SovSeleneCharacter.h"
#include "Characters/SovTarrikCharacter.h"
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "LevelSequence.h"
#include "NativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

#define LOCTEXT_NAMESPACE "SovAurelion"

// These are immutable M12/M13 historical milestones, never global live state. Later Lyric
// death and M16 containment loss remain representable without rewriting these observations.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_ContraryWitnesses, "Campaign.Aurelion.M12.Fact.ContraryWitnessesRecognized");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_SeleneAssent, "Campaign.Aurelion.M13.Fact.SeleneAssent");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_TarrikAssent, "Campaign.Aurelion.M13.Fact.TarrikAssent");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Meridian, "Campaign.Aurelion.M13.Fact.Meridian");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Containment, "Campaign.Aurelion.M13.Fact.Containment");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Authority, "Campaign.Aurelion.M13.Fact.TerminalAuthority");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Release, "Campaign.Aurelion.M13.Fact.PrisonRelease");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Boundary, "Campaign.Aurelion.M13.Fact.PrisonBoundary");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_LyricLife, "Campaign.Aurelion.M13.Fact.LyricLife");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_LyricCorruption, "Campaign.Aurelion.M13.Fact.LyricCorruption");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Stay, "Campaign.Aurelion.M13.Fact.SharedStay");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_RecorderRecipient, "Campaign.Aurelion.M13.Fact.CauldronRecorderRecipient");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Record7283Recipient, "Campaign.Aurelion.M13.Fact.Record7283Recipient");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Pact, "Campaign.Aurelion.M13.Fact.ContainmentPact");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Recognized, "Campaign.Aurelion.Value.Recognized");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Independent, "Campaign.Aurelion.Value.Independent");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Complete, "Campaign.Aurelion.Value.Complete");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Stabilized, "Campaign.Aurelion.Value.Stabilized");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Available, "Campaign.Aurelion.Value.Available");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Closed, "Campaign.Aurelion.Value.Closed");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Alive, "Campaign.Aurelion.Value.Alive");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Unreversed, "Campaign.Aurelion.Value.Unreversed");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Voluntary, "Campaign.Aurelion.Value.Voluntary");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_DirectConcurrence, "Campaign.Aurelion.Value.DirectContraryConcurrenceRequired");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Protect, "Campaign.Aurelion.Choice.ImmediateProtection");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Medical, "Campaign.Aurelion.Outcome.MedicalGroupProtected");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Trapped, "Campaign.Aurelion.Outcome.TrappedCrewProtected");

namespace
{
	FSovCampaignBeatDefinition& AddBeat(USovAurelionMissionDefinition& Mission, FName Id,
		FText Text, ESovObjectiveType Type, FGameplayTag Hero, FName Prior = NAME_None)
	{
		auto& Beat = Mission.Beats.AddDefaulted_GetRef();
		Beat.BeatId = Id; Beat.ObjectiveText = Text; Beat.ObjectiveType = Type; Beat.RequiredProtagonist = Hero;
		if (!Prior.IsNone()) { Beat.PrerequisiteBeats.Add(Prior); }
		return Beat;
	}

	void Protect(FSovCampaignBeatDefinition& Beat, FGameplayTag Key, FGameplayTag Value)
	{
		FSovCampaignStateWrite Fact; Fact.Key = Key; Fact.Value = Value; Fact.bCanonProtected = true;
		Beat.StateWrites.Add(Fact);
	}

	void Scene(USovAurelionMissionDefinition& Mission, FSovCampaignBeatDefinition& Beat, FName SceneId)
	{
		Beat.CinematicId = SceneId; Beat.bRequiresCinematicProof = true; Beat.bCanonGate = true;
		Mission.StorySequences.Add(SceneId, TSoftObjectPtr<ULevelSequence>());
	}

	void ConfigureConvergence(USovAurelionMissionDefinition& Mission, bool bSeleneLead)
	{
		const auto& Tags = FSovGameplayTags::Get();
		Mission.Protagonist = bSeleneLead ? Tags.Character_Player_Selene : Tags.Character_Player_Tarrik;
		Mission.PawnClass = bSeleneLead ? ASovSeleneCharacter::StaticClass() : ASovTarrikCharacter::StaticClass();
		auto& Alternate = Mission.AlternateProtagonists.AddDefaulted_GetRef();
		Alternate.Protagonist = bSeleneLead ? Tags.Character_Player_Tarrik : Tags.Character_Player_Selene;
		Alternate.PawnClass = bSeleneLead ? ASovTarrikCharacter::StaticClass() : ASovSeleneCharacter::StaticClass();
		for (bool bSelene : {false, true})
		{
			auto& Companion = Mission.ProtagonistCompanions.AddDefaulted_GetRef();
			Companion.Protagonist = bSelene ? Tags.Character_Player_Selene : Tags.Character_Player_Tarrik;
			Companion.CompanionId = bSelene ? FName(TEXT("Selene")) : FName(TEXT("Tarrik"));
			Companion.CompanionClass = ASovProtagonistCompanionCharacter::StaticClass();
			Companion.EntryAnchorTag = bSelene ? FName(TEXT("Aurelion_SeleneCompanion")) : FName(TEXT("Aurelion_TarrikCompanion"));
			Mission.AllowedCompanionIds.Add(Companion.CompanionId);
		}
	}

	void AddTarrikCorridor(USovAurelionMissionDefinition& Mission)
	{
		const auto Tarrik = FSovGameplayTags::Get().Character_Player_Tarrik;
		AddBeat(Mission, TEXT("TarrikArrival"), LOCTEXT("TarrikArrival", "Reach the survivors in the lower terminal."),
			ESovObjectiveType::ReachEscape, Tarrik);
		auto& Hold = AddBeat(Mission, TEXT("HoldMixedSurvivorCorridor"),
			LOCTEXT("HoldMixedSurvivorCorridor", "Protect the survivors and break the hostile formation."),
			ESovObjectiveType::ProtectHold, Tarrik, TEXT("TarrikArrival"));
		Hold.RequiredEncounterId = TEXT("M12_MixedSurvivorCorridor");
		Hold.MinimumProtectedParticipants = 2;
		AddBeat(Mission, TEXT("SecureTarrikRoute"), LOCTEXT("SecureTarrikRoute", "Reach the secured terminal approach."),
			ESovObjectiveType::ReachEscape, Tarrik, TEXT("HoldMixedSurvivorCorridor"));
	}

	void AddHandoff(USovAurelionMissionDefinition& Mission, FName BeatId, FName Prior, FGameplayTag From,
		FGameplayTag To, FName Anchor, FText Text)
	{
		auto& Beat = AddBeat(Mission, BeatId, Text, ESovObjectiveType::ReachEscape, From, Prior);
		Beat.HandoffToProtagonist = To; Beat.RequiredHandoffAnchorId = Anchor;
	}

	bool SameWrites(const TArray<FSovCampaignStateWrite>& A, const TArray<FSovCampaignStateWrite>& B)
	{
		if (A.Num() != B.Num()) { return false; }
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{ if (A[Index].Key != B[Index].Key || A[Index].Value != B[Index].Value || A[Index].bCanonProtected != B[Index].bCanonProtected) { return false; } }
		return true;
	}
}

USovAurelionTarrikPreparationMissionDefinition::USovAurelionTarrikPreparationMissionDefinition()
{
	MissionId = TEXT("M12_AurelionTarrikPreparation");
	DisplayName = LOCTEXT("TarrikPreparation", "Aurelion: Tarrik Corridor Preparation");
	Protagonist = FSovGameplayTags::Get().Character_Player_Tarrik;
	PawnClass = ASovTarrikCharacter::StaticClass();
	EntryPlayerStartTag = TEXT("Aurelion_TarrikEntry");
	AddTarrikCorridor(*this);
}

USovAurelionFireAndFrostMissionDefinition::USovAurelionFireAndFrostMissionDefinition()
{
	MissionId = TEXT("M12_FireAndFrost"); DisplayName = LOCTEXT("FireAndFrost", "Fire and Frost");
	EntryPlayerStartTag = TEXT("Aurelion_TarrikEntry");
	AllowedSuccessorMissions.Add(TEXT("M13_ContraryWitness"));
	ConfigureConvergence(*this, false); AddTarrikCorridor(*this);
	const auto& Tags = FSovGameplayTags::Get();
	AddHandoff(*this, TEXT("HandoffToSelene"), TEXT("SecureTarrikRoute"), Tags.Character_Player_Tarrik,
		Tags.Character_Player_Selene, TEXT("M12_SeleneEntry"), LOCTEXT("SeleneHandoff", "Continue through Selene's terminal approach."));
	AddBeat(*this, TEXT("SeleneArrival"), LOCTEXT("SeleneArrival", "Survey the alternate terminal route."),
		ESovObjectiveType::InvestigateAuthenticate, Tags.Character_Player_Selene, TEXT("HandoffToSelene"));
	auto& Network = AddBeat(*this, TEXT("SeverTerminalNetwork"), LOCTEXT("SeverTerminalNetwork", "Evade the sensors and sever the hostile command network."),
		ESovObjectiveType::EliminateDisable, Tags.Character_Player_Selene, TEXT("SeleneArrival"));
	Network.RequiredEncounterId = TEXT("M12_SeleneCommandNetwork");
	auto& Convergence = AddBeat(*this, TEXT("ForcedCooperation"), LOCTEXT("ForcedCooperation", "Survive the terminal confrontation together."),
		ESovObjectiveType::Survive, Tags.Character_Player_Selene, TEXT("SeverTerminalNetwork"));
	Scene(*this, Convergence, TEXT("M12_ForcedCooperation"));
	auto& Quarantine = AddBeat(*this, TEXT("QuarantineSeparation"), LOCTEXT("QuarantineSeparation", "Respond to the terminal quarantine."),
		ESovObjectiveType::Survive, Tags.Character_Player_Selene, TEXT("ForcedCooperation"));
	Scene(*this, Quarantine, TEXT("M12_QuarantineSeparation"));
	auto& Witnesses = AddBeat(*this, TEXT("ContraryWitnessRecognized"), LOCTEXT("ContraryWitnessRecognized", "Witness the terminal's recognition."),
		ESovObjectiveType::InvestigateAuthenticate, Tags.Character_Player_Selene, TEXT("QuarantineSeparation"));
	Scene(*this, Witnesses, TEXT("M12_ContraryWitnessRecognized"));
	Protect(Witnesses, TAG_Aurelion_ContraryWitnesses, TAG_Aurelion_Recognized);
}

USovAurelionContraryWitnessMissionDefinition::USovAurelionContraryWitnessMissionDefinition()
{
	MissionId = TEXT("M13_ContraryWitness"); DisplayName = LOCTEXT("ContraryWitness", "Contrary Witness");
	EntryPlayerStartTag = TEXT("Aurelion_SeleneConvergence");
	ConfigureConvergence(*this, true);
	const auto& Tags = FSovGameplayTags::Get();
	auto& Position = AddBeat(*this, TEXT("ContraryPosition"), LOCTEXT("ContraryPosition", "Bring Tarrik to the opposite terminal position."),
		ESovObjectiveType::FollowEscort, Tags.Character_Player_Selene);
	FSovCampaignStateWrite Recognized; Recognized.Key = TAG_Aurelion_ContraryWitnesses; Recognized.Value = TAG_Aurelion_Recognized;
	Position.RequiredState.Add(Recognized);
	Position.bRequiresCoActionProof = true; Position.RequiredCompanionId = TEXT("Tarrik"); Position.RequiredCoActionAnchorId = TEXT("M13_TarrikContraryPosition");
	bAllowJointResonance = true; AllowedResonanceTypes.Add(ESovResonanceType::FormationBreach);
	ResonancePrerequisiteBeats.Add(TEXT("ContraryPosition"));
	auto& Eclipse = AddBeat(*this, TEXT("EclipseEscalation"), LOCTEXT("EclipseEscalation", "Break the Eclipse formation with a coordinated opening."),
		ESovObjectiveType::Survive, Tags.Character_Player_Selene, TEXT("ContraryPosition"));
	Eclipse.RequiredEncounterId = TEXT("M13_EclipseEscalation");
	auto& Selene = AddBeat(*this, TEXT("SeleneIndependentAssent"), LOCTEXT("SeleneIndependentAssent", "Hear Selene's independent assent."),
		ESovObjectiveType::InteractOperate, Tags.Character_Player_Selene, TEXT("EclipseEscalation"));
	Scene(*this, Selene, TEXT("M13_SeleneIndependentAssent")); Protect(Selene, TAG_Aurelion_SeleneAssent, TAG_Aurelion_Independent);
	AddHandoff(*this, TEXT("HandoffToTarrikAssent"), TEXT("SeleneIndependentAssent"), Tags.Character_Player_Selene,
		Tags.Character_Player_Tarrik, TEXT("M13_TarrikAssent"), LOCTEXT("TarrikAssentHandoff", "Continue from Tarrik's terminal position."));
	auto& Tarrik = AddBeat(*this, TEXT("TarrikIndependentAssent"), LOCTEXT("TarrikIndependentAssent", "Hear Tarrik's independent assent."),
		ESovObjectiveType::InteractOperate, Tags.Character_Player_Tarrik, TEXT("HandoffToTarrikAssent"));
	Scene(*this, Tarrik, TEXT("M13_TarrikIndependentAssent")); Protect(Tarrik, TAG_Aurelion_TarrikAssent, TAG_Aurelion_Independent);
	auto& Containment = AddBeat(*this, TEXT("MeridianContainment"), LOCTEXT("MeridianContainment", "Stabilize containment while withholding release."),
		ESovObjectiveType::InteractOperate, Tags.Character_Player_Tarrik, TEXT("TarrikIndependentAssent"));
	Containment.PrerequisiteBeats.Add(TEXT("SeleneIndependentAssent"));
	Scene(*this, Containment, TEXT("M13_MeridianContainment"));
	Protect(Containment, TAG_Aurelion_Meridian, TAG_Aurelion_Complete);
	Protect(Containment, TAG_Aurelion_Containment, TAG_Aurelion_Stabilized);
	Protect(Containment, TAG_Aurelion_Authority, TAG_Aurelion_Available);
	Protect(Containment, TAG_Aurelion_Release, Tags.Campaign_Value_Withheld);
	Protect(Containment, TAG_Aurelion_Boundary, TAG_Aurelion_Closed);
	Protect(Containment, TAG_Aurelion_LyricLife, TAG_Aurelion_Alive);
	Protect(Containment, TAG_Aurelion_LyricCorruption, TAG_Aurelion_Unreversed);
	auto& Fifth = AddBeat(*this, TEXT("FifthWitness"), LOCTEXT("FifthWitness", "Observe the Fifth Witness and the containment warning."),
		ESovObjectiveType::InvestigateAuthenticate, Tags.Character_Player_Tarrik, TEXT("MeridianContainment"));
	Scene(*this, Fifth, TEXT("M13_FifthWitness")); Fifth.CriticalEvidence.Add(nullptr);
	FSovCampaignChoiceGroup Group; Group.GroupId = TEXT("ImmediateProtection"); Group.ReconciliationBeatId = TEXT("ProtectEscape");
	Group.ReconciliationNote = LOCTEXT("ProtectionReconciliation", "Immediate protection changes the escape and its acknowledgment; containment remains closed and release withheld.");
	ChoiceGroups.Add(Group);
	for (bool bMedical : {true, false})
	{
		auto& Choice = AddBeat(*this, bMedical ? FName(TEXT("ProtectMedicalGroup")) : FName(TEXT("ProtectTrappedCrew")),
			bMedical ? LOCTEXT("ProtectMedicalGroup", "Prioritize the wounded group.") : LOCTEXT("ProtectTrappedCrew", "Prioritize the trapped crew."),
			ESovObjectiveType::ChoosePrioritize, Tags.Character_Player_Tarrik, TEXT("FifthWitness"));
		Choice.bOptional = true; Choice.bInteractiveChoice = true; Choice.ChoiceGroupId = Group.GroupId;
		auto& Consequence = Choice.Consequences.AddDefaulted_GetRef();
		Consequence.ConsequenceId = bMedical ? FName(TEXT("M13_MedicalGroupProtected")) : FName(TEXT("M13_TrappedCrewProtected"));
		Consequence.SubjectIds.Add(bMedical ? FName(TEXT("AurelionMedicalGroup")) : FName(TEXT("AurelionTrappedCrew")));
		Consequence.ChoiceTag = TAG_Aurelion_Protect; Consequence.OutcomeTag = bMedical ? TAG_Aurelion_Medical : TAG_Aurelion_Trapped;
		Consequence.WitnessIds = {TEXT("Tarrik"), TEXT("Selene")}; Consequence.Publicity = ESovRecordPublicity::Shared;
		Consequence.ConsumerIds = {TEXT("M13_ProtectionEscape"), TEXT("M13_ProtectionAftermath")};
	}
	auto& Escape = AddBeat(*this, TEXT("ProtectEscape"), LOCTEXT("ProtectEscape", "Protect the selected survivors through the escape."),
		ESovObjectiveType::ProtectHold, Tags.Character_Player_Tarrik, TEXT("FifthWitness"));
	Escape.RequiredChoiceGroups.Add(Group.GroupId); Escape.RequiredEncounterId = TEXT("M13_ProtectionEscape");
	auto& Stay = AddBeat(*this, TEXT("VoluntaryStay"), LOCTEXT("VoluntaryStay", "Stay together and speak freely."),
		ESovObjectiveType::InteractOperate, Tags.Character_Player_Tarrik, TEXT("ProtectEscape"));
	Scene(*this, Stay, TEXT("M13_VoluntaryStay")); Protect(Stay, TAG_Aurelion_Stay, TAG_Aurelion_Voluntary);
	auto& Record = AddBeat(*this, TEXT("Record7283Received"), LOCTEXT("Record7283Received", "Receive Record 7283 from Selene."),
		ESovObjectiveType::RetrieveDeliver, Tags.Character_Player_Tarrik, TEXT("VoluntaryStay"));
	Scene(*this, Record, TEXT("M13_Record7283Received")); Record.CriticalEvidence.Add(nullptr);
	Protect(Record, TAG_Aurelion_Record7283Recipient, Tags.Character_Player_Tarrik);
	AddHandoff(*this, TEXT("HandoffToSeleneAftermath"), TEXT("Record7283Received"), Tags.Character_Player_Tarrik,
		Tags.Character_Player_Selene, TEXT("M13_SeleneAftermath"), LOCTEXT("SeleneAftermathHandoff", "Continue the exchange as Selene."));
	auto& Recorder = AddBeat(*this, TEXT("CauldronRecorderReceived"), LOCTEXT("CauldronRecorderReceived", "Receive Tarrik's Cauldron recorder and consider the shared warning."),
		ESovObjectiveType::RetrieveDeliver, Tags.Character_Player_Selene, TEXT("HandoffToSeleneAftermath"));
	Scene(*this, Recorder, TEXT("M13_CauldronRecorderReceived")); Recorder.CriticalEvidence.Add(nullptr); Recorder.CriticalEvidence.Add(nullptr);
	Protect(Recorder, TAG_Aurelion_RecorderRecipient, Tags.Character_Player_Selene);
	auto& Pact = AddBeat(*this, TEXT("ContainmentPact"), LOCTEXT("ContainmentPact", "Agree to withhold release without direct contrary concurrence."),
		ESovObjectiveType::InteractOperate, Tags.Character_Player_Selene, TEXT("CauldronRecorderReceived"));
	Scene(*this, Pact, TEXT("M13_ContainmentPact")); Protect(Pact, TAG_Aurelion_Pact, TAG_Aurelion_DirectConcurrence);
}

bool USovAurelionMissionDefinition::ValidateAurelionContract(FString& OutError) const
{
	OutError.Reset();
	const USovAurelionMissionDefinition* Contract = nullptr;
	if (IsA<USovAurelionTarrikPreparationMissionDefinition>()) { Contract = GetDefault<USovAurelionTarrikPreparationMissionDefinition>(); }
	else if (IsA<USovAurelionFireAndFrostMissionDefinition>()) { Contract = GetDefault<USovAurelionFireAndFrostMissionDefinition>(); }
	else if (IsA<USovAurelionContraryWitnessMissionDefinition>()) { Contract = GetDefault<USovAurelionContraryWitnessMissionDefinition>(); }
	const auto Fail = [&OutError](const FString& Error) { OutError = Error; return false; };
	if (!Contract) { return Fail(TEXT("Aurelion requires one of the three native mission contracts.")); }
	if (MissionId != Contract->MissionId || Protagonist != Contract->Protagonist || bCompletesCampaign
		|| Beats.Num() != Contract->Beats.Num() || ChoiceGroups.Num() != Contract->ChoiceGroups.Num()
		|| AllowedSuccessorMissions != Contract->AllowedSuccessorMissions
		|| bAllowJointResonance != Contract->bAllowJointResonance || AllowedResonanceTypes != Contract->AllowedResonanceTypes
		|| ResonancePrerequisiteBeats != Contract->ResonancePrerequisiteBeats || AllowedCompanionIds != Contract->AllowedCompanionIds)
	{ return Fail(TEXT("Aurelion mission identity, progression and Resonance permissions must preserve the native contract.")); }
	TSet<FName> Seen;
	for (const auto& Expected : Contract->Beats)
	{
		const auto* Beat = FindBeat(Expected.BeatId);
		if (!Beat || Seen.Contains(Beat->BeatId) || Beat->RequiredProtagonist != Expected.RequiredProtagonist
			|| Beat->PrerequisiteBeats != Expected.PrerequisiteBeats || Beat->bOptional != Expected.bOptional
			|| Beat->bCanonGate != Expected.bCanonGate || Beat->HandoffToProtagonist != Expected.HandoffToProtagonist
			|| Beat->RequiredHandoffAnchorId != Expected.RequiredHandoffAnchorId || Beat->CinematicId != Expected.CinematicId
			|| Beat->bRequiresCinematicProof != Expected.bRequiresCinematicProof || Beat->bRequiresCoActionProof != Expected.bRequiresCoActionProof
			|| Beat->RequiredCompanionId != Expected.RequiredCompanionId || Beat->RequiredCoActionAnchorId != Expected.RequiredCoActionAnchorId
			|| Beat->RequiredEncounterId != Expected.RequiredEncounterId || Beat->ChoiceGroupId != Expected.ChoiceGroupId
			|| Beat->MinimumProtectedParticipants != Expected.MinimumProtectedParticipants
			|| Beat->bInteractiveChoice != Expected.bInteractiveChoice || Beat->RequiredChoiceGroups != Expected.RequiredChoiceGroups
			|| !SameWrites(Beat->StateWrites, Expected.StateWrites) || !SameWrites(Beat->RequiredState, Expected.RequiredState)
			|| Beat->GrantedKnowledge != Expected.GrantedKnowledge || Beat->RequiredKnowledge != Expected.RequiredKnowledge
			|| Beat->Consequences != Expected.Consequences || Beat->RelationshipMemories != Expected.RelationshipMemories
			|| Beat->CriticalEvidence.Num() != Expected.CriticalEvidence.Num())
		{ return Fail(FString::Printf(TEXT("Aurelion beat %s changed its native canon, control, proof, evidence or prerequisite contract."), *Expected.BeatId.ToString())); }
		Seen.Add(Beat->BeatId);
	}
	if (!ValidateObjectives(OutError)) { return false; }
	return true;
}

bool USovAurelionMissionDefinition::ValidateDefinition(FString& OutError) const
{
	if (!ValidateAurelionContract(OutError)) { return false; }
	if (Map.IsNull()) { OutError = TEXT("Aurelion requires an authored map; native scaffolds are not playable content."); return false; }
	if (!Super::ValidateDefinition(OutError)) { return false; }
	int32 SceneCount = 0;
	for (const auto& Beat : Beats)
	{
		if (!Beat.bRequiresCinematicProof) { continue; }
		++SceneCount;
		const auto* Sequence = StorySequences.Find(Beat.CinematicId);
		if (!Sequence || Sequence->IsNull() || !Sequence->LoadSynchronous())
		{
			OutError = FString::Printf(TEXT("Aurelion scene %s requires its authored LevelSequence and matching placed native cinematic component."), *Beat.CinematicId.ToString());
			return false;
		}
	}
	if (StorySequences.Num() != SceneCount) { OutError = TEXT("Aurelion scene bindings must exactly match its native cinematic beats."); return false; }
	if (IsA<USovAurelionContraryWitnessMissionDefinition>())
	{
		const auto MatchesEvidence = [this](FName BeatId, const TArray<FName>& Ids)
		{
			const auto* Beat = FindBeat(BeatId);
			if (!Beat || Beat->CriticalEvidence.Num() != Ids.Num()) { return false; }
			for (int32 Index = 0; Index < Ids.Num(); ++Index)
			{ if (!Beat->CriticalEvidence[Index] || Beat->CriticalEvidence[Index]->EvidenceId != Ids[Index]) { return false; } }
			return true;
		};
		if (!MatchesEvidence(TEXT("FifthWitness"), {TEXT("Aurelion_FifthWitness")})
			|| !MatchesEvidence(TEXT("Record7283Received"), {TEXT("Record7283")})
			|| !MatchesEvidence(TEXT("CauldronRecorderReceived"), {TEXT("CauldronRecorder"), TEXT("Aurelion_FifthWitness")}))
		{
			OutError = TEXT("M13 requires the Fifth Witness record and protagonist-specific Record7283/CauldronRecorder critical evidence bindings.");
			return false;
		}
	}
	return true;
}

bool USovAurelionMissionDefinition::MatchesStorySequence(FName BeatId, const TSoftObjectPtr<ULevelSequence>& Sequence) const
{
	const auto* Beat = FindBeat(BeatId);
	if (!Beat || !Beat->bRequiresCinematicProof || Beat->CinematicId.IsNone() || Sequence.IsNull()) { return false; }
	const auto* Expected = StorySequences.Find(Beat->CinematicId);
	return Expected && !Expected->IsNull() && Expected->ToSoftObjectPath() == Sequence.ToSoftObjectPath();
}

#undef LOCTEXT_NAMESPACE
