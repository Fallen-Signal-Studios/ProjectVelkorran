// Copyright Narrative Tools 2024. 


#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "UnrealFramework/NarrativeAnimInstance.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include <AbilitySystemBlueprintLibrary.h>
#include "Settings/NarrativeCombatDeveloperSettings.h"
#include "AI/NarrativeNPCController.h"
#include "AI/NPCDefinition.h"
#include "ArsenalStatics.h"
#include "NarrativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include <Engine/World.h>
#include "ArsenalSettings.h"
#include "NarrativeLogChannels.h"
#include "GAS/AbilityConfiguration.h"

UNarrativeAbilitySystemComponent::UNarrativeAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
{
	NumAttackTokens = 10;
	AttackPriority = 1.f;
	bIsDead = false; 
}

int32 UNarrativeAbilitySystemComponent::HandleGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
	/**We override this function because the base class applies an Abilitylist scope lock whilst firing the event delegates. 
	Wielding/unwielding weapons relied on those delegates to remove/add abilities but couldn't because of the scope lock.
	
	So we fix their functionality - by only applying the scope lock for the duration of the TriggerAbilityFromGameplayEvent calls, 
	which is what it should probably be anyways. */
	int32 TriggeredCount = 0;
	FGameplayTag CurrentTag = EventTag;

	{
		ABILITYLIST_SCOPE_LOCK();
		while (CurrentTag.IsValid())
		{
			if (GameplayEventTriggeredAbilities.Contains(CurrentTag))
			{
				TArray<FGameplayAbilitySpecHandle> TriggeredAbilityHandles = GameplayEventTriggeredAbilities[CurrentTag];

				for (const FGameplayAbilitySpecHandle& AbilityHandle : TriggeredAbilityHandles)
				{
					if (TriggerAbilityFromGameplayEvent(AbilityHandle, AbilityActorInfo.Get(), EventTag, Payload, *this))
					{
						TriggeredCount++; 
					}
				}
			}

			CurrentTag = CurrentTag.RequestDirectParent();
		}
	}

	if (FGameplayEventMulticastDelegate* Delegate = GenericGameplayEventCallbacks.Find(EventTag))
	{
		// Make a copy before broadcasting to prevent memory stomping
		FGameplayEventMulticastDelegate DelegateCopy = *Delegate;
		DelegateCopy.Broadcast(Payload);
	}

	// Make a copy in case it changes due to callbacks
	TArray<TPair<FGameplayTagContainer, FGameplayEventTagMulticastDelegate>> LocalGameplayEventTagContainerDelegates = GameplayEventTagContainerDelegates;
	for (TPair<FGameplayTagContainer, FGameplayEventTagMulticastDelegate>& SearchPair : LocalGameplayEventTagContainerDelegates)
	{
		if (SearchPair.Key.IsEmpty() || EventTag.MatchesAny(SearchPair.Key))
		{
			SearchPair.Value.Broadcast(EventTag, Payload);
		}
	}

	return TriggeredCount;
}

void UNarrativeAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) 
{

	/*In Narrative Pro, we don't really want to reassign actor info when a pawn like a vehicle or horse gets possessed. 
	This is because certain abilities like wield weapon want to work whilst occupying vehicles, and might want to play a montage, 
	but the anim instance will point at our vehicles anim instance when we need players and this will break.
	
	We can of course still do whatever we need with the horse/vehicle by accessing it via Controller->GetControlledPawn()
	so i'm inclined to keep it this way as updating ActorInfo every time we possess a new thing is also quite confusing to script around in GA's. 
	
	So only allow NarrativeCharacters to update the Avatar pointer, not vehicles, turrets, etc. I think this makes sense - if this is uneeded for your game
	you can always override this function to remove this logic. 
	*/
	if (!GetAvatarActor())
	{
		Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
	}
	else if (ANarrativeCharacter* NewAvatarChar = Cast<ANarrativeCharacter>(InAvatarActor))
	{
		//Make sure we're a new Avatar actor. 
		if (NewAvatarChar != GetAvatarActor())
		{
			Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
		}
	}

	FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();

	if (UNarrativeAnimInstance* AnimInstance = Cast<UNarrativeAnimInstance>(ActorInfo->GetAnimInstance()))
	{
		AnimInstance->BindASC();
	}

	//TODO binding this is annoying because attribute set is const and so HandleOutOfHealth() needs to be const too - will come back to this, have higher priorites atm 
	if (const UNarrativeAttributeSetBase* AttributeSet = Cast<UNarrativeAttributeSetBase>(GetAttributeSet(UNarrativeAttributeSetBase::StaticClass())))
	{
		/*GAS wants this to be const which is fair, however we need to assign a delegate which requires non - const.
		We know assigning a delegate is safe because its in our custom attribute set and we're not touching any underlying GAS stuff so we can remove the const. */
		if (UNarrativeAttributeSetBase* MutableAttributeSet = const_cast<UNarrativeAttributeSetBase*>(AttributeSet))
		{
			MutableAttributeSet->OnOutOfHealth.AddUObject(this, &UNarrativeAbilitySystemComponent::HandleOutOfHealth);
		}
	}
}

