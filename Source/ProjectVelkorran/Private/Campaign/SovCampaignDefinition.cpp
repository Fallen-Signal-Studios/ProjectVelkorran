// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovEvidenceDefinition.h"

#include "Characters/SovSeleneCharacter.h"
#include "Characters/SovTarrikCharacter.h"
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "AI/NPCDefinition.h"
#include "Abilities/GameplayAbility.h"
#include "Sovereign/SovGameplayTags.h"

bool USovCampaignDefinition::SupportsProtagonist(FGameplayTag Lead) const
{
	return Lead.IsValid() && (Lead == Protagonist || AlternateProtagonists.ContainsByPredicate(
		[Lead](const FSovCampaignProtagonistProfile& Profile) { return Profile.Protagonist == Lead; }));
}

TSoftClassPtr<ASovPlayerCharacterBase> USovCampaignDefinition::ResolvePawnClass(FGameplayTag Lead) const
{
	if (Lead == Protagonist) { return PawnClass; }
	const auto* Profile = AlternateProtagonists.FindByPredicate([Lead](const auto& Item) { return Item.Protagonist == Lead; });
	return Profile ? Profile->PawnClass : TSoftClassPtr<ASovPlayerCharacterBase>();
}

TSoftObjectPtr<UPlayerDefinition> USovCampaignDefinition::ResolvePlayerDefinition(FGameplayTag Lead) const
{
	if (Lead == Protagonist) { return PlayerDefinition; }
	const auto* Profile = AlternateProtagonists.FindByPredicate([Lead](const auto& Item) { return Item.Protagonist == Lead; });
	return Profile ? Profile->PlayerDefinition : TSoftObjectPtr<UPlayerDefinition>();
}

const FSovCampaignBeatDefinition* USovCampaignDefinition::FindBeat(FName BeatId) const
{
	return Beats.FindByPredicate([BeatId](const FSovCampaignBeatDefinition& Beat) { return Beat.BeatId == BeatId; });
}

const FSovCampaignChoiceGroup* USovCampaignDefinition::FindChoiceGroup(FName GroupId) const
{
	return ChoiceGroups.FindByPredicate([GroupId](const auto& Group) { return Group.GroupId == GroupId; });
}

