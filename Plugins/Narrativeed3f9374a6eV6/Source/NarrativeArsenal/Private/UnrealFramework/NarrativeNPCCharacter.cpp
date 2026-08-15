// Copyright Narrative Tools 2024. 

#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "Character/CharacterMapMarker.h"
#include "Interaction/NPCInteractionComponent.h"
#include "Items/InventoryComponent.h"
#include "AI/NPCDefinition.h"
#include "AI/NPCInteractable.h"
#include "AI/NarrativeNPCController.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/Activities/NPCActivityConfiguration.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Navigation/NavigationMarkerComponent.h"
#include <Net/UnrealNetwork.h>
#include "GameplayEffectExtension.h"
#include "NarrativeArsenal.h"
#include <Runtime/AIModule/Classes/Perception/AISense_Damage.h>
#include "UnrealFramework/NarrativeGameState.h"
#include "Engine/World.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "SaveSystemStatics.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Tales/TaggedDialogueSet.h"
#include <Engine/AssetManager.h>

#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassEntityUtils.h"
#include "MassEntityView.h"
#include "NarrativeLogChannels.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "Character/NarrativeCharacterMovement.h"

ANarrativeNPCCharacter::ANarrativeNPCCharacter(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Create ability system component, and set it to be explicitly replicated
	AbilitySystemComponent = CreateDefaultSubobject<UNarrativeAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// Mixed mode means we only are replicated the GEs to ourself, not the GEs to simulated proxies. If another GDPlayerState (Hero) receives a GE,
	// we won't be told about it by the Server. Attributes, GameplayTags, and GameplayCues will still replicate to us.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Create the attribute set, this replicates by default
	// Adding it as a subobject of the owning actor of an AbilitySystemComponent
	// automatically registers the AttributeSet with the AbilitySystemComponent
	AttributeSetBase = CreateDefaultSubobject<UNarrativeAttributeSetBase>(TEXT("AttributeSetBase"));


	TradingInventoryComponent = CreateDefaultSubobject<UNarrativeInventoryComponent>("TradingInventoryComponent");
	TradingInventoryComponent->SetIsVendor(true);

	NPCInteractableComponent = CreateDefaultSubobject<UNPCInteractable>(TEXT("NPCInteractable"));

	bAggressiveOnTakeDamage = true;
}

void ANarrativeNPCCharacter::BeginPlay()
{
	//Initialize our ASC. 
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		InitializeAttributes();
		AddStartupEffects();
		AddDefaultAbilities();
		OnASCInitialized.Broadcast();
	}
 
	Super::BeginPlay();
}

void ANarrativeNPCCharacter::FellOutOfWorld(const class UDamageType& dmgType)
{
	//If a non unique NPC falls out of the world, just remove them, dont bother trying to put them back on the map
	if (UNPCDefinition* NPCD = GetNPCDefinition())
	{
		if (NPCD->bAllowMultipleInstances)
		{
			if (ANarrativeNPCController* NPC = GetNPCController())
			{
				NPC->CleanUp(0.f);
			}
			else
			{
				Destroy();
			}

			return;
		}
	}

	Super::FellOutOfWorld(dmgType);
}

ECapsuleRotationSetting ANarrativeNPCCharacter::GetCapsuleRotationSettings_Implementation() const
{
	if (ANarrativeNPCController* NPCController = GetNPCController())
	{
		//If we're focused on someone we dont want to orient to movement, we need to look at them 
		if (IsValid(NPCController->GetFocusActor()))
		{
			if (UNarrativeCharacterMovement* NCMC = GetNarrativeCharacterMovement())
			{
				return ECapsuleRotationSetting::UseControllerYawSmoothed;
			}
		}
	}

	return Super::GetCapsuleRotationSettings_Implementation();
}

void ANarrativeNPCCharacter::SpawnDefaultController()
{
	Super::SpawnDefaultController();

	if (Controller)
	{
		if (ANarrativeNPCController* NPCC = Cast<ANarrativeNPCController>(Controller))
		{
			CachedController = NPCC;
		}
	}
}

void ANarrativeNPCCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ANarrativeNPCCharacter, NPCDefinition, COND_InitialOnly);
	DOREPLIFETIME(ANarrativeNPCCharacter, NPCLevel);
	DOREPLIFETIME(ANarrativeNPCCharacter, NPCFactions);
}

ETeamAttitude::Type ANarrativeNPCCharacter::GetTeamAttitudeTowards(const AActor& Other) const
{
	if(const INarrativeTeamAgentInterface* OtherTeamAgent = Cast<const INarrativeTeamAgentInterface>(&Other))
	{
		if (NPCFactions.IsValid())
		{
			if (ShouldBeAggressiveTowardsTarget(&Other))
			{
				return ETeamAttitude::Hostile;
			}

			if (ANarrativeGameState* GS = Cast<ANarrativeGameState>(GetWorld()->GetGameState()))
			{
				FGameplayTagContainer TheirFactions = OtherTeamAgent->GetFactions();

				return GS->GetFactionsAttitudeTowardsFactions(NPCFactions, TheirFactions);
			}

		}
	}

	return ETeamAttitude::Neutral;
}

FGameplayTagContainer ANarrativeNPCCharacter::GetFactions() const
{
	return NPCFactions;
}

void ANarrativeNPCCharacter::AddFaction(const FGameplayTag& Faction)
{
	NPCFactions.AddTag(Faction);
	OnRep_NPCFactions();
}

void ANarrativeNPCCharacter::RemoveFaction(const FGameplayTag& Faction)
{
	NPCFactions.RemoveTag(Faction);
}

int32 ANarrativeNPCCharacter::GetCharacterLevel() const
{
	return NPCLevel;
}

class UCharacterDefinition* ANarrativeNPCCharacter::GetCharacterDefinition() const
{
	return NPCDefinition;
}

class UNarrativeInventoryComponent* ANarrativeNPCCharacter::GetInventoryComponent() const
{
	return InventoryComponent;
}

void ANarrativeNPCCharacter::InitNewCharacter_Implementation(UCharacterDefinition* NewDefinition)
{
	if (UNPCDefinition* NPCDef = Cast<UNPCDefinition>(NewDefinition))
	{
		Super::InitNewCharacter_Implementation(NewDefinition);

		//Apply our activity config after our items have been added. Lots of activities will require our items to be valid. 
		const TSoftObjectPtr<class UNPCActivityConfiguration> Config = SpawnInfo.SpawnParams.bOverride_ActivityConfiguration ? SpawnInfo.SpawnParams.ActivityConfiguration : NPCDef->ActivityConfiguration;

		if (Config)
		{
			ApplyActivityConfig(Config.LoadSynchronous());
		}

		if (TradingInventoryComponent)
		{
			TradingInventoryComponent->SetCurrency(NPCDef->TradingCurrency);

			for (auto& DefaultRoll : NPCDef->TradingItemLoadout)
			{
				TArray<FItemAddResult> Results;
				TradingInventoryComponent->TryAddFromLootTable(DefaultRoll, Results);
			}
		}
	}

}

TArray<FLootTableRoll> ANarrativeNPCCharacter::GetDefaultItemLoadout() const
{
	if (SpawnInfo.SpawnParams.bOverride_DefaultItemLoadout)
	{
		return SpawnInfo.SpawnParams.DefaultItemLoadout;
	}

	return Super::GetDefaultItemLoadout();
}

TSoftObjectPtr<UCharacterAppearanceBase> ANarrativeNPCCharacter::GetDefaultAppearance() const
{
	return SpawnInfo.SpawnParams.bOverride_DefaultAppearance ? SpawnInfo.SpawnParams.DefaultAppearance : Super::GetDefaultAppearance();
}

TArray<TSoftObjectPtr<class UTriggerSet>> ANarrativeNPCCharacter::GetDefaultTriggerSets() const
{
	return SpawnInfo.SpawnParams.bOverride_TriggerSets ? SpawnInfo.SpawnParams.TriggerSets : Super::GetDefaultTriggerSets();
}

