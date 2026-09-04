// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Corruption/SovCorruptionProfile.h"
#include "Sovereign/SovGameplayTags.h"

bool USovCorruptionProfile::ValidateProfile(FString& OutError) const
{
	const auto NonNegative = [](float Value) { return FMath::IsFinite(Value) && Value >= 0.0f; };
	const auto ValidBand = [](ESovCorruptionBand Band) { return Band >= ESovCorruptionBand::Trace && Band <= ESovCorruptionBand::OverwriteRisk; };
	if (SourceId.IsNone() || !NonNegative(ExposurePerSecond) || !NonNegative(ContactExposure)
		|| (ExposurePerSecond <= 0.0f && ContactExposure <= 0.0f)
		|| ExposurePerSecond > 100.0f || ContactExposure > 100.0f || !FMath::IsFinite(Radius) || Radius <= 0.0f || Radius > 100000.0f
		|| !ValidBand(MaximumBand) || !NonNegative(EscapeRecoveryPerSecond) || EscapeRecoveryPerSecond > 100.0f
		|| (Escape == ESovCorruptionEscape::LeaveField && EscapeRecoveryPerSecond <= 0.0f)
		|| Escape > ESovCorruptionEscape::CompleteObjective || RemedyText.IsEmpty() || InformationText.IsEmpty()
		|| ReducedEffectsSubstitute.IsEmpty() || PresentationProfileId.IsNone()
		|| !NonNegative(PresentationIntensity) || PresentationIntensity > 1.0f)
	{
		OutError = TEXT("Source identity, finite tuning, exposure, remedy, presentation and accessibility substitute are required."); return false;
	}
	const auto& Tags = FSovGameplayTags::Get();
	if (AllowedProtagonists.IsEmpty()) { OutError = TEXT("Allowed protagonist identities must be explicit."); return false; }
	for (const auto& Identity : AllowedProtagonists)
	{
		if (Identity != Tags.Character_Player_Tarrik && Identity != Tags.Character_Player_Selene)
		{
			OutError = TEXT("Only explicit Tarrik/Selene target identities are supported by this player exposure source."); return false;
		}
	}
	if (MissionPermissions.IsEmpty()) { OutError = TEXT("At least one explicit mission permission is required."); return false; }
	TSet<FName> Missions;
	for (const auto& Permission : MissionPermissions)
	{
		if (Permission.MissionId.IsNone() || Missions.Contains(Permission.MissionId) || !ValidBand(Permission.MaximumBand))
		{
			OutError = TEXT("Mission permissions require unique mission IDs and valid non-clear caps."); return false;
		}
		Missions.Add(Permission.MissionId);
	}
	if (bCanonPersistent && (ConsequenceBeatId.IsNone() || !ValidBand(ConsequenceBand) || ConsequenceBand > MaximumBand))
	{
		OutError = TEXT("Canon-persistent contact requires an explicit existing consequence beat and a reachable band."); return false;
	}
	if (!bCanonPersistent && !ConsequenceBeatId.IsNone())
	{
		OutError = TEXT("A consequence beat must explicitly opt in to canon persistence."); return false;
	}
	OutError.Reset(); return true;
}
bool USovCorruptionProfile::PermissionForMission(FName MissionId, ESovCorruptionBand& OutCap) const
{
	OutCap = ESovCorruptionBand::Clear;
	FString Error;
	if (!ValidateProfile(Error)) { return false; }
	for (const auto& Permission : MissionPermissions)
	{
		if (Permission.MissionId == MissionId)
		{
			OutCap = static_cast<ESovCorruptionBand>(FMath::Min(static_cast<uint8>(MaximumBand), static_cast<uint8>(Permission.MaximumBand)));
			return true;
		}
	}
	return false;
}