void UNarrativeAbilitySystemComponent::HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec& DamageEffectSpec, float DamageMagnitude) 
{
	//Health will be zero on beginplay and this will be called, we can use bStartupEffectsApplied to ensure we ignore that call 
	if (GetOwnerRole() >= ROLE_Authority && bStartupEffectsApplied) 
	{
		if (!IsDead())
		{
			FGameplayEventData Payload;
			Payload.EventTag = FNarrativeGameplayTags::Get().GameplayEvent_Death;
			Payload.Instigator = DamageInstigator;
			Payload.Target = GetAvatarActor();
			Payload.OptionalObject = DamageEffectSpec.Def;
			Payload.ContextHandle = DamageEffectSpec.GetEffectContext();
			Payload.InstigatorTags = *DamageEffectSpec.CapturedSourceTags.GetAggregatedTags();
			Payload.TargetTags = *DamageEffectSpec.CapturedTargetTags.GetAggregatedTags();
			Payload.EventMagnitude = DamageMagnitude;

			FScopedPredictionWindow NewScopedWindow(this, true);
			HandleGameplayEvent(Payload.EventTag, &Payload);

			/**Basically GAS is an absolute pain to work with and firing our Death ability on all clients does not work with
			- Owner Tag Added ability trigger
			- GameplayEvent based trigger 
			
			So we're manually writing this so that death reliably fires on all clients, and works for anything that owns an ASC. */
			const bool bOldIsDead = bIsDead;
			bIsDead = true;
			OnRep_bIsDead(bOldIsDead);
		}
	}
}	

void UNarrativeAbilitySystemComponent::Debug_Internal(struct FAbilitySystemComponentDebugInfo& Info)
{
	Super::Debug_Internal(Info);

	FString TokenStrings;
	int32 TokenCount = 1;
	int32 NumTokens = GrantedAttackTokens.Num();
	int32 AvailableTokens = GetNumAttackTokens();
	if (NumTokens > 0)
	{
		for (auto& Token : GrantedAttackTokens)
		{
			if (Token.Owner && Token.Owner->GetPawn())
			{

				TokenStrings += Token.Owner->GetNPCName().ToString();

				if (TokenCount++ < NumTokens)
				{
					TokenStrings += TEXT(", ");
				}
			} 
		}

		DebugLine(Info, FString::Printf(TEXT("Attack Tokens: %s, (%d/%d)"), *TokenStrings, GrantedAttackTokens.Num(), AvailableTokens), 4.f, 0.f, 4);
	}
	else
	{
		DebugLine(Info, FString::Printf(TEXT("Attack Tokens: None, (0/%d)"), AvailableTokens), 4.f, 0.f, 4);
	}

}

void UNarrativeAbilitySystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	DOREPLIFETIME(UNarrativeAbilitySystemComponent, bIsDead);

	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

float UNarrativeAbilitySystemComponent::GetBotAttackFrequency(FGameplayTag InputTag)
{
	TArray<FGameplayAbilitySpecHandle> Specs;
	FindAbilitiesWithTag(InputTag, Specs);

	for (auto& Spec : Specs)
	{
		if (Spec.IsValid())
		{
			if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(Spec))
			{
				if (UNarrativeCombatAbility* CombatAbility = Cast<UNarrativeCombatAbility>(AbilitySpec->GetPrimaryInstance()))
				{
					if (CombatAbility->CanActivateAbility(Spec, AbilityActorInfo.Get(), nullptr, nullptr))
					{
						return CombatAbility->GetBotAttackFrequency();
					}
				}
			}
		}
	}

	return 1.f; 
}

