// Copyright Narrative Tools 2022. 

#include "Tales/Quest.h"
#include "Tales/NarrativeDataTask.h"
#include "Tales/QuestSM.h"
#include "Tales/QuestBlueprintGeneratedClass.h"
#include "Tales/NarrativeEvent.h"
#include "Tales/TalesComponent.h"
#include "Tales/NarrativeFunctionLibrary.h"
#include "Tales/NarrativePartyComponent.h"
#include <GameFramework/PlayerController.h>
#include "Net/UnrealNetwork.h"
#include "Navigation/MapMarker.h"
#include "StableActorStatics.h"

FQuestDelegates::FOnQuestStartedSignature FQuestDelegates::OnQuestStarted;
FQuestDelegates::FOnQuestEndSignature FQuestDelegates::OnQuestEnd;

UQuest::UQuest()
{
	QuestName = FText::FromString("My New Quest");
	QuestDescription = FText::FromString("Enter a description for your quest here.");
	bTracked = true; 
	bResumeDialogueAfterLoad = true; 

	QuestDialoguePlayParams.bOverride_bFreeMovement = true;
	QuestDialoguePlayParams.bFreeMovement = true;
	QuestDialoguePlayParams.bOverride_bUnskippable = true;
	QuestDialoguePlayParams.bUnskippable = true;
	QuestDialoguePlayParams.bOverride_bCanBeExited = true;
	QuestDialoguePlayParams.bCanBeExited = false; 
}

UWorld* UQuest::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	if (OwningComp)
	{
		return OwningComp->GetWorld();
	}
	
	return nullptr;
}

void UQuest::DuplicateAndInitializeFromQuest(UQuest* QuestTemplate)
{
	if (QuestTemplate)
	{
		//Duplicate the quest template, then steal all its states and branches
		UQuest* NewQuest = Cast<UQuest>(StaticDuplicateObject(QuestTemplate, this, NAME_None, RF_Transactional));
		NewQuest->SetFlags(RF_Transient | RF_DuplicateTransient);
		
		if (NewQuest)
		{
			QuestStartState = NewQuest->QuestStartState;
			States = NewQuest->States;
			Branches = NewQuest->Branches;
		}
	}
}

bool UQuest::Initialize(class UTalesComponent* InitializingComp, const FName& QuestStartID /*= NAME_None*/)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		//We need a valid narrative component to make a quest for 
		if (!InitializingComp)
		{
			return false;
		}

		if (UQuestBlueprintGeneratedClass* BGClass = Cast<UQuestBlueprintGeneratedClass>(GetClass()))
		{
			BGClass->InitializeQuest(this);

			//If a quest doesn't have any states or branches, or doesn't have a valid start state something has gone wrong and we should abort
			if (States.Num() == 0 || Branches.Num() == 0 || !QuestStartState)
			{
				return false;
			}

			//At this point, we should have a valid quest assigned to us. Check if we have a valid start state
			if (QuestStartState)
			{
				//Add the inheritable states to the states list 
				States.Append(InheritableStates);

				OwningComp = InitializingComp;

				OwningPawn = OwningComp->GetOwningPawn();
				OwningController = OwningComp->GetOwningController();

				for (auto& Node : GetNodes())
				{
					if (Node)
					{
						Node->OwningQuest = this;
					}
				}

				return true;
			}
		}
	}


	return false;
}

void UQuest::Deinitialize()
{
	//Deactivate every node in the quest, we don't want quest tasks updating any more 
	for (auto& Branch : Branches)
	{
		if (Branch)
		{
			Branch->Deactivate();
		}
	}

	for (auto& State : States)
	{
		if (State)
		{
			State->Deactivate();
		}
	}

	for (auto& QuestActor : QuestActors)
	{
		bool bSucceeded;
		if (AActor* Actor = UStableActorStatics::GetStableActor(this, QuestActor, bSucceeded, AActor::StaticClass()))
		{	
			Actor->Destroy();
		}

	}

	QuestActors.Empty();

	for (auto& Req : QuestRequirements)
	{
		if (Req)
		{
			Req->OnRemoved(this);
		}
	}

	QuestRequirements.Empty();
}

