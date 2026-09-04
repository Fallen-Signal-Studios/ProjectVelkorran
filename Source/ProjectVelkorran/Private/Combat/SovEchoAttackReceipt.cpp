// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Combat/SovEchoAttackReceipt.h"
#include "GAS/NarrativeCombatAbility.h"
#include "Sovereign/SovGameplayTags.h"

USovEchoAttackReceipt* USovEchoAttackReceipt::CreateForActiveAbility(UNarrativeCombatAbility* Ability)
{
	if (!IsValid(Ability) || Ability->HasAnyFlags(RF_ClassDefaultObject)
		|| Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::NonInstanced || !Ability->IsActive()) return nullptr;
	AActor* Source = Ability->GetAvatarActorFromActorInfo();
	FGuid AttackId;
	if (!Ability->GetSovAttackIdentity(Source, AttackId)) return nullptr;
	USovEchoAttackReceipt* Receipt = NewObject<USovEchoAttackReceipt>(Ability);
	Receipt->SourceAbility = Ability;
	Receipt->SourceActor = Source;
	Receipt->CapturedAttackId = AttackId;
	Receipt->bHeavyAttack = Ability->GetAssetTags().HasTag(FSovGameplayTags::Get().Damage_Heavy)
		&& !Ability->GetAssetTags().HasTag(FSovGameplayTags::Get().Ability_Echo);
	return Receipt;
}

bool USovEchoAttackReceipt::GetSovAttackIdentity(const AActor* ExpectedSource, FGuid& OutAttackId) const
{
	OutAttackId.Invalidate();
	FGuid LiveId;
	if (!SourceAbility.IsValid() || !SourceActor.IsValid() || ExpectedSource != SourceActor.Get()
		|| !CapturedAttackId.IsValid() || !SourceAbility->GetSovAttackIdentity(ExpectedSource, LiveId)
		|| LiveId != CapturedAttackId) return false;
	OutAttackId = CapturedAttackId;
	return true;
}

bool USovEchoAttackReceipt::MatchesCommittedHeavyAttack(const AActor* ExpectedSource, const FGuid& AttackId) const
{
	return bHeavyAttack && SourceActor.IsValid() && SourceActor.Get() == ExpectedSource
		&& CapturedAttackId.IsValid() && AttackId == CapturedAttackId;
}