bool USovCampaignDefinition::ValidateObjectives(FString& OutError) const
{
	const auto Fail = [&OutError](const TCHAR* Message) { OutError = Message; return false; };
	if (Beats.Num() > 512 || ChoiceGroups.Num() > 64) { return Fail(TEXT("Mission objective contract exceeds 512 beats or 64 choices.")); }
	TSet<FName> Groups;
	for (const auto& Group : ChoiceGroups)
	{
		const auto* Rejoin = FindBeat(Group.ReconciliationBeatId);
		if (Group.GroupId.IsNone() || Groups.Contains(Group.GroupId) || Group.ReconciliationNote.IsEmpty()
			|| !Rejoin || Rejoin->bOptional || !Rejoin->ChoiceGroupId.IsNone()
			|| !Rejoin->RequiredChoiceGroups.Contains(Group.GroupId))
		{ return Fail(TEXT("Choice groups need a unique stable ID, explicit reconciliation note and mandatory reconciliation beat.")); }
		Groups.Add(Group.GroupId);
		const FSovCampaignBeatDefinition* First = nullptr;
		int32 Options = 0;
		for (const auto& Beat : Beats)
		{
			if (Beat.ChoiceGroupId != Group.GroupId) { continue; }
			++Options;
			if (!Beat.bOptional || !Beat.bInteractiveChoice || Beat.bCanonGate || !Beat.FailureReasonId.IsNone()
				|| Beat.HandoffToProtagonist.IsValid() || Beat.bRequiresCoActionProof || !Beat.CinematicId.IsNone()
				|| !Beat.RequiredState.IsEmpty() || !Beat.RequiredKnowledge.IsEmpty() || !Beat.GrantedKnowledge.IsEmpty()
				|| !Beat.RequiredChoiceGroups.IsEmpty())
			{ return Fail(TEXT("Choice outcomes must be optional interactive beats with shared prerequisite gating and no canon, failure, proof or unique knowledge grants.")); }
			if (First && (Beat.PrerequisiteBeats != First->PrerequisiteBeats || Beat.RequiredProtagonist != First->RequiredProtagonist))
			{ return Fail(TEXT("All choice outcomes must share the same ordered prerequisites and protagonist.")); }
			First = &Beat;
			for (FName Prior : Beat.PrerequisiteBeats)
			{ if (!Rejoin->PrerequisiteBeats.Contains(Prior)) { return Fail(TEXT("Reconciliation must explicitly require the choice's shared prerequisite beats.")); } }
			for (const auto& Write : Beat.StateWrites)
			{
				if (Write.bCanonProtected) { return Fail(TEXT("Local choice outcomes cannot create protected canon facts.")); }
				for (const auto& Required : Beats)
				{
					if (Required.StateWrites.ContainsByPredicate([&Write](const auto& Fact) { return Fact.Key == Write.Key && Fact.bCanonProtected; }))
					{ return Fail(TEXT("A local choice cannot write a key reserved for this mission's protected canon facts.")); }
					if (!Required.bOptional && Required.RequiredState.ContainsByPredicate([&Write](const auto& Fact) { return Fact.Key == Write.Key; }))
					{ return Fail(TEXT("A mandatory beat cannot depend on one local choice's variable state value.")); }
				}
			}
		}
		if (Options < 2 || Options > 4) { return Fail(TEXT("A choice group must offer two to four outcomes.")); }
		if (First && Rejoin->RequiredProtagonist != First->RequiredProtagonist)
		{ return Fail(TEXT("Choice and reconciliation must use the same authored protagonist.")); }
	}
	for (const auto& Beat : Beats)
	{
		if (static_cast<uint8>(Beat.ObjectiveType) > static_cast<uint8>(ESovObjectiveType::MasteryRescue))
		{ return Fail(TEXT("Objective type is unknown.")); }
		if (!Beat.FailureReasonId.IsNone() && (!Beat.bOptional || Beat.bCanonGate || Beat.FailureRuleText.IsEmpty() || !Beat.ChoiceGroupId.IsNone()))
		{ return Fail(TEXT("Objective failure requires an optional non-choice beat and a readable authored failure rule.")); }
		if (Beat.FailureReasonId.IsNone() && !Beat.FailureRuleText.IsEmpty())
		{ return Fail(TEXT("An objective failure rule needs its stable failure reason ID.")); }
		if (!Beat.ChoiceGroupId.IsNone() && !Groups.Contains(Beat.ChoiceGroupId))
		{ return Fail(TEXT("Outcome references an unknown choice group.")); }
		TSet<FName> Requirements;
		for (FName Id : Beat.RequiredChoiceGroups)
		{
			const auto* Group = FindChoiceGroup(Id);
			if (!Group || Group->ReconciliationBeatId != Beat.BeatId || Requirements.Contains(Id))
			{ return Fail(TEXT("Only the declared reconciliation beat can require a choice group, once.")); }
			Requirements.Add(Id);
		}
		for (FName PriorId : Beat.PrerequisiteBeats)
		{
			if (const auto* Prior = FindBeat(PriorId); Prior && !Prior->ChoiceGroupId.IsNone())
			{ return Fail(TEXT("Depend on a choice's mandatory reconciliation beat, not an exclusive optional outcome.")); }
		}
	}
	return true;
}

