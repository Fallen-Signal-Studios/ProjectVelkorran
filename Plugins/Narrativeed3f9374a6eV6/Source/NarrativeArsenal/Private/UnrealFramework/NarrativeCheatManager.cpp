// Copyright Narrative Tools 2024. 


#include "UnrealFramework/NarrativeCheatManager.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "SkillTrees/SkillTreeComponent.h"
#include "Items/InventoryComponent.h"
#include <GameFramework/PlayerController.h>
#include <Engine/World.h>
#include <AbilitySystemGlobals.h>
#include "AbilitySystemComponent.h"
#include "NarrativeGameplayTags.h"
#include "Items/NarrativeItem.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UnrealFramework/NarrativeGameMode.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Tales/TalesComponent.h"
#include "Tales/NarrativePartyComponent.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR

#include "Engine/Blueprint.h"

#endif


namespace  UE::NarrativeCheatManager::Private
{
	/**
	 * Common logic used to fuzzy-find a requested class (or alternatively, a passed-in asset path).
	 */
	template<typename ClassToFind>
	TSubclassOf<ClassToFind> FuzzyFindClass(FString SearchString)
	{
		TSubclassOf<ClassToFind> FoundClass;

		// See if we passed in a class name of a Class that already exists in memory.
		// If we passed-in Default__, just remove that part since we're looking for Classes, not CDO names.
		SearchString.RemoveFromStart(TEXT("Default__"));
		const int SearchStringLen = SearchString.Len();
		int BestClassMatchLen = INT_MAX;
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			const bool bClassMatches = ClassIt->IsChildOf(ClassToFind::StaticClass());
			if (!bClassMatches)
			{
				continue;
			}

			// Class name search
			const FString ClassName = ClassIt->GetName();
			const int ClassNameLen = ClassName.Len();
			if (ClassNameLen < BestClassMatchLen && ClassNameLen >= SearchStringLen)
			{
				bool bContains = ClassName.Contains(SearchString);
				if (bContains)
				{
					FoundClass = *ClassIt;
					BestClassMatchLen = ClassNameLen;
				}
			}
		}

		// If it wasn't a class name, then perhaps it was the path to a specific asset
		if (!FoundClass)
		{
			FSoftObjectPath SoftObjectPath{ SearchString };
			if (UObject* ReferencedObject = SoftObjectPath.ResolveObject())
			{
				if (UPackage* ReferencedPackage = Cast<UPackage>(ReferencedObject))
				{
					ReferencedObject = ReferencedPackage->FindAssetInPackage();
				}

#if WITH_EDITOR
				if (UBlueprint* ReferencedBlueprint = Cast<UBlueprint>(ReferencedObject))
				{
					FoundClass = ReferencedBlueprint->GeneratedClass;
				}
#endif
				
				if (!FoundClass)
				{
					if (ClassToFind* ReferencedGA = Cast<ClassToFind>(ReferencedObject))
					{
						FoundClass = ReferencedGA->GetClass();
					}
				}

			}
		}

		return FoundClass;
	}
}

void UNarrativeCheatManager::Ragdoll(const float Duration)
{
	if (APlayerController* PC = GetOuterAPlayerController())
	{
		if (!PC->HasAuthority())
		{
			const FString ServerCommand = FString::Printf(TEXT("%hs %f"), __func__, Duration);
			PC->ServerExec(ServerCommand);
			UE_LOG(LogConsoleResponse, Log, TEXT("Asking server for ragdoll for %f as we are not auth..."), Duration);
		}
		
		if (ANarrativeCharacter* PChar = Cast<ANarrativeCharacter>(PC->GetPawn()))
		{
			PChar->RagdollForDuration(Duration);
		}
	}
}

void UNarrativeCheatManager::GiveSkillPoints(int32 Points/*=1*/)
{
	if (APlayerController* PC = GetOuterAPlayerController())
	{
		if (!PC->HasAuthority())
		{
			const FString ServerCommand = FString::Printf(TEXT("%hs %d"), __func__, Points);
			PC->ServerExec(ServerCommand);
			UE_LOG(LogConsoleResponse, Log, TEXT("Asking server for advance to give '%d' skill points as we are not auth..."), Points);
		}
		
		if (ANarrativePlayerState* PS = Cast<ANarrativePlayerState>(PC->PlayerState))
		{
			if (USkillTreeComponent* STC = PS->GetSkillTreeComponent())
			{
				STC->GiveSkillPoints(Points);
			}
		}
	}
}

