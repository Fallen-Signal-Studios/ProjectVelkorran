// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovNarrativeTypes.h"
namespace
{
	bool ValidIds(const TArray<FName>& Ids, int32 Max)
	{
		if (Ids.Num() > Max) { return false; }
		TSet<FName> Unique;
		for (FName Id : Ids) { if (Id.IsNone() || Unique.Contains(Id)) { return false; } Unique.Add(Id); }
		return true;
	}
}
bool FSovConsequencePayload::IsValid() const
{
	if (Key.IsNone()) { return false; }
	switch (Type)
	{
	case ESovConsequencePayloadType::Count: return Count >= 0 && Count <= 1000000000 && Reference.IsNone() && !Tag.IsValid();
	case ESovConsequencePayloadType::Reference: return !Reference.IsNone() && Count == 0 && !Tag.IsValid();
	case ESovConsequencePayloadType::Tag: return Tag.IsValid() && Count == 0 && Reference.IsNone();
	default: return false;
	}
}
bool FSovConsequencePayload::operator==(const FSovConsequencePayload& Other) const
{
	return Key == Other.Key && Type == Other.Type && Count == Other.Count && Reference == Other.Reference && Tag == Other.Tag;
}
bool FSovConsequenceDefinition::Validate(FString& OutError) const
{
	if (ConsequenceId.IsNone() || !ChoiceTag.IsValid() || !OutcomeTag.IsValid() || SubjectIds.IsEmpty()
		|| !ValidIds(SubjectIds, 32) || !ValidIds(WitnessIds, 32) || !ValidIds(ConsumerIds, 32)
		|| Publicity > ESovRecordPublicity::Public || CanonClass > ESovConsequenceCanonClass::FixedPresentation
		|| Persistence > ESovConsequencePersistence::SequelExportCandidate || (!bArchivalOnly && ConsumerIds.IsEmpty()) || Payload.Num() > 16)
	{
		OutError = TEXT("Consequence requires a stable identity, action/result, bounded subjects/witnesses/payload and consumers or explicit archival purpose."); return false;
	}
	TSet<FName> Keys;
	for (const auto& Item : Payload)
	{
		if (!Item.IsValid() || Keys.Contains(Item.Key)) { OutError = TEXT("Consequence payload must have unique keys and one bounded typed value per key."); return false; }
		Keys.Add(Item.Key);
	}
	OutError.Reset(); return true;
}
bool FSovConsequenceDefinition::operator==(const FSovConsequenceDefinition& Other) const
{
	return ConsequenceId == Other.ConsequenceId && InstigatorId == Other.InstigatorId && SubjectIds == Other.SubjectIds
		&& ChoiceTag == Other.ChoiceTag && OutcomeTag == Other.OutcomeTag && WitnessIds == Other.WitnessIds
		&& Publicity == Other.Publicity && Payload == Other.Payload && CanonClass == Other.CanonClass
		&& Persistence == Other.Persistence && bArchivalOnly == Other.bArchivalOnly && ConsumerIds == Other.ConsumerIds;
}
bool FSovRelationshipMemoryDefinition::Validate(FString& OutError) const
{
	if (MemoryId.IsNone() || HolderId.IsNone() || SubjectId.IsNone() || HolderId == SubjectId || ConsequenceId.IsNone()
		|| Type > ESovRelationshipMemoryType::BoundaryCrossed || LearnedThrough > ESovKnowledgeMethod::Told)
	{
		OutError = TEXT("A relationship memory requires an identified holder, different subject, actual consequence and valid memory/knowledge kinds."); return false;
	}
	OutError.Reset(); return true;
}
bool FSovRelationshipMemoryDefinition::operator==(const FSovRelationshipMemoryDefinition& Other) const
{
	return MemoryId == Other.MemoryId && HolderId == Other.HolderId && SubjectId == Other.SubjectId
		&& ConsequenceId == Other.ConsequenceId && Type == Other.Type && LearnedThrough == Other.LearnedThrough;
}