void ANarrativeNPCCharacter::OnCharacterVisualInitialized()
{
	check(NPCDefinition);

	if (IsValid(NPCDefinition))
	{
		const bool bWasLoadedFromSave = USaveSystemStatics::LoadSingleActor(this);

		//Lets try load, and if we have nothing to load we're a new character and need initialized
		if (!bWasLoadedFromSave)
		{
			InitNewCharacter(NPCDefinition);
		}
	}

	Super::OnCharacterVisualInitialized();
	
}

FString ANarrativeNPCCharacter::GetHumanReadableName() const
{
	if (NPCDefinition)
	{
		return GetNPCName().ToString();
	}

	return Super::GetHumanReadableName();
}

bool ANarrativeNPCCharacter::IsPlayerControlled() const
{
	return false; 
}

bool ANarrativeNPCCharacter::IsBotControlled() const
{
	return true; 
}

void ANarrativeNPCCharacter::OnDefinitionSet_Implementation(UCharacterDefinition* NewDefinition)
{
	//We need to override the definition load process 
	//Super::OnDefinitionSet_Implementation(NewDefinition);

		//When our characters definition is set, try loading all the data we need now that we're spawned, such as our appearance, dialogue, and so on. 
	if (IsValid(NewDefinition))
	{
	
		
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			ASC->AddLooseGameplayTags(SpawnInfo.SpawnParams.bOverride_DefaultOwnedTags ? SpawnInfo.SpawnParams.DefaultOwnedTags : NewDefinition->DefaultOwnedTags);
		}

		if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
		{
			TArray<FSoftObjectPath> DefAssetPaths;

			//Load the paths in the definition
			Manager->GetPrimaryAssetLoadList(DefAssetPaths, NewDefinition->GetPrimaryAssetId(), {"SpawnedData"}, false);

			//Load any required overrides. 
			if (SpawnInfo.SpawnParams.bOverride_ActivityConfiguration)
			{
				DefAssetPaths.Add(SpawnInfo.SpawnParams.ActivityConfiguration.ToSoftObjectPath());
			}

			if (SpawnInfo.SpawnParams.bOverride_DefaultAppearance)
			{
				DefAssetPaths.Add(SpawnInfo.SpawnParams.DefaultAppearance.ToSoftObjectPath());
			}

			if (SpawnInfo.SpawnParams.bOverride_TriggerSets)
			{
				for (auto& TSet : SpawnInfo.SpawnParams.TriggerSets)
				{
					DefAssetPaths.Add(TSet.ToSoftObjectPath());
				}
			}

			FStreamableDelegate LoadDel = FStreamableDelegate::CreateUObject(this, &ANarrativeNPCCharacter::HandleCharacterDefinitionDataLoaded, NewDefinition->GetPrimaryAssetId());
			CharacterDefinitionLoadHandle = Manager->LoadAssetList(DefAssetPaths, LoadDel);// Manager->LoadPrimaryAsset(NewDefinition->GetPrimaryAssetId(), {"SpawnedData"}, LoadDel);
		}
	}

	if (IsValid(NPCDefinition))
	{
		//tell the character subsystem to add the char now that we know its chardef is set 
		if (UNarrativeCharacterSubsystem* NPCSubsystem = GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>())
		{
			NPCSubsystem->RegisterCharacter(this);
		}
		
		NPCFactions = SpawnInfo.SpawnParams.bOverride_DefaultFactions ? SpawnInfo.SpawnParams.DefaultFactions : NPCDefinition->DefaultFactions;

		const FText NPCName = GetNPCName();

		if (HasAuthority())
		{
			const int32 Min = SpawnInfo.SpawnParams.bOverride_LevelRange ? SpawnInfo.SpawnParams.MinLevel : NPCDefinition->MinLevel;
			const int32 Max = SpawnInfo.SpawnParams.bOverride_LevelRange ? SpawnInfo.SpawnParams.MaxLevel : NPCDefinition->MaxLevel;
			NPCLevel = FMath::RandRange(FMath::Min<int32>(Min, Max), FMath::Max<int32>(Min, Max));
		}

		if (InventoryComponent)
		{
			InventoryComponent->SetInventoryFriendlyName(NPCName);
		}

		if (NPCDefinition->bIsVendor && TradingInventoryComponent)
		{
			if (!NPCDefinition->ShopFriendlyName.IsEmptyOrWhitespace())
			{
				TradingInventoryComponent->SetInventoryFriendlyName(!NPCDefinition->ShopFriendlyName.IsEmptyOrWhitespace() ? NPCDefinition->ShopFriendlyName : NPCName);
			}

			TradingInventoryComponent->SetBuyPercentage(NPCDefinition->BuyItemPercentage);
			TradingInventoryComponent->SetSellPercentage(NPCDefinition->SellItemPercentage);
			
		}

		if (NPCInteractableComponent)
		{
			NPCInteractableComponent->SetInteractableNameText(NPCName);
		}
	}


}