void UNarrativeCheatManager::GiveCurrency(int32 Currency/*=1*/)
{
	if (APlayerController* PC = GetOuterAPlayerController())
	{
		if (!PC->HasAuthority())
		{
			const FString ServerCommand = FString::Printf(TEXT("%hs %d"), __func__, Currency);
			PC->ServerExec(ServerCommand);
			UE_LOG(LogConsoleResponse, Log, TEXT("Asking server for advance to give currency '%d' as we are not auth..."), Currency);
		}
		
		if (ANarrativeCharacter* PChar = Cast<ANarrativeCharacter>(PC->GetPawn()))
		{
			if (UNarrativeInventoryComponent* IC = PChar->GetInventoryComponent())
			{
				IC->AddCurrency(Currency);
			}
		}
	}
}

void UNarrativeCheatManager::SetInvulnerable(const bool bIsInvulnerable)
{
	if (APlayerController* PC = GetOuterAPlayerController())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC))
		{
			const FGameplayTag InvulnerabilityTag = FNarrativeGameplayTags::Get().State_Invulnerable;

			if (bIsInvulnerable)
			{
				ASC->AddLooseGameplayTag(InvulnerabilityTag);
			}
			else 
			{
				ASC->RemoveLooseGameplayTag(InvulnerabilityTag);
			}
		}
	}
}

void UNarrativeCheatManager::AdvanceTime(const float Amount)
{
	if (Amount > 0.f)
	{
		if (APlayerController* PC = GetOuterAPlayerController())
		{
			if (!PC->HasAuthority())
			{
				const FString ServerCommand = FString::Printf(TEXT("%hs %f"), __func__, Amount);
				PC->ServerExec(ServerCommand);
				UE_LOG(LogConsoleResponse, Log, TEXT("Asking server for advance by '%f' as we are not auth..."), Amount);
			}
			
			if (UWorld* World = PC->GetWorld())
			{
				if (ANarrativeGameState* NGS = World->GetGameState<ANarrativeGameState>())
				{
					NGS->AdvanceTimeOfDay(Amount);
				}
			}
		}
	}
}

void UNarrativeCheatManager::AdvanceToTime(const float TargetTime)
{
	if (TargetTime >= 0.f && TargetTime <= 2400.f)
	{
		if (APlayerController* PC = GetOuterAPlayerController())
		{
			if (!PC->HasAuthority())
			{
				const FString ServerCommand = FString::Printf(TEXT("%hs %f"), __func__, TargetTime);
				PC->ServerExec(ServerCommand);
				UE_LOG(LogConsoleResponse, Log, TEXT("Asking server for advance to time '%f' as we are not auth..."), TargetTime);
			}
			
			if (UWorld* World = PC->GetWorld())
			{
				if (ANarrativeGameState* NGS = World->GetGameState<ANarrativeGameState>())
				{
					NGS->AdvanceToTimeOfDay(TargetTime);
				}
			}
		}
	}

}

void UNarrativeCheatManager::SetGameplayAttribute(FString AttributeName, float NewValue)
{
	if (APlayerController* PC = GetOuterAPlayerController())
	{
		if (!PC->HasAuthority())
		{
			const FString ServerCommand = FString::Printf(TEXT("%hs %s %f"), __func__, *AttributeName, NewValue);
			PC->ServerExec(ServerCommand);
			UE_LOG(LogConsoleResponse, Log, TEXT("Asking server to set attribute'%s' to %f as we are not auth."), *AttributeName, NewValue);
			return;
		}
		
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC))
		{
			TArray<FGameplayAttribute> OurAttributes;
			ASC->GetAllAttributes(OurAttributes);

			//Iterate our attributes
			for (auto& Attribute : OurAttributes)
			{
				//Once we find, set it, and break
				if (AttributeName == Attribute.AttributeName)
				{
					ASC->ApplyModToAttribute(Attribute, EGameplayModOp::Override, NewValue);
					return;
				}
			}

			UE_LOG(LogConsoleResponse, Log, TEXT("Failed to find attribute'%s'."), *AttributeName);
		}
	}
}

