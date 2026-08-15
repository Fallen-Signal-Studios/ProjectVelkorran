// Copyright Narrative Tools 2022. 


#include "Tales/NarrativeFunctionLibrary.h"
#include "Tales/TalesComponent.h"
#include "Tales/NarrativeTaskManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"

class UTalesComponent* UNarrativeFunctionLibrary::GetTalesComponent(const UObject* WorldContextObject)
{
	return GetTalesComponentFromTarget(UGameplayStatics::GetPlayerController(WorldContextObject, 0));
}

class UTalesComponent* UNarrativeFunctionLibrary::GetTalesComponentFromTarget(AActor* Target)
{
	if (!Target)
	{
		return nullptr;
	}

	//In pro, we just find the tales component from PC as it always lives there. 
	if (INarrativeCharacterOwner* CharOwner = Cast<INarrativeCharacterOwner>(Target))
	{
		if (ANarrativeCharacter* Char = CharOwner->GetNarrativeCharacter())
		{
			if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(Char->GetOwningController()))
			{
				return PC->GetTalesComponent();
			}
		}
	}


	//Highly unlikely this code below will ever get called as is legacy, but no harm keeping it. 
	if (UTalesComponent* NarrativeComp = Target->FindComponentByClass<UTalesComponent>())
	{
		return NarrativeComp;
	}

	//Narrative comp may be on the controllers pawn or pawns controller
	if (APlayerController* OwningController = Cast<APlayerController>(Target))
	{
		if (OwningController->GetPawn())
		{
			return OwningController->GetPawn()->FindComponentByClass<UTalesComponent>();
		}
	}

	if (APawn* OwningPawn = Cast<APawn>(Target))
	{
		if (OwningPawn->GetController())
		{
			return OwningPawn->GetController()->FindComponentByClass<UTalesComponent>();
		}
	}

	return nullptr;
}

bool UNarrativeFunctionLibrary::CompleteNarrativeDataTask(class UTalesComponent* Target, const UNarrativeDataTask* Task, const FString& Argument, const int32 Quantity)
{
	if (Target)
	{
		return Target->CompleteNarrativeDataTask(Task, Argument, Quantity);
	}
	return false;
}

static FString DefaultString("LooseTask");

bool UNarrativeFunctionLibrary::CompleteLooseNarrativeDataTask(class UTalesComponent* Target, const FString& Argument, const int32 Quantity /*= 1*/)
{
	if (Target)
	{
		return Target->CompleteNarrativeDataTask(DefaultString, Argument, Quantity);
	}
	return false;
}

class UNarrativeDataTask* UNarrativeFunctionLibrary::GetTaskByName(const UObject* WorldContextObject, const FString& EventName)
{
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject))
	{
		if (UNarrativeTaskManager* EventManager = GI->GetSubsystem<UNarrativeTaskManager>())
		{
			return EventManager->GetTask(EventName);
		}
	}

	return nullptr;
}

FString UNarrativeFunctionLibrary::MakeDisplayString(const FString& String)
{
	return FName::NameToDisplayString(String, false);
}

FDialogueNodeSelector UNarrativeFunctionLibrary::MakeDialogueNodeSelector(FDialogueNodeSelector Selector)
{
	return Selector;
}

FDialogueNodeSelector UNarrativeFunctionLibrary::MakeDialogueNodeSelectorFromID(FName NodeID)
{
	FDialogueNodeSelector Sel;
	Sel.NodeID = NodeID;
	Sel.Asset = nullptr;
	return Sel;
}

void UNarrativeFunctionLibrary::BreakDialogueNodeSelector(const FDialogueNodeSelector& Selector, FName& NodeID)
{
	NodeID = Selector.NodeID;
}

FName UNarrativeFunctionLibrary::Conv_DialogueNodeSelectorToName(const FDialogueNodeSelector& Selector)
{
	return Selector.NodeID;
}

FDialogueNodeSelector UNarrativeFunctionLibrary::Conv_NameToDialogueNodeSelector(const FName& NodeID)
{
	FDialogueNodeSelector Sel;
	Sel.NodeID = NodeID;
	return Sel;
}

FQuestStateSelector UNarrativeFunctionLibrary::MakeQuestStateSelector(FQuestStateSelector Selector)
{
	return Selector;
}

FQuestStateSelector UNarrativeFunctionLibrary::MakeQuestStateSelectorFromID(FName NodeID)
{
	FQuestStateSelector Sel;
	Sel.NodeID = NodeID;
	Sel.Asset = nullptr;
	return Sel;
}

void UNarrativeFunctionLibrary::BreakQuestStateSelector(const FQuestStateSelector& Selector, FName& NodeID)
{
	NodeID = Selector.NodeID;
}

FName UNarrativeFunctionLibrary::Conv_QuestStateSelectorToName(const FQuestStateSelector& Selector)
{
	return Selector.NodeID;
}

FQuestStateSelector UNarrativeFunctionLibrary::Conv_NameToQuestStateSelector(const FName& NodeID)
{
	FQuestStateSelector Sel;
	Sel.NodeID = NodeID;
	return Sel;
}

FQuestBranchSelector UNarrativeFunctionLibrary::MakeQuestBranchSelector(FQuestBranchSelector Selector)
{
	return Selector;
}

FQuestBranchSelector UNarrativeFunctionLibrary::MakeQuestBranchSelectorFromID(FName NodeID)
{
	FQuestBranchSelector Sel;
	Sel.NodeID = NodeID;
	Sel.Asset = nullptr;
	return Sel;
}

void UNarrativeFunctionLibrary::BreakQuestBranchSelector(const FQuestBranchSelector& Selector, FName& NodeID)
{
	NodeID = Selector.NodeID;
}

FName UNarrativeFunctionLibrary::Conv_QuestBranchSelectorToName(const FQuestBranchSelector& Selector)
{
	return Selector.NodeID;
}

FQuestBranchSelector UNarrativeFunctionLibrary::Conv_NameToQuestBranchSelector(const FName& NodeID)
{
	FQuestBranchSelector Sel;
	Sel.NodeID = NodeID;
	return Sel;
}