float UNarrativeAbilitySystemComponent::GetBotAttackRange(FGameplayTag InputTag)
{
	TArray<FGameplayAbilitySpecHandle> Specs;
	FindAbilitiesWithTag(InputTag, Specs);

	for (auto& Spec : Specs)
	{
		if (Spec.IsValid())
		{
			if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(Spec))
			{
				if (UNarrativeCombatAbility* CombatAbility = Cast<UNarrativeCombatAbility>(AbilitySpec->GetPrimaryInstance()))
				{
					//Our unarmed punch etc will still be granted so find the ability we're actually going to activate. 
					if (CombatAbility->CanActivateAbility(Spec, AbilityActorInfo.Get(), nullptr, nullptr))
					{
						return CombatAbility->GetBotAttackRange();
					}
				}
			}
		}
	}

	return 1.f;
}

void UNarrativeAbilitySystemComponent::HealedBy(UNarrativeAbilitySystemComponent* Healer, const float Damage, const FGameplayEffectSpec& Spec)
{
	if (Healer)
	{
		OnHealedBy.Broadcast(Healer, Damage, Spec);
	}
}

void UNarrativeAbilitySystemComponent::DamagedBy(UNarrativeAbilitySystemComponent* DamageCauser, const float Damage, const FGameplayEffectSpec& Spec)
{
	if (DamageCauser)
	{
		OnDamagedBy.Broadcast(DamageCauser, Damage, Spec);
	}
}

void UNarrativeAbilitySystemComponent::DealtDamage(UNarrativeAbilitySystemComponent* DamagedTarget, const float Damage, const FGameplayEffectSpec& Spec)
{
	if (DamagedTarget)
	{
		OnDealtDamage.Broadcast(DamagedTarget, Damage, Spec);
	}
}

void UNarrativeAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	// Logic is similar to AbilityLocalInputPressed
	const auto& NarrativeTags =  FNarrativeGameplayTags::Get();
	
	// Consume the input if this InputID is overloaded with GenericConfirm/Cancel and the GenericConfim/Cancel callback is bound
	bool bConfirmBound = InputTag == NarrativeTags.Narrative_Input_Confirm && GenericLocalConfirmCallbacks.IsBound();
	if (bConfirmBound)
	{
		LocalInputConfirm();
		return;
	}

	bool bCancelBound = InputTag == NarrativeTags.Narrative_Input_Cancel && GenericLocalCancelCallbacks.IsBound();
	if (bCancelBound)
	{
		LocalInputCancel();
		return;
	}

	// ---------------------------------------------------------

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			if (Spec.Ability)
			{
				Spec.InputPressed = true;
				if (Spec.IsActive())
				{
					if (Spec.Ability->bReplicateInputDirectly && IsOwnerActorAuthoritative() == false)
					{
						ServerSetInputPressed(Spec.Handle);
					}

					AbilitySpecInputPressed(Spec);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
					// Fixing this up to use the instance activation, but this function should be deprecated as it cannot work with InstancedPerExecution
					UE_CLOG(Spec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution, LogAbilitySystemComponent, Warning, TEXT("%hs: %s is InstancedPerExecution. This is unreliable for Input as you may only interact with the latest spawned Instance"), __func__, *GetNameSafe(Spec.Ability));
					TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
					const FGameplayAbilityActivationInfo& ActivationInfo = Instances.IsEmpty() ? Spec.ActivationInfo : Instances.Last()->GetCurrentActivationInfoRef();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
					// Invoke the InputPressed event. This is not replicated here. If someone is listening, they may replicate the InputPressed event to the server.
					InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, ActivationInfo.GetActivationPredictionKey());					
				}
				else
				{
					// Ability is not active, so try to activate it
					TryActivateAbility(Spec.Handle);
				}
			}
		}
	}
}

void UNarrativeAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	// Logic is similar to AbilityLocalInputReleased
	
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			if (Spec.Ability)
			{
				Spec.InputPressed = false;
				if (Spec.IsActive())
				{
					if (Spec.Ability->bReplicateInputDirectly && IsOwnerActorAuthoritative() == false)
					{
						ServerSetInputReleased(Spec.Handle);
					}

					AbilitySpecInputReleased(Spec);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
					// Fixing this up to use the instance activation, but this function should be deprecated as it cannot work with InstancedPerExecution
					UE_CLOG(Spec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution, LogAbilitySystemComponent, Warning, TEXT("%hs: %s is InstancedPerExecution. This is unreliable for Input as you may only interact with the latest spawned Instance"), __func__, *GetNameSafe(Spec.Ability));
					TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
					const FGameplayAbilityActivationInfo& ActivationInfo = Instances.IsEmpty() ? Spec.ActivationInfo : Instances.Last()->GetCurrentActivationInfoRef();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
					// Invoke the InputPressed event. This is not replicated here. If someone is listening, they may replicate the InputPressed event to the server.
					InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, ActivationInfo.GetActivationPredictionKey());					
				}
			}
		}
	}
}