void UNarrativeCheatManager::AddGameplayTag(FString TagName)
{
	if (APlayerController* PC = GetOuterAPlayerController())
	{
		if (!PC->HasAuthority())
		{
			const FString ServerCommand = FString::Printf(TEXT("%hs %s"), __func__, *TagName);
			PC->ServerExec(ServerCommand);
			UE_LOG(LogConsoleResponse, Log, TEXT("Asking server to also grant tag '%s' as we are not auth..."), *TagName);
		}
		
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC))
		{
			FGameplayTag TagToAdd = FGameplayTag::RequestGameplayTag(FName(TagName), false);

			ASC->AddLooseGameplayTag(TagToAdd);
		}
	}
}

void UNarrativeCheatManager::RemoveGameplayTag(FString TagName)
{
	if (APlayerController* PC = GetOuterAPlayerController())
	{
		if (!PC->HasAuthority())
		{
			const FString ServerCommand = FString::Printf(TEXT("%hs %s"), __func__, *TagName);
			PC->ServerExec(ServerCommand);
			UE_LOG(LogConsoleResponse, Log, TEXT("Asking server to also remove tag '%s' as we are not auth..."), *TagName);
		}
		
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC))
		{
			FGameplayTag TagToRemove = FGameplayTag::RequestGameplayTag(FName(TagName), false);

			ASC->RemoveLooseGameplayTag(TagToRemove);
		}
	}
}

void UNarrativeCheatManager::ChangeUsername(FString NewUsername)
{
	if (APlayerController* PC = GetOuterAPlayerController())
	{
		if (!PC->HasAuthority())
		{
			const FString ServerCommand = FString::Printf(TEXT("%hs %s"), __func__, *NewUsername);
			PC->ServerExec(ServerCommand);
			UE_LOG(LogConsoleResponse, Log, TEXT("Asking server to change username to '%s' as we are not auth..."), *NewUsername);
		}

		if (UWorld* World = PC->GetWorld())
		{
			if (ANarrativeGameMode* NGM = Cast<ANarrativeGameMode>(World->GetAuthGameMode()))
			{
				NGM->ChangeName(PC, NewUsername, true);
			}
		}

	}
}

void UNarrativeCheatManager::GiveItem(FString ItemName, int32 Quantity)
{
	if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(GetOuterAPlayerController()))
	{
		if (!PC->HasAuthority())
		{
			const FString ServerCommand = FString::Printf(TEXT("%hs %s %d"), __func__, *ItemName, Quantity);
			PC->ServerExec(ServerCommand);
			UE_LOG(LogConsoleResponse, Log, TEXT("Asking server for grant of item '%s' as we are not auth..."), *ItemName);
			return; 
		}
		
		if (INarrativeCharacterOwner* NCharOwner = Cast<INarrativeCharacterOwner>(PC))
		{
			if (ANarrativeCharacter* NChar = NCharOwner->GetNarrativeCharacter())
			{
				if (UNarrativeInventoryComponent* NInv = NChar->GetInventoryComponent())
				{
					// We couldn't find anything the user was searching for, so early out
					TSubclassOf<UNarrativeItem> ItemClass = UE::NarrativeCheatManager::Private::FuzzyFindClass<UNarrativeItem>(ItemName);
					if (!ItemClass)
					{
						UE_LOG(LogConsoleResponse, Log, TEXT("Could not find a valid Item based on Search String '%s'"), *ItemName);
						return;
					}
					else
					{
						NInv->TryAddItemFromClass(ItemClass, Quantity);
					}
				}
			}
			
		}
	}
}

