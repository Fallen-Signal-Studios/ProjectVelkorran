// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Progression/SovTechniqueComponent.h"
#include "Progression/SovTechniqueTypes.h"
#include "Progression/SovTechniqueRewardSource.h"
#include "Progression/SovTechniqueSafePoint.h"
#include "Progression/SovTechniquePolicy.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/SovGameplayAbility_Echo.h"
#include "Campaign/SovEncounterDirector.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerState.h"
#include "GameFramework/Pawn.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

ASovPlayerState* USovTechniqueComponent::Player() const { return Cast<ASovPlayerState>(GetOwner()); }
FGameplayTag USovTechniqueComponent::ActiveIdentity() const
{
	const ASovPlayerState* PS = Player();
	const UAbilitySystemComponent* ASC = PS ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<ASovPlayerState*>(PS)) : nullptr;
	if (!ASC || !PS->GetPawn() || ASC->GetAvatarActor() != PS->GetPawn()) { return {}; }
	const auto& Tags = FSovGameplayTags::Get();
	const bool bTarrik = ASC->HasMatchingGameplayTag(Tags.Character_Player_Tarrik);
	const bool bSelene = ASC->HasMatchingGameplayTag(Tags.Character_Player_Selene);
	return bTarrik != bSelene ? (bTarrik ? Tags.Character_Player_Tarrik : Tags.Character_Player_Selene) : FGameplayTag();
}
bool USovTechniqueComponent::IsCurrentIdentity() const
{
	return LedgerProtagonist.IsValid() && LedgerProtagonist == ActiveIdentity();
}
int32 USovTechniqueComponent::PointBudget() const
{
	return LedgerProtagonist == FSovGameplayTags::Get().Character_Player_Tarrik ? TarrikPointBudget : SelenePointBudget;
}
int32 USovTechniqueComponent::GetEarnedTechniquePoints() const
{
	int64 Total = 0;
	for (const auto& Reward : ClaimedRewards)
	{
		if (Reward.Key.IsNone() || Reward.Value < 1 || Reward.Value > 5) { return -1; }
		Total += Reward.Value;
		if (Total > 22) { return -1; }
	}
	return static_cast<int32>(Total);
}
void USovTechniqueComponent::BeginPlay()
{
	SkillTreeSkills.RemoveAll([](UTreeSkill* Skill) { return !IsValid(Skill); });
	Super::BeginPlay();
	if (Player() && Player()->HasAuthority() && !LedgerProtagonist.IsValid() && ClaimedRewards.IsEmpty()
		&& PurchasedPerks.IsEmpty() && SkillTreePoints == 0) { LedgerProtagonist = ActiveIdentity(); }
}
void USovTechniqueComponent::GiveSkillPoints(int32 Points)
{
	// Intentionally disabled, including calls made through a USkillTreeComponent pointer.
	// Campaign points require a unique authored reward and native completion proof.
}
bool USovTechniqueComponent::InitializeNewProtagonist(FGameplayTag Protagonist)
{
	const auto& Tags = FSovGameplayTags::Get();
	if (!Player() || !Player()->HasAuthority() || bMutating
		|| (Protagonist != Tags.Character_Player_Tarrik && Protagonist != Tags.Character_Player_Selene)
		|| Protagonist != ActiveIdentity()
		|| (LedgerProtagonist == Protagonist && (!ClaimedRewards.IsEmpty() || !PurchasedPerks.IsEmpty() || SkillTreePoints != 0))) { return false; }
	CaptureMutationContext();
	TGuardValue<bool> Guard(bMutating, true);
	ClearPurchasedPerksForRestore();
	// Removing the outgoing profile can synchronously invoke GAS listeners. Do
	// not reset its durable ledger if one replaced/destroyed our active avatar.
	if (!IsMutationOwnershipCurrent()) { bStateValid = false; return false; }
	SkillTreeSaveData.ClearData();
	ClaimedRewards.Reset(); SelectedAugments.Reset(); SkillTreePoints = 0; LedgerProtagonist = Protagonist; bStateValid = true;
	RebuildBranchLevels(); BroadcastChanged();
	return true;
}
TArray<UTreeSkill*> USovTechniqueComponent::GetActiveTechniqueBranches() const
{
	TArray<UTreeSkill*> Result;
	const FGameplayTag Identity = ActiveIdentity();
	for (UTreeSkill* Skill : SkillTreeSkills)
	{
		const auto* Technique = Cast<USovTechniqueSkill>(Skill);
		if (Technique && Technique->Protagonist == Identity) { Result.Add(Skill); }
	}
	return Result;
}
USovTechniqueSkill* USovTechniqueComponent::FindBranch(TSubclassOf<UTreePerk> Perk) const
{
	USovTechniqueSkill* Found = nullptr;
	for (UTreeSkill* Skill : SkillTreeSkills)
	{
		auto* Branch = Cast<USovTechniqueSkill>(Skill);
		if (!Branch) { continue; }
		for (const FPerkConfig& Node : Branch->Perks)
		{
			if (Node.Perk == Perk) { if (Found) { return nullptr; } Found = Branch; }
		}
	}
	return Found;
}
bool USovTechniqueComponent::HasValidActiveTree() const
{
	if (!SovTechniquePolicy::ValidBudget(TarrikPointBudget) || !SovTechniquePolicy::ValidBudget(SelenePointBudget)) { return false; }
	const FGameplayTag Identity = ActiveIdentity();
	if (!Identity.IsValid()) { return false; }
	TSet<FName> BranchIds; TSet<UClass*> SkillClasses; TSet<UClass*> PerkClasses;
	int32 TotalRanks = 0;
	for (UTreeSkill* Skill : SkillTreeSkills)
	{
		auto* Branch = Cast<USovTechniqueSkill>(Skill);
		if (!Branch || Branch->Protagonist != Identity) { continue; }
		if (Branch->BranchId.IsNone() || BranchIds.Contains(Branch->BranchId) || SkillClasses.Contains(Branch->GetClass())) { return false; }
		BranchIds.Add(Branch->BranchId); SkillClasses.Add(Branch->GetClass());
		for (const FPerkConfig& Node : Branch->Perks)
		{
			const auto* Perk = Node.Perk.Get() ? Cast<USovTechniquePerk>(Node.Perk->GetDefaultObject()) : nullptr;
			if (!Perk || PerkClasses.Contains(Node.Perk.Get()) || FindBranch(Node.Perk) != Branch || !Perk->HasValidNativeGrantPolicy()) { return false; }
			if (Perk->AugmentedAbility.IsValid() && !Perk->AugmentedAbility.ToString().StartsWith(
				Identity==FSovGameplayTags::Get().Character_Player_Tarrik ? TEXT("Sov.Ability.Echo.Tarrik.") : TEXT("Sov.Ability.Echo.Selene."))) { return false; }
			PerkClasses.Add(Node.Perk.Get()); TotalRanks += Perk->MaxLevels;
			for (TSubclassOf<UTreePerk> Link : Node.LinkedTo)
			{
				const USovTechniqueSkill* LinkedBranch = FindBranch(Link);
				if (!LinkedBranch || LinkedBranch->Protagonist != Identity || Link == Node.Perk) { return false; }
			}
		}
	}
	if (BranchIds.Num() != 3 || TotalRanks < 24 || TotalRanks > 30) { return false; }
	// Reject cycles in the existing authored graph; no inaccessible point sinks.
	TMap<UClass*, int32> Incoming;
	TMap<UClass*, TArray<UClass*>> Outgoing;
	for (UClass* Class : PerkClasses) { Incoming.Add(Class, 0); }
	for (UTreeSkill* Skill : SkillTreeSkills)
	{
		const auto* Branch = Cast<USovTechniqueSkill>(Skill);
		if (!Branch || Branch->Protagonist != Identity) { continue; }
		for (const FPerkConfig& Node : Branch->Perks)
		{
			for (TSubclassOf<UTreePerk> Link : Node.LinkedTo)
			{
				Outgoing.FindOrAdd(Node.Perk.Get()).Add(Link.Get());
				++Incoming.FindChecked(Link.Get());
			}
		}
	}
	TArray<UClass*> Ready;
	for (const auto& Pair : Incoming) { if (Pair.Value == 0) { Ready.Add(Pair.Key); } }
	int32 Visited = 0;
	while (!Ready.IsEmpty())
	{
		UClass* Class = Ready.Pop(EAllowShrinking::No); ++Visited;
		if (const TArray<UClass*>* Links = Outgoing.Find(Class))
		{
			for (UClass* Link : *Links) { if (--Incoming.FindChecked(Link) == 0) { Ready.Add(Link); } }
		}
	}
	return Visited == PerkClasses.Num();
}
bool USovTechniqueComponent::CanModifyTechniques() const
{
	if (!Player() || !Player()->HasAuthority() || !bStateValid || bMutating || !IsCurrentIdentity() || !HasValidActiveTree()) { return false; }
	for (TActorIterator<ASovTechniqueSafePoint> It(GetWorld()); It; ++It)
	{
		if (It->AllowsModification(Player())) { return true; }
	}
	return false;
}
bool USovTechniqueComponent::HasRequiredPerks(TSubclassOf<UTreePerk> Perk)
{
	// Preserve Narrative's authored incoming links, but all listed prerequisites
	// must be satisfied; a converging cross-branch node cannot unlock via one arm.
	if (const FPerkArray* Prerequisites = PrerequisiteMap.Find(Perk))
	{
		for (TSubclassOf<UTreePerk> Required : Prerequisites->Array) { if (!HasPerk(Required)) { return false; } }
	}
	return true;
}
bool USovTechniqueComponent::CanBuyPerk(TSubclassOf<UTreePerk> Perk, FText& OutCantBuyReason)
{
	if (bMutating)
	{
		const bool bAllowed = bAllowParentPurchaseCheck && PendingPurchase == Perk;
		bAllowParentPurchaseCheck = false;
		return bAllowed;
	}
	USovTechniqueSkill* Branch = FindBranch(Perk);
	const USovTechniquePerk* Defaults = Perk.Get() ? Cast<USovTechniquePerk>(Perk->GetDefaultObject()) : nullptr;
	if (!CanModifyTechniques() || !Branch || Branch->Protagonist != ActiveIdentity() || !Defaults || !Defaults->HasValidNativeGrantPolicy())
	{
		OutCantBuyReason = NSLOCTEXT("SovTechnique", "Unavailable", "Techniques can be changed at a safe point."); return false;
	}
	int32 Investment = 0;
	for (UTreePerk* Purchased : PurchasedPerks)
	{
		if (!Purchased) { continue; }
		if (FindBranch(Purchased->GetClass()) == Branch && Purchased->GetClass() != Perk.Get()) { Investment += Purchased->PerkLevel + 1; }
		const auto* Native = Cast<USovTechniquePerk>(Purchased);
		if (Defaults->IncompatiblePerks.Contains(Purchased->GetClass()) || (Native && Native->IncompatiblePerks.Contains(Perk)))
		{
			OutCantBuyReason = NSLOCTEXT("SovTechnique", "Exclusive", "Choose one of these Techniques."); return false;
		}
	}
	if (Investment < Defaults->RequiredBranchInvestment)
	{
		OutCantBuyReason = NSLOCTEXT("SovTechnique", "Investment", "More investment in this branch is required."); return false;
	}
	return Super::CanBuyPerk(Perk, OutCantBuyReason);
}
bool USovTechniqueComponent::BuyPerk(TSubclassOf<UTreePerk> Perk, UTreeSkill* OwnerSkill)
{
	FText Reason;
	if (bMutating || FindBranch(Perk) != OwnerSkill || !CanBuyPerk(Perk, Reason)) { return false; }
	Super::PrepareForSave_Implementation();
	const FSkillTreeSaveData Previous = SkillTreeSaveData;
	const int32 PreviousPoints = SkillTreePoints;
	CaptureMutationContext();
	TGuardValue<bool> Guard(bMutating, true);
	AllowedGrantLevels.Reset(); AllowedGrantLevels.Add(Perk, GetPerkLevel(Perk) + 1);
	PendingPurchase = Perk; bAllowParentPurchaseCheck = true;
	const bool bPurchased = Super::BuyPerk(Perk, OwnerSkill);
	bAllowParentPurchaseCheck = false; PendingPurchase = nullptr;
	const auto* Applied = Cast<USovTechniquePerk>(GetPerk(Perk));
	if (!IsMutationContextCurrent()) { AllowedGrantLevels.Reset(); bStateValid = false; return false; }
	if (!bPurchased || !Applied || !Applied->DidLastGrantSucceed())
	{
		SkillTreeSaveData = Previous; SkillTreePoints = PreviousPoints;
		AllowedGrantLevels.Reset();
		for (const FSavedPerk& Saved : Previous.SavedPerks) { AllowedGrantLevels.Add(Saved.PerkClass, Saved.PerkLevel); }
		Super::Load_Implementation(); AllowedGrantLevels.Reset();
		if (!IsMutationContextCurrent()) { bStateValid = false; return false; }
		for (UTreePerk* Restored : PurchasedPerks)
		{
			const auto* Native = Cast<USovTechniquePerk>(Restored);
			if (!Native || !Native->DidLastGrantSucceed()) { bStateValid = false; break; }
		}
		if (!bStateValid) { ClearPurchasedPerksForRestore(); SkillTreePoints = 0; }
		return false;
	}
	AllowedGrantLevels.Reset(); RebuildBranchLevels(); BroadcastChanged();
	return true;
}
bool USovTechniqueComponent::ClaimReward(USovTechniqueRewardSource* Source)
{
	if (!Player() || !Player()->HasAuthority() || bMutating || !bStateValid || !IsValid(Source)) { return false; }
	if (!LedgerProtagonist.IsValid() && ClaimedRewards.IsEmpty() && PurchasedPerks.IsEmpty() && SkillTreePoints == 0) { LedgerProtagonist = ActiveIdentity(); }
	if (!IsCurrentIdentity() || Source->Protagonist != LedgerProtagonist || !Source->HasNativeProof(Player())
		|| !SovTechniquePolicy::CanClaim(GetEarnedTechniquePoints(), SkillTreePoints, Source->Points, PointBudget(), ClaimedRewards.Contains(Source->RewardId))) { return false; }
	const FName RewardId = Source->RewardId;
	const int32 Points = Source->Points;
	CaptureMutationContext();
	TGuardValue<bool> Guard(bMutating, true);
	if (Source->Proof == ESovTechniqueRewardProof::EncounterComplete && !Source->Encounter->ClaimCompletionReward(RewardId)) { return false; }
	ClaimedRewards.Add(RewardId, Points); SkillTreePoints += Points;
	BroadcastChanged();
	return true;
}
bool USovTechniqueComponent::RespecAtSafePoint(ASovTechniqueSafePoint* SafePoint)
{
	if (!CanModifyTechniques() || !IsValid(SafePoint) || !SafePoint->AllowsModification(Player())) { return false; }
	CaptureMutationContext();
	TGuardValue<bool> Guard(bMutating, true);
	ClearPurchasedPerksForRestore(); SkillTreeSaveData.ClearData();
	if (!IsMutationContextCurrent()) { bStateValid = false; return false; }
	SelectedAugments.Reset();
	SkillTreePoints = GetEarnedTechniquePoints(); RebuildBranchLevels(); BroadcastChanged();
	return true;
}
void USovTechniqueComponent::RebuildBranchLevels()
{
	for (UTreeSkill* Skill : SkillTreeSkills)
	{
		if (Skill) { Skill->SkillLevel = GetDefault<UTreeSkill>(Skill->GetClass())->SkillLevel; }
	}
	for (UTreePerk* Perk : PurchasedPerks)
	{
		if (Perk) { if (auto* Branch = FindBranch(Perk->GetClass())) { Branch->SkillLevel += Perk->PerkLevel + 1; } }
	}
}
void USovTechniqueComponent::PrepareForSave_Implementation()
{
	if (!Player() || !Player()->HasAuthority() || bMutating) { return; }
	if (!LedgerProtagonist.IsValid() && ClaimedRewards.IsEmpty() && PurchasedPerks.IsEmpty() && SkillTreePoints == 0) { LedgerProtagonist = ActiveIdentity(); }
	Super::PrepareForSave_Implementation();
}
bool USovTechniqueComponent::ValidateSavedState() const
{
	if (!IsCurrentIdentity() || (!SkillTreeSaveData.SavedPerks.IsEmpty() && !HasValidActiveTree())) { return false; }
	int32 Spent = 0; TSet<UClass*> Seen;
	for (const FSavedPerk& Saved : SkillTreeSaveData.SavedPerks)
	{
		const auto* Default = Saved.PerkClass.Get() ? Cast<USovTechniquePerk>(Saved.PerkClass->GetDefaultObject()) : nullptr;
		const auto* Branch = FindBranch(Saved.PerkClass);
		if (!Default || !Branch || Branch->Protagonist != LedgerProtagonist || Seen.Contains(Saved.PerkClass.Get())
			|| Saved.PerkLevel < 0 || Saved.PerkLevel >= Default->MaxLevels || !Default->HasValidNativeGrantPolicy()) { return false; }
		Seen.Add(Saved.PerkClass.Get()); Spent += Saved.PerkLevel + 1;
	}
	if (SelectedAugments.Num()>16) { return false; }
	for (const auto& Selection:SelectedAugments)
	{
		const auto* Defaults=Selection.Value.Get() ? Selection.Value->GetDefaultObject<USovTechniquePerk>() : nullptr;
		if (!Selection.Key.IsValid() || !Defaults || Defaults->AugmentedAbility!=Selection.Key || !Seen.Contains(Selection.Value.Get())) { return false; }
	}
	for (const FSavedPerk& Saved : SkillTreeSaveData.SavedPerks)
	{
		const auto* Default = Cast<USovTechniquePerk>(Saved.PerkClass->GetDefaultObject());
		if (const FPerkArray* Prerequisites = PrerequisiteMap.Find(Saved.PerkClass))
		{
			for (TSubclassOf<UTreePerk> Required : Prerequisites->Array) { if (!Seen.Contains(Required.Get())) { return false; } }
		}
		for (TSubclassOf<UTreePerk> Incompatible : Default->IncompatiblePerks) { if (Seen.Contains(Incompatible.Get())) { return false; } }
		int32 Investment = 0;
		for (const FSavedPerk& Other : SkillTreeSaveData.SavedPerks)
		{
			if (Other.PerkClass != Saved.PerkClass && FindBranch(Other.PerkClass) == FindBranch(Saved.PerkClass)) { Investment += Other.PerkLevel + 1; }
		}
		if (Investment < Default->RequiredBranchInvestment) { return false; }
	}
	return SovTechniquePolicy::ValidLedger(GetEarnedTechniquePoints(), SkillTreePoints, Spent, PointBudget());
}
void USovTechniqueComponent::Load_Implementation()
{
	if (!Player() || !Player()->HasAuthority() || bMutating) { return; }
	CaptureMutationContext();
	TGuardValue<bool> Guard(bMutating, true);
	bStateValid = ValidateSavedState();
	if (!bStateValid) { ClearPurchasedPerksForRestore(); SkillTreePoints = 0; BroadcastChanged(); return; }
	AllowedGrantLevels.Reset();
	for (const FSavedPerk& Saved : SkillTreeSaveData.SavedPerks) { AllowedGrantLevels.Add(Saved.PerkClass, Saved.PerkLevel); }
	Super::Load_Implementation(); AllowedGrantLevels.Reset();
	if (!IsMutationContextCurrent()) { bStateValid = false; return; }
	for (UTreePerk* Perk : PurchasedPerks)
	{
		const auto* Native = Cast<USovTechniquePerk>(Perk);
		if (!Native || !Native->DidLastGrantSucceed()) { bStateValid = false; break; }
	}
	if (!bStateValid) { ClearPurchasedPerksForRestore(); SkillTreePoints = 0; }
	RebuildBranchLevels(); BroadcastChanged();
}
bool USovTechniqueComponent::MayApplyPerkGrant(const USovTechniquePerk* Perk, int32 Level) const
{
	const int32* Expected = Perk ? AllowedGrantLevels.Find(Perk->GetClass()) : nullptr;
	return !bApplyingGrant && Expected && *Expected == Level && IsGrantContextCurrent(Perk);
}
bool USovTechniqueComponent::ShouldEnablePerkGrant(const USovTechniquePerk* Perk) const
{
	return Perk && (!Perk->AugmentedAbility.IsValid() || SelectedAugments.FindRef(Perk->AugmentedAbility).Get()==Perk->GetClass());
}
USovTechniquePerk* USovTechniqueComponent::GetSelectedAugment(FGameplayTag Ability) const
{
	if (!bStateValid || !IsCurrentIdentity() || (bMutating&&!bSnapshotPublished)) { return nullptr; }
	const auto Class=SelectedAugments.FindRef(Ability);
	return Class.Get() ? Cast<USovTechniquePerk>(GetPerk(Class)) : nullptr;
}
TArray<USovTechniquePerk*> USovTechniqueComponent::GetUnlockedAugments(FGameplayTag Ability) const
{
	TArray<USovTechniquePerk*> Result;
	if (!bStateValid || !IsCurrentIdentity() || (bMutating&&!bSnapshotPublished)) { return Result; }
	for (UTreePerk* Purchased:PurchasedPerks)
	{
		auto* Perk=Cast<USovTechniquePerk>(Purchased);
		if (Perk && Perk->PerkLevel>=0 && Perk->AugmentedAbility==Ability && Ability.IsValid()) { Result.Add(Perk); }
	}
	return Result;
}
bool USovTechniqueComponent::HasAugmentAbilityContext(FGameplayTag Ability) const
{
	const auto* PS=Player(); const UAbilitySystemComponent* ASC=PS ? PS->GetAbilitySystemComponent() : nullptr;
	if (!ASC || !ASC->AbilityActorInfo.IsValid() || !PS->GetPawn() || ASC->GetAvatarActor()!=PS->GetPawn()
		|| !Ability.IsValid() || Ability==FSovGameplayTags::Get().Ability_Echo || !Ability.MatchesTag(FSovGameplayTags::Get().Ability_Echo)
		|| ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Weapon_VerityAbsent)
		|| ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Weapon_Equipping)) { return false; }
	const FGameplayAbilitySpec* Core=nullptr;
	for (const FGameplayAbilitySpec& Spec:ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetAssetTags().HasTagExact(Ability))
		{
			if (Core || Spec.IsActive() || Spec.PendingRemove || Spec.RemoveAfterActivation) { return false; }
			Core=&Spec;
		}
	}
	const auto* Echo=Core ? Cast<USovGameplayAbility_EchoBase>(Core->Ability) : nullptr;
	return Echo && Echo->CanUseEchoWeaponContext(Core->Handle,ASC->AbilityActorInfo.Get());
}
bool USovTechniqueComponent::SelectAugmentAtSafePoint(ASovTechniqueSafePoint* SafePoint,FGameplayTag Ability,
	TSubclassOf<USovTechniquePerk> Augment,FText& FailureReason)
{
	FailureReason=FText::GetEmpty();
	if (!CanModifyTechniques() || !IsValid(SafePoint) || !SafePoint->AllowsModification(Player()))
	{ FailureReason=NSLOCTEXT("SovTechnique","AugmentSafePoint","Change augments at a safe point outside combat."); return false; }
	if (!HasAugmentAbilityContext(Ability))
	{ FailureReason=NSLOCTEXT("SovTechnique","AugmentEquipment","Equip the matching available weapon before changing this ability's augment."); return false; }
	auto* NewPerk=Augment.Get() ? Cast<USovTechniquePerk>(GetPerk(Augment)) : nullptr;
	if (Augment.Get() && (!NewPerk || NewPerk->PerkLevel<0 || NewPerk->AugmentedAbility!=Ability || !NewPerk->HasValidNativeGrantPolicy()))
	{ FailureReason=NSLOCTEXT("SovTechnique","AugmentLocked","Unlock an augment for this ability first."); return false; }
	const auto Previous=SelectedAugments.FindRef(Ability);
	if (Previous==Augment) { return true; }
	auto* OldPerk=Previous.Get() ? Cast<USovTechniquePerk>(GetPerk(Previous)) : nullptr;
	if (Previous.Get() && (!OldPerk || OldPerk->PerkLevel<0)) { bStateValid=false; return false; }
	const int32 OldLevel=OldPerk ? OldPerk->PerkLevel : -1, NewLevel=NewPerk ? NewPerk->PerkLevel : -1;
	CaptureMutationContext(); TGuardValue<bool> Mutation(bMutating,true);
	if (Augment.Get()) { SelectedAugments.Add(Ability,Augment); } else { SelectedAugments.Remove(Ability); }
	const auto Refresh=[this](USovTechniquePerk* Perk,int32 Level)
	{
		if (!Perk) { return true; }
		AllowedGrantLevels.Reset(); AllowedGrantLevels.Add(Perk->GetClass(),Level);
		ApplyPurchasedPerkLevel(Perk,Level); AllowedGrantLevels.Reset();
		return IsMutationContextCurrent() && Perk->DidLastGrantSucceed() && Perk->PerkLevel==Level;
	};
	const auto RequireRecovery=[this,OldPerk,NewPerk,&FailureReason]()
	{
		bStateValid=false;
		if (NewPerk) { NewPerk->RemoveNativeGrants(); }
		if (OldPerk) { OldPerk->RemoveNativeGrants(); }
		FailureReason=NSLOCTEXT("SovTechnique","AugmentRecoveryRequired","Loadout update failed. Reload a checkpoint before continuing.");
	};
	const bool bApplied=Refresh(OldPerk,OldLevel) && Refresh(NewPerk,NewLevel);
	if (!IsMutationContextCurrent()) { RequireRecovery(); return false; }
	if (!bApplied || !IsValid(SafePoint) || !SafePoint->AllowsModification(Player()) || !HasAugmentAbilityContext(Ability))
	{
		// No points or core ability specs changed. Restore only the two affected owned grant sets.
		if (Previous.Get()) { SelectedAugments.Add(Ability,Previous); } else { SelectedAugments.Remove(Ability); }
		const bool bRestored=Refresh(NewPerk,NewLevel) && Refresh(OldPerk,OldLevel);
		if (!bRestored) { RequireRecovery(); return false; }
		FailureReason=NSLOCTEXT("SovTechnique","AugmentInterrupted","The loadout change was interrupted; the previous selection was retained.");
		return false;
	}
	BroadcastChanged(); return true;
}
void USovTechniqueComponent::BroadcastChanged()
{
	// Publish a complete save image before any UI/mission callback can save.
	Super::PrepareForSave_Implementation();
	bSnapshotPublished = true;
	OnTechniquesChanged.Broadcast(LedgerProtagonist, SkillTreePoints, GetEarnedTechniquePoints());
}

