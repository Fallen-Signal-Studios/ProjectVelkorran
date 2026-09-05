// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Resonance/SovResonanceAbility.h"
#include "Resonance/SovResonanceComponent.h"
#include "AbilitySystemComponent.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

USovResonanceAbility::USovResonanceAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationOwnedTags.AddTag(FSovGameplayTags::Get().State_Resonance_Committed);
	ActivationOwnedTags.AddTag(FNarrativeGameplayTags::Get().State_Busy);
}
void USovResonanceAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* Event)
{
	Ticket = Cast<USovResonanceTicket>(GetSourceObject(Handle, Info));
	if (!Ticket || !Ticket->Coordinator.IsValid() || !Ticket->Coordinator->IsTicketCurrent(Ticket)
		|| !Info || Info->AbilitySystemComponent.Get() != Ticket->ExpectedASC.Get()
		|| !CommitAbility(Handle, Info, ActivationInfo))
	{ EndAbility(Handle, Info, ActivationInfo, true, true); }
}
void USovResonanceAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEnd, bool bCanceled)
{
	USovResonanceTicket* Previous = Ticket; Ticket = nullptr;
	Super::EndAbility(Handle, Info, ActivationInfo, bReplicateEnd, bCanceled);
	if (IsValid(Previous) && Previous->Coordinator.IsValid()) { Previous->Coordinator->NotifyParticipationEnded(Previous); }
}

USovGameplayEffect_ResonanceCorridor::USovGameplayEffect_ResonanceCorridor()
{
	StackingType = EGameplayEffectStackingType::None;
}