void UNarrativeCheatManager::SetQuestState(bool bPartyQuests, const int32 QuestIndex, FName NewState)
{

	if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(GetOuterAPlayerController()))
	{
		if (!PC->HasAuthority())
		{
			const FString ServerCommand = FString::Printf(TEXT("%hs %d %d %s"), __func__, bPartyQuests, QuestIndex, *NewState.ToString());
			PC->ServerExec(ServerCommand);
			UE_LOG(LogConsoleResponse, Log, TEXT("Asking server for advance quests as we are not auth..."));
			return; 
		}
		
		if (UTalesComponent* Tales = PC->GetTalesComponent())
		{
			UTalesComponent* DesiredComp = bPartyQuests ? Tales->GetParty() : Tales;

			if (DesiredComp)
			{
				if (DesiredComp->GetAllQuests().IsValidIndex(QuestIndex))
				{
					if (UQuest* QIP = DesiredComp->GetAllQuests()[QuestIndex])
					{
						//If no new state, assume we just want to go the next current state we can find 
						if (NewState.IsNone() && QIP->GetCurrentState())
						{
							for (auto& Branch : QIP->GetCurrentState()->Branches)
							{
								if (Branch && Branch->DestinationState)
								{
									QIP->EnterState(Branch->DestinationState);
									break;
								}

							}

						}
						else
						{
							QIP->EnterState(QIP->GetState(NewState));	
						}
					}
				}
			}
		}
	}
}

void UNarrativeCheatManager::MultiplayerSaveWorld()
{
	if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(GetOuterAPlayerController()))
	{
		if (!PC->HasAuthority())
		{
			const FString ServerCommand = FString::Printf(TEXT("%hs"), __func__);
			PC->ServerExec(ServerCommand);
			UE_LOG(LogConsoleResponse, Log, TEXT("Asking server to create save world and players for us."));
			return; 
		}
		else
		{
			if (UNarrativeSaveSubsystem* SaveSub = PC->GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
			{
				SaveSub->MultiplayerSave();
			}
		}
	}
}

void UNarrativeCheatManager::MultiplayerLoadWorld()
{
	if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(GetOuterAPlayerController()))
	{
		if (!PC->HasAuthority())
		{
			const FString ServerCommand = FString::Printf(TEXT("%hs"), __func__);
			PC->ServerExec(ServerCommand);
			UE_LOG(LogConsoleResponse, Log, TEXT("Asking server to load its world and players from its save data."));
			return; 
		}
		else
		{
			if (UNarrativeSaveSubsystem* SaveSub = PC->GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
			{
				SaveSub->MultiplayerLoad();
			}
		}
	}
}

void UNarrativeCheatManager::MultiplayerSave()
{
	if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(GetOuterAPlayerController()))
	{
		if (!PC->HasAuthority())
		{
			const FString ServerCommand = FString::Printf(TEXT("%hs"), __func__);
			PC->ServerExec(ServerCommand);
			UE_LOG(LogConsoleResponse, Log, TEXT("Asking server to create a networked Load for us."));
			return; 
		}
		else
		{
			if (UNarrativeSaveSubsystem* SaveSub = PC->GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
			{
				SaveSub->CreatePlayerOnlySave(PC);
			}
		}
	}
}

void UNarrativeCheatManager::MultiplayerLoad()
{
	if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(GetOuterAPlayerController()))
	{
		if (!PC->HasAuthority())
		{
			const FString ServerCommand = FString::Printf(TEXT("%hs"), __func__);
			PC->ServerExec(ServerCommand);
			UE_LOG(LogConsoleResponse, Log, TEXT("Asking server to load our save data. This won't change the level, just load your players data."));
			return; 
		}
		else
		{
			if (UNarrativeSaveSubsystem* SaveSub = PC->GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
			{
				SaveSub->LoadPlayerOnlySave(PC);
			}
		}
	}
}

void UNarrativeCheatManager::MultiplayerDeleteSave()
{
	if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(GetOuterAPlayerController()))
	{
		if (!PC->HasAuthority())
		{
			const FString ServerCommand = FString::Printf(TEXT("%hs"), __func__);
			PC->ServerExec(ServerCommand);
			UE_LOG(LogConsoleResponse, Log, TEXT("Asking server to delete our save data."));
			return; 
		}
		else
		{
			if (UNarrativeSaveSubsystem* SaveSub = PC->GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
			{
				SaveSub->DeletePlayerOnlySave(PC);
			}
		}
	}
}
