// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovCampaignDefinition.h"

#include "Characters/SovSeleneCharacter.h"
#include "Characters/SovTarrikCharacter.h"
#include "Sovereign/SovGameplayTags.h"

const FSovCampaignBeatDefinition* USovCampaignDefinition::FindBeat(FName BeatId) const
{
	return Beats.FindByPredicate([BeatId](const FSovCampaignBeatDefinition& Beat) { return Beat.BeatId == BeatId; });
}

bool USovCampaignDefinition::ValidateDefinition(FString& OutError) const
{
	OutError.Reset();
	const auto Fail = [&OutError](const FString& Message) { OutError = Message; return false; };
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	if (SchemaVersion != 1 || MissionId.IsNone() || Beats.IsEmpty()) { return Fail(TEXT("Mission requires schema 1, stable ID and at least one beat.")); }
	if (Protagonist != Tags.Character_Player_Tarrik && Protagonist != Tags.Character_Player_Selene)
	{ return Fail(TEXT("Campaign mission must select exactly Tarrik or Selene.")); }
	if (PawnClass.IsNull() || PlayerDefinition.IsNull()) { return Fail(TEXT("Mission requires a pawn class and matching PlayerDefinition.")); }
	if (!FMath::IsFinite(EntryEchoReserve) || EntryEchoReserve < 0.f || EntryEchoReserve > 100.f)
	{ return Fail(TEXT("Entry Echo reserve must be finite and within 0..100.")); }
	TSet<FName> Ids;
	bool bHasMandatoryBeat = false;
	for (const FSovCampaignBeatDefinition& Beat : Beats)
	{
		if (Beat.BeatId.IsNone() || Ids.Contains(Beat.BeatId)) { return Fail(TEXT("Beat IDs must be nonempty and unique within the mission.")); }
		Ids.Add(Beat.BeatId);
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
	if (!bHasMandatoryBeat) { return Fail(TEXT("Mission requires at least one mandatory completion beat.")); }
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
		if (Cinematic) { Beat.CinematicId = FName(Cinematic); }
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