void UNarrativeAbilitySystemComponent::ClearAbilitiesWithTag(const FGameplayTag& InputTag)
{
	TArray<FGameplayAbilitySpecHandle> OutSpecs;
	FindAbilitiesWithTag(InputTag, OutSpecs);

	// iterate through the bound abilities
	for (const FGameplayAbilitySpecHandle& CurrentSpec : OutSpecs)
	{
		// clear the ability
		ClearAbility(CurrentSpec);
	}
}

void UNarrativeAbilitySystemComponent::FindAbilitiesWithTag(const FGameplayTag& InputTag, TArray<FGameplayAbilitySpecHandle>& OutAbilitySpecs)
{
	// find all matching abilities
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		// add maching abilities to the list
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			OutAbilitySpecs.Emplace(Spec.Handle);
		}
	}
}

class AActor* UNarrativeAbilitySystemComponent::GetAvatarOwner() const
{
	return GetAvatarActor();
}	

class ANarrativeCharacter* UNarrativeAbilitySystemComponent::GetCharacterOwner() const
{
	return Cast<ANarrativeCharacter>(GetAvatarActor());
}

bool UNarrativeAbilitySystemComponent::TryClaimToken(class ANarrativeNPCController* Claimer)
{
	if (Claimer && Claimer->GetPawn() && GetAvatarActor())
	{
		const bool bHasTokensAvailable = GrantedAttackTokens.Num() < GetNumAttackTokens();
		const APawn* ClaimerPawn = Claimer->GetPawn();
		const AActor* OurActor = GetAvatarActor();

		//If no tokens are available, we may be able to steal one 
		if (!bHasTokensAvailable)
		{
			int32 StealTokenFromIdx = INDEX_NONE;
			float BestScore = 0.f;

			const float DistFromClaimer = FVector::DistSquared(ClaimerPawn->GetActorLocation(), OurActor->GetActorLocation());

			int32 Idx = 0;
			//Loop over everyone and see if we can steal from them. 
			for (auto& ExistingToken : GrantedAttackTokens)
			{
				//In some cases we should always steal a token, for example if the token holder has died. In these cases just early out 
				if (ShouldImmediatelyStealToken(ExistingToken))
				{
					StealTokenFromIdx = Idx;
					break;
				}

				//See if we can steal and grab the steal score. 
				float StealScore = 0.f;
				if (CanStealToken(Claimer, ExistingToken, StealScore))
				{
					if (StealScore > BestScore)
					{
						//UE_LOG(LogTemp, Warning, TEXT("New best steal score is %f. We'll steal from %s"), StealScore, *ExistingToken.Owner->GetNPCName().ToString());
						StealTokenFromIdx = Idx;
						BestScore = StealScore;
					}
				}

				++Idx;
			}

			if (StealTokenFromIdx != INDEX_NONE && GrantedAttackTokens.IsValidIndex(StealTokenFromIdx))
			{
				//UE_LOG(LogTemp, Warning, TEXT("We stole a token from %s!"), *GrantedAttackTokens[StealTokenFromIdx].Owner->GetNPCName().ToString());

				GrantedAttackTokens[StealTokenFromIdx].Owner->TokenStolen();
				ReturnTokenAtIndex(StealTokenFromIdx);
			}
		}

		//Sanity check this again then add the token 
		if (GrantedAttackTokens.Num() < GetNumAttackTokens())
		{
			FAttackToken NewToken = FAttackToken(Claimer, GetWorld()->GetTimeSeconds());
			GrantedAttackTokens.Add(NewToken);

			Claimer->GrantedToken = this;
			return true; 
		}
	}

	return false; 
}

void UNarrativeAbilitySystemComponent::ReturnTokenAtIndex(int32 Index)
{
	if (GrantedAttackTokens.IsValidIndex(Index))
	{
		if (GrantedAttackTokens[Index].Owner)
		{
			GrantedAttackTokens[Index].Owner->GrantedToken = nullptr;
		}
	}

	GrantedAttackTokens.RemoveAt(Index);
}