bool USovCampaignDefinition::ValidateDefinition(FString& OutError) const
{
	OutError.Reset();
	const auto Fail = [&OutError](const FString& Message) { OutError = Message; return false; };
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	if (SchemaVersion != 1 || MissionId.IsNone() || Beats.IsEmpty()) { return Fail(TEXT("Mission requires schema 1, stable ID and at least one beat.")); }
	if (!ValidateObjectives(OutError)) { return false; }
	if (Protagonist != Tags.Character_Player_Tarrik && Protagonist != Tags.Character_Player_Selene)
	{ return Fail(TEXT("Campaign mission must select exactly Tarrik or Selene.")); }
	if (PawnClass.IsNull() || PlayerDefinition.IsNull()) { return Fail(TEXT("Mission requires a pawn class and matching PlayerDefinition.")); }
	if (bCompletesCampaign && (!MissionId.ToString().StartsWith(TEXT("E17_")) || !AllowedSuccessorMissions.IsEmpty()))
	{ return Fail(TEXT("Campaign completion is reserved for a terminal E17 epilogue.")); }
	if (!AlternateProtagonists.IsEmpty() && !MissionId.ToString().StartsWith(TEXT("M12_")) && !MissionId.ToString().StartsWith(TEXT("M13_")))
	{ return Fail(TEXT("Alternate protagonist profiles are limited to authored M12/M13 handoffs.")); }
	TSet<FGameplayTag> ProfileIds;
	ProfileIds.Add(Protagonist);
	for (const auto& Profile : AlternateProtagonists)
	{
		if ((Profile.Protagonist != Tags.Character_Player_Tarrik && Profile.Protagonist != Tags.Character_Player_Selene)
			|| ProfileIds.Contains(Profile.Protagonist) || Profile.PawnClass.IsNull() || Profile.PlayerDefinition.IsNull())
		{ return Fail(TEXT("Alternate profiles require a unique canonical protagonist and its class/definition.")); }
		ProfileIds.Add(Profile.Protagonist);
	}
	if (!FMath::IsFinite(EntryEchoReserve) || EntryEchoReserve < 0.f || EntryEchoReserve > 100.f)
	{ return Fail(TEXT("Entry Echo reserve must be finite and within 0..100.")); }
	TSet<FName> InheritedConsequences;
	if (RequiredPriorConsequenceIds.Num() > 64) { return Fail(TEXT("Inherited consequence dependencies exceed the bounded contract.")); }
	for (FName Id : RequiredPriorConsequenceIds)
	{
		if (Id.IsNone() || InheritedConsequences.Contains(Id)) { return Fail(TEXT("Inherited consequence dependencies must have unique nonempty IDs.")); }
		InheritedConsequences.Add(Id);
	}
	TSet<FName> Ids;
	TSet<FName> ConsequenceIds, MemoryIds;
	bool bHasMandatoryBeat = false;
	for (const FSovCampaignBeatDefinition& Beat : Beats)
	{
		if (Beat.BeatId.IsNone() || Ids.Contains(Beat.BeatId)) { return Fail(TEXT("Beat IDs must be nonempty and unique within the mission.")); }
		Ids.Add(Beat.BeatId);
		if (Beat.RequiredProtagonist.IsValid() && !SupportsProtagonist(Beat.RequiredProtagonist))
		{ return Fail(TEXT("Beat requires a protagonist not present in this mission.")); }
		if (Beat.HandoffToProtagonist.IsValid())
		{
			if (!Beat.RequiredProtagonist.IsValid() || !SupportsProtagonist(Beat.HandoffToProtagonist)
				|| Beat.RequiredProtagonist == Beat.HandoffToProtagonist || Beat.RequiredHandoffAnchorId.IsNone()
				|| Beat.bOptional || Beat.bInteractiveChoice || Beat.bRequiresCoActionProof || !Beat.CinematicId.IsNone()
				|| AlternateProtagonists.IsEmpty())
			{ return Fail(TEXT("Handoff requires explicit different supported leads, a native anchor, and a mandatory non-cinematic beat.")); }
		}
		else if (!Beat.RequiredHandoffAnchorId.IsNone()) { return Fail(TEXT("Handoff anchor requires a handoff beat.")); }
		if (Beat.bRequiresCinematicProof && (Beat.CinematicId.IsNone() || Beat.bInteractiveChoice || Beat.bRequiresCoActionProof || Beat.HandoffToProtagonist.IsValid()))
		{ return Fail(TEXT("Native cinematic proof requires a presentation-only scene beat with no choice, co-action or protagonist handoff.")); }
		bHasMandatoryBeat |= !Beat.bOptional;
		if (Beat.bCanonGate && Beat.bOptional) { return Fail(TEXT("Canon gates cannot be optional.")); }
		if (Beat.bRequiresCoActionProof)
		{
			if (Beat.RequiredCompanionId.IsNone() || Beat.RequiredCoActionAnchorId.IsNone()
				|| !Beat.CinematicId.IsNone() || Beat.bInteractiveChoice)
			{ return Fail(TEXT("Co-action beats require companion/anchor IDs and must precede cinematic or choice beats.")); }
		}
		else if (!Beat.RequiredCompanionId.IsNone() || !Beat.RequiredCoActionAnchorId.IsNone())
		{ return Fail(TEXT("Companion/anchor proof IDs require a co-action beat.")); }
		if (Beat.Consequences.Num() > 16 || Beat.RelationshipMemories.Num() > 32 || Beat.CriticalEvidence.Num() > 16) { return Fail(TEXT("Beat narrative records exceed their bounded contract.")); }
		TSet<FName> CriticalIds;
		for (const auto& Evidence : Beat.CriticalEvidence)
		{
			if (Beat.bOptional || !IsValid(Evidence) || !Evidence->bCriticalPath || !Evidence->ValidateDefinition(OutError)
				|| !Evidence->RelevantMissions.Contains(MissionId) || CriticalIds.Contains(Evidence->EvidenceId))
			{ return Fail(TEXT("Critical evidence must be a valid, unique, mission-relevant record guaranteed by a mandatory beat.")); }
			CriticalIds.Add(Evidence->EvidenceId);
		}
		for (const auto& Consequence : Beat.Consequences)
		{
			if (!Consequence.Validate(OutError) || ConsequenceIds.Contains(Consequence.ConsequenceId) || InheritedConsequences.Contains(Consequence.ConsequenceId)) { return Fail(TEXT("Invalid or duplicated authored consequence.")); }
			ConsequenceIds.Add(Consequence.ConsequenceId);
		}
		for (const auto& Memory : Beat.RelationshipMemories)
		{
			if (!Memory.Validate(OutError) || MemoryIds.Contains(Memory.MemoryId)) { return Fail(TEXT("Invalid or duplicated asymmetric memory.")); }
			MemoryIds.Add(Memory.MemoryId);
		}
		TSet<FGameplayTag> WrittenKeys;
		for (const FSovCampaignStateWrite& Write : Beat.StateWrites)
		{
			if (!Write.Key.IsValid() || !Write.Value.IsValid() || WrittenKeys.Contains(Write.Key))
			{ return Fail(TEXT("State writes require valid tags and one value per key.")); }
			WrittenKeys.Add(Write.Key);
		}
		TSet<FGameplayTag> RequiredKeys;
		for (const FSovCampaignStateWrite& Requirement : Beat.RequiredState)
		{
			if (!Requirement.Key.IsValid() || !Requirement.Value.IsValid() || RequiredKeys.Contains(Requirement.Key))
			{ return Fail(TEXT("State requirements need valid tags and exactly one required value per key.")); }
			RequiredKeys.Add(Requirement.Key);
		}
	}
	for (const auto& Beat : Beats)
	{
		TSet<FName> Available = InheritedConsequences;
		for (const auto& C : Beat.Consequences) { Available.Add(C.ConsequenceId); }
		TArray<FName> Pending = Beat.PrerequisiteBeats; TSet<FName> Seen;
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			if (Seen.Contains(Pending[Index])) { continue; } Seen.Add(Pending[Index]);
			if (const auto* Prior = FindBeat(Pending[Index]))
			{
				for (const auto& C : Prior->Consequences) { Available.Add(C.ConsequenceId); }
				Pending.Append(Prior->PrerequisiteBeats);
			}
		}
		for (const auto& Memory : Beat.RelationshipMemories)
		{
			if (!Available.Contains(Memory.ConsequenceId)) { return Fail(TEXT("Memory refers to a consequence that is not guaranteed by this beat's prerequisites.")); }
		}
	}
	if (!bHasMandatoryBeat) { return Fail(TEXT("Mission requires at least one mandatory completion beat.")); }
	if (bAllowJointResonance && (!MissionId.ToString().StartsWith(TEXT("M12_")) && !MissionId.ToString().StartsWith(TEXT("M13_"))))
	{ return Fail(TEXT("Joint Resonance is limited to authored M12/M13 convergence missions.")); }
	if (bAllowJointResonance != !AllowedResonanceTypes.IsEmpty())
	{ return Fail(TEXT("Resonance permission must explicitly enumerate its enabled interactions.")); }
	TSet<uint8> ResonanceTypes;
	for (ESovResonanceType Type : AllowedResonanceTypes)
	{
		const uint8 Value = static_cast<uint8>(Type);
		if (Value > static_cast<uint8>(ESovResonanceType::AdvanceCorridor) || ResonanceTypes.Contains(Value))
		{ return Fail(TEXT("Resonance interaction types must be valid and unique.")); }
		ResonanceTypes.Add(Value);
	}
	for (FName Required : ResonancePrerequisiteBeats)
	{ if (!FindBeat(Required)) { return Fail(TEXT("Resonance permission references a missing beat.")); } }
	TSet<FName> CompanionIds;
	for (FName Id : AllowedCompanionIds)
	{
		if (Id.IsNone() || CompanionIds.Contains(Id)) { return Fail(TEXT("Mission companion IDs must be nonempty and unique.")); }
		CompanionIds.Add(Id);
	}
	if (bAllowJointResonance && AllowedCompanionIds.IsEmpty())
	{ return Fail(TEXT("Joint Resonance requires an explicitly permitted companion.")); }
	for (const FSovCampaignBeatDefinition& Beat : Beats)
	{
		TSet<FName> PrerequisiteIds;
		for (FName PriorId : Beat.PrerequisiteBeats)
		{
			const FSovCampaignBeatDefinition* Prior = FindBeat(PriorId);
			if (!Prior || PriorId == Beat.BeatId || PrerequisiteIds.Contains(PriorId)) { return Fail(TEXT("A beat references a missing or self prerequisite.")); }
			PrerequisiteIds.Add(PriorId);
			if (!Beat.bOptional && Prior->bOptional) { return Fail(TEXT("A mandatory beat cannot require an optional beat.")); }
		}
	}
	// Topological validation: no recursive traversal depth or designer-created loop.
	TSet<FName> Resolved;
	for (int32 Pass = 0; Pass < Beats.Num(); ++Pass)
	{
		bool bChanged = false;
		for (const FSovCampaignBeatDefinition& Beat : Beats)
		{
			if (Resolved.Contains(Beat.BeatId)) { continue; }
			bool bReady = true;
			for (FName Prior : Beat.PrerequisiteBeats) { bReady &= Resolved.Contains(Prior); }
			if (bReady) { Resolved.Add(Beat.BeatId); bChanged = true; }
		}
		if (!bChanged) { break; }
	}
	if (Resolved.Num() != Beats.Num()) { return Fail(TEXT("Mission beat prerequisites contain a cycle.")); }
	// Mandatory control changes form one ordered path. Otherwise a valid-looking DAG
	// can switch away from a still-required character and make the mission unwinnable.
	TMap<FName, TSet<FName>> BeatAncestors;
	TArray<const FSovCampaignBeatDefinition*> Handoffs;
	for (const auto& Beat : Beats)
	{
		TSet<FName>& Ancestors = BeatAncestors.FindOrAdd(Beat.BeatId);
		TArray<FName> Pending = Beat.PrerequisiteBeats;
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			if (Ancestors.Contains(Pending[Index])) { continue; }
			Ancestors.Add(Pending[Index]); Pending.Append(FindBeat(Pending[Index])->PrerequisiteBeats);
		}
		if (Beat.HandoffToProtagonist.IsValid()) { Handoffs.Add(&Beat); }
	}
	for (int32 First = 0; First < Handoffs.Num(); ++First)
	for (int32 Second = First + 1; Second < Handoffs.Num(); ++Second)
	{
		if (!BeatAncestors.FindChecked(Handoffs[First]->BeatId).Contains(Handoffs[Second]->BeatId)
			&& !BeatAncestors.FindChecked(Handoffs[Second]->BeatId).Contains(Handoffs[First]->BeatId))
		{ return Fail(TEXT("Mandatory protagonist handoffs must be ordered by beat prerequisites.")); }
	}
	Handoffs.Sort([&BeatAncestors](const auto& A, const auto& B)
	{ return BeatAncestors.FindChecked(B.BeatId).Contains(A.BeatId); });
	for (const auto& Beat : Beats)
	{
		if (!Beat.RequiredProtagonist.IsValid()) { continue; }
		FGameplayTag ExpectedLead = Protagonist;
		const FSovCampaignBeatDefinition* NextHandoff = nullptr;
		for (const auto* Handoff : Handoffs)
		{
			if (BeatAncestors.FindChecked(Beat.BeatId).Contains(Handoff->BeatId)) { ExpectedLead = Handoff->HandoffToProtagonist; }
			else if (!NextHandoff) { NextHandoff = Handoff; }
		}
		if (Beat.RequiredProtagonist != ExpectedLead)
		{ return Fail(TEXT("Beat protagonist is unreachable from its required control handoffs.")); }
		if (!Beat.bOptional && NextHandoff && NextHandoff->BeatId != Beat.BeatId
			&& !BeatAncestors.FindChecked(NextHandoff->BeatId).Contains(Beat.BeatId))
		{ return Fail(TEXT("A handoff can strand a mandatory beat for the outgoing protagonist; add the beat as its prerequisite.")); }
	}
	// A mandatory prerequisite's protected fact cannot be overwritten or required to be its opposite.
	for (const auto& Beat : Beats)
	{
		TArray<FName> Pending = Beat.PrerequisiteBeats;
		TSet<FName> Ancestors;
		TMap<FGameplayTag, FGameplayTag> GuaranteedProtectedFacts;
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			if (Ancestors.Contains(Pending[Index])) { continue; }
			Ancestors.Add(Pending[Index]);
			const auto* Prior = FindBeat(Pending[Index]);
			if (!Prior) { return Fail(TEXT("Missing prerequisite while validating canon facts.")); }
			Pending.Append(Prior->PrerequisiteBeats);
			for (const auto& Write : Prior->StateWrites)
			{
				if (!Write.bCanonProtected) { continue; }
				if (const FGameplayTag* Existing = GuaranteedProtectedFacts.Find(Write.Key); Existing && *Existing != Write.Value)
				{ return Fail(TEXT("Required ancestors commit contradictory protected facts.")); }
				GuaranteedProtectedFacts.Add(Write.Key, Write.Value);
			}
		}
		for (const auto& Write : Beat.StateWrites)
		{
			if (const FGameplayTag* Existing = GuaranteedProtectedFacts.Find(Write.Key); Existing && *Existing != Write.Value)
			{ return Fail(TEXT("Beat attempts to change a protected prerequisite fact.")); }
		}
		for (const auto& Requirement : Beat.RequiredState)
		{
			if (const FGameplayTag* Existing = GuaranteedProtectedFacts.Find(Requirement.Key); Existing && *Existing != Requirement.Value)
			{ return Fail(TEXT("Beat requires a value incompatible with a protected prerequisite fact.")); }
		}
	}
	TSet<FGameplayTag> CompanionHeroes;
	TSet<FName> CompanionProfileIds, CompanionAnchorTags;
	for (const auto& Profile : ProtagonistCompanions)
	{
		if (!SupportsProtagonist(Profile.Protagonist) || CompanionHeroes.Contains(Profile.Protagonist)
			|| Profile.CompanionId.IsNone() || !AllowedCompanionIds.Contains(Profile.CompanionId)
			|| CompanionProfileIds.Contains(Profile.CompanionId) || Profile.EntryAnchorTag.IsNone() || CompanionAnchorTags.Contains(Profile.EntryAnchorTag)
			|| Profile.CompanionClass.IsNull() || Profile.CompanionDefinition.IsNull())
		{ return Fail(TEXT("Convergence companion profiles require distinct supported identities, permitted IDs, native proxy classes and NPC definitions.")); }
		UClass* ProxyClass = Profile.CompanionClass.LoadSynchronous();
		if (!ProxyClass || !ProxyClass->IsChildOf(ASovProtagonistCompanionCharacter::StaticClass()) || ProxyClass->HasAnyClassFlags(CLASS_Abstract)
			|| !Profile.CompanionDefinition.LoadSynchronous())
		{ return Fail(TEXT("Convergence companion content must load as a concrete native proxy class and NPC definition.")); }
		TSet<UClass*> CuratedClasses;
		for (const auto& Ability : Profile.CuratedCompanionAbilities)
		{
			if (!Ability || Ability->HasAnyClassFlags(CLASS_Abstract) || CuratedClasses.Contains(Ability.Get()))
			{ return Fail(TEXT("Curated companion abilities must be concrete and unique; locked abilities remain unavailable at runtime.")); }
			CuratedClasses.Add(Ability.Get());
		}
		CompanionHeroes.Add(Profile.Protagonist); CompanionProfileIds.Add(Profile.CompanionId); CompanionAnchorTags.Add(Profile.EntryAnchorTag);
	}
	if (bAllowJointResonance && (CompanionHeroes.Num() != 2 || !CompanionHeroes.Contains(Tags.Character_Player_Tarrik) || !CompanionHeroes.Contains(Tags.Character_Player_Selene)))
	{ return Fail(TEXT("Joint Resonance requires both canonical protagonist companion profiles.")); }
	if (!AlternateProtagonists.IsEmpty() && (!CompanionHeroes.Contains(Protagonist) || CompanionHeroes.Num() != AlternateProtagonists.Num() + 1))
	{ return Fail(TEXT("Every playable identity in a convergence handoff needs its native companion profile.")); }
	TSet<FName> SuccessorIds;
	for (const FName Successor : AllowedSuccessorMissions)
	{
		if (Successor.IsNone() || Successor == MissionId || SuccessorIds.Contains(Successor))
		{ return Fail(TEXT("Successors must have distinct nonempty mission IDs.")); }
		SuccessorIds.Add(Successor);
	}
	return true;
}

