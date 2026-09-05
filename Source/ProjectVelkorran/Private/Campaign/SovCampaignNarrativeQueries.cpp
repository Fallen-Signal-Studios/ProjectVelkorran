// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovEvidencePolicy.h"
#include "Sovereign/SovGameplayTags.h"

FName USovCampaignStateComponent::NarrativeIdentity(FGameplayTag Protagonist)
{
	if (Protagonist == FSovGameplayTags::Get().Character_Player_Tarrik) { return TEXT("Tarrik"); }
	if (Protagonist == FSovGameplayTags::Get().Character_Player_Selene) { return TEXT("Selene"); }
	return NAME_None;
}
bool USovCampaignStateComponent::EvidenceKnownTo(const TArray<FSovEvidenceAcquisition>& History, FName Id, FName Observer)
{
	return !Id.IsNone() && !Observer.IsNone() && History.ContainsByPredicate([Id, Observer](const auto& Item)
	{
		return Item.EvidenceId == Id && (NarrativeIdentity(Item.Protagonist) == Observer
			|| Item.WitnessIds.Contains(Observer) || Item.CopyDestination == Observer);
	});
}
ESovEvidenceStage USovCampaignStateComponent::EvidenceStageIn(const TArray<FSovEvidenceAcquisition>& History, FName Id, FGameplayTag Hero)
{
	ESovEvidenceStage Stage = ESovEvidenceStage::Unknown;
	for (const auto& Item : History)
	{
		if (Item.EvidenceId == Id && Item.Protagonist == Hero && Item.Stage > Stage) { Stage = Item.Stage; }
	}
	if (Stage == ESovEvidenceStage::Unknown && EvidenceKnownTo(History, Id, NarrativeIdentity(Hero))) { Stage = ESovEvidenceStage::Observed; }
	return Stage;
}
ESovEvidenceStage USovCampaignStateComponent::GetEvidenceStage(FName EvidenceId, FGameplayTag Protagonist) const
{
	return bStateValid ? EvidenceStageIn(Evidence, EvidenceId, Protagonist) : ESovEvidenceStage::Unknown;
}
bool USovCampaignStateComponent::HasEvidenceCopy(FName Id, FName Destination) const
{
	return bStateValid && !Destination.IsNone() && Evidence.ContainsByPredicate([Id, Destination](const auto& Item)
	{ return Item.EvidenceId == Id && Item.Stage == ESovEvidenceStage::Distributed && Item.CopyDestination == Destination; });
}
bool USovCampaignStateComponent::ObserverKnowsEvidence(FName Id, FName Observer) const
{
	return bStateValid && EvidenceKnownTo(Evidence, Id, Observer);
}
bool USovCampaignStateComponent::ValidateEvidenceStep(const FSovEvidenceAcquisition& Item, const TArray<FSovEvidenceAcquisition>& Prior)
{
	if (Item.EvidenceId.IsNone() || !Item.SourceId.IsValid() || NarrativeIdentity(Item.Protagonist).IsNone()
		|| Item.Stage < ESovEvidenceStage::Observed || Item.Stage > ESovEvidenceStage::Distributed
		|| Item.Publicity > ESovRecordPublicity::Public || Item.WitnessIds.Num() > 32) { return false; }
	TSet<FName> Witnesses;
	for (FName Witness : Item.WitnessIds) { if (Witness.IsNone() || Witnesses.Contains(Witness)) { return false; } Witnesses.Add(Witness); }
	const auto Current = EvidenceStageIn(Prior, Item.EvidenceId, Item.Protagonist);
	if (!Item.Definition)
	{
		return Current == ESovEvidenceStage::Unknown && Item.Stage == ESovEvidenceStage::Observed
			&& Item.SourceLocationId.IsNone() && Item.CustodianId.IsNone() && Item.SupportingEvidenceId.IsNone()
			&& Item.CopyDestination.IsNone() && Item.WitnessIds.IsEmpty() && Item.Publicity == ESovRecordPublicity::Private;
	}
	FString Error;
	if (!Item.Definition->ValidateDefinition(Error) || Item.Definition->EvidenceId != Item.EvidenceId
		|| !Item.Definition->RelevantMissions.Contains(Item.MissionId) || Item.SourceLocationId.IsNone()
		|| !Item.Definition->SourceCustodians.Contains(Item.CustodianId)) { return false; }
	const auto* Original = Prior.FindByPredicate([&Item](const auto& Existing)
	{
		const FName Hero = NarrativeIdentity(Item.Protagonist);
		return Existing.EvidenceId == Item.EvidenceId && (Existing.Protagonist == Item.Protagonist
			|| Existing.WitnessIds.Contains(Hero) || Existing.CopyDestination == Hero);
	});
	for (const auto& Existing : Prior)
	{
		if (Existing.EvidenceId == Item.EvidenceId && Existing.Definition && Existing.Definition != Item.Definition) { return false; }
		if (Existing.SourceId == Item.SourceId && (Existing.EvidenceId != Item.EvidenceId || Existing.MissionId != Item.MissionId
			|| Existing.SourceLocationId != Item.SourceLocationId || Existing.CustodianId != Item.CustodianId
			|| Existing.Definition != Item.Definition || Existing.AcquisitionBeat != Item.AcquisitionBeat)) { return false; }
	}
	const bool Independent = Original && Item.SourceId != Original->SourceId && Item.CustodianId != Original->CustodianId
		&& Item.Definition->SupportingEvidenceIds.Contains(Item.SupportingEvidenceId)
		&& EvidenceKnownTo(Prior, Item.SupportingEvidenceId, NarrativeIdentity(Item.Protagonist));
	const bool Authentic = Item.Definition->AuthenticationAuthorities.Contains(Item.CustodianId);
	const bool Destination = Item.Stage == ESovEvidenceStage::Distributed && Item.CopyDestination == Item.CustodianId
		&& Item.Definition->CopyDestinations.Contains(Item.CopyDestination);
	const bool Copied = Prior.ContainsByPredicate([&Item](const auto& Existing)
	{ return Existing.EvidenceId == Item.EvidenceId && !Item.CopyDestination.IsNone() && Existing.CopyDestination == Item.CopyDestination; });
	if (Item.Stage != ESovEvidenceStage::Distributed && !Item.CopyDestination.IsNone()) { return false; }
	if (Item.Stage != ESovEvidenceStage::Corroborated && !Item.SupportingEvidenceId.IsNone()) { return false; }
	return SovEvidencePolicy::CanAdvance(static_cast<unsigned>(Current), static_cast<unsigned>(Item.Stage), Independent, Authentic, Destination, Copied);
}
bool USovCampaignStateComponent::FindConsequence(FName Id, FName Observer, FSovConsequenceRecord& OutRecord) const
{
	OutRecord = FSovConsequenceRecord();
	if (!bStateValid || Id.IsNone()) { return false; }
	for (const auto& Entry : Journal)
	{
		for (const auto& Record : Entry.Consequences)
		{
			if (Record.Definition.ConsequenceId != Id) { continue; }
			bool Knows = Observer.IsNone() || Record.ResolvedInstigatorId == Observer || Record.Definition.WitnessIds.Contains(Observer);
			if (!Knows)
			{
				Knows = Journal.ContainsByPredicate([Id, Observer](const auto& Other)
				{ return Other.RelationshipMemories.ContainsByPredicate([Id, Observer](const auto& Memory) { return Memory.ConsequenceId == Id && Memory.HolderId == Observer; }); });
			}
			if (Knows) { OutRecord = Record; return true; }
			return false;
		}
	}
	return false;
}
bool USovCampaignStateComponent::HasRelationshipMemory(FName Holder, FName Subject, ESovRelationshipMemoryType Type, FName Consequence) const
{
	return bStateValid && !Holder.IsNone() && !Subject.IsNone() && Journal.ContainsByPredicate([=](const auto& Entry)
	{
		return Entry.RelationshipMemories.ContainsByPredicate([=](const auto& Memory)
		{ return Memory.HolderId == Holder && Memory.SubjectId == Subject && Memory.Type == Type && (Consequence.IsNone() || Memory.ConsequenceId == Consequence); });
	});
}