void UQuest::BeginQuest(const FName& QuestStartID /** = NAME_None*/)
{
	BPPreQuestStarted(this);

	QuestCompletion = EQuestCompletion::QC_Started;
	EnterState_Internal(QuestStartID.IsNone() ? QuestStartState : GetState(QuestStartID));

	BPOnQuestStarted(this);

	if (OwningComp)
	{
		// notify other listening objects
		FQuestDelegates::OnQuestStarted.Broadcast(OwningComp, this, QuestStartID);
		
		OwningComp->OnQuestStarted.Broadcast(this);
	}
}

void UQuest::HandleBeginDialogue()
{
	if (!QuestDialogue || !OwningComp)
	{
		return;
	}
	
	FDialoguePlayParams PlayParams = QuestDialoguePlayParams;

	PlayParams.StartFromID = CurrentState ? CurrentState->GetID() : NAME_None;

	if (OwningComp->HasDialogueAvailable(QuestDialogue, PlayParams))
	{
		OwningComp->BeginDialogue(QuestDialogue, PlayParams);
	}
}

void UQuest::TakeBranch(UQuestBranch* Branch)
{
	//We're taking a branch, deactivate it, fire off its bound function and events, and then head to the destination state
	if (Branch)
	{
		Branch->Deactivate();
	}

	OnQuestBranchCompleted(Branch);

	//Client can call this function in order to process delegates and things but server needs to be setting the state, not client 
	if (OwningComp)
	{
		EnterState_Internal(Branch->DestinationState);
	}
}

void UQuest::SetTracked(const bool bNewTracked)
{
	if (bNewTracked != bTracked)
	{
		bTracked = bNewTracked;

		//Get our current quest tasks markers and update them 
		if (CurrentState)
		{
			for (auto& Branch : CurrentState->Branches)
			{
				if (Branch)
				{
					for (auto& Task : Branch->QuestTasks)
					{
						if (Task && Task->SpawnedMarker)
						{
							if (UMapMarker* NavMarker = Task->SpawnedMarker)
							{
								if (bTracked)
								{
									NavMarker->RegisterMarker();
								}
								else
								{
									NavMarker->RemoveMarker();
								}
								//TODO MARKERWORK
								//NavMarker->SetActive(bNewTracked);
							}
						}
					}
				}
			}
		}
	}
}

void UQuest::EnterState(UQuestState* NewState)
{
	if (NewState && States.Contains(NewState))
	{
		EnterState_Internal(NewState);

		//If we're explicitly setting the state instead of moving to a new state by completing tasks, clients need to be told to go also 
		if (OwningComp)
		{
			OwningComp->SendNarrativeUpdate(FNarrativeUpdate::QuestNewState(GetClass(), NewState->GetID()));
		}
	}
}

void UQuest::EnterState_Internal(UQuestState* NewState)
{
	if (NewState && OwningComp)
	{
		//Before we set our new state, deactivate the old one
		if (CurrentState)
		{
			CurrentState->Deactivate();
		}

		CurrentState = NewState;
		ReachedStates.Add(CurrentState);

		if (!OwningComp->bIsLoading)
		{
			HandleBeginDialogue();
		}

		//Update the quests completion
		if (NewState->StateNodeType == EStateNodeType::Success)
		{
			SucceedQuest(CurrentState->Description);
		}
		else if (NewState->StateNodeType == EStateNodeType::Failure)
		{
			FailQuest(CurrentState->Description);
		}

		//We dont call delegate updates when loading, as delegates are typically just for UI updates and things 
		if (!OwningComp->bIsLoading)
		{
			//Fire delegates because we're about to activate the current state. This can actually cause another state change, which will cause delegates to fire in the wrong order. 
			BPOnQuestNewState(this, NewState);

			if (OwningComp)
			{
				OwningComp->OnQuestNewState.Broadcast(this, NewState);
			}

			QuestNewState.Broadcast(this, CurrentState);
		}

		//Finally, activate our new state, therefore activating its branches allowing us to take one to progress through the quest. 
		CurrentState->Activate();

	}
}

class UQuestState* UQuest::GetState(FName ID) const
{
	for (auto& State : States)
	{
		if (State && State->GetID() == ID)
		{
			return State;
		}
	}
	return nullptr;
}