void UNarrativeAbilitySystemComponent::ReturnToken(class ANarrativeNPCController* Returner)
{
	int32 FoundIdx = INDEX_NONE;

	for (int32 i = 0; i < GrantedAttackTokens.Num(); ++i)
	{
		if (GrantedAttackTokens.IsValidIndex(i) && GrantedAttackTokens[i].Owner == Returner)
		{
			FoundIdx = i;
			break;
		}
	}
	
	if (FoundIdx != INDEX_NONE)
	{
		ReturnTokenAtIndex(FoundIdx);
	}

}

bool UNarrativeAbilitySystemComponent::ShouldImmediatelyStealToken(const FAttackToken& Token) const
{
	return !Token.Owner || !Token.Owner->IsAlive() || !Token.Owner->GetPawn();
}

bool UNarrativeAbilitySystemComponent::CanStealToken(class ANarrativeNPCController* Stealer, const FAttackToken& ExistingToken, float& StealScore) const
{
	if (const UNarrativeCombatDeveloperSettings* CombatSettings = GetDefault<UNarrativeCombatDeveloperSettings>())
	{
		if (Stealer && ExistingToken.Owner)
		{
			const APawn* StealerPawn = Stealer->GetPawn();
			const APawn* ExistingPawn = ExistingToken.Owner->GetPawn();
			const AActor* OurActor = GetAvatarActor();

			if (StealerPawn && ExistingPawn)
			{
				const float DistFromStealer = FVector::DistSquared(StealerPawn->GetActorLocation(), OurActor->GetActorLocation());
				const float DistFromExisting = FVector::DistSquared(ExistingPawn->GetActorLocation(), OurActor->GetActorLocation());
				const float TimeSinceGrant = GetWorld()->TimeSince(ExistingToken.TokenGrantedTime);
				bool bCanSteal = false;

				//The existing token was granted ages ago, lets allow for a steal!
				if (TimeSinceGrant > CombatSettings->TokenStealableAgeSeconds)
				{
					//Every second past the stealable age is a point of score. 
					StealScore += TimeSinceGrant - CombatSettings->TokenStealableAgeSeconds;
					bCanSteal = true; 
				}

				//If the stealer is x% closer to us than the existing token, then lets steal it! 
				if (DistFromStealer < DistFromExisting && DistFromStealer / DistFromExisting < CombatSettings->StealTokenProximity)
				{
					//Higher score means we should steal. So use DistFromExisting as the score. Further away will be scored higher. 
					StealScore += DistFromExisting / (50000.f * 50000.f);
					bCanSteal = true;
				}

				return bCanSteal;
			}

		}
	}

	return false; 
}

int32 UNarrativeAbilitySystemComponent::GetNumAttackTokens() const
{
	if (const UNarrativeCombatDeveloperSettings* CombatSettings = GetDefault<UNarrativeCombatDeveloperSettings>())
	{
		return CombatSettings->GetAttackTokensForDifficulty(UArsenalStatics::GetGameplayDifficultyLevel());
	}
	else
	{
		return INT_MAX;
	}
}

int32 UNarrativeAbilitySystemComponent::GetAvailableAttackTokens() const
{
	return GetNumAttackTokens() - GrantedAttackTokens.Num();
}

int32 UNarrativeAbilitySystemComponent::GetNumGrantedAttackTokens() const
{
	return GrantedAttackTokens.Num();
}

float UNarrativeAbilitySystemComponent::GetAttackPriority() const
{	
	return AttackPriority;
}

void UNarrativeAbilitySystemComponent::DealDamage(const float Damage)
{
	if (!IsDead() && Damage > 0.f)
	{
		if (const UArsenalSettings* Settings = GetDefault<UArsenalSettings>())
		{
			if (const TSubclassOf<UGameplayEffect> DamageGE = Settings->DamageGameplayEffect_SetByCaller)
			{
				FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(DamageGE, 1.0f, MakeEffectContext());
				if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
				{
					Spec->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, Damage);
					ApplyGameplayEffectSpecToSelf(*Spec);
				}
			}
		}
	}
}

void UNarrativeAbilitySystemComponent::Instakill()
{
	if (!IsDead())
	{
		if (const UNarrativeAttributeSetBase* AttributeSet = Cast<UNarrativeAttributeSetBase>(GetAttributeSet(UNarrativeAttributeSetBase::StaticClass())))
		{
			DealDamage(AttributeSet->GetMaxHealth());
		}
	}
}

