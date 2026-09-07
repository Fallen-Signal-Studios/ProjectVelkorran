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
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Protect, "Campaign.Aurelion.Choice.EvacuationPriority");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_WestFirst, "Campaign.Aurelion.Outcome.WestStretchersFirst");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_EastFirst, "Campaign.Aurelion.Outcome.EastWalkersFirst");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_DominionResonator, "Campaign.Aurelion.M12.Fact.DominionResonator");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_ReformationCage, "Campaign.Aurelion.M12.Fact.ReformationCage");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_ThreatSharing, "Campaign.Aurelion.M12.Fact.ThreatSharing");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_CommandAuthority, "Campaign.Aurelion.M12.Fact.CommandAuthority");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_WestSurvivors, "Campaign.Aurelion.M12.Fact.WestSurvivors");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_EastSurvivors, "Campaign.Aurelion.M12.Fact.EastSurvivors");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Grammar, "Campaign.Aurelion.M13.Fact.ContainmentGrammar");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Crownmark, "Campaign.Aurelion.M13.Fact.Crownmark");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Departure, "Campaign.Aurelion.M13.Fact.Departure");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Destroyed, "Campaign.Aurelion.Value.Destroyed");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Isolated, "Campaign.Aurelion.Value.IsolatedThreatDataOnly");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Retained, "Campaign.Aurelion.Value.AuthorityRetained");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Propagated, "Campaign.Aurelion.Value.Propagated");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Integrated, "Campaign.Aurelion.Value.Integrated");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Aurelion_Separate, "Campaign.Aurelion.Value.SeparateDepartures");

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

	bool SameReceiverIds(const TSet<FName>& A, const TSet<FName>& B)
	{
		if (A.Num() != B.Num()) { return false; }
		for (FName Id : A) { if (!B.Contains(Id)) { return false; } }
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
	ConfigureConvergence(*this, false);
	CompanionActivationBeat = TEXT("HandoffToTarrikRescue");
	const auto& Tags = FSovGameplayTags::Get();
	// Z00-Z04: separate approaches. This is not the mixed-survivor preparation fixture.
	AddBeat(*this, TEXT("TarrikArrival"), LOCTEXT("LayoutTarrikArrival", "Advance with the Dominion squad toward the pressure hall."),
		ESovObjectiveType::ReachEscape, Tags.Character_Player_Tarrik);
	auto& Pressure = AddBeat(*this, TEXT("PressureHall"), LOCTEXT("PressureHall", "Break through the pressure hall."),
		ESovObjectiveType::EliminateDisable, Tags.Character_Player_Tarrik, TEXT("TarrikArrival"));
	Pressure.RequiredEncounterId = TEXT("M12_E1_PressureHall");
	AddBeat(*this, TEXT("SecureTarrikRoute"), LOCTEXT("LayoutSecureTarrikRoute", "Secure the Dominion approach to the terminal."),
		ESovObjectiveType::ReachEscape, Tags.Character_Player_Tarrik, TEXT("PressureHall"));
	AddHandoff(*this, TEXT("HandoffToSelene"), TEXT("SecureTarrikRoute"), Tags.Character_Player_Tarrik,
		Tags.Character_Player_Selene, TEXT("M12_SeleneEntry"), LOCTEXT("SeleneHandoff", "Continue through Selene's terminal approach."));
	Beats.Last().bIsolatedPerspectiveCut = true;
	AddBeat(*this, TEXT("SeleneArrival"), LOCTEXT("SeleneArrival", "Survey the alternate terminal route."),
		ESovObjectiveType::InvestigateAuthenticate, Tags.Character_Player_Selene, TEXT("HandoffToSelene"));
	auto& Relay = AddBeat(*this, TEXT("RelayOverlook"), LOCTEXT("RelayOverlook", "Disable both receivers and clear the relay overlook."),
		ESovObjectiveType::EliminateDisable, Tags.Character_Player_Selene, TEXT("SeleneArrival"));
	Relay.RequiredEncounterId = TEXT("M12_E2_RelayOverlook");
	Relay.RequiredReceiverIds = {TEXT("M12_E2_ReceiverWest"), TEXT("M12_E2_ReceiverEast")};

	// Z05-Z06: meeting and first shared fight precede the trapped-marine rescue and grounding.
	auto& Meeting = AddBeat(*this, TEXT("MeetingAndCarrierRescue"), LOCTEXT("MeetingAndCarrierRescue", "Meet the other bearer and help the carrier."),
		ESovObjectiveType::Survive, Tags.Character_Player_Selene, TEXT("RelayOverlook"));
	Scene(*this, Meeting, TEXT("M12_MeetingAndCarrierRescue"));
	AddHandoff(*this, TEXT("HandoffToTarrikRescue"), TEXT("MeetingAndCarrierRescue"), Tags.Character_Player_Selene,
		Tags.Character_Player_Tarrik, TEXT("M12_TarrikSharedBreach"), LOCTEXT("TarrikRescueHandoff", "Lead the shared breach as Tarrik."));
	auto& Shared = AddBeat(*this, TEXT("BreachSharedJunction"), LOCTEXT("BreachSharedJunction", "Protect the mixed survivors through the shared breach."),
		ESovObjectiveType::ProtectHold, Tags.Character_Player_Tarrik, TEXT("HandoffToTarrikRescue"));
	Shared.RequiredEncounterId = TEXT("M12_E3_SharedBreach"); Shared.MinimumProtectedParticipants = 2;
	auto& Marine = AddBeat(*this, TEXT("FreeTrappedMarine"), LOCTEXT("FreeTrappedMarine", "Free the trapped Reformation marine."),
		ESovObjectiveType::MasteryRescue, Tags.Character_Player_Tarrik, TEXT("BreachSharedJunction"));
	Scene(*this, Marine, TEXT("M12_FreeTrappedMarine"));
	auto& Grounding = AddBeat(*this, TEXT("GroundLyric"), LOCTEXT("GroundLyric", "Give Selene room to ground Lyric."),
		ESovObjectiveType::InteractOperate, Tags.Character_Player_Tarrik, TEXT("FreeTrappedMarine"));
	Scene(*this, Grounding, TEXT("M12_GroundLyric"));

	// Z07: fixed destruction and isolated threat data are not optional faction surrender.
	auto& Resonator = AddBeat(*this, TEXT("DestroyDominionResonator"), LOCTEXT("DestroyDominionResonator", "Destroy the Dominion resonator."),
		ESovObjectiveType::InteractOperate, Tags.Character_Player_Tarrik, TEXT("GroundLyric"));
	Scene(*this, Resonator, TEXT("M12_DestroyDominionResonator")); Protect(Resonator, TAG_Aurelion_DominionResonator, TAG_Aurelion_Destroyed);
	AddHandoff(*this, TEXT("HandoffToSeleneCage"), TEXT("DestroyDominionResonator"), Tags.Character_Player_Tarrik,
		Tags.Character_Player_Selene, TEXT("M12_SeleneCage"), LOCTEXT("SeleneCageHandoff", "Continue the terminal work as Selene."));
	auto& Cage = AddBeat(*this, TEXT("DestroyReformationCage"), LOCTEXT("DestroyReformationCage", "Destroy the Reformation cage."),
		ESovObjectiveType::InteractOperate, Tags.Character_Player_Selene, TEXT("HandoffToSeleneCage"));
	Scene(*this, Cage, TEXT("M12_DestroyReformationCage")); Protect(Cage, TAG_Aurelion_ReformationCage, TAG_Aurelion_Destroyed);
	auto& Sharing = AddBeat(*this, TEXT("ShareIsolatedThreatData"), LOCTEXT("ShareIsolatedThreatData", "Share threat information through the isolated feed."),
		ESovObjectiveType::InteractOperate, Tags.Character_Player_Selene, TEXT("DestroyReformationCage"));
	Scene(*this, Sharing, TEXT("M12_ShareIsolatedThreatData"));
	Protect(Sharing, TAG_Aurelion_ThreatSharing, TAG_Aurelion_Isolated); Protect(Sharing, TAG_Aurelion_CommandAuthority, TAG_Aurelion_Retained);
	FSovCampaignChoiceGroup Group; Group.GroupId = TEXT("ImmediateProtection"); Group.ReconciliationBeatId = TEXT("LocalPriorityCommitted");
	Group.ReconciliationNote = LOCTEXT("LayoutProtectionReconciliation", "Both mixed survivor groups survive. Priority changes their evacuation order and provides either the west medical cache or the east shutter, never both.");
	ChoiceGroups.Add(Group);
	for (bool bWest : {true, false})
	{
		auto& Choice = AddBeat(*this, bWest ? FName(TEXT("PriorityWestStretchers")) : FName(TEXT("PriorityEastWalkers")),
			bWest ? LOCTEXT("PriorityWestStretchers", "Move the west stretcher group first.") : LOCTEXT("PriorityEastWalkers", "Move the east walking group first."),
			ESovObjectiveType::ChoosePrioritize, Tags.Character_Player_Selene, TEXT("ShareIsolatedThreatData"));
		Choice.bOptional = true; Choice.bInteractiveChoice = true; Choice.ChoiceGroupId = Group.GroupId;
		auto& Consequence = Choice.Consequences.AddDefaulted_GetRef();
		Consequence.ConsequenceId = bWest ? FName(TEXT("M12_WestStretchersPrioritized")) : FName(TEXT("M12_EastWalkersPrioritized"));
		Consequence.SubjectIds.Add(bWest ? FName(TEXT("Aurelion_WestStretchers")) : FName(TEXT("Aurelion_EastWalkers")));
		Consequence.ChoiceTag = TAG_Aurelion_Protect; Consequence.OutcomeTag = bWest ? TAG_Aurelion_WestFirst : TAG_Aurelion_EastFirst;
		Consequence.WitnessIds = {TEXT("Tarrik"), TEXT("Selene")}; Consequence.Publicity = ESovRecordPublicity::Shared;
		Consequence.ConsumerIds = {TEXT("M12_PriorityEvacuation"), TEXT("M13_PriorityAftermath")};
	}
	auto& Priority = AddBeat(*this, TEXT("LocalPriorityCommitted"), LOCTEXT("LocalPriorityCommitted", "Prepare both survivor groups for evacuation."),
		ESovObjectiveType::FollowEscort, Tags.Character_Player_Selene, TEXT("ShareIsolatedThreatData"));
	Priority.RequiredChoiceGroups.Add(Group.GroupId);

	// Z08: a live, typed two-phase encounter. Thermal Fracture is not the existing FormationBreach action.
	auto& Links = AddBeat(*this, TEXT("SeverCrucibleLinks"), LOCTEXT("SeverCrucibleLinks", "Sever the crucible links while protecting both survivor groups."),
		ESovObjectiveType::ProtectHold, Tags.Character_Player_Selene, TEXT("LocalPriorityCommitted"));
	Links.RequiredEncounterId = TEXT("M12_E4_QuarantineCrucibleA"); Links.MinimumProtectedParticipants = 2;
	Links.RequiredEncounterProof = ESovEncounterProofType::AurelionLinks;
	AddHandoff(*this, TEXT("HandoffToTarrikCrucible"), TEXT("SeverCrucibleLinks"), Tags.Character_Player_Selene,
		Tags.Character_Player_Tarrik, TEXT("M12_TarrikCrucible"), LOCTEXT("TarrikCrucibleHandoff", "Take Tarrik's side of the crucible."));
	auto& Thermal = AddBeat(*this, TEXT("ThermalFracture"), LOCTEXT("ThermalFracture", "Use Selene's frost and Tarrik's heat to fracture the elite, then clear the escape."),
		ESovObjectiveType::ProtectHold, Tags.Character_Player_Tarrik, TEXT("HandoffToTarrikCrucible"));
	Thermal.RequiredEncounterId = TEXT("M12_E4_QuarantineCrucibleB"); Thermal.MinimumProtectedParticipants = 2;
	Thermal.RequiredEncounterProof = ESovEncounterProofType::AurelionThermalFracture;
	auto& Quarantine = AddBeat(*this, TEXT("SurvivorsClearAndQuarantine"), LOCTEXT("SurvivorsClearAndQuarantine", "Let both survivor groups clear before the quarantine seals."),
		ESovObjectiveType::ReachEscape, Tags.Character_Player_Tarrik, TEXT("ThermalFracture"));
	Scene(*this, Quarantine, TEXT("M12_SurvivorsClearAndQuarantine"));
	Protect(Quarantine, TAG_Aurelion_WestSurvivors, TAG_Aurelion_Alive); Protect(Quarantine, TAG_Aurelion_EastSurvivors, TAG_Aurelion_Alive);
	// Z09: contrary-witness recognition follows all combat, evacuation and quarantine.
	auto& Witnesses = AddBeat(*this, TEXT("ContraryWitnessRecognized"), LOCTEXT("ContraryWitnessRecognized", "Witness the terminal's recognition."),
		ESovObjectiveType::InvestigateAuthenticate, Tags.Character_Player_Tarrik, TEXT("SurvivorsClearAndQuarantine"));
	Scene(*this, Witnesses, TEXT("M12_ContraryWitnessRecognized"));
	Protect(Witnesses, TAG_Aurelion_ContraryWitnesses, TAG_Aurelion_Recognized);
}