class UQuestBranch* UQuest::GetBranch(FName ID) const
{
	for (auto& Branch : Branches)
	{
		if (Branch && Branch->GetID() == ID)
		{
			return Branch;
		}
	}
	return nullptr;
}

FText UQuest::GetQuestName() const
{
	return QuestName;
}

FText UQuest::GetQuestDescription() const
{
	return QuestDescription;
}

void UQuest::SetQuestName(const FText& NewName)
{
	QuestName = NewName;
}

void UQuest::SetQuestDescription(const FText& NewDescription)
{
	QuestDescription = NewDescription;
}

#if WITH_EDITOR
void UQuest::PostLoad()
{
	// fix missing transaction flag on nodes
	for (UQuestNode* Node : GetNodes())
	{
		Node->SetFlags(RF_Transactional);
	}

	// fix dialogue connection
	UpdateDialogueConnection();
	
	Super::PostLoad();
}

void UQuest::PreEditChange(FProperty* PropertyAboutToChange)
{
	const FName PropertyName = (PropertyAboutToChange != nullptr) ? PropertyAboutToChange->GetFName() : NAME_None;

	// update quest asset
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UQuest, QuestDialogue))
	{
		LastQuestDialogue = QuestDialogue;
	}
	
	Super::PreEditChange(PropertyAboutToChange);
}