FActiveGameplayEffectHandle UNarrativeAbilitySystemComponent::AddDynamicTagsGameplayEffect(const FGameplayTagContainer& TagsToAdd)
{
	if (TagsToAdd.IsValid())
	{
		if (const UArsenalSettings* Settings = GetDefault<UArsenalSettings>())
		{
			if (const TSubclassOf<UGameplayEffect> DynamicTagGE = Settings->DynamicTagGameplayEffect)
			{
				const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(DynamicTagGE, 1.0f, MakeEffectContext());
				FGameplayEffectSpec* Spec = SpecHandle.Data.Get();

				if (!Spec)
				{
					UE_LOG(LogNarrativeAbilities, Warning, TEXT("AddDynamicTagsGameplayEffect: Unable to make outgoing spec for [%s]."), *GetNameSafe(DynamicTagGE));
					return FActiveGameplayEffectHandle();
				}

				Spec->DynamicGrantedTags = TagsToAdd;

				return ApplyGameplayEffectSpecToSelf(*Spec);
			}
		}
	}

	return FActiveGameplayEffectHandle();
}

TArray<FActiveGameplayEffectHandle> UNarrativeAbilitySystemComponent::ApplyGameplayEffectSpecToTargetData(const FGameplayEffectSpecHandle SpecHandle, const FGameplayAbilityTargetDataHandle& TargetData) 
{	
	TArray<FActiveGameplayEffectHandle> EffectHandles;

	if (SpecHandle.IsValid() && GetOwnerRole() >= ROLE_Authority)
	{
		for (TSharedPtr<FGameplayAbilityTargetData> Data : TargetData.Data)
		{
			if (Data.IsValid())
			{
				EffectHandles.Append(Data->ApplyGameplayEffectSpec(*SpecHandle.Data.Get()));
			}
			else
			{
				UE_LOG(LogNarrativeAbilities, Warning, TEXT("UNarrativeAbilitySystemComponent::ApplyGameplayEffectSpecToTarget invalid target data passed in. Ability: %s"), *GetPathName());
			}
		}
	}
	return EffectHandles;
}

void UNarrativeAbilitySystemComponent::Revive()
{
	if (GetOwnerRole() >= ROLE_Authority && bIsDead)
	{
		bIsDead = false;
		OnRep_bIsDead(true);
	}
}

void UNarrativeAbilitySystemComponent::OnRep_bIsDead(const bool bOldIsDead)
{
	if (bIsDead != bOldIsDead)
	{
		OnDeathStateChanged.Broadcast(GetAvatarActor(), this, bIsDead);
	}

	//Add a dead tag for abilities, effects etc. 
	if (bIsDead && !HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead))
	{
		AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_IsDead);
	}
	else if(HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead))
	{
		RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_IsDead);
	}

}

void UNarrativeAbilitySystemComponent::Load_Implementation()
{
	TArray<FGameplayAttribute> OurAttributes;
	GetAllAttributes(OurAttributes);

	//Iterate our attributes
	for (auto& Attribute : OurAttributes)
	{
		if (AttributesToSave.Contains(Attribute))
		{
			//Search the attribute in the saved ones
			for (auto& SA : SavedAttributes)
			{
				//Once we find, restore it, and break
				if (SA.AttributeName == Attribute.AttributeName)
				{
					ApplyModToAttribute(Attribute, EGameplayModOp::Override, SA.Value);
					break;
				}
			}
		}
	}

}

void UNarrativeAbilitySystemComponent::PrepareForSave_Implementation()
{

	TArray<FGameplayAttribute> OurAttributes;
	GetAllAttributes(OurAttributes);

	SavedAttributes.Empty();


	//Iterate our attributes, adding save data if they are in attributes to save. 
	for (auto& Attribute : OurAttributes)
	{
		if (Attribute.IsValid() && AttributesToSave.Contains(Attribute))
		{
			bool bFound;
			float AttributeValue = UAbilitySystemBlueprintLibrary::GetFloatAttributeFromAbilitySystemComponent(this, Attribute, bFound);

			if (bFound)
			{
				UE_LOG(LogTemp, Verbose, TEXT("Saving attribute %s with value %f"), *Attribute.AttributeName, AttributeValue);
				FSavedAttribute SavedAttribute;
				SavedAttribute.AttributeName = Attribute.AttributeName;
				SavedAttribute.Value = AttributeValue;

				SavedAttributes.Add(SavedAttribute);
			}
		}
	}

}
