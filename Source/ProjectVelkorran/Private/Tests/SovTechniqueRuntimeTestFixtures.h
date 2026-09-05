// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Progression/SovTechniqueTypes.h"
#include "Campaign/SovEncounterDirector.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/SovGameplayAbility_Echo.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "SovTechniqueRuntimeTestFixtures.generated.h"
class USovTechniqueComponent;
UCLASS(Transient, NotBlueprintable)
class USovTechniqueCoreTestAbility : public USovGameplayAbility_EchoBase
{
	GENERATED_BODY()
public: USovTechniqueCoreTestAbility();
};
UCLASS(Transient, NotBlueprintable)
class USovTechniqueOwnedTestAbility : public UGameplayAbility
{
	GENERATED_BODY()
};
UCLASS(Transient, NotBlueprintable)
class USovTechniqueUnrelatedTestAbility : public UGameplayAbility
{
	GENERATED_BODY()
};
/** A real dynamic UI/save listener; captures only the committed notification. */
UCLASS(Transient, NotBlueprintable)
class USovTechniqueSnapshotTestObserver : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<USovTechniqueComponent> Techniques;
	FNarrativeSaveComponent Record;
	int32 Notifications = 0;
	int32 AvailableAtNotification = INDEX_NONE;
	bool bCaptureSucceeded = false;
	UFUNCTION() void OnChanged(FGameplayTag Protagonist, int32 Available, int32 Earned);
};
UCLASS(Transient, NotBlueprintable)
class USovTechniqueTestEffect : public UGameplayEffect
{
	GENERATED_BODY()
public: USovTechniqueTestEffect();
};
UCLASS(Transient, NotBlueprintable)
class USovTechniqueTestPerkA : public USovTechniquePerk
{
	GENERATED_BODY()
public: USovTechniqueTestPerkA(const FObjectInitializer& ObjectInitializer);
};
UCLASS(Transient, NotBlueprintable)
class USovTechniqueTestPerkB : public USovTechniquePerk
{
	GENERATED_BODY()
public: USovTechniqueTestPerkB(const FObjectInitializer& ObjectInitializer);
};
UCLASS(Transient, NotBlueprintable)
class USovTechniqueTestPerkC : public USovTechniquePerk
{
	GENERATED_BODY()
public: USovTechniqueTestPerkC(const FObjectInitializer& ObjectInitializer);
};
UCLASS(Transient, NotBlueprintable)
class USovTechniqueTestPerkD : public USovTechniquePerk
{
	GENERATED_BODY()
public: USovTechniqueTestPerkD(const FObjectInitializer& ObjectInitializer);
};
UCLASS(Transient, NotBlueprintable)
class USovTechniqueTestPerkE : public USovTechniquePerk
{
	GENERATED_BODY()
public: USovTechniqueTestPerkE(const FObjectInitializer& ObjectInitializer);
};
UCLASS(Transient, NotBlueprintable)
class USovTechniqueTestPerkF : public USovTechniquePerk
{
	GENERATED_BODY()
public: USovTechniqueTestPerkF(const FObjectInitializer& ObjectInitializer);
};
UCLASS(Transient, NotBlueprintable)
class USovTechniqueTestSkillA : public USovTechniqueSkill
{
	GENERATED_BODY()
public: USovTechniqueTestSkillA(const FObjectInitializer& ObjectInitializer);
};
UCLASS(Transient, NotBlueprintable)
class USovTechniqueTestSkillB : public USovTechniqueSkill
{
	GENERATED_BODY()
public: USovTechniqueTestSkillB(const FObjectInitializer& ObjectInitializer);
};
UCLASS(Transient, NotBlueprintable)
class USovTechniqueTestSkillC : public USovTechniqueSkill
{
	GENERATED_BODY()
public: USovTechniqueTestSkillC(const FObjectInitializer& ObjectInitializer);
};
UCLASS(Transient, NotBlueprintable)
class ASovTechniqueTestEncounter : public ASovEncounterDirector
{
	GENERATED_BODY()
public:
	void SetTestState(ESovEncounterState Value) { State = Value; }
};
