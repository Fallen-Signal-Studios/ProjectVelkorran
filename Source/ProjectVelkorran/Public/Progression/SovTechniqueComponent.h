// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "SkillTrees/SkillTreeComponent.h"
#include "SovTechniqueComponent.generated.h"
class USovTechniqueRewardSource;
class USovTechniqueSkill;
class USovTechniquePerk;
class ASovTechniqueSafePoint;
class ASovPlayerState;
class UAbilitySystemComponent;
class APawn;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSovTechniqueChanged, FGameplayTag, Protagonist, int32, AvailablePoints, int32, EarnedPoints);
/** Campaign policy over Narrative's existing skills, purchases, saves and owned GAS grants. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent), within=PlayerState)
class PROJECTVELKORRAN_API USovTechniqueComponent : public USkillTreeComponent
{
	GENERATED_BODY()
public:
	virtual void GiveSkillPoints(int32 Points) override;
	virtual void Serialize(FArchive& Archive) override;
	bool IsTechniqueMutationInProgress() const { return bMutating; }
	virtual bool BuyPerk(TSubclassOf<UTreePerk> Perk, UTreeSkill* OwnerSkill) override;
	virtual bool CanBuyPerk(TSubclassOf<UTreePerk> Perk, FText& OutCantBuyReason) override;
	virtual bool HasRequiredPerks(TSubclassOf<UTreePerk> Perk) override;
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;
	/** First-ever protagonist entry only. Existing protagonists restore their component record instead. */
	bool InitializeNewProtagonist(FGameplayTag Protagonist);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Technique") bool ClaimReward(USovTechniqueRewardSource* Source);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Technique") bool RespecAtSafePoint(ASovTechniqueSafePoint* SafePoint);
	/** Select one purchased augment for an existing core ability, or pass no perk to clear that slot. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Technique|Augment")
	bool SelectAugmentAtSafePoint(ASovTechniqueSafePoint* SafePoint,FGameplayTag Ability,TSubclassOf<USovTechniquePerk> Augment,FText& FailureReason);
	UFUNCTION(BlueprintPure, Category="Technique|Augment") USovTechniquePerk* GetSelectedAugment(FGameplayTag Ability) const;
	UFUNCTION(BlueprintPure, Category="Technique|Augment") TArray<USovTechniquePerk*> GetUnlockedAugments(FGameplayTag Ability) const;
	UFUNCTION(BlueprintPure, Category="Technique") bool CanModifyTechniques() const;
	UFUNCTION(BlueprintPure, Category="Technique") int32 GetAvailableTechniquePoints() const { return SkillTreePoints; }
	UFUNCTION(BlueprintPure, Category="Technique") int32 GetEarnedTechniquePoints() const;
	UFUNCTION(BlueprintPure, Category="Technique") bool IsTechniqueStateValid() const { return bStateValid; }
	UFUNCTION(BlueprintPure, Category="Technique") TArray<UTreeSkill*> GetActiveTechniqueBranches() const;
	UPROPERTY(BlueprintAssignable, Category="Technique") FSovTechniqueChanged OnTechniquesChanged;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Technique", meta=(ClampMin="18", ClampMax="22")) int32 TarrikPointBudget = 20;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Technique", meta=(ClampMin="18", ClampMax="22")) int32 SelenePointBudget = 20;
protected:
	virtual void BeginPlay() override;
private:
	ASovPlayerState* Player() const;
	FGameplayTag ActiveIdentity() const;
	int32 PointBudget() const;
	USovTechniqueSkill* FindBranch(TSubclassOf<UTreePerk> Perk) const;
	bool HasValidActiveTree() const;
	bool ValidateSavedState() const;
	bool IsCurrentIdentity() const;
	void RebuildBranchLevels();
	void BroadcastChanged();
	bool MayApplyPerkGrant(const USovTechniquePerk* Perk, int32 Level) const;
	bool ShouldEnablePerkGrant(const USovTechniquePerk* Perk) const;
	bool HasAugmentAbilityContext(FGameplayTag Ability) const;
	bool IsGrantContextCurrent(const USovTechniquePerk* Perk) const;
	bool IsMutationOwnershipCurrent() const;
	bool IsMutationContextCurrent() const;
	void CaptureMutationContext();
	UPROPERTY(SaveGame) FGameplayTag LedgerProtagonist;
	UPROPERTY(SaveGame) TMap<FName, int32> ClaimedRewards;
	/** Stored with the existing LedgerProtagonist record in each protagonist snapshot. */
	UPROPERTY(SaveGame) TMap<FGameplayTag,TSubclassOf<USovTechniquePerk>> SelectedAugments;
	bool bStateValid = true;
	bool bMutating = false;
	bool bAllowParentPurchaseCheck = false;
	TSubclassOf<UTreePerk> PendingPurchase;
	TMap<TSubclassOf<UTreePerk>, int32> AllowedGrantLevels;
	bool bApplyingGrant = false;
	bool bSnapshotPublished = false;
	TWeakObjectPtr<UAbilitySystemComponent> MutationASC;
	TWeakObjectPtr<APawn> MutationPawn;
	FGameplayTag MutationIdentity;
	friend class USovTechniquePerk;
	friend struct FSovTechniqueTestAccess;
};
