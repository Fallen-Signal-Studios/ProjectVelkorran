// Copyright Narrative Tools 2025.

#pragma once

#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "TaggedMusicSet.generated.h"

USTRUCT(BlueprintType)
struct FMusicSound
{
	GENERATED_BODY()

	// sound to play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MusicSound")
	TObjectPtr<class USoundBase> Music;

	// length of time it takes to fade in this sound
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MusicSound")
	float FadeInDuration;

	// length of time it takes to fade out this sound
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MusicSound")
	float FadeOutDuration;

	FMusicSound() : FadeInDuration(3.0f), FadeOutDuration(3.0f) {}
};


USTRUCT(BlueprintType)
struct FMusicTracksContainer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MusicContainer")
	TArray<FMusicSound> MusicSounds;
};

UCLASS(MinimalAPI, BlueprintType)
class UTaggedMusicSet : public UDataAsset
{
	GENERATED_BODY()

protected:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="TaggedMusicSet", meta=(Categories="Music"))
	TMap<FGameplayTag, FMusicTracksContainer> MusicSets;

public:

	bool Has(const FGameplayTag& Tag) const { return !MusicSets.IsEmpty()? MusicSets.Contains(Tag) : false; }
	FMusicSound Get(const FGameplayTag& Tag);
	
};
