// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include <GameplayTagContainer.h>
#include "NPCGoalItem.h"
#include "ActivityGroup.generated.h"

/**
 * Groups activities so we can filter out invalid ones early 
 */
UCLASS(Blueprintable, EditInlineNew, AutoExpandCategories = ("Default"))
class NARRATIVEARSENAL_API UActivityGroup : public UObject
{
	GENERATED_BODY()
	
public:

	 UActivityGroup(const FObjectInitializer& ObjectInitializer);

	
	void SetOwner(class ANarrativeNPCController* OwnerController, class UNPCActivityComponent* OwnerComp);

	/**Define whether or not this group is usable. can optionally use FailReason to define why it failed */
	UFUNCTION(BlueprintNativeEvent, Category = "Activity Group")
	bool CanUseGroup(FString& FailReason) const;
	virtual bool CanUseGroup_Implementation(FString& FailReason) const;

	void GetActivitesInGroup(TArray<class UNPCActivity*>& OutActivities) const;

protected:

	//Our controller, cached
	UPROPERTY(BlueprintReadOnly, Category = "Activities")
	TObjectPtr<ANarrativeNPCController> OwnerController;

	//Owner comp, cached
	UPROPERTY(BlueprintReadOnly, Category = "Activities")
	TObjectPtr<UNPCActivityComponent> OwnerActivityComp;

	//An optional tag, can be used to grab the group in case you want to add/remove an activity from it. 
	UPROPERTY(EditDefaultsOnly, Category = "Activity Group")
	FGameplayTag GroupTag;

	//The goals this group supports. This is useful to "early out", ie don't check the group unless we have a goal of this type. 
	UPROPERTY(EditDefaultsOnly, Category = "Activity Group")
	TSubclassOf<class UNPCGoalItem> SupportedGoal;

	//Subgroups of the group 
	UPROPERTY(Instanced, EditDefaultsOnly, BlueprintReadOnly, Category = "Activity Group")
	TArray<TObjectPtr<class UActivityGroup>> Subgroups; 

	//Activities in the group 
	UPROPERTY(Instanced, EditDefaultsOnly, BlueprintReadOnly, Category = "Activity Group")
	TArray<TObjectPtr<class UNPCActivity>> GroupActivities;
};
