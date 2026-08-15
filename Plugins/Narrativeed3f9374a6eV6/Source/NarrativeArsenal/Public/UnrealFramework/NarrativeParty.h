// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Tales/NarrativePartyComponent.h"
#include "NarrativeParty.generated.h"

/**
 * Generic implementation for a party full of characters. Mostly used to encapsulate a party component for a group, but useful for any other scenarios where you have some data that should only rep to party members. 
 */
UCLASS(Blueprintable)
class NARRATIVEARSENAL_API ANarrativeParty : public AInfo
{
	GENERATED_BODY()
	
public:
	
	ANarrativeParty(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
	
	UFUNCTION(BlueprintCallable, Category = "Narrative Party")
	virtual void AddPartyMember(ANarrativePlayerState* PS);

	UFUNCTION(BlueprintCallable, Category = "Narrative Party")
	virtual void RemovePartyMember(ANarrativePlayerState* PS);

	//This parties tales component, in case any members want to do dialogue/quests together. 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Playback")
	TObjectPtr<UNarrativePartyComponent> PartyTalesComponent; 
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Playback")
	TArray<TObjectPtr<class ANarrativePlayerState>> PartyMembers;

	//Allows for efficient calls of IsNetRelevantFor
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Playback")
	TSet<TObjectPtr<class AActor>> PartyMemberControllers;
};
