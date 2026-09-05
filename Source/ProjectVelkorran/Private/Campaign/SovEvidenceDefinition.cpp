// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovEvidenceDefinition.h"
bool USovEvidenceDefinition::ValidateDefinition(FString& OutError) const
{
	if (EvidenceId.IsNone() || CanonicalContentId.IsNone() || Summary.IsEmpty() || OriginalCustodian.IsNone()
		|| !SourceCustodians.Contains(OriginalCustodian) || RelevantMissions.IsEmpty())
	{
		OutError = TEXT("Evidence requires canonical content, accessible summary, original custodian and explicit mission relevance."); return false;
	}
	const TArray<FName>* Lists[] = { &SourceCustodians, &AuthenticationAuthorities, &CopyDestinations,
		&SupportingEvidenceIds, &Claims, &Contradictions, &KnownAlterations, &RelevantMissions };
	for (const auto* List : Lists)
	{
		if (List->Num() > 64) { OutError = TEXT("Evidence metadata list exceeds the bounded 64-entry contract."); return false; }
		TSet<FName> Seen;
		for (FName Id : *List) { if (Id.IsNone() || Seen.Contains(Id)) { OutError = TEXT("Evidence metadata IDs must be nonempty and unique."); return false; } Seen.Add(Id); }
	}
	for (FName Authority : AuthenticationAuthorities)
	{
		if (!SourceCustodians.Contains(Authority)) { OutError = TEXT("An authentication authority must be an explicit source custodian."); return false; }
	}
	if (CopyDestinations.Contains(OriginalCustodian) || SupportingEvidenceIds.Contains(EvidenceId))
	{
		OutError = TEXT("Distribution must leave the original custodian; corroboration must be independent evidence."); return false;
	}
	if (CustodianInstitutions.Num() > 64) { OutError = TEXT("Evidence institution map exceeds the bounded contract."); return false; }
	for (const auto& Pair : CustodianInstitutions)
	{
		if (!SourceCustodians.Contains(Pair.Key) || Pair.Value.IsNone()) { OutError = TEXT("Institution entries require an allowed custodian and a named control domain."); return false; }
	}
	const FName* OriginalInstitution = CustodianInstitutions.Find(OriginalCustodian);
	for (FName Destination : CopyDestinations)
	{
		const FName* DestinationInstitution = CustodianInstitutions.Find(Destination);
		if (!SourceCustodians.Contains(Destination) || !OriginalInstitution || !DestinationInstitution || *OriginalInstitution == *DestinationInstitution)
		{ OutError = TEXT("Every distribution destination must be an allowed custodian outside the original institution's control."); return false; }
	}
	OutError.Reset(); return true;
}