void UQuest::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// update last dialogue connected quest
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UQuest, QuestDialogue))
	{
		UpdateDialogueConnection();
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UQuest::UpdateDialogueConnection()
{
	if (UDialogue* LastDialogue = Cast<UDialogue>(LastQuestDialogue.GetDefaultObject()))
	{
		// remove connected quest ref
		if (LastDialogue->EditorLinkedQuest == GetClass()->GetDefaultObject()->GetClass())
		{
			LastDialogue->Modify();
			LastDialogue->EditorLinkedQuest = nullptr;
			LastDialogue->MarkPackageDirty();
		}
		LastQuestDialogue = nullptr;
	}

	// update new dialogue connected quest
	if (UDialogue* NewDialogue = Cast<UDialogue>(QuestDialogue.GetDefaultObject()))
	{
		// set connected quest ref
		if (NewDialogue->EditorLinkedQuest == nullptr)
		{
			NewDialogue->Modify();
			NewDialogue->EditorLinkedQuest = GetClass()->GetDefaultObject()->GetClass();
			NewDialogue->MarkPackageDirty();
		}
	}
}
#endif

void UQuest::QuestPostLoad()
{
	if (bResumeDialogueAfterLoad && QuestCompletion == EQuestCompletion::QC_Started)
	{
		HandleBeginDialogue();
	}
	
	//When loading back in we need to re-register the quest markers. 
	const bool bWasTracked = bTracked;
	bTracked = !bTracked;
	SetTracked(bWasTracked);

	BPQuestPostLoad();
	OnQuestPostLoad.Broadcast(this);
}

void UQuest::FailQuest(FText QuestFailedMessage)
{
	QuestCompletion = EQuestCompletion::QC_Failed;

	if (OwningComp && !OwningComp->bIsLoading)
	{
		// notify listeners this quest has ended 
		FQuestDelegates::OnQuestEnd.Broadcast(OwningComp, this);
		
		BPOnQuestFailed(this, QuestFailedMessage);

		QuestFailed.Broadcast(this, QuestFailedMessage);

		OwningComp->OnQuestFailed.Broadcast(this, QuestFailedMessage);
	}

	Deinitialize();
}

void UQuest::SucceedQuest(FText QuestSucceededMessage)
{
	QuestCompletion = EQuestCompletion::QC_Succeded;

	if (OwningComp && !OwningComp->bIsLoading)
	{
		// notify listeners this quest has ended 
		FQuestDelegates::OnQuestEnd.Broadcast(OwningComp, this);
		
		BPOnQuestSucceeded(this, QuestSucceededMessage);

		QuestSucceeded.Broadcast(this, QuestSucceededMessage);

		OwningComp->OnQuestSucceeded.Broadcast(this, QuestSucceededMessage);

		QuestDialogue = nullptr;
	}

	Deinitialize();
}

void UQuest::OnQuestTaskProgressChanged(const UNarrativeTask* Task, const class UQuestBranch* Step, int32 CurrentProgress, int32 RequiredProgress)
{
	BPOnQuestTaskProgressChanged(this, Task, Step, CurrentProgress, RequiredProgress);

	QuestTaskProgressChanged.Broadcast(this, Task, Step, CurrentProgress, RequiredProgress);

	if (OwningComp)
	{
		OwningComp->OnQuestTaskProgressChanged.Broadcast(this, Task, Step, CurrentProgress, RequiredProgress);
	}
}

void UQuest::OnQuestTaskCompleted(const UNarrativeTask* Task, const class UQuestBranch* Branch)
{
	BPOnQuestTaskCompleted(this, Task, Branch);

	QuestTaskCompleted.Broadcast(this, Task, Branch);

	if (OwningComp)
	{
		OwningComp->OnQuestTaskCompleted.Broadcast(this, Task, Branch);
	}
}

void UQuest::OnQuestBranchCompleted(const class UQuestBranch* Step)
{
	QuestBranchCompleted.Broadcast(this, Step);

	BPOnQuestBranchCompleted(this, Step);

	if (OwningComp)
	{
		OwningComp->OnQuestBranchCompleted.Broadcast(this, Step);
	}
}

bool UQuest::SpawnQuestActor_Implementation(TSubclassOf<class AActor> ActorClass, FTransform ActorTransform, FStableActor& SpawnedActor)
{
	AActor* Actor = nullptr;

	//When reloading we often don't want to respawn quest actors, as those actors should respawn themselves via the save system. 
	if (OwningComp && OwningComp->bIsLoading)
	{
		return false; 
	}

	if (GetWorld())
	{
		FActorSpawnParameters SpawnParams;

		SpawnParams.bNoFail = true;
		SpawnParams.Owner = GetOwningController();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		Actor = GetWorld()->SpawnActor<AActor>(ActorClass, ActorTransform);
	}

	if (!Actor)
	{
		return false; 
	}

	SpawnedActor = UStableActorStatics::MakeStableActor(Actor);

	QuestActors.Add(SpawnedActor);

	return true;
}

void UQuest::AddQuestRequirement_Implementation(UQuestRequirement* Requirement)
{
	if (!QuestRequirements.Contains(Requirement))
	{
		QuestRequirements.Add(Requirement);
		Requirement->OnAdded(this);
	}
}

void UQuest::RemoveQuestRequirement_Implementation(UQuestRequirement* Requirement)
{
	if (QuestRequirements.Contains(Requirement))
	{
		QuestRequirements.Remove(Requirement);
		Requirement->OnRemoved(this);
	}
}

void UQuest::AddState(class UQuestState* State)
{
	States.Add(State);
}

void UQuest::AddBranch(class UQuestBranch* Branch)
{
	Branches.Add(Branch);
}

void UQuest::RemoveState(class UQuestState* State)
{
	States.Remove(State);
}

void UQuest::RemoveBranch(class UQuestBranch* Branch)
{
	Branches.Remove(Branch);
}

void UQuest::SetQuestStartState(class UQuestState* State)
{
	QuestStartState = State;
}

TArray<UQuestNode*> UQuest::GetNodes() const
{
	TArray<UQuestNode*> Ret;

	for (auto& State : States)
	{
		Ret.Add(State);
	}

	for (auto& Branch : Branches)
	{
		Ret.Add(Branch);
	}

	return Ret;
}

TArray<class APlayerController*> UQuest::GetGroupMembers() const
{
	TArray<class APlayerController*> GroupMembers;

	if (UNarrativePartyComponent* PartyComp = Cast<UNarrativePartyComponent>(OwningComp))
	{
		for (auto& GroupMemberComp : PartyComp->GetPartyMembers())
		{
			if (GroupMemberComp)
			{
				GroupMembers.Add(GroupMemberComp->GetOwningController());
			}
		}
	}
	else
	{
		GroupMembers.Add(OwningController);
	}

	return GroupMembers;
}

UQuestRequirement::UQuestRequirement()
{

}

void UQuestRequirement::OnAdded_Implementation(UQuest* Quest)
{

}

void UQuestRequirement::OnRemoved_Implementation(UQuest* Quest)
{

}

UQuest* UQuestRequirement::GetOwningQuest() const
{
	return Cast<UQuest>(GetOuter());
}