namespace
{
	void AddOpeningBeat(USovCampaignDefinition& Mission, const TCHAR* Id, const TCHAR* Text,
		const TCHAR* Prior, bool bCanon, const TCHAR* Cinematic = nullptr)
	{
		FSovCampaignBeatDefinition Beat;
		Beat.BeatId = FName(Id);
		Beat.ObjectiveText = FText::FromString(Text);
		if (Prior) { Beat.PrerequisiteBeats.Add(FName(Prior)); }
		Beat.bCanonGate = bCanon;
		if (Cinematic) { Beat.CinematicId = FName(Cinematic); Beat.bRequiresCinematicProof = true; }
		Mission.Beats.Add(MoveTemp(Beat));
	}
}

USovMantleMissionDefinition::USovMantleMissionDefinition()
{
	MissionId = TEXT("M01_Mantle");
	DisplayName = NSLOCTEXT("SovCampaign", "Mantle", "The Mantle");
	Protagonist = FSovGameplayTags::Get().Character_Player_Tarrik;
	PawnClass = ASovTarrikCharacter::StaticClass();
	AllowedSuccessorMissions.Add(TEXT("M02_OneDegree"));
	AddOpeningBeat(*this, TEXT("Coronation"), TEXT("Attend the coronation."), nullptr, false);
	AddOpeningBeat(*this, TEXT("HeirNamed"), TEXT("Witness Caelus name Tarrik heir."), TEXT("Coronation"), true, TEXT("M01_HeirNamed"));
	AddOpeningBeat(*this, TEXT("CaelusWounded"), TEXT("Respond to the attack."), TEXT("HeirNamed"), true, TEXT("M01_Shot"));
	AddOpeningBeat(*this, TEXT("Pursuit"), TEXT("Pursue the attacker."), TEXT("CaelusWounded"), false);
	AddOpeningBeat(*this, TEXT("MarketShotWithheld"), TEXT("Resolve the market confrontation."), TEXT("Pursuit"), true, TEXT("M01_Market"));
	AddOpeningBeat(*this, TEXT("CrownmarkLead"), TEXT("Follow the Crownmark lead."), TEXT("MarketShotWithheld"), true);
	const auto& Tags = FSovGameplayTags::Get();
	const auto Protect = [this](FName BeatId, FGameplayTag Key, FGameplayTag Value)
	{
		if (auto* Beat = Beats.FindByPredicate([BeatId](const FSovCampaignBeatDefinition& Item) { return Item.BeatId == BeatId; }))
		{
			FSovCampaignStateWrite Write; Write.Key = Key; Write.Value = Value; Write.bCanonProtected = true;
			Beat->StateWrites.Add(Write);
		}
	};
	Protect(TEXT("HeirNamed"), Tags.Campaign_Fact_Heir, Tags.Campaign_Value_Tarrik);
	Protect(TEXT("CaelusWounded"), Tags.Campaign_Fact_Caelus, Tags.Campaign_Value_WoundedAlive);
	Protect(TEXT("MarketShotWithheld"), Tags.Campaign_Fact_MarketShot, Tags.Campaign_Value_Withheld);
}

