// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "AI/Activities/NPCActivityComponent.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "NarrativeSavableActor.h"
#include "SovNPCActivityRestoreTestFixtures.generated.h"

UCLASS()
class USovNPCActivityRestoreTestActivity : public UNPCActivity
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) int32 SavedValue = 0;
	bool HasRealOwner(const ANarrativeNPCController* Controller) const
	{ return OwnerController == Controller && OwnerActivityComponent == Controller->GetActivityComponent(); }
protected:
	virtual float ScoreActivity_Implementation(const FNPCGoalContainer& Container,
		UNPCGoalItem*& BestGoal, TArray<UNPCGoalItem*>& InvalidGoals) override;
};

UCLASS()
class USovNPCActivityRestoreTestGenerator : public UNPCGoalGenerator
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) int32 SavedValue = 0;
	virtual void InitializeGoalGenerator_Implementation() override;
};

UCLASS()
class USovNPCActivityRestoreTestGoal : public UNPCGoalItem
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) int32 SavedValue = 0;
	virtual void Initialize_Implementation() override;
};

/** Access to fixture data only. Owner cache and lifecycle are never initialized here. */
UCLASS()
class USovNPCActivityRestoreTestComponent : public UNPCActivityComponent
{
	GENERATED_BODY()
public:
	TArray<int32> GeneratorInitializations;
	TArray<int32> GoalInitializations;
	TFunction<void()> DuringGeneratorInitialization;
	TFunction<void()> DuringGoalInitialization;
	TFunction<void()> DuringActivityScore;
	bool bAllCallbacksHadRealInitializedOwner = true;
	int32 NumSavedActivities() const { return SavedActivities.Num(); }
	int32 NumSavedGenerators() const { return SavedGoalGenerators.Num(); }
	int32 NumSavedGoals() const { return SavedGoals.Num(); }
	bool IsRestorePending() const { return bSavedActivityRestorePending; }
	bool HasCachedOwner() const { return OwnerController != nullptr; }
	TArray<uint8> FirstSavedGoalBytes() const { return SavedGoals.IsEmpty() ? TArray<uint8>() : SavedGoals[0].Data; }
	void RecordInitialization(bool bGenerator, int32 Value, ANarrativeNPCController* Controller);
	void SeedSavedValues(int32 Value, bool bIncludeGenerator = true);
	void InvalidateSavedActivityClass() { SavedActivities[0].Class = nullptr; }
};

UCLASS()
/** Supplies the save interface and identity normally authored on the controller Blueprint. */
class ASovNPCActivityRestoreTestController : public ANarrativeNPCController, public INarrativeSavableActor
{
	GENERATED_BODY()
public:
	ASovNPCActivityRestoreTestController(const FObjectInitializer& Initializer)
		: Super(Initializer.SetDefaultSubobjectClass<USovNPCActivityRestoreTestComponent>(TEXT("NPCActivityComponent"))) {}
	USovNPCActivityRestoreTestComponent* TestActivities() const
	{ return CastChecked<USovNPCActivityRestoreTestComponent>(GetActivityComponent()); }
	virtual FGuid GetActorGUID_Implementation() const override { return TestStableGuid; }
	virtual void SetActorGUID_Implementation(const FGuid& SavedGuid) override { TestStableGuid = SavedGuid; }
private:
	// Unreflected so each constructed actor keeps its own identity instead of copying the CDO's.
	FGuid TestStableGuid = FGuid::NewGuid();
};

/** Keeps actual possession and Actor/component startup; excludes asynchronous appearance assets. */
UCLASS()
class ASovNPCActivityRestoreTestPawn : public ANarrativeNPCCharacter
{
	GENERATED_BODY()
public:
	ASovNPCActivityRestoreTestPawn(const FObjectInitializer& Initializer) : Super(Initializer)
	{ AutoPossessAI = EAutoPossessAI::Disabled; }
	virtual void BeginPlay() override { ACharacter::BeginPlay(); }
	virtual void PossessedBy(AController* NewController) override { APawn::PossessedBy(NewController); }
	virtual FGuid GetActorGUID_Implementation() const override { return TestStableGuid; }
	virtual void SetActorGUID_Implementation(const FGuid& SavedGuid) override { TestStableGuid = SavedGuid; }
private:
	// The native NPC requires the same stable identity that its ordinary Blueprint supplies.
	FGuid TestStableGuid = FGuid::NewGuid();
};
