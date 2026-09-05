// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovTechniqueRuntimeTestFixtures.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Sovereign/SovGameplayTags.h"
#include "Progression/SovTechniqueComponent.h"

USovTechniqueCoreTestAbility::USovTechniqueCoreTestAbility()
{
	bRequiresAllowedWeapon=false;
	FGameplayTagContainer Tags=GetAssetTags(); Tags.AddTag(FSovGameplayTags::Get().Ability_Echo_Tarrik_CinderSlam); SetAssetTags(Tags);
}

void USovTechniqueSnapshotTestObserver::OnChanged(FGameplayTag Protagonist, int32 Available, int32 Earned)
{
	++Notifications;
	AvailableAtNotification = Available;
	bCaptureSucceeded = USovEncounterSnapshotLibrary::CaptureComponent(Techniques.Get(), Record);
}

USovTechniqueTestEffect::USovTechniqueTestEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UNarrativeAttributeSetBase::GetDamageResistanceAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(10.f));
	Modifiers.Add(Modifier);
}
USovTechniqueTestPerkA::USovTechniqueTestPerkA(const FObjectInitializer& O) : Super(O)
{
	MaxLevels = 4;
	PersistentEffects.Add(USovTechniqueTestEffect::StaticClass());
	GrantedAbilities.Add(USovTechniqueOwnedTestAbility::StaticClass());
}
USovTechniqueTestPerkB::USovTechniqueTestPerkB(const FObjectInitializer& O) : Super(O) { MaxLevels = 4; PersistentEffects.Add(USovTechniqueTestEffect::StaticClass()); AugmentedAbility=FSovGameplayTags::Get().Ability_Echo_Tarrik_CinderSlam; }
USovTechniqueTestPerkC::USovTechniqueTestPerkC(const FObjectInitializer& O) : Super(O) { MaxLevels = 4; PersistentEffects.Add(USovTechniqueTestEffect::StaticClass()); }
USovTechniqueTestPerkD::USovTechniqueTestPerkD(const FObjectInitializer& O) : Super(O) { MaxLevels = 4; PersistentEffects.Add(USovTechniqueTestEffect::StaticClass()); AugmentedAbility=FSovGameplayTags::Get().Ability_Echo_Tarrik_CinderSlam; }
USovTechniqueTestPerkE::USovTechniqueTestPerkE(const FObjectInitializer& O) : Super(O) { MaxLevels = 4; PersistentEffects.Add(USovTechniqueTestEffect::StaticClass()); }
USovTechniqueTestPerkF::USovTechniqueTestPerkF(const FObjectInitializer& O) : Super(O) { MaxLevels = 4; PersistentEffects.Add(USovTechniqueTestEffect::StaticClass()); }
USovTechniqueTestSkillA::USovTechniqueTestSkillA(const FObjectInitializer& O) : Super(O)
{
	Protagonist = FSovGameplayTags::Get().Character_Player_Tarrik; BranchId = TEXT("Guard");
	FPerkConfig A; A.Perk = USovTechniqueTestPerkA::StaticClass(); Perks.Add(A);
	FPerkConfig B; B.Perk = USovTechniqueTestPerkB::StaticClass(); Perks.Add(B);
}
USovTechniqueTestSkillB::USovTechniqueTestSkillB(const FObjectInitializer& O) : Super(O)
{
	Protagonist = FSovGameplayTags::Get().Character_Player_Tarrik; BranchId = TEXT("Command");
	FPerkConfig A; A.Perk = USovTechniqueTestPerkC::StaticClass(); Perks.Add(A);
	FPerkConfig B; B.Perk = USovTechniqueTestPerkD::StaticClass(); Perks.Add(B);
}
USovTechniqueTestSkillC::USovTechniqueTestSkillC(const FObjectInitializer& O) : Super(O)
{
	Protagonist = FSovGameplayTags::Get().Character_Player_Tarrik; BranchId = TEXT("Cinder");
	FPerkConfig A; A.Perk = USovTechniqueTestPerkE::StaticClass(); Perks.Add(A);
	FPerkConfig B; B.Perk = USovTechniqueTestPerkF::StaticClass(); Perks.Add(B);
}