USovAurelionContraryWitnessMissionDefinition::USovAurelionContraryWitnessMissionDefinition()
{
	MissionId = TEXT("M13_ContraryWitness"); DisplayName = LOCTEXT("ContraryWitness", "Contrary Witness");
	EntryPlayerStartTag = TEXT("Aurelion_TarrikConvergence");
	ConfigureConvergence(*this, false);
	const auto& Tags = FSovGameplayTags::Get();
	// Z10: this positional receipt cannot stand in for either protagonist's independent assent.
	auto& Position = AddBeat(*this, TEXT("ContraryPosition"), LOCTEXT("ContraryPosition", "Wait for Selene at the opposite terminal position."),
		ESovObjectiveType::FollowEscort, Tags.Character_Player_Tarrik);
	FSovCampaignStateWrite Recognized; Recognized.Key = TAG_Aurelion_ContraryWitnesses; Recognized.Value = TAG_Aurelion_Recognized;
	Position.RequiredState.Add(Recognized);
	Position.bRequiresCoActionProof = true; Position.RequiredCompanionId = TEXT("Selene"); Position.RequiredCoActionAnchorId = TEXT("M13_SeleneContraryPosition");
	auto& Tarrik = AddBeat(*this, TEXT("TarrikIndependentAssent"), LOCTEXT("TarrikIndependentAssent", "Hear Tarrik's independent assent."),
		ESovObjectiveType::InteractOperate, Tags.Character_Player_Tarrik, TEXT("ContraryPosition"));
	Scene(*this, Tarrik, TEXT("M13_TarrikIndependentAssent")); Protect(Tarrik, TAG_Aurelion_TarrikAssent, TAG_Aurelion_Independent);
	AddHandoff(*this, TEXT("HandoffToSeleneAssent"), TEXT("TarrikIndependentAssent"), Tags.Character_Player_Tarrik,
		Tags.Character_Player_Selene, TEXT("M13_SeleneAssent"), LOCTEXT("SeleneAssentHandoff", "Continue from Selene's terminal position."));
	auto& Selene = AddBeat(*this, TEXT("SeleneIndependentAssent"), LOCTEXT("SeleneIndependentAssent", "Hear Selene's independent assent."),
		ESovObjectiveType::InteractOperate, Tags.Character_Player_Selene, TEXT("HandoffToSeleneAssent"));
	Scene(*this, Selene, TEXT("M13_SeleneIndependentAssent")); Protect(Selene, TAG_Aurelion_SeleneAssent, TAG_Aurelion_Independent);
	auto& Containment = AddBeat(*this, TEXT("MeridianContainment"), LOCTEXT("MeridianContainment", "Stabilize containment while withholding release."),
		ESovObjectiveType::InteractOperate, Tags.Character_Player_Selene, TEXT("SeleneIndependentAssent"));
	Containment.PrerequisiteBeats.Add(TEXT("TarrikIndependentAssent"));
	Scene(*this, Containment, TEXT("M13_MeridianContainment"));
	Protect(Containment, TAG_Aurelion_Meridian, TAG_Aurelion_Complete);
	Protect(Containment, TAG_Aurelion_Containment, TAG_Aurelion_Stabilized);
	Protect(Containment, TAG_Aurelion_Authority, TAG_Aurelion_Available);
	Protect(Containment, TAG_Aurelion_Release, Tags.Campaign_Value_Withheld);
	Protect(Containment, TAG_Aurelion_Boundary, TAG_Aurelion_Closed);
	Protect(Containment, TAG_Aurelion_Crownmark, TAG_Aurelion_Integrated);
	Protect(Containment, TAG_Aurelion_LyricLife, TAG_Aurelion_Alive);
	Protect(Containment, TAG_Aurelion_LyricCorruption, TAG_Aurelion_Unreversed);
	auto& Fifth = AddBeat(*this, TEXT("FifthWitness"), LOCTEXT("FifthWitness", "Observe the Fifth Witness and the containment warning."),
		ESovObjectiveType::InvestigateAuthenticate, Tags.Character_Player_Selene, TEXT("MeridianContainment"));
	Scene(*this, Fifth, TEXT("M13_FifthWitness")); Fifth.CriticalEvidence.Add(nullptr);
	Fifth.CriticalEvidenceObserverIds = {TEXT("Tarrik"), TEXT("Selene")};
	auto& Grammar = AddBeat(*this, TEXT("GrammarPropagation"), LOCTEXT("GrammarPropagation", "Witness the containment grammar propagate beyond the terminal."),
		ESovObjectiveType::InvestigateAuthenticate, Tags.Character_Player_Selene, TEXT("FifthWitness"));
	Scene(*this, Grammar, TEXT("M13_GrammarPropagation")); Protect(Grammar, TAG_Aurelion_Grammar, TAG_Aurelion_Propagated);

	// Z11: voluntary conversation and evidence custody. There is no new battle or rescue choice here.
	auto& Stay = AddBeat(*this, TEXT("VoluntaryStay"), LOCTEXT("VoluntaryStay", "Stay together and speak freely."),
		ESovObjectiveType::InteractOperate, Tags.Character_Player_Selene, TEXT("GrammarPropagation"));
	Scene(*this, Stay, TEXT("M13_VoluntaryStay")); Protect(Stay, TAG_Aurelion_Stay, TAG_Aurelion_Voluntary);
	auto& Recorder = AddBeat(*this, TEXT("CauldronRecorderReceived"), LOCTEXT("CauldronRecorderReceived", "Receive Tarrik's Cauldron recorder."),
		ESovObjectiveType::RetrieveDeliver, Tags.Character_Player_Selene, TEXT("VoluntaryStay"));
	Scene(*this, Recorder, TEXT("M13_CauldronRecorderReceived")); Recorder.CriticalEvidence.Add(nullptr);
	Protect(Recorder, TAG_Aurelion_RecorderRecipient, Tags.Character_Player_Selene);
	AddHandoff(*this, TEXT("HandoffToTarrikAftermath"), TEXT("CauldronRecorderReceived"), Tags.Character_Player_Selene,
		Tags.Character_Player_Tarrik, TEXT("M13_TarrikAftermath"), LOCTEXT("TarrikAftermathHandoff", "Continue the exchange as Tarrik."));
	auto& Record = AddBeat(*this, TEXT("Record7283Received"), LOCTEXT("Record7283Received", "Receive Record 7283 and consider the shared warning."),
		ESovObjectiveType::RetrieveDeliver, Tags.Character_Player_Tarrik, TEXT("HandoffToTarrikAftermath"));
	Scene(*this, Record, TEXT("M13_Record7283Received")); Record.CriticalEvidence.Add(nullptr);
	Protect(Record, TAG_Aurelion_Record7283Recipient, Tags.Character_Player_Tarrik);
	auto& Pact = AddBeat(*this, TEXT("ContainmentPact"), LOCTEXT("ContainmentPact", "Agree to withhold release without direct contrary concurrence."),
		ESovObjectiveType::InteractOperate, Tags.Character_Player_Tarrik, TEXT("Record7283Received"));
	Scene(*this, Pact, TEXT("M13_ContainmentPact")); Protect(Pact, TAG_Aurelion_Pact, TAG_Aurelion_DirectConcurrence);
	// Z12: the slice ends on separate departures, not a shared faction allegiance.
	auto& Departure = AddBeat(*this, TEXT("SeparateDepartures"), LOCTEXT("SeparateDepartures", "Return to your own people carrying the shared warning."),
		ESovObjectiveType::ReachEscape, Tags.Character_Player_Tarrik, TEXT("ContainmentPact"));
	Scene(*this, Departure, TEXT("M13_SeparateDepartures")); Protect(Departure, TAG_Aurelion_Departure, TAG_Aurelion_Separate);
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
		|| CompanionActivationBeat != Contract->CompanionActivationBeat
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
			|| Beat->bIsolatedPerspectiveCut != Expected.bIsolatedPerspectiveCut
			|| Beat->RequiredHandoffAnchorId != Expected.RequiredHandoffAnchorId || Beat->CinematicId != Expected.CinematicId
			|| Beat->bRequiresCinematicProof != Expected.bRequiresCinematicProof || Beat->bRequiresCoActionProof != Expected.bRequiresCoActionProof
			|| Beat->RequiredCompanionId != Expected.RequiredCompanionId || Beat->RequiredCoActionAnchorId != Expected.RequiredCoActionAnchorId
			|| Beat->RequiredEncounterId != Expected.RequiredEncounterId || Beat->ChoiceGroupId != Expected.ChoiceGroupId
			|| Beat->RequiredEncounterProof != Expected.RequiredEncounterProof || !SameReceiverIds(Beat->RequiredReceiverIds, Expected.RequiredReceiverIds)
			|| Beat->MinimumProtectedParticipants != Expected.MinimumProtectedParticipants
			|| Beat->bInteractiveChoice != Expected.bInteractiveChoice || Beat->RequiredChoiceGroups != Expected.RequiredChoiceGroups
			|| !SameWrites(Beat->StateWrites, Expected.StateWrites) || !SameWrites(Beat->RequiredState, Expected.RequiredState)
			|| Beat->GrantedKnowledge != Expected.GrantedKnowledge || Beat->RequiredKnowledge != Expected.RequiredKnowledge
			|| Beat->Consequences != Expected.Consequences || Beat->RelationshipMemories != Expected.RelationshipMemories
			|| Beat->CriticalEvidence.Num() != Expected.CriticalEvidence.Num()
			|| Beat->CriticalEvidenceObserverIds != Expected.CriticalEvidenceObserverIds)
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
			|| !MatchesEvidence(TEXT("CauldronRecorderReceived"), {TEXT("CauldronRecorder")}))
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
