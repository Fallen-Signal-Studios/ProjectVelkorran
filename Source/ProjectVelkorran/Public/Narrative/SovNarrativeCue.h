// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SovNarrativeCue.generated.h"
class UDialogue;
class USoundBase;
class USoundClass;

UENUM(BlueprintType)
enum class ESovNarrativeCuePriority : uint8 { LethalWarning, ObjectiveCritical, CompanionRescue, Tactical, Relationship, Ambient };
USTRUCT(BlueprintType)
struct FSovBarkVariant
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Caption;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<USoundBase> Sound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.5",ClampMax="30")) float CaptionSeconds = 3.f;
};
/** An authored cue through the existing Narrative dialogue/audio paths. Lower priority value wins. */
UCLASS(BlueprintType)
class PROJECTVELKORRAN_API USovNarrativeCue : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CueId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SpeakerId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ESovNarrativeCuePriority Priority = ESovNarrativeCuePriority::Ambient;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag RequiredProtagonist;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTagContainer RequiredKnowledge;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName RequiredMission;
	/** Optional native Narrative graph; otherwise select the next bark variant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<UDialogue> Dialogue;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FSovBarkVariant> BarkVariants;
	/** Optional platform-authored voice class. Must request ControllerFallbackToSpeaker, never controller-only.
	 * Unsupported/unconfigured classes retain ordinary positional speech. Hardware delivery is platform-dependent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio") TSoftObjectPtr<USoundClass> ControllerAudioClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bPlayerSpeaker = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="300")) float CooldownSeconds = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1",ClampMax="120")) float ContextLifetimeSeconds = 15.f;
	/** Critical directions may persist until safe; ambient cues may be discarded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bCritical = false;
	/** Only a diegetically justified summary may appear in the record interface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bRecordUnheardSummary = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText RecordSummary;
	bool Validate(FString& Error) const;
};