USovOneDegreeMissionDefinition::USovOneDegreeMissionDefinition()
{
	MissionId = TEXT("M02_OneDegree");
	DisplayName = NSLOCTEXT("SovCampaign", "OneDegree", "One Degree");
	Protagonist = FSovGameplayTags::Get().Character_Player_Selene;
	PawnClass = ASovSeleneCharacter::StaticClass();
	AddOpeningBeat(*this, TEXT("Infiltration"), TEXT("Reach the firing position."), nullptr, false);
	AddOpeningBeat(*this, TEXT("PrecisionShot"), TEXT("Take the assigned shot."), TEXT("Infiltration"), true, TEXT("M02_Shot"));
	AddOpeningBeat(*this, TEXT("Escape"), TEXT("Escape the pursuit."), TEXT("PrecisionShot"), false);
	AddOpeningBeat(*this, TEXT("LyricAtExtraction"), TEXT("Bring Lyric to the extraction mark."), TEXT("Escape"), false);
	Beats.Last().bRequiresCoActionProof = true;
	Beats.Last().RequiredCompanionId = TEXT("Lyric");
	Beats.Last().RequiredCoActionAnchorId = TEXT("M02_LyricExtraction");
	AddOpeningBeat(*this, TEXT("LyricExtraction"), TEXT("Reach the extraction."), TEXT("LyricAtExtraction"), true, TEXT("M02_Extraction"));
	AddOpeningBeat(*this, TEXT("VossDirection"), TEXT("Receive the next assignment."), TEXT("LyricExtraction"), true, TEXT("M02_Voss"));
	if (auto* Shot = Beats.FindByPredicate([](const FSovCampaignBeatDefinition& Beat) { return Beat.BeatId == TEXT("PrecisionShot"); }))
	{
		FSovCampaignStateWrite Canon;
		Canon.Key = FSovGameplayTags::Get().Campaign_Fact_Caelus;
		Canon.Value = FSovGameplayTags::Get().Campaign_Value_WoundedAlive;
		Canon.bCanonProtected = true;
		Shot->StateWrites.Add(Canon);
	}
}

const FSovCampaignCompanionProfile* USovCampaignDefinition::FindCompanionProfile(FGameplayTag Identity) const
{
	return ProtagonistCompanions.FindByPredicate([Identity](const FSovCampaignCompanionProfile& Profile) { return Profile.Protagonist == Identity; });
}
