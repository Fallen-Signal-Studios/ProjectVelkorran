// Copyright Narrative Tools 2025.

#include "Music/TaggedMusicSet.h"

FMusicSound UTaggedMusicSet::Get(const FGameplayTag& Tag)
{
	if (Has(Tag))
	{
		FMusicTracksContainer& TracksContainer = MusicSets[Tag];
		const int32 SelectedSoundIndex = FMath::RandRange(0, TracksContainer.MusicSounds.Num()-1);
		if (TracksContainer.MusicSounds.IsValidIndex(SelectedSoundIndex))
		{
			return TracksContainer.MusicSounds[SelectedSoundIndex];
		}
	}
	return FMusicSound();
}
