#include "Narrative/SovNarrativeCue.h"
#include "Tales/Dialogue.h"
bool USovNarrativeCue::Validate(FString& Error) const
{
	if (CueId.IsNone() || SpeakerId.IsNone() || static_cast<uint8>(Priority) > 5
		|| !FMath::IsFinite(CooldownSeconds) || CooldownSeconds < 0.f || CooldownSeconds > 300.f
		|| !FMath::IsFinite(ContextLifetimeSeconds) || ContextLifetimeSeconds < 1.f || ContextLifetimeSeconds > 120.f
		|| (bCritical && Priority != ESovNarrativeCuePriority::ObjectiveCritical)
		|| (bRecordUnheardSummary && (!bCritical || RecordSummary.IsEmpty())))
	{ Error = TEXT("Cue requires stable IDs, valid timing/priority, and a diegetic summary for recorded critical content."); return false; }
	if (Dialogue)
	{
		if (!BarkVariants.IsEmpty() || !Dialogue->GetDefaultObject<UDialogue>()->CanSuspendPlayback())
		{ Error = TEXT("Queued conversations must be free-movement Narrative graphs without speaker control tags, body montages or camera shots."); return false; }
	}
	else
	{
		if (BarkVariants.IsEmpty() || BarkVariants.Num() > 32) { Error = TEXT("Bark needs 1..32 captioned variants."); return false; }
		for (const auto& Variant : BarkVariants)
		{ if (Variant.Caption.IsEmpty() || !FMath::IsFinite(Variant.CaptionSeconds) || Variant.CaptionSeconds < .5f || Variant.CaptionSeconds > 30.f)
			{ Error = TEXT("Every bark needs a localized caption and a bounded readable duration."); return false; } }
	}
	return true;
}
