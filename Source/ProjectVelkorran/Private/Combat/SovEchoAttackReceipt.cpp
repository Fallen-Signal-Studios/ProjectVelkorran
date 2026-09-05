// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Combat/SovEchoAttackReceipt.h"
#include "GAS/NarrativeCombatAbility.h"
#include "Sovereign/SovGameplayTags.h"
#include "Melee/SovGameplayAbility_Melee.h"
#include "GAS/SovCombatTypes.h"

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
	if (const auto* Melee = Cast<USovGameplayAbility_Melee>(Ability)) { Receipt->bHeavyAttack = Melee->IsCurrentNodeHeavy(); }
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

bool USovEchoAttackReceipt::ConsumeHeavyMultiTargetReward(const UObject* Consumer, uint32 Scope, const FSovDamageResult& Result) const
{
	if (!IsValid(Consumer) || bHeavyRewardConsumed || !Result.HasNativeReceipt() || !IsValid(Result.TargetActor)
		|| !MatchesCommittedHeavyAttack(Result.SourceActor.Get(), Result.AttackId)) { return false; }
	if (!bHeavyRewardStarted)
	{
		bHeavyRewardStarted = true; HeavyRewardConsumer = Consumer; HeavyRewardScope = Scope;
	}
	if (HeavyRewardConsumer.Get() != Consumer || HeavyRewardScope != Scope) { return false; }
	HeavyRewardTargets.Add(Result.TargetActor.Get());
	if (HeavyRewardTargets.Num() < 3) { return false; }
	bHeavyRewardConsumed = true;
	HeavyRewardTargets.Reset();
	return true;
}
