// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Dismemberment/SovDismembermentProfile.h"

bool USovDismembermentProfile::GetRegionDefinition(
	const ESovDismembermentRegion Region,
	FSovDismembermentRegionDefinition& OutDefinition) const
{
	if (const FSovDismembermentRegionDefinition* Definition = Regions.FindByPredicate(
		[Region](const FSovDismembermentRegionDefinition& Candidate)
		{
			return Candidate.Region == Region;
		}))
	{
		OutDefinition = *Definition;
		return true;
	}

	return false;
}