void ANarrativeNPCCharacter::OnCharacterDefinitionDataLoaded(FPrimaryAssetId LoadedId)
{
	//Make sure our controller is spawned before we apply any loaded data as some of it gets applied to the controller 
	SpawnDefaultController();

	//Apply the character definition - our appearance, etc. 
	Super::OnCharacterDefinitionDataLoaded(LoadedId);

	//apply npc specific stuff
	if (UNPCDefinition* NPCDef = GetNPCDefinition())
	{
		if (NPCDef->ActivitySchedules.Num())
		{
			ApplyActivitySchedules(NPCDef->ActivitySchedules);
		}

		if (NPCDef->Dialogue.IsValid())
		{
			ApplyDialogue(NPCDef->Dialogue.LoadSynchronous());
		}

		if (NPCDef->TaggedDialogueSet.IsValid())
		{
			if (UTaggedDialogueSet* DSet = NPCDefinition->TaggedDialogueSet.LoadSynchronous())
			{
				TaggedDialogues = DSet->TaggedDialogues;
			}
		}
	}
}

void ANarrativeNPCCharacter::HandleSpawnParamOverridesLoaded()
{

}

void ANarrativeNPCCharacter::ApplyActivityConfig_Implementation(class UNPCActivityConfiguration* NPCActivityConfig)
{
	if (IsValid(NPCActivityConfig))
	{
		if (UNPCActivityComponent* NPCA = GetActivityComponent())
		{
			NPCA->SetActivityConfiguration(NPCActivityConfig);
		}
	}
}

void ANarrativeNPCCharacter::ApplyActivitySchedules_Implementation(const TArray<TSoftObjectPtr<class UNPCActivitySchedule>>& ActivitySchedules)
{
	for (auto& Schedule : ActivitySchedules)
	{
		if (Schedule.IsValid())
		{
			if (UNPCActivityComponent* NPCA = GetActivityComponent())
			{
				NPCA->AddActivitySchedule(Schedule.LoadSynchronous());
			}
		}
	}
}

void ANarrativeNPCCharacter::ApplyDialogue_Implementation(TSubclassOf<class UDialogue> NPCDialogue)
{
	if (IsValid(NPCDialogue))
	{
		if (NPCInteractableComponent)
		{
			NPCInteractableComponent->Dialogue = NPCDialogue;
		}
	}
}