void USovTechniqueComponent::CaptureMutationContext()
{
	MutationASC = Player() ? Player()->GetAbilitySystemComponent() : nullptr;
	MutationPawn = Player() ? Player()->GetPawn() : nullptr;
	MutationIdentity = ActiveIdentity(); bSnapshotPublished = false;
}
bool USovTechniqueComponent::IsMutationOwnershipCurrent() const
{
	const ASovPlayerState* PS = Player();
	return IsValid(PS) && !PS->IsActorBeingDestroyed() && PS->HasAuthority()
		&& MutationASC.IsValid() && MutationPawn.IsValid() && !MutationPawn->IsActorBeingDestroyed()
		&& PS->GetAbilitySystemComponent() == MutationASC.Get() && PS->GetPawn() == MutationPawn.Get()
		&& MutationASC->GetAvatarActor() == MutationPawn.Get() && ActiveIdentity() == MutationIdentity;
}
bool USovTechniqueComponent::IsMutationContextCurrent() const
{
	return IsMutationOwnershipCurrent() && LedgerProtagonist == MutationIdentity;
}
bool USovTechniqueComponent::IsGrantContextCurrent(const USovTechniquePerk* Perk) const
{
	return bMutating && IsMutationContextCurrent()
		&& PurchasedPerks.ContainsByPredicate([Perk](const UTreePerk* Owned) { return Owned == Perk; });
}
void USovTechniqueComponent::Serialize(FArchive& Archive)
{
	if (Archive.IsSaveGame() && Archive.IsLoading())
	{
		if (bMutating) { Archive.SetError(); return; }
		// Earlier compatible Technique records have no augment field. Never carry a live
		// protagonist's current selection into a record that predates the field.
		SelectedAugments.Reset();
	}
	if (Archive.IsSaveGame() && Archive.IsSaving() && (!bStateValid || (bMutating && !bSnapshotPublished)))
	{
		// An invalid ledger or callback inside a grant/removal is not a checkpoint.
		Archive.SetError(); return;
	}
	Super::Serialize(Archive);
}