void ANarrativeNPCCharacter::HandleDeath_Implementation(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead)
{
	Super::HandleDeath_Implementation(KilledActor, KilledActorASC, bIsDead);

	if (bIsDead)
	{
		if (NPCInteractableComponent)
		{
			NPCInteractableComponent->SetInteractableActionText(NSLOCTEXT("NPCCharacter", "LootInteractText", "Loot"));
		}

		//Dead NPCs should never save, remove this NPCs record from the save system. TODO think about whether record could be added back later accidentally? 
		if (UNarrativeSaveSubsystem* SaveSub = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
		{
			SaveSub->RemoveSingleActor(this);
		}
	}
}

void ANarrativeNPCCharacter::Load_Implementation()
{
	if (!Controller)
	{
		SpawnDefaultController();
	}

	if(AICRecord.IsValid())
	{
		if (UNarrativeSaveSubsystem* SaveSub = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
		{
			if (ANarrativeNPCController* NPCController = GetNPCController())
			{
				SaveSub->LoadActorFromRecord(NPCController, AICRecord);
			}
		}
	}

}

void ANarrativeNPCCharacter::PrepareForSave_Implementation()
{
	//Store our AIC in a record - make sure to use GetNPCController as we'll be unpossessed at this point so Controller is null. 
	if (UNarrativeSaveSubsystem* SaveSub = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
	{
		if (ANarrativeNPCController* NPCController = GetNPCController())
		{
			SaveSub->CreateActorRecord(NPCController, AICRecord);
		}
	}
}

void ANarrativeNPCCharacter::OnRep_NPCDefinition()
{
	if (NPCDefinition)
	{
		OnDefinitionSet(NPCDefinition);
	}
}

bool ANarrativeNPCCharacter::ShouldBeAggressiveTowardsTarget_Implementation(const class AActor* Target) const
{
	return Hostiles.Contains(Target);
}

FText ANarrativeNPCCharacter::GetNPCName() const
{
	if (SpawnInfo.SpawnParams.bOverride_NPCName)
	{
		return SpawnInfo.SpawnParams.NPCName;
	}

	if (NPCDefinition)
	{
		return NPCDefinition->NPCName;
	}

	return FText::GetEmpty();
}

FText ANarrativeNPCCharacter::GetCharacterName() const
{
	return GetNPCName();
}

void ANarrativeNPCCharacter::SetNPCDefinition(class UNPCDefinition* NewNPCData)
{
	if (IsValid(NewNPCData))
	{
		NPCDefinition = NewNPCData;
		OnRep_NPCDefinition();
	}
}

void ANarrativeNPCCharacter::PlayTaggedDialogue(FGameplayTag Tag, AActor* DialogueInstigator)
{
	if (IsValid(NPCDefinition) && Tag.IsValid())
	{
		for (auto& TaggedDialogue : TaggedDialogues)
		{
			if (TaggedDialogue.Tag == Tag)
			{
				if (IsValid(AbilitySystemComponent))
				{
					const bool bHasRequiredTags = AbilitySystemComponent->HasAllMatchingGameplayTags(TaggedDialogue.RequiredTags);
					const bool bHasAnyBlockedTags = AbilitySystemComponent->HasAnyMatchingGameplayTags(TaggedDialogue.BlockedTags);
					const bool bHasCooldown = FMath::IsNearlyZero(TaggedDialogue.LastPlayTime) || GetWorld()->TimeSince(TaggedDialogue.LastPlayTime) > TaggedDialogue.Cooldown;

					if (bHasRequiredTags && !bHasAnyBlockedTags && bHasCooldown)
					{
						ExecutePlayTaggedDialogue(TaggedDialogue, DialogueInstigator);
						TaggedDialogue.LastPlayTime = GetWorld()->GetTimeSeconds();
					}
				}
			}
		}
	}
}

class UNPCActivityComponent* ANarrativeNPCCharacter::GetActivityComponent() const
{
	if (ANarrativeNPCController* OurController = GetNPCController())
	{
		return OurController->GetActivityComponent();
	}

	return nullptr; 
}

ANarrativeNPCController* ANarrativeNPCCharacter::GetNPCController() const
{
	if (CachedController)
	{
		return CachedController;
	}

	if (ANarrativeNPCController* NPCC = Cast<ANarrativeNPCController>(GetController()))
	{
		return NPCC;
	}

	if (ANarrativeNPCController* NPCC = Cast<ANarrativeNPCController>(PreviousController))
	{
		return NPCC;
	}

	return nullptr; 
}

AController* ANarrativeNPCCharacter::GetOwningController() const
{
	return GetNPCController();
}

void ANarrativeNPCCharacter::OnRep_NPCFactions()
{
	//If our factions change our marker needs to update. 
	if (MapMarker)
	{
		MapMarker->RefreshMarker();
	}
}
